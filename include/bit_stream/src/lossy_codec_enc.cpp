#include "bit_stream.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sndfile.h>
#include <vector>

using namespace std;

const int BLOCK_SIZE = 1024;
const double BASE_QUANTIZATION = 0.002;

// DCT Type-II implementation
void dct(const vector<double> &input, vector<double> &output) {
    int N = input.size();
    output.resize(N);

    for (int k = 0; k < N; k++) {
        double sum = 0.0;
        double scale = (k == 0) ? sqrt(1.0 / N) : sqrt(2.0 / N);

        for (int n = 0; n < N; n++) {
            sum += input[n] * cos(M_PI * k * (n + 0.5) / N);
        }
        output[k] = sum * scale;
    }
}

// Psychoacoustic weighting - reduce quantization for perceptually important
// frequencies
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

// Calculate block energy for adaptive quantization
double calculate_energy(const vector<double> &block) {
    double energy = 0.0;
    for (double val : block) {
        energy += val * val;
    }
    return sqrt(energy / block.size());
}

// Quantize with psychoacoustic weighting
void quantize_weighted(const vector<double> &dct_coeffs,
                       vector<int32_t> &quantized, double base_step,
                       double energy_factor) {
    quantized.resize(dct_coeffs.size());

    for (size_t i = 0; i < dct_coeffs.size(); i++) {
        double weight = get_weight(i, dct_coeffs.size());
        double adaptive_step = base_step * weight * energy_factor;
        quantized[i] =
            static_cast<int32_t>(round(dct_coeffs[i] / adaptive_step));
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " input.wav output.dct\n";
        return 1;
    }

    SF_INFO sfinfo;
    SNDFILE *infile = sf_open(argv[1], SFM_READ, &sfinfo);
    if (!infile) {
        cerr << "Error opening input file: " << sf_strerror(nullptr) << endl;
        return 1;
    }

    if (sfinfo.channels != 1) {
        cerr << "Error: Only mono audio files are supported\n";
        sf_close(infile);
        return 1;
    }

    cout << "Sample rate: " << sfinfo.samplerate << " Hz\n";
    cout << "Total frames: " << sfinfo.frames << "\n";

    fstream ofs(argv[2], ios::out | ios::binary);
    if (!ofs.is_open()) {
        cerr << "Error opening output file\n";
        sf_close(infile);
        return 1;
    }

    BitStream obs(ofs, STREAM_WRITE);

    obs.write_n_bits(sfinfo.samplerate, 32);
    obs.write_n_bits(sfinfo.frames, 32);
    obs.write_n_bits(BLOCK_SIZE, 16);

    uint32_t quant_fixed = static_cast<uint32_t>(BASE_QUANTIZATION * 1000000);
    obs.write_n_bits(quant_fixed, 32);

    vector<double> buffer(BLOCK_SIZE);
    vector<double> dct_coeffs;
    vector<int32_t> quantized;

    long long total_frames = sfinfo.frames;
    long long frames_read = 0;
    int blocks_processed = 0;

    while (frames_read < total_frames) {
        sf_count_t count = sf_read_double(infile, buffer.data(), BLOCK_SIZE);

        if (count < BLOCK_SIZE) {
            for (sf_count_t i = count; i < BLOCK_SIZE; i++) {
                buffer[i] = 0.0;
            }
        }

        double energy = calculate_energy(buffer);
        double energy_factor = max(0.5, min(2.0, energy * 10.0));

        dct(buffer, dct_coeffs);

        quantize_weighted(dct_coeffs, quantized, BASE_QUANTIZATION,
                          energy_factor);

        uint16_t energy_enc = static_cast<uint16_t>(energy_factor * 1000);
        obs.write_n_bits(energy_enc, 16);

        for (int32_t coeff : quantized) {
            if (coeff < 0) {
                obs.write_bit(1);
                coeff = -coeff;
            } else {
                obs.write_bit(0);
            }

            int bits_needed = 0;
            int32_t temp = coeff;
            while (temp > 0) {
                bits_needed++;
                temp >>= 1;
            }

            if (bits_needed == 0)
                bits_needed = 1;
            if (bits_needed > 20)
                bits_needed = 20;

            obs.write_n_bits(bits_needed, 5);

            obs.write_n_bits(coeff, bits_needed);
        }

        frames_read += count;
        blocks_processed++;

        if (count < BLOCK_SIZE)
            break;
    }

    obs.close();
    sf_close(infile);

    cout << "Encoding complete.\n";
    cout << "Processed " << blocks_processed << " blocks\n";
    cout << "Using adaptive quantization and psychoacoustic weighting.\n";

    return 0;
}
