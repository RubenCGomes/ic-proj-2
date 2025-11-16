#ifndef LOSSLESS_IMAGE_HPP
#define LOSSLESS_IMAGE_HPP

#include <string>
#include <cstdint>

enum class ImagePredictor {
    NONE = 0,
    LEFT = 1,
    UP = 2,
    UP_LEFT = 3,
    LEFT_UP_DIFF = 4,
    LEFT_AVG = 5,
    UP_AVG = 6,
    AVG = 7,
    JPEG_LS = 8
};

bool encodeImage(const std::string& inputImage,
                 const std::string& outputFile,
                 ImagePredictor predictor,
                 uint32_t m,
                 uint32_t blockSize,
                 bool verbose,
                 bool autoSelectPredictor = false);

bool decodeImage(const std::string& inputFile,
                 const std::string& outputImage,
                 bool verbose);

#endif