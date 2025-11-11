#include "golomb.hpp"
#include <cmath>
#include <stdexcept>
#include <sstream>

Golomb::Golomb(uint32_t m, NegativeMode mode) : m(m), mode(mode) {
    if (m == 0) {
        throw std::invalid_argument("Golomb parameter m must be greater than 0");
    }
    calculateB();
}

void Golomb::setM(uint32_t m) {
    if (m == 0) {
        throw std::invalid_argument("Golomb parameter m must be greater than 0");
    }
    this->m = m;
    calculateB();
}

void Golomb::calculateB() {
    // b = ceil(log2(m))
    b = static_cast<uint32_t>(std::ceil(std::log2(static_cast<double>(m))));
}

uint32_t Golomb::mapToUnsigned(int32_t n) const {
    if (mode == NegativeMode::SIGN_MAGNITUDE) {
        // Sign-magnitude: use the absolute value
        // The sign will be handled separately
        return static_cast<uint32_t>(std::abs(n));
    }
    // Interleaving: map negative and positive numbers alternately
    // 0 -> 0, 1 -> 1, -1 -> 2, 2 -> 3, -2 -> 4, 3 -> 5, -3 -> 6, ...
    // Formula: n >= 0 ? 2*n : -2*n - 1
    if (n >= 0) {
        return static_cast<uint32_t>(2 * n);
    }
    return static_cast<uint32_t>(-2 * n - 1);
}

int32_t Golomb::mapToSigned(uint32_t n) const {
    if (mode == NegativeMode::SIGN_MAGNITUDE) {
        // This should not be called in sign-magnitude mode
        // as the sign is handled separately
        return static_cast<int32_t>(n);
    }
    // Interleaving: reverse the mapping
    // 0 -> 0, 1 -> 1, 2 -> -1, 3 -> 2, 4 -> -2, 5 -> 3, 6 -> -3, ...
    if (n % 2 == 0) {
        return static_cast<int32_t>(n / 2);
    }
    return -static_cast<int32_t>((n + 1) / 2);
}

std::vector<uint8_t> Golomb::encode(int32_t n) const {
    std::vector<uint8_t> result;

    // Handle sign for sign-magnitude mode
    if (mode == NegativeMode::SIGN_MAGNITUDE) {
        bool isNegative = false;
        isNegative = (n < 0);
        result.push_back(isNegative ? 1 : 0);
    }

    // Map to unsigned
    const uint32_t mapped = mapToUnsigned(n);

    // Golomb encoding
    const uint32_t q = mapped / m;  // quotient
    const uint32_t r = mapped % m;  // remainder

    // Encode quotient in unary: q zeros followed by a one
    for (uint32_t i = 0; i < q; i++) {
        result.push_back(0);
    }
    result.push_back(1);

    // Encode remainder in truncated binary
    // Calculate the cutoff value

    if (const uint32_t cutoff = (1u << b) - m; r < cutoff) {
        // Use b-1 bits
        for (int i = static_cast<int>(b) - 2; i >= 0; i--) {
            result.push_back((r >> i) & 1);
        }
    } else {
        // Use b bits
        const uint32_t adjusted = r + cutoff;
        for (int i = static_cast<int>(b) - 1; i >= 0; i--) {
            result.push_back((adjusted >> i) & 1);
        }
    }

    return result;
}

int32_t Golomb::decode(const std::vector<uint8_t>& bits, size_t& bitsUsed) const {
    if (bits.empty()) {
        throw std::invalid_argument("Bit sequence is empty");
    }

    size_t pos = 0;
    bool isNegative = false;

    // Handle sign for sign-magnitude mode
    if (mode == NegativeMode::SIGN_MAGNITUDE) {
        if (pos >= bits.size()) {
            throw std::invalid_argument("Insufficient bits for sign");
        }
        isNegative = (bits[pos++] == 1);
    }

    // Decode quotient (count zeros until we hit a one)
    uint32_t q = 0;
    while (pos < bits.size() && bits[pos] == 0) {
        q++;
        pos++;
    }

    if (pos >= bits.size()) {
        throw std::invalid_argument("Insufficient bits for quotient terminator");
    }
    pos++; // Skip the terminating 1

    // Decode remainder
    const uint32_t cutoff = (1u << b) - m;
    uint32_t r = 0;

    // Read b-1 bits first
    if (pos + (b - 1) > bits.size()) {
        throw std::invalid_argument("Insufficient bits for remainder");
    }

    for (uint32_t i = 0; i < b - 1; i++) {
        r = (r << 1) | bits[pos++];
    }

    if (r < cutoff) {
        // We're done, r uses b-1 bits
    } else {
        // Need to read one more bit
        if (pos >= bits.size()) {
            throw std::invalid_argument("Insufficient bits for remainder (extended)");
        }
        r = (r << 1) | bits[pos++];
        r -= cutoff;
    }

    // Calculate the mapped value
    const uint32_t mapped = q * m + r;

    // Update bits used
    bitsUsed = pos;

    // Map back to signed
    int32_t result;
    if (mode == NegativeMode::SIGN_MAGNITUDE) {
        result = static_cast<int32_t>(mapped);
        if (isNegative) {
            result = -result;
        }
    } else {
        result = mapToSigned(mapped);
    }

    return result;
}

int32_t Golomb::decode(const std::vector<uint8_t>& bits) const {
    size_t bitsUsed;
    return decode(bits, bitsUsed);
}

std::string Golomb::bitsToString(const std::vector<uint8_t>& bits) {
    std::ostringstream oss;
    for (size_t i = 0; i < bits.size(); i++) {
        oss << static_cast<int>(bits[i]);
        if ((i + 1) % 8 == 0 && i + 1 < bits.size()) {
            oss << " ";
        }
    }
    return oss.str();
}

