#ifndef FFT_ANALYZER_HPP
#define FFT_ANALYZER_HPP

#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>
#include <numeric>

/**
 * Lightweight FFT-based frequency domain analyzer for vibration data
 * Implements Cooley-Tukey FFT algorithm for anomaly detection
 */
class FFTAnalyzer {
public:
    struct FrequencyDomain {
        std::vector<double> magnitudes;
        std::vector<double> frequencies;
        double dominant_freq;
        double total_power;
    };

    FFTAnalyzer(size_t window_size = 256, double sample_rate = 1000.0)
        : window_size_(window_size), sample_rate_(sample_rate) {}

    /**
     * Add a vibration sample and return anomaly flag
     * Returns true if anomaly detected based on frequency domain analysis
     */
    bool addSample(double vibration_value) {
        samples_.push_back(vibration_value);
        
        if (samples_.size() > window_size_) {
            samples_.erase(samples_.begin());
        }

        // Need enough samples for meaningful FFT
        if (samples_.size() >= window_size_) {
            return analyzeFrequencyDomain();
        }

        return false;
    }

    /**
     * Perform FFT and analyze frequency domain
     */
    FrequencyDomain analyze() {
        FrequencyDomain result;
        
        if (samples_.size() < 2) {
            return result;
        }

        // Perform FFT
        std::vector<std::complex<double>> fft_result = fft(samples_);
        
        // Calculate magnitudes and frequencies
        result.magnitudes.resize(fft_result.size() / 2);
        result.frequencies.resize(fft_result.size() / 2);
        
        double max_magnitude = 0.0;
        size_t max_index = 0;
        result.total_power = 0.0;

        for (size_t i = 0; i < result.magnitudes.size(); ++i) {
            double magnitude = std::abs(fft_result[i]);
            result.magnitudes[i] = magnitude;
            result.frequencies[i] = (i * sample_rate_) / samples_.size();
            result.total_power += magnitude * magnitude;

            if (magnitude > max_magnitude) {
                max_magnitude = magnitude;
                max_index = i;
            }
        }

        result.dominant_freq = result.frequencies[max_index];
        return result;
    }

    /**
     * Get current samples
     */
    const std::vector<double>& getSamples() const {
        return samples_;
    }

    /**
     * Reset analyzer
     */
    void reset() {
        samples_.clear();
    }

private:
    /**
     * Cooley-Tukey FFT implementation
     */
    std::vector<std::complex<double>> fft(const std::vector<double>& input) {
        size_t n = input.size();
        
        // Pad to next power of 2 if needed
        size_t n_padded = 1;
        while (n_padded < n) {
            n_padded <<= 1;
        }

        std::vector<std::complex<double>> x(n_padded);
        for (size_t i = 0; i < n; ++i) {
            x[i] = std::complex<double>(input[i], 0.0);
        }
        for (size_t i = n; i < n_padded; ++i) {
            x[i] = std::complex<double>(0.0, 0.0);
        }

        return fftRecursive(x);
    }

    std::vector<std::complex<double>> fftRecursive(const std::vector<std::complex<double>>& x) {
        size_t n = x.size();
        
        if (n <= 1) {
            return x;
        }

        // Divide
        std::vector<std::complex<double>> even(n / 2);
        std::vector<std::complex<double>> odd(n / 2);
        
        for (size_t i = 0; i < n / 2; ++i) {
            even[i] = x[i * 2];
            odd[i] = x[i * 2 + 1];
        }

        // Conquer
        std::vector<std::complex<double>> even_fft = fftRecursive(even);
        std::vector<std::complex<double>> odd_fft = fftRecursive(odd);

        // Combine
        std::vector<std::complex<double>> result(n);
        for (size_t k = 0; k < n / 2; ++k) {
            double angle = -2.0 * M_PI * k / n;
            std::complex<double> t = std::exp(std::complex<double>(0, angle)) * odd_fft[k];
            result[k] = even_fft[k] + t;
            result[k + n / 2] = even_fft[k] - t;
        }

        return result;
    }

    /**
     * Analyze frequency domain for anomalies
     * Detects unusual frequency patterns or power spikes
     */
    bool analyzeFrequencyDomain() {
        FrequencyDomain fd = analyze();
        
        // Calculate mean and stddev of magnitudes
        double mean_mag = std::accumulate(fd.magnitudes.begin(), fd.magnitudes.end(), 0.0) 
                         / fd.magnitudes.size();
        
        double variance = 0.0;
        for (double mag : fd.magnitudes) {
            double diff = mag - mean_mag;
            variance += diff * diff;
        }
        double stddev_mag = std::sqrt(variance / fd.magnitudes.size());

        // Check for high power spikes (anomalies)
        double max_magnitude = *std::max_element(fd.magnitudes.begin(), fd.magnitudes.end());
        
        // Anomaly if magnitude is > 3 sigma above mean
        if (stddev_mag > 0.0 && max_magnitude > mean_mag + 3.0 * stddev_mag) {
            return true;
        }

        // Check for unusual dominant frequency (outside normal range 0-50 Hz)
        if (fd.dominant_freq > 50.0) {
            return true;
        }

        // Check for excessive total power
        double avg_power = fd.total_power / fd.magnitudes.size();
        if (avg_power > 100.0) { // Threshold for excessive vibration power
            return true;
        }

        return false;
    }

    size_t window_size_;
    double sample_rate_;
    std::vector<double> samples_;
};

#endif // FFT_ANALYZER_HPP

