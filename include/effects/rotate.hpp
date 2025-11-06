#ifndef ROTATE_HPP
#define ROTATE_HPP

#include <opencv2/opencv.hpp>

// Rotates an image by 90 degrees clockwise
cv::Mat rotate90(const cv::Mat& src);

// Rotates an image by a multiple of 90 degrees
// rotations: number of 90-degree rotations (can be negative)
cv::Mat rotateMultiple90(const cv::Mat& src, int rotations);

#endif // ROTATE_HPP
