#include "lossless_image.hpp"
#include <iostream>
#include <string>
#include <cstdlib>

void printUsage(const char* prog) {
    std::cerr << "Usage:\n";
    std::cerr << "  Encode: " << prog << " encode <input.ppm> <output.gimg> <predictor> <m> <blockSize> [-v]\n";
    std::cerr << "  Decode: " << prog << " decode <input.gimg> <output.ppm> [-v]\n";
    std::cerr << "\nPredictors:\n";
    std::cerr << "  0 = NONE (no prediction)\n";
    std::cerr << "  1 = LEFT (use left pixel)\n";
    std::cerr << "  2 = UP (use upper pixel)\n";
    std::cerr << "  3 = AVERAGE (avg of left and up)\n";
    std::cerr << "  4 = PAETH (PNG predictor)\n";
    std::cerr << "  5 = JPEG-LS (best for natural images)\n";
    std::cerr << "\nParameters:\n";
    std::cerr << "  m          : Golomb parameter (0 = adaptive, >0 = fixed)\n";
    std::cerr << "  blockSize  : Block size for adaptive m (0 = per-row, >0 = per block)\n";
    std::cerr << "  -v         : Verbose mode\n";
    std::cerr << "\nExamples:\n";
    std::cerr << "  " << prog << " encode images/lena.ppm lena.gimg 5 0 0 -v   # JPEG-LS, adaptive m, per-row\n";
    std::cerr << "  " << prog << " encode images/lena.ppm lena.gimg 4 8 256 -v # PAETH, fixed m=8, 256-pixel blocks\n";
    std::cerr << "  " << prog << " decode lena.gimg lena_decoded.ppm -v\n";
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
        
        std::string inputImage = argv[2];
        std::string outputFile = argv[3];
        uint32_t predictorNum = std::atoi(argv[4]);
        uint32_t m = std::atoi(argv[5]);
        uint32_t blockSize = std::atoi(argv[6]);
        
        if (predictorNum > 5) {
            std::cerr << "Error: Invalid predictor (must be 0-5)\n";
            return 1;
        }
        
        ImagePredictor predictor = static_cast<ImagePredictor>(predictorNum);
        
        bool ok = encodeImage(inputImage, outputFile, predictor, m, blockSize, verbose);
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