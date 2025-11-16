#include "lossless_image.hpp"
#include "golomb.hpp"
#include "bit_stream.h"
#include <fstream>
#include <vector>
#include <cstdint>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <deque>

// Progress bar helper
static void showProgress(double fraction, const std::string& label, bool verbose) {
    if (!verbose) return;
    const int width = 50;
    int pos = static_cast<int>(fraction * width);
    std::cout << "\r" << label << " [";
    for (int i = 0; i < width; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] " << std::setw(6) << std::fixed << std::setprecision(2) 
              << (fraction * 100.0) << "%" << std::flush;
}

// Paeth predictor (PNG standard)
static uint8_t paethPredictor(uint8_t a, uint8_t b, uint8_t c) {
    int p = static_cast<int>(a) + b - c;
    int pa = std::abs(p - a);
    int pb = std::abs(p - b);
    int pc = std::abs(p - c);
    
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

// JPEG-LS predictor
static uint8_t jpegLSPredictor(uint8_t a, uint8_t b, uint8_t c) {
    int minAB = std::min(static_cast<int>(a), static_cast<int>(b));
    int maxAB = std::max(static_cast<int>(a), static_cast<int>(b));
    
    if (c >= maxAB) return minAB;
    if (c <= minAB) return maxAB;
    return static_cast<uint8_t>(a + b - c);
}

// Compute prediction using JPEG lossless modes 1-7 + JPEG-LS
static int32_t predict(ImagePredictor predictor, uint8_t left, uint8_t up, uint8_t upLeft) {
    int32_t a = static_cast<int32_t>(left);     // pixel to the left
    int32_t b = static_cast<int32_t>(up);       // pixel above
    int32_t c = static_cast<int32_t>(upLeft);   // pixel upper-left diagonal
    
    switch (predictor) {
        case ImagePredictor::NONE:
            // No prediction (baseline for testing)
            return 0;
            
        case ImagePredictor::LEFT:
            // Mode 1: a (left pixel)
            return a;
            
        case ImagePredictor::UP:
            // Mode 2: b (upper pixel)
            return b;
            
        case ImagePredictor::UP_LEFT:
            // Mode 3: c (upper-left diagonal pixel)
            return c;
            
        case ImagePredictor::LEFT_UP_DIFF:
            // Mode 4: a + b - c (Paeth-like predictor)
            return a + b - c;
            
        case ImagePredictor::LEFT_AVG:
            // Mode 5: a + (b - c)/2
            return a + ((b - c) / 2);
            
        case ImagePredictor::UP_AVG:
            // Mode 6: b + (a - c)/2
            return b + ((a - c) / 2);
            
        case ImagePredictor::AVG:
            // Mode 7: (a + b)/2
            return (a + b) / 2;
            
        case ImagePredictor::JPEG_LS: {
            // JPEG-LS nonlinear predictor (equation 6.6 from PDF)
            // Edge handling: at image boundaries, use simpler predictors
            if (a == 0 && b == 0) return 0;  // Top-left corner
            if (a == 0) return b;             // Left edge (x=0)
            if (b == 0) return a;             // Top edge (y=0)
            
            // Standard JPEG-LS MED (Median Edge Detection) predictor
            if (c >= std::max(a, b)) {
                return std::min(a, b);        // min(a,b) if c >= max(a,b)
            } else if (c <= std::min(a, b)) {
                return std::max(a, b);        // max(a,b) if c <= min(a,b)
            } else {
                return a + b - c;             // a + b - c otherwise
            }
        }
            
        default:
            return 0;
    }
}

/**
 * @brief Try all predictors and return the one with best compression
 * 
 * @param inputImage Input PPM file
 * @param tempDir Directory for temporary files
 * @param m Golomb parameter
 * @param blockSize Block size for adaptive m
 * @param verbose Print progress
 * @return Best predictor type
 */
static ImagePredictor findBestPredictor(const std::string& inputImage,
                                        const std::string& tempDir,
                                        uint32_t m,
                                        uint32_t blockSize,
                                        bool verbose) {
    if (verbose) {
        std::cout << "\n=== Testing all predictors to find best compression ===\n";
    }
    
    ImagePredictor bestPredictor = ImagePredictor::JPEG_LS;
    size_t bestSize = SIZE_MAX;
    
    // Test all predictors (0-8)
    for (int p = 0; p <= 8; ++p) {
        ImagePredictor predictor = static_cast<ImagePredictor>(p);
        std::string tempFile = tempDir + "/temp_p" + std::to_string(p) + ".gimg";
        
        // Encode with this predictor (silent mode)
        bool ok = encodeImage(inputImage, tempFile, predictor, m, blockSize, false);
        
        if (ok) {
            // Check compressed file size
            std::ifstream check(tempFile, std::ios::binary | std::ios::ate);
            size_t compressedSize = check.tellg();
            check.close();
            
            if (verbose) {
                const char* names[] = {"NONE", "LEFT", "UP", "UP_LEFT", "a+b-c", 
                                      "a+(b-c)/2", "b+(a-c)/2", "(a+b)/2", "JPEG-LS"};
                std::cout << "  Predictor " << p << " (" << names[p] << "): " 
                          << compressedSize << " bytes";
                if (compressedSize < bestSize) {
                    std::cout << " ← NEW BEST!";
                }
                std::cout << "\n";
            }
            
            if (compressedSize < bestSize) {
                bestSize = compressedSize;
                bestPredictor = predictor;
            }
            
            // Delete temp file
            std::remove(tempFile.c_str());
        }
    }
    
    if (verbose) {
        const char* names[] = {"NONE", "LEFT", "UP", "UP_LEFT", "a+b-c", 
                              "a+(b-c)/2", "b+(a-c)/2", "(a+b)/2", "JPEG-LS"};
        std::cout << "\nBest predictor: " << static_cast<int>(bestPredictor) 
                  << " (" << names[static_cast<int>(bestPredictor)] << ")\n";
        std::cout << "Best size: " << bestSize << " bytes\n\n";
    }
    
    return bestPredictor;
}

bool encodeImage(const std::string& inputImage,
                 const std::string& outputFile,
                 ImagePredictor predictor,
                 uint32_t m,
                 uint32_t blockSize,
                 bool verbose,
                 bool autoSelectPredictor) {  // NO default value here!
    
    // Auto-select best predictor if requested
    if (autoSelectPredictor) {
        std::string tempDir = ".";
        size_t lastSlash = outputFile.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            tempDir = outputFile.substr(0, lastSlash);
        }
        
        predictor = findBestPredictor(inputImage, tempDir, m, blockSize, verbose);
    }
    
    // Read PPM P5
    std::ifstream ifs(inputImage, std::ios::binary);
    if (!ifs) {
        if (verbose) std::cerr << "Error: Cannot open input image: " << inputImage << "\n";
        return false;
    }
    
    std::string magic;
    ifs >> magic;
    if (magic != "P5") {
        if (verbose) std::cerr << "Error: Only PPM P5 (grayscale) format supported\n";
        return false;
    }
    
    uint32_t width, height, maxval;
    ifs >> width >> height >> maxval;
    ifs.ignore(1);
    
    if (maxval != 255) {
        if (verbose) std::cerr << "Error: Only 8-bit grayscale supported\n";
        return false;
    }
    
    std::vector<uint8_t> pixels(width * height);
    ifs.read(reinterpret_cast<char*>(pixels.data()), width * height);
    ifs.close();
    
    // Use per-row blocks if blockSize is 0
    uint32_t effectiveBlockSize = (blockSize == 0) ? width : blockSize;
    
    if (verbose) {
        std::cout << "Encoding: " << inputImage << " -> " << outputFile << "\n";
        std::cout << "Image: " << width << "x" << height << " (8-bit grayscale)\n";
        std::cout << "Predictor: ";
        switch (predictor) {
            case ImagePredictor::NONE: std::cout << "0 (NONE - no prediction)\n"; break;
            case ImagePredictor::LEFT: std::cout << "1 (LEFT: a)\n"; break;
            case ImagePredictor::UP: std::cout << "2 (UP: b)\n"; break;
            case ImagePredictor::UP_LEFT: std::cout << "3 (UP_LEFT: c)\n"; break;
            case ImagePredictor::LEFT_UP_DIFF: std::cout << "4 (LEFT+UP-UPLEFT: a+b-c)\n"; break;
            case ImagePredictor::LEFT_AVG: std::cout << "5 (LEFT_AVG: a+(b-c)/2)\n"; break;
            case ImagePredictor::UP_AVG: std::cout << "6 (UP_AVG: b+(a-c)/2)\n"; break;
            case ImagePredictor::AVG: std::cout << "7 (AVG: (a+b)/2)\n"; break;
            case ImagePredictor::JPEG_LS: std::cout << "8 (JPEG-LS nonlinear)\n"; break;
        }
        std::cout << "Golomb m: " << (m == 0 ? "adaptive" : std::to_string(m)) << "\n";
        std::cout << "Block size: " << effectiveBlockSize << " pixels\n";
    }
    
    std::fstream ofs(outputFile, std::ios::out | std::ios::binary);
    if (!ofs) {
        if (verbose) std::cerr << "Error: Cannot create output file\n";
        return false;
    }
    
    BitStream bs(ofs, STREAM_WRITE);
    
    // Write file header
    const uint32_t MAGIC = 0x47494D47; // "GIMG"
    bs.write_n_bits(MAGIC, 32);
    bs.write_n_bits(width, 32);
    bs.write_n_bits(height, 32);
    bs.write_n_bits(static_cast<uint8_t>(predictor), 8);
    bs.write_n_bits(m, 8);
    bs.write_n_bits(effectiveBlockSize, 32);
    
    uint64_t totalPixels = static_cast<uint64_t>(width) * height;
    uint64_t processedPixels = 0;
    
    // Encode blocks
    for (uint64_t blockStart = 0; blockStart < totalPixels; blockStart += effectiveBlockSize) {
        uint32_t currentBlockSize = std::min<uint32_t>(effectiveBlockSize, totalPixels - blockStart);
        
        // Collect residuals for this block
        std::vector<int32_t> residuals;
        residuals.reserve(currentBlockSize);
        
        for (uint32_t i = 0; i < currentBlockSize; ++i) {
            uint64_t pixelIndex = blockStart + i;
            uint32_t y = pixelIndex / width;
            uint32_t x = pixelIndex % width;
            
            uint8_t pixel = pixels[pixelIndex];
            
            // Get neighbors for prediction
            uint8_t left = (x > 0) ? pixels[y * width + (x - 1)] : 0;
            uint8_t up = (y > 0) ? pixels[(y - 1) * width + x] : 0;
            uint8_t upLeft = (x > 0 && y > 0) ? pixels[(y - 1) * width + (x - 1)] : 0;
            
            // Compute prediction
            int32_t pred = predict(predictor, left, up, upLeft);
            
            // Compute residual
            int32_t resid = static_cast<int32_t>(pixel) - pred;
            residuals.push_back(resid);
        }
        
        // Compute adaptive m if needed
        uint32_t blockM = m;
        if (m == 0) {
            // Compute mean absolute residual (same as audio codec)
            double sumAbs = 0.0;
            for (auto r : residuals) {
                sumAbs += std::abs(r);
            }
            double meanAbs = residuals.empty() ? 1.0 : sumAbs / residuals.size();
            
            // Theoretically optimal m for geometric distribution (Golomb 1966)
            // α = mean / (mean + 1)
            // m = ceil(-1 / log₂(α))
            double alpha = meanAbs / (meanAbs + 1.0);
            blockM = static_cast<uint32_t>(std::ceil(-1.0 / std::log2(alpha)));
            
            // Clamp to reasonable range (optional - prevents extreme values)
            blockM = std::max<uint32_t>(1, std::min<uint32_t>(256, blockM));
            
            // Safety check
            if (blockM == 0) blockM = 1;
        }
        
        // DEBUG: Print block header info
        if (verbose && blockStart < 20000) {
            std::cout << "\n[Encoder Block " << (blockStart / effectiveBlockSize) 
                      << " @ pixel " << blockStart << "]";
            std::cout << "\n  Writing m=" << blockM << " (8 bits)";
            std::cout << std::flush;
        }
        
        // Write block header (only if adaptive m)
        if (m == 0) {
            bs.write_n_bits(blockM, 8);
        }
        
        // MANUAL Golomb encode (matching decoder)
        uint32_t b = static_cast<uint32_t>(std::ceil(std::log2(static_cast<double>(blockM))));
        uint32_t cutoff = (1u << b) - blockM;
        
        // FIX: When m=1, b=0, which causes issues. Ensure b >= 1
        if (b == 0) b = 1;
        
        size_t totalBitsThisBlock = 0;
        
        for (auto resid : residuals) {
            // Map signed -> unsigned (interleaving)
            uint32_t mapped = (resid >= 0) ? static_cast<uint32_t>(resid) << 1u
                                          : (static_cast<uint32_t>(-resid) << 1u) - 1u;
            
            uint32_t q = mapped / blockM;
            uint32_t r = mapped % blockM;
            
            // Write unary quotient
            for (uint32_t j = 0; j < q; ++j) {
                bs.write_bit(0);
                totalBitsThisBlock++;
            }
            bs.write_bit(1);
            totalBitsThisBlock++;
            
            // Write truncated binary remainder
            if (r < cutoff) {
                if (b > 1) {
                    bs.write_n_bits(r, b - 1);
                    totalBitsThisBlock += (b - 1);
                }
                // If b == 1, remainder is 0 bits (m=1 case)
            } else {
                uint32_t adjusted = r + cutoff;
                bs.write_n_bits(adjusted, b);
                totalBitsThisBlock += b;
            }
        }
        
        if (verbose && blockStart < 20000) {
            std::cout << " [wrote " << totalBitsThisBlock << " bits for " << residuals.size() << " samples]\n";
        }
        
        processedPixels += currentBlockSize;
        if (verbose && (processedPixels % 10000) == 0) {
            showProgress(static_cast<double>(processedPixels) / totalPixels, "Encoding", verbose);
        }
    }
    
    bs.close();
    
    if (verbose) {
        showProgress(1.0, "Encoding", verbose);
        std::cout << "\nEncoding complete.\n";
        
        std::ifstream checkSize(outputFile, std::ios::binary | std::ios::ate);
        size_t compressedSize = checkSize.tellg();
        size_t originalSize = width * height + 15;
        double ratio = 100.0 * (1.0 - static_cast<double>(compressedSize) / originalSize);
        
        std::cout << "Original size:   " << originalSize << " bytes\n";
        std::cout << "Compressed size: " << compressedSize << " bytes\n";
        std::cout << "Compression:     " << std::fixed << std::setprecision(2) << ratio << "%\n";
    }
    
    return true;
}

