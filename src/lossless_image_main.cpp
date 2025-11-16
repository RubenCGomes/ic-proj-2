#include "lossless_image.hpp"
#include <iostream>
#include <string>
#include <cstdlib>

void printUsage(const char* prog) {
    std::cerr << "Usage:\n";
    std::cerr << "  Encode: " << prog << " encode <input.ppm> <output.gimg> <predictor> <m> <blockSize> [-v] [-auto]\n";
    std::cerr << "  Decode: " << prog << " decode <input.gimg> <output.ppm> [-v]\n";
    std::cerr << "\nPredictors (JPEG lossless modes 1-7 + JPEG-LS):\n";
    std::cerr << "  0 = NONE (no prediction - baseline)\n";
    std::cerr << "  1 = LEFT (a)\n";
    std::cerr << "  2 = UP (b)\n";
    std::cerr << "  3 = UP_LEFT (c)\n";
    std::cerr << "  4 = a + b - c\n";
    std::cerr << "  5 = a + (b - c)/2\n";
    std::cerr << "  6 = b + (a - c)/2\n";
    std::cerr << "  7 = (a + b)/2\n";
    std::cerr << "  8 = JPEG-LS (nonlinear - best for natural images)\n";
    std::cerr << "  -1 = AUTO (test all and pick best) ← NEW!\n";
    std::cerr << "\nParameters:\n";
    std::cerr << "  m          : Golomb parameter (0 = adaptive, >0 = fixed)\n";
    std::cerr << "  blockSize  : Block size for adaptive m (0 = per-row, >0 = per block)\n";
    std::cerr << "  -v         : Verbose mode\n";
    std::cerr << "  -auto      : Auto-select best predictor (same as predictor=-1)\n";
    std::cerr << "\nExamples:\n";
    std::cerr << "  " << prog << " encode images/lena.ppm lena.gimg 8 0 0 -v      # JPEG-LS predictor\n";
    std::cerr << "  " << prog << " encode images/lena.ppm lena.gimg -1 0 0 -v     # Auto-select best\n";
    std::cerr << "  " << prog << " encode images/lena.ppm lena.gimg 0 0 0 -v -auto # Auto-select best\n";
    std::cerr << "  " << prog << " decode lena.gimg lena_decoded.ppm -v\n";
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string cmd = argv[1];
    bool verbose = false;
    bool autoSelect = false;
    
    // Check for flags
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-v") {
            verbose = true;
        }
        if (std::string(argv[i]) == "-auto") {
            autoSelect = true;
        }
    }
    
    if (cmd == "encode") {
        if (argc < 7) {
            std::cerr << "Error: Encode requires 5 parameters + optional -v/-auto\n";
            printUsage(argv[0]);
            return 1;
        }
        
        std::string inputImage = argv[2];
        std::string outputFile = argv[3];
        int predictorNum = std::atoi(argv[4]);
        uint32_t m = std::atoi(argv[5]);
        uint32_t blockSize = std::atoi(argv[6]);
        
        // Handle auto-selection
        if (predictorNum == -1 || autoSelect) {
            autoSelect = true;
            predictorNum = 8;  // Default fallback (JPEG-LS)
        }
        
        if (predictorNum < -1 || predictorNum > 8) {
            std::cerr << "Error: Invalid predictor (must be -1 to 8)\n";
            return 1;
        }
        
        ImagePredictor predictor = static_cast<ImagePredictor>(predictorNum);
        
        bool ok = encodeImage(inputImage, outputFile, predictor, m, blockSize, verbose, autoSelect);
        return ok ? 0 : 2;
        
    } else if (cmd == "decode") {
        if (argc < 4) {
            std::cerr << "Error: Decode requires 2 parameters + optional -v\n";
            printUsage(argv[0]);
            return 1;
        }
        
        std::string inputFile = argv[2];
        std::string outputImage = argv[3];
        
        bool ok = decodeImage(inputFile, outputImage, verbose);
        return ok ? 0 : 2;
        
    } else {
        std::cerr << "Error: Unknown command '" << cmd << "'\n";
        printUsage(argv[0]);
        return 1;
    }
}