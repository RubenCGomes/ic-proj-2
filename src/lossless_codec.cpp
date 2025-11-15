#include "lossless_codec.hpp"
#include <sndfile.h>
#include <vector>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <cmath>
#include <iomanip>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <chrono>

// File header
struct FileHeader {
    uint32_t magic;         // 'GBLK'
    uint32_t sampleRate;
    uint16_t channels;
    uint64_t frames;
    uint32_t blockSamples;
    uint32_t sf_format;     // original SF_INFO.format to allow exact reconstruction
};

// Fast BitWriter (MSB-first)
struct BitWriter {
    std::vector<uint8_t> bytes;
    uint8_t cur = 0;
    int bitPos = 0; // 0..7
    void writeBit(uint8_t b) {
        cur |= (b & 1u) << (7 - bitPos);
        ++bitPos;
        if (bitPos == 8) { bytes.push_back(cur); cur = 0; bitPos = 0; }
    }
    void writeBits(uint32_t value, int n) {
        for (int i = n - 1; i >= 0; --i) writeBit((value >> i) & 1u);
    }
    void writeUnary(uint32_t q) {
        for (uint32_t i = 0; i < q; ++i) writeBit(0);
        writeBit(1);
    }
    void flush() { if (bitPos) { bytes.push_back(cur); cur = 0; bitPos = 0; } }
    size_t bitCount() const { return bytes.size() * 8 + bitPos; }
};

// Fast BitReader (MSB-first)
struct BitReader {
    const uint8_t* data = nullptr;
    size_t byteLen = 0;
    size_t bytePos = 0;
    int bitPos = 0;
    void set(const std::vector<uint8_t>& buf) { data = buf.empty() ? nullptr : buf.data(); byteLen = buf.size(); bytePos = 0; bitPos = 0; }
    bool readBit(uint8_t &out) {
        if (!data || bytePos >= byteLen) return false;
        out = (data[bytePos] >> (7 - bitPos)) & 1u;
        ++bitPos;
        if (bitPos == 8) { bitPos = 0; ++bytePos; }
        return true;
    }
    bool readBits(uint32_t &out, int n) {
        out = 0;
        uint8_t b;
        for (int i = 0; i < n; ++i) {
            if (!readBit(b)) return false;
            out = (out << 1) | b;
        }
        return true;
    }
};

// progress bar (verbose only)
static void showProgressBar(double fraction, uint64_t processed, uint64_t total, bool verbose) {
    if (!verbose) return;
    const int width = 50;
    int pos = static_cast<int>(fraction * width);
    if (pos < 0) pos = 0; if (pos > width) pos = width;
    std::cout << '\r' << "[";
    for (int i = 0; i < width; ++i) {
        if (i < pos) std::cout << '=';
        else if (i == pos) std::cout << '>';
        else std::cout << ' ';
    }
    std::cout << "] " << std::setw(6) << std::fixed << std::setprecision(2) << (fraction * 100.0) << "%";
    std::cout << " (" << processed << "/" << total << " samples)" << std::flush;
}

// signed <-> unsigned mapping (Golomb folding)
static inline uint32_t mapSignedToUnsigned(int32_t v) {
    return (v >= 0) ? (static_cast<uint32_t>(v) << 1u) : ((static_cast<uint32_t>(-v) << 1u) - 1u);
}
static inline int32_t mapUnsignedToSigned(uint32_t u) {
    return (u & 1u) ? -static_cast<int32_t>((u + 1u) >> 1) : static_cast<int32_t>(u >> 1);
}

