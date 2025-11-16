#ifndef LOSSLESS_IMAGE_HPP
#define LOSSLESS_IMAGE_HPP

#include <string>
#include <cstdint>

/**
 * @brief Predictor types for image compression
 * Based on JPEG lossless mode (ISO/IEC 10918-1) - 7 linear predictors
 * Plus JPEG-LS nonlinear predictor
 */
enum class ImagePredictor {
    NONE = 0,        // No prediction (for testing)
    LEFT = 1,        // Mode 1: a (left pixel)
    UP = 2,          // Mode 2: b (upper pixel)  
    UP_LEFT = 3,     // Mode 3: c (upper-left pixel)
    LEFT_UP_DIFF = 4,// Mode 4: a + b - c (Paeth-like)
    LEFT_AVG = 5,    // Mode 5: a + (b - c)/2
    UP_AVG = 6,      // Mode 6: b + (a - c)/2
    AVG = 7,         // Mode 7: (a + b)/2
    JPEG_LS = 8      // JPEG-LS nonlinear predictor (best for natural images)
};

/**
 * @brief Encode a grayscale image using Golomb coding
 * 
 * @param inputImage Input image file (PPM P5 format - grayscale)
 * @param outputFile Output compressed file (.gimg)
 * @param predictor Predictor type to use (0-8)
 * @param m Golomb parameter (0 = adaptive, >0 = fixed)
 * @param blockSize Block size for adaptive m (0 = per-row adaptation)
 * @param verbose Print progress and statistics
 * @return true on success
 */
bool encodeImage(const std::string& inputImage,
                 const std::string& outputFile,
                 ImagePredictor predictor,
                 uint32_t m,
                 uint32_t blockSize,
                 bool verbose);

/**
 * @brief Decode a compressed image back to PPM
 * 
 * @param inputFile Input compressed file (.gimg)
 * @param outputImage Output image file (PPM P5 format)
 * @param verbose Print progress and statistics
 * @return true on success
 */
bool decodeImage(const std::string& inputFile,
                 const std::string& outputImage,
                 bool verbose);

#endif // IMAGE_CODEC_HPP