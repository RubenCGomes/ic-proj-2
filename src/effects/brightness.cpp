#include "effects/brightness.hpp"
#include <algorithm>

cv::Mat adjustBrightness(const cv::Mat& src, int delta) {
    cv::Mat result(src.rows, src.cols, src.type());

    // For color images
    if (src.channels() == 3) {
        for (int row = 0; row < src.rows; ++row) {
            for (int col = 0; col < src.cols; ++col) {
                cv::Vec3b pixel = src.at<cv::Vec3b>(row, col);
                result.at<cv::Vec3b>(row, col) = cv::Vec3b(
                    std::clamp(pixel[0] + delta, 0, 255),  // Blue
                    std::clamp(pixel[1] + delta, 0, 255),  // Green
                    std::clamp(pixel[2] + delta, 0, 255)   // Red
                );
            }
        }
    }
    // For grayscale images
    else if (src.channels() == 1) {
        for (int row = 0; row < src.rows; ++row) {
            for (int col = 0; col < src.cols; ++col) {
                int value = src.at<uchar>(row, col) + delta;
                result.at<uchar>(row, col) = std::clamp(value, 0, 255);
            }
        }
    }

    return result;
}
