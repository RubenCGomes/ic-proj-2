#include "lossless_codec.hpp"
#include <sndfile.h>
#include <iostream>
#include <string>
#include <vector>

static bool compareWavFiles(const std::string& a, const std::string& b) {
    SF_INFO ia{}; SNDFILE* ina = sf_open(a.c_str(), SFM_READ, &ia);
    SF_INFO ib{}; SNDFILE* inb = sf_open(b.c_str(), SFM_READ, &ib);
    if (!ina || !inb) {
        if (ina) sf_close(ina);
        if (inb) sf_close(inb);
        return false;
    }
    if (ia.frames != ib.frames || ia.channels != ib.channels || ia.samplerate != ib.samplerate) {
        sf_close(ina); sf_close(inb); return false;
    }
    sf_count_t frames = ia.frames;
    const size_t bufFrames = 4096;
    std::vector<int> A(bufFrames * ia.channels), B(bufFrames * ib.channels);
    sf_count_t pos = 0;
    while (pos < frames) {
        sf_count_t toread = (frames - pos) > (sf_count_t)bufFrames ? bufFrames : (frames - pos);
        sf_count_t ra = sf_readf_int(ina, A.data(), toread);
        sf_count_t rb = sf_readf_int(inb, B.data(), toread);
        if (ra != rb) { sf_close(ina); sf_close(inb); return false; }
        for (sf_count_t i = 0; i < ra * ia.channels; ++i) if (A[i] != B[i]) { sf_close(ina); sf_close(inb); return false; }
        pos += ra;
    }
    sf_close(ina); sf_close(inb);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "Usage:\n  " << argv[0] << " encode in.wav out.gblk [blockSamples] [m] [-v]\n  " << argv[0] << " decode in.gblk out.wav [-v]\n  " << argv[0] << " test in.wav [-v]\n"; return 1; }

    std::string cmd = argv[1];
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        if (s == "-v" || s == "--verbose") verbose = true;
    }

    if (cmd == "encode") {
        if (argc < 4) { std::cerr << "encode requires input and output\n"; return 1; }
        std::string in = argv[2], out = argv[3];
        uint32_t block = 4096; uint32_t m = 0;
        if (argc >= 5) block = static_cast<uint32_t>(std::stoul(argv[4]));
        if (argc >= 6) m = static_cast<uint32_t>(std::stoul(argv[5]));
        bool ok = encodeWavWithGolomb(in, out, m, block, verbose);
        return ok ? 0 : 2;
    } else if (cmd == "decode") {
        if (argc < 4) { std::cerr << "decode requires input and output\n"; return 1; }
        std::string in = argv[2], out = argv[3];
        bool ok = decodeGolombToWav(in, out, verbose);
        return ok ? 0 : 2;
    } else if (cmd == "test") {
        if (argc < 3) { std::cerr << "test requires input wav\n"; return 1; }
        std::string in = argv[2];
        std::string tmpEncoded = "tmp_test.gblk";
        std::string tmpDecoded = "tmp_test_decoded.wav";
        std::cout << "Running lossless round-trip test (this will encode+decode)\n";
        bool ok = encodeWavWithGolomb(in, tmpEncoded, 0, 4096, verbose);
        if (!ok) { std::cerr << "encode failed\n"; return 2; }
        ok = decodeGolombToWav(tmpEncoded, tmpDecoded, verbose);
        if (!ok) { std::cerr << "decode failed\n"; return 3; }
        bool same = compareWavFiles(in, tmpDecoded);
        if (same) { std::cout << "Round-trip OK: identical samples\n"; std::remove(tmpEncoded.c_str()); std::remove(tmpDecoded.c_str()); return 0; }
        else { std::cerr << "Round-trip FAILED: decoded differs\n"; std::cerr << "Keep files: " << tmpEncoded << " " << tmpDecoded << "\n"; return 4; }
    } else {
        std::cerr << "Unknown command\n"; return 1;
    }
}