#include "lossless_codec.hpp"
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

bool encodeWavWithGolomb(const std::string& inWav, const std::string& outFile, uint32_t m, uint32_t blockSamples, bool verbose) {
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
    }

    // Write file header (add blockSamples so decoder knows block size!)
    bs.write_n_bits(sfinfo.samplerate, 32);
    bs.write_n_bits(sfinfo.channels, 16);
    bs.write_n_bits(sfinfo.frames, 64);
    bs.write_n_bits(blockSamples, 32);  // NEW: write block size

    std::vector<short> buffer(blockSamples * sfinfo.channels);
    std::vector<int16_t> prev1(sfinfo.channels, 0);
    std::vector<int16_t> prev2(sfinfo.channels, 0);

    sf_count_t readFrames;
    uint64_t totalSamples = static_cast<uint64_t>(sfinfo.frames) * sfinfo.channels;
    uint64_t processedSamples = 0;

    const size_t updateInterval = std::max<size_t>(512, blockSamples / 8);
    size_t blockIndex = 0;

    while ((readFrames = sf_readf_short(in, buffer.data(), blockSamples)) > 0) {
        ++blockIndex;

        // Collect residuals for this block
        std::vector<int32_t> residuals;
        residuals.reserve(readFrames * sfinfo.channels);

        for (sf_count_t i = 0; i < readFrames; ++i) {
            for (int ch = 0; ch < sfinfo.channels; ++ch) {
                int idx = i * sfinfo.channels + ch;
                int16_t sample = buffer[idx];

                int32_t pred = 2 * static_cast<int32_t>(prev1[ch]) - static_cast<int32_t>(prev2[ch]);
                pred = std::max<int32_t>(-32768, std::min<int32_t>(32767, pred));

                int32_t resid = static_cast<int32_t>(sample) - pred;
                residuals.push_back(resid);

                prev2[ch] = prev1[ch];
                prev1[ch] = sample;
            }
        }

        // Adaptive m
        uint32_t blockM = m;
        if (m == 0) {
            double sumAbs = 0.0;
            for (auto r : residuals) sumAbs += std::abs(r);
            double meanAbs = residuals.empty() ? 1.0 : sumAbs / residuals.size();
            blockM = std::max<uint32_t>(1, static_cast<uint32_t>(std::round(0.95 * meanAbs)));
            blockM = std::max<uint32_t>(1, std::min<uint32_t>(256, blockM));
        }

        uint32_t b = static_cast<uint32_t>(std::ceil(std::log2(static_cast<double>(blockM))));
        uint32_t cutoff = (1u << b) - blockM;

        // Write per-block header: m + number of samples in this block
        bs.write_n_bits(blockM, 16);
        bs.write_n_bits(static_cast<uint32_t>(residuals.size()), 32);  // NEW: write sample count for this block

        if (verbose && blockIndex % 10 == 1) {
            std::cout << "\n[block " << blockIndex << "] m=" << blockM << " samples=" << residuals.size() << "\n";
        }

        // Encode residuals
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

    // Read file header
    uint32_t samplerate = bs.read_n_bits(32);
    uint16_t channels = bs.read_n_bits(16);
    uint64_t frames = bs.read_n_bits(64);
    uint32_t blockSamples = bs.read_n_bits(32);  // NEW: read block size

    if (verbose) {
        std::cout << "Decoding: " << inFile << " -> " << outWav << "\n";
        std::cout << "Sample rate: " << samplerate << ", channels: " << channels
                  << ", frames: " << frames << ", block size: " << blockSamples << "\n";
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
    std::vector<int16_t> prev1(channels, 0);
    std::vector<int16_t> prev2(channels, 0);

    const size_t bufferFrames = 4096;
    std::vector<short> outBuffer;
    outBuffer.reserve(bufferFrames * channels);

    size_t blockIndex = 0;

    while (processedSamples < totalSamples) {
        ++blockIndex;

        // Read per-block header
        uint32_t blockM = bs.read_n_bits(16);
        uint32_t blockSampleCount = bs.read_n_bits(32);  // NEW: read sample count for this block

        if (blockM == 0 || blockSampleCount == 0) {
            if (verbose) std::cerr << "\nWarning: blockM or sampleCount is 0 (EOF?)\n";
            break;
        }

        uint32_t b = static_cast<uint32_t>(std::ceil(std::log2(static_cast<double>(blockM))));
        uint32_t cutoff = (1u << b) - blockM;

        if (verbose && blockIndex % 10 == 1) {
            std::cout << "\n[decode block " << blockIndex << "] m=" << blockM << " samples=" << blockSampleCount << "\n";
        }

        // Decode exactly blockSampleCount samples
        for (uint32_t s = 0; s < blockSampleCount; ++s) {
            // Read unary quotient
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

            // Read truncated binary remainder
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

            int ch = processedSamples % channels;

            int32_t pred = 2 * static_cast<int32_t>(prev1[ch]) - static_cast<int32_t>(prev2[ch]);
            pred = std::max<int32_t>(-32768, std::min<int32_t>(32767, pred));

            int16_t sample = static_cast<int16_t>(pred + resid);

            outBuffer.push_back(sample);

            prev2[ch] = prev1[ch];
            prev1[ch] = sample;

            ++processedSamples;
        }

        // Write buffer when full
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