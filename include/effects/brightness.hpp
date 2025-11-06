#ifndef BRIGHTNESS_HPP
#define BRIGHTNESS_HPP

#include <opencv2/opencv.hpp>

// Adjusts the brightness of an image
// delta: positive values increase brightness, negative values decrease brightness
cv::Mat adjustBrightness(const cv::Mat& src, int delta);

#endif // BRIGHTNESS_HPP