bool decodeImage(const std::string& inputFile,
                 const std::string& outputImage,
                 bool verbose) {
    std::fstream ifs(inputFile, std::ios::in | std::ios::binary);
    if (!ifs) {
        if (verbose) std::cerr << "Error: Cannot open input file\n";
        return false;
    }

    BitStream bs(ifs, STREAM_READ);

    // Read header
    const uint32_t MAGIC = 0x47494D47; // "GIMG"
    uint32_t magic = bs.read_n_bits(32);
    if (magic != MAGIC) {
        if (verbose) std::cerr << "Error: Invalid file format\n";
        return false;
    }

    uint32_t width = bs.read_n_bits(32);
    uint32_t height = bs.read_n_bits(32);
    uint8_t predictorByte = bs.read_n_bits(8);
    uint32_t mFlag = bs.read_n_bits(8);
    uint32_t blockSize = bs.read_n_bits(32);

    ImagePredictor predictor = static_cast<ImagePredictor>(predictorByte);

    if (verbose) {
        std::cout << "Decoding: " << inputFile << " -> " << outputImage << "\n";
        std::cout << "Image: " << width << "x" << height << "\n";
        std::cout << "Block size: " << blockSize << " pixels\n";
    }

    std::vector<uint8_t> pixels(width * height);
    uint64_t totalPixels = static_cast<uint64_t>(width) * height;
    uint64_t processedPixels = 0;

    // Decode blocks
    for (uint64_t blockStart = 0; blockStart < totalPixels; blockStart += blockSize) {
        uint32_t currentBlockSize = std::min<uint32_t>(blockSize, totalPixels - blockStart);

        // Read block m (if adaptive)
        uint32_t blockM = mFlag;
        if (mFlag == 0) {
            blockM = bs.read_n_bits(8);
            if (verbose && blockStart < 10000) {
                std::cout << "\n[Decoder] Block at pixel " << blockStart 
                          << ": read m=" << blockM << std::flush;
            }
        }

        if (blockM == 0) {
            if (verbose) {
                std::cerr << "\n\n=== DECODE ERROR ===\n";
                std::cerr << "Block start pixel: " << blockStart << "\n";
                std::cerr << "mFlag (from header): " << mFlag << "\n";
                std::cerr << "Read blockM: " << blockM << " (INVALID!)\n";
            }
            return false;
        }

        // Manual Golomb decoding (same as encoder - don't use Golomb class)
        uint32_t b = static_cast<uint32_t>(std::ceil(std::log2(static_cast<double>(blockM))));
        uint32_t cutoff = (1u << b) - blockM;
        
        // FIX: When m=1, b=0, which causes issues. Ensure b >= 1
        if (b == 0) b = 1;

        // Decode residuals for this block
        for (uint32_t i = 0; i < currentBlockSize; ++i) {
            uint64_t pixelIndex = blockStart + i;
            uint32_t y = pixelIndex / width;
            uint32_t x = pixelIndex % width;

            // Read unary quotient
            uint32_t q = 0;
            int bit;
            while ((bit = bs.read_bit()) == 0) {
                ++q;
                if (q > 100000) {
                    if (verbose) std::cerr << "\nError: Runaway unary decode at pixel " << pixelIndex << "\n";
                    return false;
                }
            }
            if (bit == EOF) {
                if (verbose) std::cerr << "\nError: Unexpected EOF in quotient at pixel " << pixelIndex << "\n";
                return false;
            }

            // Read truncated binary remainder
            uint32_t r = 0;
            if (b > 1) {
                r = bs.read_n_bits(b - 1);
            }
            // If b == 1 (m=1), remainder is always 0 (no bits to read)

            if (r < cutoff) {
                // Done - r uses b-1 bits
            } else {
                // Need one more bit
                int extraBit = bs.read_bit();
                if (extraBit == EOF) {
                    if (verbose) std::cerr << "\nError: Unexpected EOF in remainder at pixel " << pixelIndex << "\n";
                    return false;
                }
                r = (r << 1) | extraBit;
                r -= cutoff;
            }

            // Calculate mapped value
            uint32_t mapped = q * blockM + r;

            // Map unsigned -> signed (interleaving)
            int32_t resid = (mapped & 1u) ? -static_cast<int32_t>((mapped + 1) >> 1)
                                         : static_cast<int32_t>(mapped >> 1);

            // Get neighbors for prediction
            uint8_t left = (x > 0) ? pixels[y * width + (x - 1)] : 0;
            uint8_t up = (y > 0) ? pixels[(y - 1) * width + x] : 0;
            uint8_t upLeft = (x > 0 && y > 0) ? pixels[(y - 1) * width + (x - 1)] : 0;

            // Reconstruct pixel
            int32_t pred = predict(predictor, left, up, upLeft);
            int32_t pixelValue = pred + resid;

            // Clamp to [0, 255]
            if (pixelValue < 0) pixelValue = 0;
            if (pixelValue > 255) pixelValue = 255;

            pixels[pixelIndex] = static_cast<uint8_t>(pixelValue);
        }

        processedPixels += currentBlockSize;
        if (verbose && (processedPixels % 10000) == 0) {
            showProgress(static_cast<double>(processedPixels) / totalPixels, "Decoding", verbose);
        }
    }

    bs.close();

    // Write PPM
    std::ofstream ofs(outputImage, std::ios::binary);
    if (!ofs) {
        if (verbose) std::cerr << "Error: Cannot create output file\n";
        return false;
    }

    ofs << "P5\n" << width << " " << height << "\n255\n";
    ofs.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());

    if (verbose) {
        std::cout << "\nDecoding complete.\n";
        std::cout << "Output written: " << outputImage << "\n";
    }

    return true;
}