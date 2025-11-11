#ifndef GOLOMB_HPP
#define GOLOMB_HPP

#include <vector>
#include <cstdint>
#include <string>

/**
 * @brief Golomb coding class for encoding and decoding integers
 *
 * This class implements Golomb coding with support for negative numbers
 * using two different approaches:
 * 1. Sign and magnitude representation
 * 2. Positive/negative value interleaving
 */
class Golomb {
public:
    /**
     * @brief Enum for choosing negative number encoding method
     */
    enum class NegativeMode {
        SIGN_MAGNITUDE,      // Uses separate sign bit
        INTERLEAVING         // Interleaves positive and negative values
    };

private:
    uint32_t m;              // Golomb parameter
    uint32_t b{};              // Number of bits for remainder encoding
    NegativeMode mode;       // Mode for handling negative numbers

    /**
     * @brief Calculate the number of bits needed for remainder
     */
    void calculateB();

    /**
     * @brief Map a signed integer to unsigned based on the mode
     * @param n The signed integer to map
     * @return The mapped unsigned integer
     */
    [[nodiscard]] uint32_t mapToUnsigned(int32_t n) const;

    /**
     * @brief Map an unsigned integer back to signed based on the mode
     * @param n The unsigned integer to map
     * @return The mapped signed integer
     */
    [[nodiscard]] int32_t mapToSigned(uint32_t n) const;

public:
    /**
     * @brief Construct a Golomb coder with parameter m
     * @param m The Golomb parameter (must be > 0)
     * @param mode The mode for handling negative numbers
     */
    explicit Golomb(uint32_t m = 1, NegativeMode mode = NegativeMode::INTERLEAVING);

    /**
     * @brief Set the Golomb parameter m
     * @param m The new Golomb parameter (must be > 0)
     */
    void setM(uint32_t m);

    /**
     * @brief Get the current Golomb parameter m
     * @return The current Golomb parameter
     */
    [[nodiscard]] uint32_t getM() const { return m; }

    /**
     * @brief Set the negative number encoding mode
     * @param mode The new mode
     */
    void setMode(NegativeMode mode) { this->mode = mode; }

    /**
     * @brief Get the current negative number encoding mode
     * @return The current mode
     */
    [[nodiscard]] NegativeMode getMode() const { return mode; }

    /**
     * @brief Encode an integer using Golomb coding
     * @param n The integer to encode
     * @return A vector of bits (0 or 1) representing the encoded value
     */
    [[nodiscard]] std::vector<uint8_t> encode(int32_t n) const;

    /**
     * @brief Decode a sequence of bits using Golomb coding
     * @param bits The bit sequence to decode
     * @param bitsUsed Output parameter - number of bits consumed from the input
     * @return The decoded integer
     */
    int32_t decode(const std::vector<uint8_t>& bits, size_t& bitsUsed) const;

    /**
     * @brief Decode a sequence of bits using Golomb coding (simple version)
     * @param bits The bit sequence to decode
     * @return The decoded integer
     */
    [[nodiscard]] int32_t decode(const std::vector<uint8_t>& bits) const;

    /**
     * @brief Convert bit vector to string for debugging
     * @param bits The bit vector
     * @return String representation
     */
    static std::string bitsToString(const std::vector<uint8_t>& bits);
};

#endif // GOLOMB_HPP

