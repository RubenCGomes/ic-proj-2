#include "lossless_audio.hpp"
#include <iostream>
#include <string>
#include <cstdlib>

void printUsage(const char* prog) {
    std::cerr << "Usage:\n";
    std::cerr << "  Encode: " << prog << " encode <input.wav> <output.gblk> <blockSamples> <m> <predictorOrder> [-v]\n";
    std::cerr << "  Decode: " << prog << " decode <input.gblk> <output.wav> [-v]\n";
    std::cerr << "\nParameters:\n";
    std::cerr << "  blockSamples    : Frames per block (e.g., 4096)\n";
    std::cerr << "  m               : Golomb parameter (0=adaptive, >0=fixed)\n";
    std::cerr << "  predictorOrder  : 0=none, 1=s[n-1], 2=2*s[n-1]-s[n-2], 3=3*s[n-1]-3*s[n-2]+s[n-3]\n";
    std::cerr << "  -v              : Verbose mode\n";
    std::cerr << "\nExamples:\n";
    std::cerr << "  " << prog << " encode input.wav out.gblk 4096 0 2 -v   # Adaptive m, 2-tap predictor\n";
    std::cerr << "  " << prog << " encode input.wav out.gblk 4096 32 1 -v  # Fixed m=32, 1-tap predictor\n";
    std::cerr << "  " << prog << " decode out.gblk output.wav -v\n";
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    std::string cmd = argv[1];
    bool verbose = false;

    // Check for -v flag
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-v") {
            verbose = true;
            break;
        }
    }

    if (cmd == "encode") {
        if (argc < 7) {
            std::cerr << "Error: Encode requires 5 parameters + optional -v\n";
            printUsage(argv[0]);
            return 1;
        }

        std::string inWav = argv[2];
        std::string outFile = argv[3];
        uint32_t blockSamples = std::atoi(argv[4]);
        uint32_t m = std::atoi(argv[5]);
        uint32_t predictorOrder = std::atoi(argv[6]);

        // Validate predictor order
        if (predictorOrder > 3) {
            std::cerr << "Error: predictorOrder must be 0-3 (got " << predictorOrder << ")\n";
            return 1;
        }

        bool ok = encodeWavWithGolomb(inWav, outFile, m, blockSamples, predictorOrder, verbose);
        return ok ? 0 : 2;

    } else if (cmd == "decode") {
        if (argc < 4) {
            std::cerr << "Error: Decode requires 2 parameters + optional -v\n";
            printUsage(argv[0]);
            return 1;
        }

        std::string inFile = argv[2];
        std::string outWav = argv[3];

        bool ok = decodeGolombToWav(inFile, outWav, verbose);
        return ok ? 0 : 2;

    } else {
        std::cerr << "Error: Unknown command '" << cmd << "'\n";
        printUsage(argv[0]);
        return 1;
    }
}