#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include "effects/negative.hpp"
#include "effects/mirror.hpp"
#include "effects/rotate.hpp"
#include "effects/brightness.hpp"

void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " <input_image> <output_image> <effect> [parameters]" << std::endl;
    std::cout << "\nAvailable effects:" << std::endl;
    std::cout << "  negative              - Creates negative version of image" << std::endl;
    std::cout << "  mirror-h              - Mirrors image horizontally" << std::endl;
    std::cout << "  mirror-v              - Mirrors image vertically" << std::endl;
    std::cout << "  rotate <n>            - Rotates image by n*90 degrees (e.g., 1=90°, 2=180°, 3=270°)" << std::endl;
    std::cout << "  brightness <delta>    - Adjusts brightness (positive=lighter, negative=darker)" << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  " << progName << " input.jpg output.jpg negative" << std::endl;
    std::cout << "  " << progName << " input.jpg output.jpg mirror-h" << std::endl;
    std::cout << "  " << progName << " input.jpg output.jpg rotate 2" << std::endl;
    std::cout << "  " << progName << " input.jpg output.jpg brightness 50" << std::endl;
}

int main(int argc, char** argv) {
    // Check minimum arguments
    if (argc < 4) {
        printUsage(argv[0]);
        return -1;
    }

    std::string inputFile = argv[1];
    std::string outputFile = argv[2];
    std::string effect = argv[3];

    // Read the input image
    cv::Mat src = cv::imread(inputFile, cv::IMREAD_COLOR);

    if (src.empty()) {
        std::cerr << "Error: Could not read image from " << inputFile << std::endl;
        return -1;
    }

    cv::Mat result;

    // Apply the requested effect
    if (effect == "negative") {
        result = createNegative(src);
        std::cout << "Applied negative effect" << std::endl;
    }
    else if (effect == "mirror-h") {
        result = mirrorHorizontal(src);
        std::cout << "Applied horizontal mirror effect" << std::endl;
    }
    else if (effect == "mirror-v") {
        result = mirrorVertical(src);
        std::cout << "Applied vertical mirror effect" << std::endl;
    }
    else if (effect == "rotate") {
        if (argc < 5) {
            std::cerr << "Error: rotate effect requires rotation parameter" << std::endl;
            std::cout << "Usage: " << argv[0] << " <input> <output> rotate <n>" << std::endl;
            return -1;
        }
        int rotations = std::stoi(argv[4]);
        result = rotateMultiple90(src, rotations);
        std::cout << "Applied rotation by " << (rotations * 90) << " degrees" << std::endl;
    }
    else if (effect == "brightness") {
        if (argc < 5) {
            std::cerr << "Error: brightness effect requires delta parameter" << std::endl;
            std::cout << "Usage: " << argv[0] << " <input> <output> brightness <delta>" << std::endl;
            return -1;
        }
        int delta = std::stoi(argv[4]);
        result = adjustBrightness(src, delta);
        std::cout << "Applied brightness adjustment: " << (delta > 0 ? "+" : "") << delta << std::endl;
    }
    else {
        std::cerr << "Error: Unknown effect '" << effect << "'" << std::endl;
        printUsage(argv[0]);
        return -1;
    }

    // Write the output image
    if (!cv::imwrite(outputFile, result)) {
        std::cerr << "Error: Could not write image to " << outputFile << std::endl;
        return -1;
    }

    std::cout << "Successfully processed " << inputFile << " -> " << outputFile << std::endl;
    std::cout << "Image size: " << src.rows << "x" << src.cols << std::endl;

    return 0;
}
