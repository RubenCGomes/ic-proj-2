#include "lossless_audio.hpp"
#include "golomb.hpp"
#include "bit_stream.h"
#include <sndfile.h>
#include <vector>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <cmath>
#include <iomanip>
#include <algorithm>

static void showProgressBar(double fraction, uint64_t processed, uint64_t total, bool verbose) {
    if (!verbose) return;
    const int width = 50;
    int pos = static_cast<int>(fraction * width);
    std::cout << "\r[";
    for (int i = 0; i < width; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << std::setw(6) << std::fixed << std::setprecision(2) << (fraction * 100.0) << "%";
    std::cout << " (" << processed << "/" << total << " samples)" << std::flush;
}

// Predictor function: computes prediction based on order
static int32_t computePrediction(uint32_t order, const std::vector<int16_t>& history) {
    // history[0] = s[n-1], history[1] = s[n-2], history[2] = s[n-3]
    int32_t pred = 0;
    
    switch (order) {
        case 0:
            // No prediction
            pred = 0;
            break;
        case 1:
            // 1-tap: pred = s[n-1]
            pred = history[0];
            break;
        case 2:
            // 2-tap: pred = 2*s[n-1] - s[n-2]
            pred = 2 * static_cast<int32_t>(history[0]) - static_cast<int32_t>(history[1]);
            break;
        case 3:
            // 3-tap: pred = 3*s[n-1] - 3*s[n-2] + s[n-3]
            pred = 3 * static_cast<int32_t>(history[0]) 
                 - 3 * static_cast<int32_t>(history[1]) 
                 + static_cast<int32_t>(history[2]);
            break;
        default:
            pred = 0;
            break;
    }
    
    // Clamp to valid 16-bit range
    return std::max<int32_t>(-32768, std::min<int32_t>(32767, pred));
}

bool encodeWavWithGolomb(const std::string& inWav, const std::string& outFile, uint32_t m, 
                         uint32_t blockSamples, uint32_t predictorOrder, bool verbose) {
    SF_INFO sfinfo{};
    SNDFILE* in = sf_open(inWav.c_str(), SFM_READ, &sfinfo);
    if (!in) {
        if (verbose) std::cerr << "Failed to open input WAV: " << inWav << "\n";
        return false;
    }

    std::fstream ofs(outFile, std::ios::out | std::ios::binary);
    if (!ofs) {
        if (verbose) std::cerr << "Failed to open output file: " << outFile << "\n";
        sf_close(in);
        return false;
    }

    BitStream bs(ofs, STREAM_WRITE);

    if (verbose) {
        std::cout << "Encoding: " << inWav << " -> " << outFile << "\n";
        std::cout << "Sample rate: " << sfinfo.samplerate << ", channels: " << sfinfo.channels
                  << ", frames: " << sfinfo.frames << "\n";
        std::cout << "Block samples: " << blockSamples << ", initial m: " << (m == 0 ? "adaptive" : std::to_string(m)) << "\n";
        std::cout << "Predictor order: " << predictorOrder;
        switch (predictorOrder) {
            case 0: std::cout << " (none)\n"; break;
            case 1: std::cout << " (1-tap: s[n-1])\n"; break;
            case 2: std::cout << " (2-tap: 2*s[n-1]-s[n-2])\n"; break;
            case 3: std::cout << " (3-tap: 3*s[n-1]-3*s[n-2]+s[n-3])\n"; break;
        }
        if (sfinfo.channels == 2) {
            std::cout << "Using Mid/Side stereo coding\n";
        }
    }

    // Write file header (add predictor order!)
    bs.write_n_bits(sfinfo.samplerate, 32);
    bs.write_n_bits(sfinfo.channels, 16);
    bs.write_n_bits(sfinfo.frames, 64);
    bs.write_n_bits(blockSamples, 32);
    bs.write_n_bits(predictorOrder, 8);  // NEW: store predictor order

    std::vector<short> buffer(blockSamples * sfinfo.channels);
    
    int numEncodedChannels = (sfinfo.channels == 2) ? 2 : sfinfo.channels;
    
    // Predictor history: need up to 3 previous samples per channel
    std::vector<std::vector<int16_t>> history(numEncodedChannels, std::vector<int16_t>(3, 0));

    sf_count_t readFrames;
    uint64_t totalSamples = static_cast<uint64_t>(sfinfo.frames) * sfinfo.channels;
    uint64_t processedSamples = 0;

    const size_t updateInterval = std::max<size_t>(512, blockSamples / 8);
    size_t blockIndex = 0;

    while ((readFrames = sf_readf_short(in, buffer.data(), blockSamples)) > 0) {
        ++blockIndex;

        // For stereo: convert to Mid/Side (LOSSLESS VERSION)
        std::vector<int16_t> encodingChannels;
        if (sfinfo.channels == 2) {
            encodingChannels.reserve(readFrames * 2);
            for (sf_count_t i = 0; i < readFrames; ++i) {
                int16_t left = buffer[i * 2];
                int16_t right = buffer[i * 2 + 1];
                
                // LOSSLESS Mid/Side transform (matches decoder exactly)
                int16_t side = left - right;
                int16_t mid = right + (side >> 1);  // mid = (L+R)/2 rounded toward right
                
                encodingChannels.push_back(mid);
                encodingChannels.push_back(side);
            }
        } else {
            encodingChannels.assign(buffer.begin(), buffer.begin() + readFrames * sfinfo.channels);
        }

        // Compute residuals for this block
        std::vector<int32_t> residuals;
        residuals.reserve(encodingChannels.size());

        for (size_t i = 0; i < static_cast<size_t>(readFrames); ++i) {
            for (int ch = 0; ch < numEncodedChannels; ++ch) {
                int idx = i * numEncodedChannels + ch;
                int16_t sample = encodingChannels[idx];

                // Compute prediction using selected predictor order
                int32_t pred = computePrediction(predictorOrder, history[ch]);

                int32_t resid = static_cast<int32_t>(sample) - pred;
                residuals.push_back(resid);

                // Update history: shift left and add new sample
                history[ch][2] = history[ch][1];  // s[n-3] ← s[n-2]
                history[ch][1] = history[ch][0];  // s[n-2] ← s[n-1]
                history[ch][0] = sample;           // s[n-1] ← s[n]
            }
        }

        // Compute optimal m for this block (adaptive)
        uint32_t blockM = m;
        if (m == 0) {
            // Compute mean absolute residual
            double sumAbs = 0.0;
            for (auto r : residuals) sumAbs += std::abs(r);
            double meanAbs = residuals.empty() ? 1.0 : sumAbs / residuals.size();
            
            // Theoretically optimal m for geometric distribution (Golomb 1966)
            // α = mean / (mean + 1)
            // m = ceil(-1 / log₂(α))
            double alpha = meanAbs / (meanAbs + 1.0);
            blockM = std::ceil(-1.0 / std::log2(alpha));
        }

        uint32_t b = static_cast<uint32_t>(std::ceil(std::log2(static_cast<double>(blockM))));
        uint32_t cutoff = (1u << b) - blockM;

        // Write block header
        bs.write_n_bits(blockM, 16);
        bs.write_n_bits(static_cast<uint32_t>(residuals.size()), 32);

        if (verbose && blockIndex % 10 == 1) {
            std::cout << "\n[block " << blockIndex << "] m=" << blockM << " samples=" << residuals.size() << "\n";
        }

        // Golomb encode residuals
        for (auto resid : residuals) {
            uint32_t mapped = (resid >= 0) ? static_cast<uint32_t>(resid) << 1u
                                          : (static_cast<uint32_t>(-resid) << 1u) - 1u;

            uint32_t q = mapped / blockM;
            uint32_t r = mapped % blockM;

            if (q > 10000) {
                if (verbose) std::cerr << "\nWarning: huge q=" << q << " resid=" << resid << " m=" << blockM << "\n";
                q = 10000;
            }

            for (uint32_t j = 0; j < q; ++j) bs.write_bit(0);
            bs.write_bit(1);

            if (r < cutoff) {
                if (b > 1) bs.write_n_bits(r, b - 1);
            } else {
                uint32_t adjusted = r + cutoff;
                bs.write_n_bits(adjusted, b);
            }

            ++processedSamples;

            if ((processedSamples % updateInterval) == 0 && verbose) {
                double frac = totalSamples ? static_cast<double>(processedSamples) / static_cast<double>(totalSamples) : 0.0;
                if (frac > 1.0) frac = 1.0;
                showProgressBar(frac, processedSamples, totalSamples, verbose);
            }
        }
    }

    bs.close();
    sf_close(in);

    if (verbose) {
        double frac = 1.0;
        showProgressBar(frac, processedSamples, totalSamples, verbose);
        std::cout << "\nEncoding finished.\n";
        std::cout << "Output file: " << outFile << "\n";
    }

    return true;
}

bool decodeGolombToWav(const std::string& inFile, const std::string& outWav, bool verbose) {
    std::fstream ifs(inFile, std::ios::in | std::ios::binary);
    if (!ifs) {
        if (verbose) std::cerr << "Failed to open input file: " << inFile << "\n";
        return false;
    }

    BitStream bs(ifs, STREAM_READ);

    // Read file header (including predictor order)
    uint32_t samplerate = bs.read_n_bits(32);
    uint16_t channels = bs.read_n_bits(16);
    uint64_t frames = bs.read_n_bits(64);
    uint32_t blockSamples = bs.read_n_bits(32);
    uint32_t predictorOrder = bs.read_n_bits(8);  // NEW: read predictor order

    if (verbose) {
        std::cout << "Decoding: " << inFile << " -> " << outWav << "\n";
        std::cout << "Sample rate: " << samplerate << ", channels: " << channels
                  << ", frames: " << frames << ", block size: " << blockSamples << "\n";
        std::cout << "Predictor order: " << predictorOrder;
        switch (predictorOrder) {
            case 0: std::cout << " (none)\n"; break;
            case 1: std::cout << " (1-tap)\n"; break;
            case 2: std::cout << " (2-tap)\n"; break;
            case 3: std::cout << " (3-tap)\n"; break;
        }
        if (channels == 2) {
            std::cout << "Using Mid/Side stereo decoding\n";
        }
    }

    SF_INFO sfinfo{};
    sfinfo.samplerate = samplerate;
    sfinfo.channels = channels;
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* out = sf_open(outWav.c_str(), SFM_WRITE, &sfinfo);
    if (!out) {
        if (verbose) std::cerr << "Failed to create output WAV: " << outWav << "\n";
        bs.close();
        return false;
    }

    uint64_t totalSamples = frames * channels;
    uint64_t processedSamples = 0;
    
    int numEncodedChannels = (channels == 2) ? 2 : channels;
    std::vector<std::vector<int16_t>> history(numEncodedChannels, std::vector<int16_t>(3, 0));

    const size_t bufferFrames = 4096;
    std::vector<short> outBuffer;
    outBuffer.reserve(bufferFrames * channels);

    size_t blockIndex = 0;

    while (processedSamples < totalSamples) {
        ++blockIndex;

        uint32_t blockM = bs.read_n_bits(16);
        uint32_t blockSampleCount = bs.read_n_bits(32);

        if (blockM == 0 || blockSampleCount == 0) {
            if (verbose) std::cerr << "\nWarning: blockM or sampleCount is 0 (EOF?)\n";
            break;
        }

        uint32_t b = static_cast<uint32_t>(std::ceil(std::log2(static_cast<double>(blockM))));
        uint32_t cutoff = (1u << b) - blockM;

        if (verbose && blockIndex % 10 == 1) {
            std::cout << "\n[decode block " << blockIndex << "] m=" << blockM << " samples=" << blockSampleCount << "\n";
        }

        std::vector<int16_t> decodedSamples;
        decodedSamples.reserve(blockSampleCount);

        for (uint32_t s = 0; s < blockSampleCount; ++s) {
            uint32_t q = 0;
            int bit;
            while ((bit = bs.read_bit()) == 0) {
                ++q;
                if (q > 100000) {
                    if (verbose) std::cerr << "\nError: runaway unary\n";
                    sf_close(out);
                    bs.close();
                    return false;
                }
            }
            if (bit == EOF) break;

            uint32_t r = 0;
            if (b > 1) {
                r = bs.read_n_bits(b - 1);
            }
            if (r < cutoff) {
                // done
            } else {
                int extraBit = bs.read_bit();
                if (extraBit == EOF) break;
                r = (r << 1) | extraBit;
                r -= cutoff;
            }

            uint32_t mapped = q * blockM + r;

            int32_t resid = (mapped & 1u) ? -static_cast<int32_t>((mapped + 1) >> 1)
                                         : static_cast<int32_t>(mapped >> 1);

            int ch = s % numEncodedChannels;

            // Use same predictor as encoder
            int32_t pred = computePrediction(predictorOrder, history[ch]);

            int16_t sample = static_cast<int16_t>(pred + resid);

            decodedSamples.push_back(sample);

            // Update history
            history[ch][2] = history[ch][1];
            history[ch][1] = history[ch][0];
            history[ch][0] = sample;
        }

        // Convert Mid/Side → L/R for stereo (LOSSLESS VERSION)
        if (channels == 2) {
            for (size_t i = 0; i < decodedSamples.size(); i += 2) {
                int16_t mid = decodedSamples[i];
                int16_t side = decodedSamples[i + 1];
                
                // LOSSLESS inverse transform (matches encoder exactly)
                int16_t right = mid - (side >> 1);
                int16_t left = right + side;
                
                outBuffer.push_back(left);
                outBuffer.push_back(right);
                processedSamples += 2;
            }
        } else {
            for (auto sample : decodedSamples) {
                outBuffer.push_back(sample);
                ++processedSamples;
            }
        }

        if (outBuffer.size() >= bufferFrames * channels) {
            sf_count_t written = sf_writef_short(out, outBuffer.data(), outBuffer.size() / channels);
            if (written != static_cast<sf_count_t>(outBuffer.size() / channels)) {
                if (verbose) std::cerr << "Write error\n";
                break;
            }
            outBuffer.clear();
        }

        if (verbose && (processedSamples % 10000 == 0)) {
            double frac = totalSamples ? static_cast<double>(processedSamples) / static_cast<double>(totalSamples) : 0.0;
            if (frac > 1.0) frac = 1.0;
            showProgressBar(frac, processedSamples, totalSamples, verbose);
        }
    }

    // Write remaining
    if (!outBuffer.empty()) {
        sf_writef_short(out, outBuffer.data(), outBuffer.size() / channels);
    }

    bs.close();
    sf_close(out);

    if (verbose) {
        std::cout << "\nDecoding finished.\n";
        std::cout << "Output file: " << outWav << "\n";
    }

    return true;
}