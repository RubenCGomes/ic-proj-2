#include "effects/mirror.hpp"

cv::Mat mirrorHorizontal(const cv::Mat& src) {
    cv::Mat result(src.rows, src.cols, src.type());

    // For color images
    if (src.channels() == 3) {
        for (int row = 0; row < src.rows; ++row) {
            for (int col = 0; col < src.cols; ++col) {
                result.at<cv::Vec3b>(row, col) = src.at<cv::Vec3b>(row, src.cols - 1 - col);
            }
        }
    }
    // For grayscale images
    else if (src.channels() == 1) {
        for (int row = 0; row < src.rows; ++row) {
            for (int col = 0; col < src.cols; ++col) {
                result.at<uchar>(row, col) = src.at<uchar>(row, src.cols - 1 - col);
            }
        }
    }

    return result;
}

cv::Mat mirrorVertical(const cv::Mat& src) {
    cv::Mat result(src.rows, src.cols, src.type());

    // For color images
    if (src.channels() == 3) {
        for (int row = 0; row < src.rows; ++row) {
            for (int col = 0; col < src.cols; ++col) {
                result.at<cv::Vec3b>(row, col) = src.at<cv::Vec3b>(src.rows - 1 - row, col);
            }
        }
    }
    // For grayscale images
    else if (src.channels() == 1) {
        for (int row = 0; row < src.rows; ++row) {
            for (int col = 0; col < src.cols; ++col) {
                result.at<uchar>(row, col) = src.at<uchar>(src.rows - 1 - row, col);
            }
        }
    }

    return result;
}
