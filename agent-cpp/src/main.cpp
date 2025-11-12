#include "config.hpp"
#include "http_client.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

// Get current timestamp in ISO 8601 format
std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    return oss.str();
}

// Generate simulated metrics
MetricPoint generateMetrics(double t, double anomaly_prob, std::mt19937& gen, std::normal_distribution<>& normal_dist) {
    MetricPoint point;
    point.ts = getCurrentTimestamp();

    // Base values with sinusoidal variation
    double temp_base = 22.0 + 3.0 * std::sin(t / 60.0); // ~1 minute cycle
    double vib_base = 0.02;
    double hum_base = 45.0;
    double volt_base = 4.9;

    // Add noise
    double temp_noise = normal_dist(gen) * 0.2;
    double vib_noise = std::abs(normal_dist(gen) * 0.01);
    double hum_noise = normal_dist(gen) * 0.5;
    double volt_noise = normal_dist(gen) * 0.01;

    // Occasionally inject anomalies
    std::uniform_real_distribution<> anomaly_dist(0.0, 1.0);
    bool inject_anomaly = anomaly_dist(gen) < anomaly_prob;

    if (inject_anomaly) {
        // Temperature spike
        if (anomaly_dist(gen) < 0.5) {
            temp_noise += 8.0; // +8C spike
            std::cout << "[ANOMALY] Temperature spike detected!" << std::endl;
        } else {
            // Vibration spike
            vib_noise += 0.5; // Large vibration spike
            std::cout << "[ANOMALY] Vibration spike detected!" << std::endl;
        }
    }

    point.temperature_c = temp_base + temp_noise;
    point.vibration_g = vib_base + vib_noise;
    point.humidity_pct = hum_base + hum_noise;
    point.voltage_v = volt_base + volt_noise;

    return point;
}

int main(int argc, char* argv[]) {
    std::cout << "IoT Edge Agent - Starting..." << std::endl;

    // Load configuration
    AgentConfig config;
    
    // Try to load from file first (relative to executable or current directory)
    std::string config_path = "config/agent.json";
    // Also try parent directory if running from build/
    if (!config.loadFromFile(config_path)) {
        config.loadFromFile("../config/agent.json");
    }
    
    // Override with environment variables
    config.loadFromEnv();
    
    // Override with command line arguments
    config.parseArgs(argc, argv);

    std::cout << "Configuration:" << std::endl;
    std::cout << "  Device ID: " << config.device_id << std::endl;
    std::cout << "  API URL: " << config.api_base_url << std::endl;
    std::cout << "  Interval: " << config.interval_ms << " ms" << std::endl;
    std::cout << "  Anomaly Probability: " << config.anomaly_probability << std::endl;

    // Initialize HTTP client
    HttpClient client(config.api_base_url);

    // Random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> normal_dist(0.0, 1.0);
    std::uniform_int_distribution<> jitter_dist(-config.jitter_ms, config.jitter_ms);

    // Time tracking
    auto start_time = std::chrono::steady_clock::now();
    double t = 0.0;

    // Retry configuration
    const int max_retries = 3;
    const int retry_delay_ms = 1000;

    std::cout << "Starting metric collection loop..." << std::endl;

    while (true) {
        try {
            // Generate metrics
            MetricPoint point = generateMetrics(t, config.anomaly_probability, gen, normal_dist);

            // Print metrics
            std::cout << "[" << point.ts << "] "
                      << "Temp: " << std::fixed << std::setprecision(2) << point.temperature_c << "°C, "
                      << "Vib: " << point.vibration_g << "g, "
                      << "Hum: " << point.humidity_pct << "%, "
                      << "Volt: " << point.voltage_v << "V"
                      << std::endl;

            // Send metrics with retry logic
            std::vector<MetricPoint> metrics = {point};
            bool success = false;

            for (int retry = 0; retry < max_retries; retry++) {
                if (client.postMetrics(config.device_id, metrics)) {
                    success = true;
                    break;
                }

                if (retry < max_retries - 1) {
                    std::cerr << "Warning: Failed to send metrics (attempt " << (retry + 1) << "/" << max_retries << "): "
                              << client.getLastError() << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms * (retry + 1)));
                }
            }

            if (!success) {
                std::cerr << "Error: Failed to send metrics after " << max_retries << " attempts: "
                          << client.getLastError() << std::endl;
            }

            // Calculate sleep time with jitter
            int sleep_ms = config.interval_ms + jitter_dist(gen);
            sleep_ms = std::max(100, sleep_ms); // Minimum 100ms

            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

            // Update time
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time).count();
            t = elapsed / 1000.0; // Convert to seconds

        } catch (const std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(config.interval_ms));
        }
    }

    return 0;
}

