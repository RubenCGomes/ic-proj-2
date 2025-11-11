#include "golomb.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>

void printUsage(const char *progName)
{
    std::cout << "Usage: " << progName << " [OPTIONS] <command> <values...>\n\n";
    std::cout << "Commands:\n";
    std::cout << "  encode <int>...     Encode one or more integers\n";
    std::cout << "  decode <bits>...    Decode one or more bit strings (e.g., \"10110\")\n\n";
    std::cout << "Options:\n";
    std::cout << "  -m <value>          Set Golomb parameter m (default: 4)\n";
    std::cout << "  -mode <mode>        Set negative number mode:\n";
    std::cout << "                        interleaving (default) - interleave positive/negative\n";
    std::cout << "                        sign-magnitude - use sign bit\n";
    std::cout << "  -h, --help          Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " encode 5 -3 10 0\n";
    std::cout << "  " << progName << " -m 8 encode 42\n";
    std::cout << "  " << progName << " -mode sign-magnitude encode -15 20\n";
    std::cout << "  " << progName << " decode 10110 0110\n";
    std::cout << "  " << progName << " -m 4 decode 00010\n\n";
}

/**
 * Simple helper function that converts a sequence of bits (as a string) into
 * a vector of unsigned 8-bit integers where each bit is represented as 0 or 1.
 *
 * @param bitStr A string containing only '0' and '1' characters, representing the bit sequence.
 * @return A vector of uint8_t where each element is either 0 or 1, corresponding to bits in the input string.
 * @throws std::invalid_argument If the input string contains any character other than '0' or '1'.
 */
std::vector<uint8_t> stringToBits(const std::string& bitStr)
{
    std::vector<uint8_t> bits;
    for (char c : bitStr) {
        if (c == '0') {
            bits.push_back(0);
        } else if (c == '1') {
            bits.push_back(1);
        } else {
            throw std::invalid_argument("Invalid bit string - must contain only 0 and 1");
        }
    }
    return bits;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    // Default parameters
    uint32_t m = 4;
    Golomb::NegativeMode mode = Golomb::NegativeMode::INTERLEAVING;
    std::string command;
    std::vector<std::string> values;

    // Parse args
    int i = 1;
    while (i < argc) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }

        if (arg == "-m") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -m requires a value\n";
                return 1;
            }
            try {
                m = std::stoul(argv[i + 1]);
                if (m == 0) {
                    std::cerr << "Error: m must be greater than 0\n";
                    return 1;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid value for -m: " << argv[i + 1] << "\n";
                return 1;
            }
            i += 2;
        }
        else if (arg == "-mode") {
            if (i + 1 >= argc) {
                std::cerr << "Error: -mode requires a value\n";
                return 1;
            }
            std::string modeStr = argv[i + 1];
            if (modeStr == "interleaving") {
                mode = Golomb::NegativeMode::INTERLEAVING;
            } else if (modeStr == "sign-magnitude") {
                mode = Golomb::NegativeMode::SIGN_MAGNITUDE;
            } else {
                std::cerr << "Error: Invalid mode '" << modeStr << "'. Use 'interleaving' or 'sign-magnitude'\n";
                return 1;
            }
            i += 2;
        }
        else if (arg == "encode" || arg == "decode") {
            command = arg;
            i++;
            // What's ahead is considered to be values to parse
            while (i < argc) {
                values.push_back(argv[i]);
                i++;
            }
            break;
        }
        else {
            std::cerr << "Error: Unknown option or command '" << arg << "'\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    // Validate command
    if (command.empty()) {
        std::cerr << "Error: No command specified (encode or decode)\n";
        printUsage(argv[0]);
        return 1;
    }

    if (values.empty()) {
        std::cerr << "Error: No values provided for " << command << " operation\n";
        return 1;
    }

    // Create Golomb coder
    Golomb golomb(m, mode);

    // Display configuration
    std::cout << "Golomb Coding Configuration:\n";
    std::cout << "  m = " << m << "\n";
    std::cout << "  Mode = " << (mode == Golomb::NegativeMode::INTERLEAVING ? "INTERLEAVING" : "SIGN_MAGNITUDE") << "\n";
    std::cout << "\n";

    try {
        if (command == "encode") {
            // Encode integers
            std::cout << "Encoding integers:\n";
            std::cout << std::string(60, '-') << "\n";

            std::vector<uint8_t> allBits;

            for (const auto& valStr : values) {
                int32_t value = std::stoi(valStr);
                auto encoded = golomb.encode(value);

                std::cout << std::setw(8) << value << " -> "
                          << Golomb::bitsToString(encoded)
                          << " (" << encoded.size() << " bits)\n";

                // Accumulate bits for sequential stream
                allBits.insert(allBits.end(), encoded.begin(), encoded.end());
            }

            if (values.size() > 1) {
                std::cout << std::string(60, '-') << "\n";
                std::cout << "Complete bit stream (" << allBits.size() << " bits):\n";
                std::cout << Golomb::bitsToString(allBits) << "\n";
            }
        }
        else if (command == "decode") {
            // Decode bit strings
            std::cout << "Decoding bit strings:\n";
            std::cout << std::string(60, '-') << "\n";

            for (const auto& bitStr : values) {
                auto bits = stringToBits(bitStr);
                size_t bitsUsed;
                int32_t decoded = golomb.decode(bits, bitsUsed);

                std::cout << bitStr << " -> " << std::setw(8) << decoded;
                if (bitsUsed < bits.size()) {
                    std::cout << " (used " << bitsUsed << "/" << bits.size() << " bits, "
                              << (bits.size() - bitsUsed) << " bits remaining)";
                }
                std::cout << "\n";
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}