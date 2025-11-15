#ifndef LOSSLESS_AUDIO_HPP
#define LOSSLESS_AUDIO_HPP

#include <string>
#include <cstdint>

/**
 * Encode a WAV file using Golomb coding of prediction residuals.
 * 
 * @param inWav Input WAV file path
 * @param outFile Output compressed file path
 * @param m Golomb parameter (0 = adaptive, >0 = fixed)
 * @param blockSamples Number of frames per block
 * @param predictorOrder Predictor order (0-3): 0=none, 1=1-tap, 2=2-tap, 3=3-tap
 * @param verbose Print progress/statistics
 * @return true on success
 */
bool encodeWavWithGolomb(const std::string& inWav, 
                         const std::string& outFile, 
                         uint32_t m, 
                         uint32_t blockSamples,
                         uint32_t predictorOrder,
                         bool verbose);

/**
 * Decode a Golomb-compressed file to WAV.
 * 
 * @param inFile Input compressed file path
 * @param outWav Output WAV file path
 * @param verbose Print progress/statistics
 * @return true on success
 */
bool decodeGolombToWav(const std::string& inFile, 
                       const std::string& outWav, 
                       bool verbose);

#endif // LOSSLESS_CODEC_HPP