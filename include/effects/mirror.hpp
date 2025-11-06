#ifndef MIRROR_HPP
#define MIRROR_HPP

#include <opencv2/opencv.hpp>

// Creates a horizontally mirrored version of an image
cv::Mat mirrorHorizontal(const cv::Mat& src);

// Creates a vertically mirrored version of an image
cv::Mat mirrorVertical(const cv::Mat& src);

#endif // MIRROR_HPP