// Encode function
bool encodeWavWithGolomb(const std::string& inWav, const std::string& outFile,
                         uint32_t m, uint32_t blockSamples, bool verbose) {
    SF_INFO sfinfo{};
    SNDFILE* in = sf_open(inWav.c_str(), SFM_READ, &sfinfo);
    if (!in) { std::cerr << "Failed to open input WAV: " << inWav << "\n"; return false; }

    std::ofstream ofs(outFile, std::ios::binary);
    if (!ofs) { sf_close(in); std::cerr << "Cannot open output: " << outFile << "\n"; return false; }

    FileHeader hdr;
    hdr.magic = 0x47424C4Bu;
    hdr.sampleRate = static_cast<uint32_t>(sfinfo.samplerate);
    hdr.channels = static_cast<uint16_t>(sfinfo.channels);
    hdr.frames = static_cast<uint64_t>(sfinfo.frames);
    hdr.blockSamples = blockSamples;
    hdr.sf_format = static_cast<uint32_t>(sfinfo.format);
    ofs.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

    const uint32_t initialM = (m == 0 ? 4u : m);
    uint32_t currentM = initialM;

    std::vector<int32_t> buffer(blockSamples * sfinfo.channels);
    std::vector<int32_t> prev(sfinfo.channels, 0);

    uint64_t totalSamples = static_cast<uint64_t>(sfinfo.frames) * sfinfo.channels;
    uint64_t processedSamples = 0;
    size_t blockIndex = 0;
    uint64_t encodedBytes = 0;

    const size_t intraBlockUpdateFrames = std::max<size_t>(64, blockSamples / 64);

    sf_count_t readFrames;
    auto tstart = std::chrono::steady_clock::now();

    while ((readFrames = sf_readf_int(in, buffer.data(), blockSamples)) > 0) {
        ++blockIndex;
        if (verbose) {
            std::cout << "\n[block " << blockIndex << "] readFrames=" << readFrames
                      << " samples=" << (readFrames * sfinfo.channels)
                      << " m=" << currentM << "\n";
        }

        BitWriter bw;
        bw.bytes.reserve(static_cast<size_t>(readFrames) * sfinfo.channels / 2 + 16);

        uint64_t blockSampleCount = static_cast<uint64_t>(readFrames) * sfinfo.channels;
        uint64_t blockProcessed = 0;

        // precompute Golomb truncated binary parameters for currentM
        uint32_t b = (currentM <= 1) ? 1u : static_cast<uint32_t>(std::ceil(std::log2(static_cast<double>(currentM))));
        uint32_t pow2b = (1u << b);
        uint32_t t = pow2b - currentM;

        for (sf_count_t i = 0; i < readFrames; ++i) {
            for (int ch = 0; ch < sfinfo.channels; ++ch) {
                int idx = i * sfinfo.channels + ch;
                int32_t s = buffer[idx];
                int32_t pred = prev[ch];
                int32_t resid = s - pred;
                uint32_t mapped = mapSignedToUnsigned(resid);

                // Golomb encode mapped with parameter currentM
                uint32_t q = mapped / currentM;
                uint32_t r = mapped % currentM;

                // unary for q: q zeros then a one
                for (uint32_t qi = 0; qi < q; ++qi) bw.writeBit(0);
                bw.writeBit(1);

                // truncated binary for remainder
                if (currentM != 1) {
                    if (r < t) {
                        // b-1 bits
                        if (b > 1) bw.writeBits(r, b - 1);
                    } else {
                        bw.writeBits(r + t, b);
                    }
                }

                prev[ch] = s;
                ++blockProcessed;
            }

            if ((i % intraBlockUpdateFrames) == 0) {
                uint64_t vis = processedSamples + blockProcessed;
                double frac = totalSamples ? static_cast<double>(vis) / static_cast<double>(totalSamples) : 0.0;
                if (frac > 1.0) frac = 1.0;
                showProgressBar(frac, vis, totalSamples, verbose);
            }
        }

        // adaptive m: use mean bits/sample in this block as proxy (cheap and simple)
        if (m == 0) {
            double avgBitsPerSample = static_cast<double>(bw.bitCount()) / static_cast<double>(blockSampleCount);
            uint32_t newM = std::max<uint32_t>(1u, static_cast<uint32_t>(std::round(avgBitsPerSample)));
            if (newM != currentM) {
                currentM = newM;
                if (verbose) std::cout << "[debug] adaptive m -> " << currentM << " (after block " << blockIndex << ")\n";
                // recalc b / t next block
            }
        }

        bw.flush();
        uint32_t bitsCount = static_cast<uint32_t>(bw.bitCount());
        uint32_t byteCount = static_cast<uint32_t>(bw.bytes.size());

        // write per-block header + bytes
        ofs.write(reinterpret_cast<const char*>(&currentM), sizeof(currentM));
        ofs.write(reinterpret_cast<const char*>(&bitsCount), sizeof(bitsCount));
        if (byteCount) ofs.write(reinterpret_cast<const char*>(bw.bytes.data()), byteCount);

        encodedBytes += sizeof(currentM) + sizeof(bitsCount) + byteCount;

        processedSamples += blockProcessed;
        double frac = totalSamples ? static_cast<double>(processedSamples) / static_cast<double>(totalSamples) : 0.0;
        if (frac > 1.0) frac = 1.0;
        showProgressBar(frac, processedSamples, totalSamples, verbose);

        if (verbose) {
            auto tnow = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(tnow - tstart).count();
            double rate = elapsed > 0.0 ? processedSamples / elapsed : 0.0;
            std::cout << "  -> block " << blockIndex << " bits=" << bitsCount << " bytes=" << byteCount
                      << "  encoded_total=" << encodedBytes << "  rate=" << static_cast<int>(rate) << " samples/s\n";
        }
    }

    if (verbose) {
        std::cout << "\nEncoding finished. Encoded bytes=" << encodedBytes << "\n";
    }

    sf_close(in);
    ofs.close();
    return true;
}

