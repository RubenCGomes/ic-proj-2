#include "effects/negative.hpp"

cv::Mat createNegative(const cv::Mat& src) {
    cv::Mat result(src.rows, src.cols, src.type());

    // For color images
    if (src.channels() == 3) {
        for (int row = 0; row < src.rows; ++row) {
            for (int col = 0; col < src.cols; ++col) {
                cv::Vec3b pixel = src.at<cv::Vec3b>(row, col);
                result.at<cv::Vec3b>(row, col) = cv::Vec3b(
                    255 - pixel[0],  // Blue
                    255 - pixel[1],  // Green
                    255 - pixel[2]   // Red
                );
            }
        }
    }
    // For grayscale images
    else if (src.channels() == 1) {
        for (int row = 0; row < src.rows; ++row) {
            for (int col = 0; col < src.cols; ++col) {
                result.at<uchar>(row, col) = 255 - src.at<uchar>(row, col);
            }
        }
    }

    return result;
}
