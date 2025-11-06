#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>

cv::Mat extractChannel(const cv::Mat& src, int channel) {
    // Create output image (single channel, grayscale)
    cv::Mat result(src.rows, src.cols, CV_8UC1);

    // Process pixel by pixel
    for (int row = 0; row < src.rows; ++row) {
        for (int col = 0; col < src.cols; ++col) {
            // Read BGR pixel
            cv::Vec3b pixel = src.at<cv::Vec3b>(row, col);

            // Extract the specified channel and write to result
            result.at<uchar>(row, col) = pixel[channel];
        }
    }

    return result;
}

int main(int argc, char** argv) {
    // Check command line arguments
    if (argc != 4) {
        std::cout << "Usage: " << argv[0] << " <input_image> <output_image> <channel>" << std::endl;
        std::cout << "  channel: 0=Blue, 1=Green, 2=Red" << std::endl;
        std::cout << "Example: " << argv[0] << " input.jpg output.jpg 2" << std::endl;
        return -1;
    }

    std::string inputFile = argv[1];
    std::string outputFile = argv[2];
    int channel = std::stoi(argv[3]);

    // Read the input image
    cv::Mat src = cv::imread(inputFile, cv::IMREAD_COLOR);

    if (src.empty()) {
        std::cerr << "Error: Could not read image from " << inputFile << std::endl;
        return -1;
    }

    // Validate channel number (0=Blue, 1=Green, 2=Red)
    if (channel < 0 || channel > 2) {
        std::cerr << "Error: Invalid channel number. Must be 0 (Blue), 1 (Green), or 2 (Red)" << std::endl;
        return -1;
    }

    // Extract channel pixel by pixel
    cv::Mat result = extractChannel(src, channel);

    // Write the output image
    if (!cv::imwrite(outputFile, result)) {
        std::cerr << "Error: Could not write image to " << outputFile << std::endl;
        return -1;
    }

    std::cout << "Successfully extracted channel " << channel << " from " << inputFile
              << " to " << outputFile << std::endl;
    std::cout << "Image size: " << src.rows << "x" << src.cols << std::endl;

    return 0;
}
