#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP

#include <string>
#include <vector>

struct MetricPoint {
    double temperature_c;
    double vibration_g;
    double humidity_pct;
    double voltage_v;
    std::string ts; // ISO 8601 timestamp
};

class HttpClient {
public:
    HttpClient(const std::string& base_url);
    ~HttpClient();

    // POST metrics to /api/ingest
    bool postMetrics(const std::string& device_id, const std::vector<MetricPoint>& metrics);

    // Get last error message
    std::string getLastError() const { return last_error_; }

private:
    std::string base_url_;
    std::string last_error_;
    
    // Helper to format JSON
    std::string formatMetricsJson(const std::string& device_id, const std::vector<MetricPoint>& metrics);
};

#endif // HTTP_CLIENT_HPP

