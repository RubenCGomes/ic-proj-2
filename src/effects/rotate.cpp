#include "effects/rotate.hpp"

cv::Mat rotate90(const cv::Mat& src) {
    cv::Mat result(src.cols, src.rows, src.type());

    // For color images
    if (src.channels() == 3) {
        for (int row = 0; row < src.rows; ++row) {
            for (int col = 0; col < src.cols; ++col) {
                result.at<cv::Vec3b>(col, src.rows - 1 - row) = src.at<cv::Vec3b>(row, col);
            }
        }
    }
    // For grayscale images
    else if (src.channels() == 1) {
        for (int row = 0; row < src.rows; ++row) {
            for (int col = 0; col < src.cols; ++col) {
                result.at<uchar>(col, src.rows - 1 - row) = src.at<uchar>(row, col);
            }
        }
    }

    return result;
}

cv::Mat rotateMultiple90(const cv::Mat& src, int rotations) {
    // Normalize rotations to 0-3 range
    rotations = ((rotations % 4) + 4) % 4;

    cv::Mat result = src.clone();

    for (int i = 0; i < rotations; ++i) {
        result = rotate90(result);
    }

    return result;
}
