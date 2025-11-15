#ifndef IMAGE_CODEC_HPP
#define IMAGE_CODEC_HPP

#include <string>
#include <cstdint>

/**
 * @brief Predictor types for image compression
 */
enum class ImagePredictor {
    NONE = 0,        // No prediction (raw pixels)
    LEFT = 1,        // pred = pixel[x-1, y]
    UP = 2,          // pred = pixel[x, y-1]
    AVERAGE = 3,     // pred = (pixel[x-1,y] + pixel[x,y-1]) / 2
    PAETH = 4,       // pred = Paeth predictor (PNG standard)
    JPEG_LS = 5      // pred = JPEG-LS predictor (best for natural images)
};

/**
 * @brief Encode a grayscale image using Golomb coding
 * 
 * @param inputImage Input image file (PPM P5 format - grayscale)
 * @param outputFile Output compressed file (.gimg)
 * @param predictor Predictor type to use
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