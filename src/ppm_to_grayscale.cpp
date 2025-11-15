#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " input.ppm output_gray.ppm\n";
        return 1;
    }

    std::ifstream ifs(argv[1], std::ios::binary);
    if (!ifs) {
        std::cerr << "Error: Cannot open " << argv[1] << "\n";
        return 1;
    }

    std::string magic;
    ifs >> magic;

    if (magic != "P6") {
        std::cerr << "Error: Input must be P6 (color) PPM\n";
        return 1;
    }

    uint32_t width, height, maxval;
    ifs >> width >> height >> maxval;
    ifs.ignore(1);

    std::vector<uint8_t> rgb(width * height * 3);
    ifs.read(reinterpret_cast<char*>(rgb.data()), width * height * 3);
    ifs.close();

    // Convert RGB to grayscale using standard formula: Y = 0.299*R + 0.587*G + 0.114*B
    std::vector<uint8_t> gray(width * height);
    for (size_t i = 0; i < width * height; ++i) {
        uint8_t r = rgb[i * 3 + 0];
        uint8_t g = rgb[i * 3 + 1];
        uint8_t b = rgb[i * 3 + 2];
        gray[i] = static_cast<uint8_t>(0.299 * r + 0.587 * g + 0.114 * b);
    }

    // Write P5 (grayscale) output
    std::ofstream ofs(argv[2], std::ios::binary);
    ofs << "P5\n" << width << " " << height << "\n" << maxval << "\n";
    ofs.write(reinterpret_cast<const char*>(gray.data()), width * height);
    ofs.close();

    std::cout << "Converted " << argv[1] << " (" << width << "x" << height << " RGB)\n";
    std::cout << "       -> " << argv[2] << " (" << width << "x" << height << " grayscale)\n";

    return 0;
}