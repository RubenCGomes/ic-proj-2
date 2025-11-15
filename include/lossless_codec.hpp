#ifndef LOSSLESS_CODEC_HPP
#define LOSSLESS_CODEC_HPP

#include <cstdint>
#include <string>

bool encodeWavWithGolomb(const std::string& inWav,
                         const std::string& outFile,
                         uint32_t m,
                         uint32_t blockSamples,
                         bool verbose);

bool decodeGolombToWav(const std::string& inFile,
                       const std::string& outWav,
                       bool verbose);

#endif // LOSSLESS_CODEC_HPP