// Decode function (must match encoder exactly)
bool decodeGolombToWav(const std::string& inFile, const std::string& outWav, bool verbose) {
    std::ifstream ifs(inFile, std::ios::binary);
    if (!ifs) { std::cerr << "Cannot open input: " << inFile << "\n"; return false; }

    FileHeader hdr;
    ifs.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!ifs) { std::cerr << "Invalid header\n"; return false; }
    if (hdr.magic != 0x47424C4Bu) { std::cerr << "Not a GBLK file\n"; return false; }

    SF_INFO sfinfo{};
    sfinfo.channels = hdr.channels;
    sfinfo.samplerate = hdr.sampleRate;
    sfinfo.frames = static_cast<sf_count_t>(hdr.frames);
    sfinfo.format = static_cast<int>(hdr.sf_format);
    // If original format is zero/unexpected, fall back to 16-bit WAV
    if (sfinfo.format == 0) sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* out = sf_open(outWav.c_str(), SFM_WRITE, &sfinfo);
    if (!out) { std::cerr << "Cannot open output WAV: " << outWav << "\n"; return false; }

    std::vector<int32_t> outbuf;
    outbuf.reserve(hdr.blockSamples * hdr.channels);

    std::vector<int32_t> prev(hdr.channels, 0);

    uint64_t totalSamples = hdr.frames * hdr.channels;
    uint64_t producedSamples = 0;
    size_t blockIndex = 0;

    while (true) {
        uint32_t blockM;
        ifs.read(reinterpret_cast<char*>(&blockM), sizeof(blockM));
        if (!ifs) break;
        uint32_t bitsCount;
        ifs.read(reinterpret_cast<char*>(&bitsCount), sizeof(bitsCount));
        if (!ifs) break;
        uint32_t byteCount = (bitsCount + 7) / 8;
        std::vector<uint8_t> bytes(byteCount);
        if (byteCount) ifs.read(reinterpret_cast<char*>(bytes.data()), byteCount);

        ++blockIndex;
        BitReader br;
        br.set(bytes);

        uint64_t blockSamples = static_cast<uint64_t>(hdr.blockSamples) * hdr.channels;
        // decode until we exhaust bits or fill block sample count or reach total
        for (uint64_t s = 0; s < static_cast<uint64_t>(hdr.blockSamples); ++s) {
            for (uint16_t ch = 0; ch < hdr.channels; ++ch) {
                // read unary q (zeros then one)
                uint32_t q = 0;
                uint8_t bit;
                // Expect unary encoded as q zeros then a one. Encoder wrote q zeros then 1.
                // Read bits until 1 is found; count zeros as q.
                while (true) {
                    if (!br.readBit(bit)) goto block_done; // not enough bits: end
                    if (bit == 1) break;
                    ++q;
                }

                uint32_t val = 0;
                if (blockM == 1) {
                    val = q;
                } else {
                    // compute b and cutoff t
                    uint32_t b = static_cast<uint32_t>(std::ceil(std::log2(static_cast<double>(blockM))));
                    uint32_t pow2b = (1u << b);
                    uint32_t t = pow2b - blockM;

                    uint32_t x = 0;
                    if (!br.readBits(x, b - 1)) goto block_done;
                    if (x < t) {
                        val = q * blockM + x;
                    } else {
                        uint32_t more = 0;
                        if (!br.readBit(bit)) goto block_done;
                        x = (x << 1) | (bit & 1u);
                        val = q * blockM + (x - t);
                    }
                }

                int32_t resid = mapUnsignedToSigned(val);
                int32_t sample = prev[ch] + resid;
                prev[ch] = sample;
                outbuf.push_back(sample);
                ++producedSamples;

                // if buffer big enough, write frames
                if (outbuf.size() >= static_cast<size_t>(hdr.blockSamples) * hdr.channels) {
                    sf_count_t framesToWrite = static_cast<sf_count_t>(outbuf.size() / hdr.channels);
                    sf_writef_int(out, reinterpret_cast<const int*>(outbuf.data()), framesToWrite);
                    outbuf.clear();
                }
            }
        }

block_done:
        // write any remaining
        if (!outbuf.empty()) {
            sf_count_t framesToWrite = static_cast<sf_count_t>(outbuf.size() / hdr.channels);
            sf_writef_int(out, reinterpret_cast<const int*>(outbuf.data()), framesToWrite);
            outbuf.clear();
        }

        double frac = totalSamples ? static_cast<double>(producedSamples) / static_cast<double>(totalSamples) : 0.0;
        if (frac > 1.0) frac = 1.0;
        showProgressBar(frac, producedSamples, totalSamples, verbose);
    }

    if (verbose) std::cout << "\nDecoding finished. Produced samples=" << producedSamples << "\n";
    sf_close(out);
    ifs.close();
    return true;
}