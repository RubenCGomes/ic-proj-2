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

static uint8_t paethPredictor(uint8_t a, uint8_t b, uint8_t c) {
    int32_t p = static_cast<int32_t>(a) + static_cast<int32_t>(b) - static_cast<int32_t>(c);
    int32_t pa = std::abs(p - a);
    int32_t pb = std::abs(p - b);
    int32_t pc = std::abs(p - c);
    
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

static uint8_t jpegLSPredictor(uint8_t a, uint8_t b, uint8_t c) {
    if (c >= std::max(a, b)) {
        return std::min(a, b);
    } else if (c <= std::min(a, b)) {
        return std::max(a, b);
    } else {
        return a + b - c;
    }
}

static int32_t predict(ImagePredictor predictor, uint8_t left, uint8_t up, uint8_t upLeft) {
    int32_t a = static_cast<int32_t>(left);
    int32_t b = static_cast<int32_t>(up);
    int32_t c = static_cast<int32_t>(upLeft);
    
    switch (predictor) {
        case ImagePredictor::NONE:
            return 0;
            
        case ImagePredictor::LEFT:
            return a;
            
        case ImagePredictor::UP:
            return b;
            
        case ImagePredictor::UP_LEFT:
            return c;
            
        case ImagePredictor::LEFT_UP_DIFF:
            return a + b - c;
            
        case ImagePredictor::LEFT_AVG:
            return a + ((b - c) / 2);
            
        case ImagePredictor::UP_AVG:
            return b + ((a - c) / 2);
            
        case ImagePredictor::AVG:
            return (a + b) / 2;
            
        case ImagePredictor::JPEG_LS: {
            if (a == 0 && b == 0) return 0;
            if (a == 0) return b;
            if (b == 0) return a;
            
            if (c >= std::max(a, b)) {
                return std::min(a, b);
            } else if (c <= std::min(a, b)) {
                return std::max(a, b);
            } else {
                return a + b - c;
            }
        }
            
        default:
            return 0;
    }
}

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
    
    for (int p = 0; p <= 8; ++p) {
        ImagePredictor predictor = static_cast<ImagePredictor>(p);
        std::string tempFile = tempDir + "/temp_p" + std::to_string(p) + ".gimg";
        
        bool ok = encodeImage(inputImage, tempFile, predictor, m, blockSize, false);
        
        if (ok) {
            std::ifstream check(tempFile, std::ios::binary | std::ios::ate);
            size_t compressedSize = check.tellg();
            check.close();
            
            if (verbose) {
                const char* names[] = {"NONE", "LEFT", "UP", "UP_LEFT", "a+b-c", 
                                      "a+(b-c)/2", "b+(a-c)/2", "(a+b)/2", "JPEG-LS"};
                std::cout << "  Predictor " << p << " (" << names[p] << "): " 
                          << compressedSize << " bytes";
                if (compressedSize < bestSize) {
                    std::cout << " <- NEW BEST!";
                }
                std::cout << "\n";
            }
            
            if (compressedSize < bestSize) {
                bestSize = compressedSize;
                bestPredictor = predictor;
            }
            
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
                 bool autoSelectPredictor) {
    
    if (autoSelectPredictor) {
        std::string tempDir = ".";
        size_t lastSlash = outputFile.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            tempDir = outputFile.substr(0, lastSlash);
        }
        
        predictor = findBestPredictor(inputImage, tempDir, m, blockSize, verbose);
    }
    
    std::ifstream ifs(inputImage, std::ios::binary);
    if (!ifs) {
        if (verbose) std::cerr << "Error: Cannot open input image: " << inputImage << "\n";
        return false;
    }
    
    std::string magic;
    ifs >> magic;
    if (magic != "P5") {
        if (verbose) std::cerr << "Error: Not a P5 PPM file\n";
        return false;
    }
    
    uint32_t width, height, maxVal;
    ifs >> width >> height >> maxVal;
    ifs.get();
    
    if (maxVal != 255) {
        if (verbose) std::cerr << "Error: Only 8-bit grayscale supported\n";
        return false;
    }
    
    std::vector<uint8_t> pixels(width * height);
    ifs.read(reinterpret_cast<char*>(pixels.data()), pixels.size());
    ifs.close();
    
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
    
    const uint32_t MAGIC = 0x47494D47;
    bs.write_n_bits(MAGIC, 32);
    bs.write_n_bits(width, 32);
    bs.write_n_bits(height, 32);
    bs.write_n_bits(static_cast<uint8_t>(predictor), 8);
    bs.write_n_bits(m == 0 ? 0 : 255, 8);
    bs.write_n_bits(effectiveBlockSize, 32);
    
    uint64_t totalPixels = static_cast<uint64_t>(width) * height;
    uint64_t processedPixels = 0;
    
    for (uint64_t blockStart = 0; blockStart < totalPixels; blockStart += effectiveBlockSize) {
        uint32_t currentBlockSize = std::min<uint32_t>(effectiveBlockSize, totalPixels - blockStart);
        
        std::vector<int32_t> residuals;
        residuals.reserve(currentBlockSize);
        
        for (uint32_t i = 0; i < currentBlockSize; ++i) {
            uint64_t pixelIndex = blockStart + i;
            uint32_t y = pixelIndex / width;
            uint32_t x = pixelIndex % width;
            
            uint8_t pixel = pixels[pixelIndex];
            
            uint8_t left = (x > 0) ? pixels[y * width + (x - 1)] : 0;
            uint8_t up = (y > 0) ? pixels[(y - 1) * width + x] : 0;
            uint8_t upLeft = (x > 0 && y > 0) ? pixels[(y - 1) * width + (x - 1)] : 0;
            
            int32_t pred = predict(predictor, left, up, upLeft);
            int32_t resid = static_cast<int32_t>(pixel) - pred;
            residuals.push_back(resid);
        }
        
        uint32_t blockM = m;
        if (m == 0) {
            double sumAbs = 0.0;
            for (auto r : residuals) {
                sumAbs += std::abs(r);
            }
            double meanAbs = residuals.empty() ? 1.0 : sumAbs / residuals.size();
            
            double alpha = meanAbs / (meanAbs + 1.0);
            blockM = static_cast<uint32_t>(std::ceil(-1.0 / std::log2(alpha)));
            
            blockM = std::max<uint32_t>(1, std::min<uint32_t>(4096, blockM));
            
            if (blockM == 0) blockM = 1;
        }
        
        if (verbose && blockStart < 20000) {
            std::cout << "\n[Encoder Block " << (blockStart / effectiveBlockSize) 
                      << " @ pixel " << blockStart << "]";
            std::cout << "\n  Writing m=" << blockM << " (8 bits)";
            std::cout << std::flush;
        }
        
        if (m == 0) {
            bs.write_n_bits(blockM, 8);
        }
        
        uint32_t b = static_cast<uint32_t>(std::ceil(std::log2(static_cast<double>(blockM))));
        uint32_t cutoff = (1u << b) - blockM;
        
        if (b == 0) b = 1;
        
        size_t totalBitsThisBlock = 0;
        
        for (auto resid : residuals) {
            uint32_t mapped = (resid >= 0) ? static_cast<uint32_t>(resid) << 1u
                                          : (static_cast<uint32_t>(-resid) << 1u) - 1u;
            
            uint32_t q = mapped / blockM;
            uint32_t r = mapped % blockM;
            
            for (uint32_t j = 0; j < q; ++j) {
                bs.write_bit(0);
                totalBitsThisBlock++;
            }
            bs.write_bit(1);
            totalBitsThisBlock++;
            
            if (r < cutoff) {
                if (b > 1) {
                    bs.write_n_bits(r, b - 1);
                    totalBitsThisBlock += (b - 1);
                }
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
    
    std::ifstream checkSize(outputFile, std::ios::binary | std::ios::ate);
    size_t compressedSize = checkSize.tellg();
    checkSize.close();
    
    size_t originalSize = pixels.size() + 15;
    
    if (verbose) {
        std::cout << "\nEncoding complete.\n";
        std::cout << "Original size:   " << originalSize << " bytes\n";
        std::cout << "Compressed size: " << compressedSize << " bytes\n";
        double ratio = 100.0 * (1.0 - static_cast<double>(compressedSize) / originalSize);
        std::cout << "Compression:     " << std::fixed << std::setprecision(2) 
                  << ratio << "%\n";
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

    const uint32_t MAGIC = 0x47494D47;
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

    for (uint64_t blockStart = 0; blockStart < totalPixels; blockStart += blockSize) {
        uint32_t currentBlockSize = std::min<uint32_t>(blockSize, totalPixels - blockStart);

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

        uint32_t b = static_cast<uint32_t>(std::ceil(std::log2(static_cast<double>(blockM))));
        uint32_t cutoff = (1u << b) - blockM;
        
        if (b == 0) b = 1;

        for (uint32_t i = 0; i < currentBlockSize; ++i) {
            uint64_t pixelIndex = blockStart + i;
            uint32_t y = pixelIndex / width;
            uint32_t x = pixelIndex % width;

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

            uint32_t r = 0;
            if (b > 1) {
                r = bs.read_n_bits(b - 1);
            }

            if (r < cutoff) {
            } else {
                int extraBit = bs.read_bit();
                if (extraBit == EOF) {
                    if (verbose) std::cerr << "\nError: Unexpected EOF in remainder at pixel " << pixelIndex << "\n";
                    return false;
                }
                r = (r << 1) | extraBit;
                r -= cutoff;
            }

            uint32_t mapped = q * blockM + r;

            int32_t resid = (mapped & 1u) ? -static_cast<int32_t>((mapped + 1) >> 1)
                                         : static_cast<int32_t>(mapped >> 1);

            uint8_t left = (x > 0) ? pixels[y * width + (x - 1)] : 0;
            uint8_t up = (y > 0) ? pixels[(y - 1) * width + x] : 0;
            uint8_t upLeft = (x > 0 && y > 0) ? pixels[(y - 1) * width + (x - 1)] : 0;

            int32_t pred = predict(predictor, left, up, upLeft);
            int32_t pixelValue = pred + resid;

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