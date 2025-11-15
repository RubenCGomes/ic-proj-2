#include "bit_stream.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sndfile.h>
#include <vector>

using namespace std;

// Inverse DCT (DCT Type-III)
void idct(const vector<double> &input, vector<double> &output) {
    int N = input.size();
    output.resize(N);

    for (int n = 0; n < N; n++) {
        double sum = 0.0;

        for (int k = 0; k < N; k++) {
            double scale = (k == 0) ? sqrt(1.0 / N) : sqrt(2.0 / N);
            sum += scale * input[k] * cos(M_PI * k * (n + 0.5) / N);
        }

        output[n] = sum;
    }
}

// Psychoacoustic weighting (same as encoder)
double get_weight(int index, int block_size) {
    double freq_ratio = static_cast<double>(index) / block_size;

    if (freq_ratio < 0.1)
        return 0.5;
    else if (freq_ratio < 0.3)
        return 1.0;
    else if (freq_ratio < 0.5)
        return 1.5;
    else
        return 2.5;
}

// Dequantize with psychoacoustic weighting
void dequantize_weighted(const vector<int32_t> &quantized,
                         vector<double> &dct_coeffs, double base_step,
                         double energy_factor) {
    dct_coeffs.resize(quantized.size());

    for (size_t i = 0; i < quantized.size(); i++) {
        double weight = get_weight(i, quantized.size());
        double adaptive_step = base_step * weight * energy_factor;
        dct_coeffs[i] = quantized[i] * adaptive_step;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " input.dct output.wav\n";
        return 1;
    }

    fstream ifs(argv[1], ios::in | ios::binary);
    if (!ifs.is_open()) {
        cerr << "Error opening input file\n";
        return 1;
    }

    BitStream ibs(ifs, STREAM_READ);

    uint32_t samplerate = ibs.read_n_bits(32);
    uint32_t total_frames = ibs.read_n_bits(32);
    uint16_t block_size = ibs.read_n_bits(16);
    uint32_t quant_fixed = ibs.read_n_bits(32);
    double base_quant = quant_fixed / 1000000.0;

    cout << "Sample rate: " << samplerate << " Hz\n";
    cout << "Total frames: " << total_frames << "\n";
    cout << "Block size: " << block_size << "\n";
    cout << "Base quantization: " << base_quant << "\n";

    SF_INFO sfinfo;
    sfinfo.samplerate = samplerate;
    sfinfo.channels = 1;
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE *outfile = sf_open(argv[2], SFM_WRITE, &sfinfo);
    if (!outfile) {
        cerr << "Error opening output file: " << sf_strerror(nullptr) << endl;
        return 1;
    }

    vector<int32_t> quantized(block_size);
    vector<double> dct_coeffs;
    vector<double> samples;

    long long frames_written = 0;
    int blocks_processed = 0;

    while (frames_written < total_frames) {
        uint16_t energy_enc = ibs.read_n_bits(16);
        if (energy_enc == 0)
            break; // EOF check
        double energy_factor = energy_enc / 1000.0;

        bool eof_reached = false;
        for (int i = 0; i < block_size; i++) {
            int sign_bit = ibs.read_bit();
            if (sign_bit == EOF) {
                eof_reached = true;
                break;
            }

            int bits_needed = ibs.read_n_bits(5);
            if (bits_needed == 0)
                bits_needed = 1;

            uint64_t magnitude = ibs.read_n_bits(bits_needed);

            quantized[i] = (sign_bit == 1) ? -static_cast<int32_t>(magnitude)
                                           : static_cast<int32_t>(magnitude);
        }

        if (eof_reached)
            break;

        dequantize_weighted(quantized, dct_coeffs, base_quant, energy_factor);

        idct(dct_coeffs, samples);

        for (double &s : samples) {
            if (s > 1.0)
                s = 1.0;
            if (s < -1.0)
                s = -1.0;
        }

        long long to_write =
            min((long long)block_size, total_frames - frames_written);
        sf_count_t written = sf_write_double(outfile, samples.data(), to_write);

        frames_written += written;
        blocks_processed++;
    }

    ibs.close();
    sf_close(outfile);

    cout << "Decoding complete.\n";
    cout << "Processed " << blocks_processed << " blocks\n";
    cout << "Reconstructed " << frames_written << " frames\n";
    cout << "Using adaptive dequantization and psychoacoustic weighting.\n";

    return 0;
}
