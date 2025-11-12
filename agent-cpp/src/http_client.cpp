#include "http_client.hpp"
#include <curl/curl.h>
#include <sstream>
#include <iostream>
#include <iomanip>

namespace {
    // Callback for writing response data
    size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
        size_t total_size = size * nmemb;
        data->append((char*)contents, total_size);
        return total_size;
    }
}

HttpClient::HttpClient(const std::string& base_url)
    : base_url_(base_url)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpClient::~HttpClient() {
    curl_global_cleanup();
}

std::string HttpClient::formatMetricsJson(const std::string& device_id, const std::vector<MetricPoint>& metrics) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(2);
    
    json << "{\n";
    json << "  \"deviceId\": \"" << device_id << "\",\n";
    json << "  \"metrics\": [\n";
    
    for (size_t i = 0; i < metrics.size(); i++) {
        const auto& m = metrics[i];
        json << "    {\n";
        if (!m.ts.empty()) {
            json << "      \"ts\": \"" << m.ts << "\",\n";
        }
        json << "      \"temperature_c\": " << m.temperature_c << ",\n";
        json << "      \"vibration_g\": " << m.vibration_g << ",\n";
        json << "      \"humidity_pct\": " << m.humidity_pct << ",\n";
        json << "      \"voltage_v\": " << m.voltage_v << "\n";
        json << "    }";
        if (i < metrics.size() - 1) json << ",";
        json << "\n";
    }
    
    json << "  ]\n";
    json << "}";
    
    return json.str();
}

bool HttpClient::postMetrics(const std::string& device_id, const std::vector<MetricPoint>& metrics) {
    if (metrics.empty()) {
        last_error_ = "No metrics to send";
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        last_error_ = "Failed to initialize CURL";
        return false;
    }

    std::string json_data = formatMetricsJson(device_id, metrics);
    std::string response_data;

    std::string url = base_url_ + "/api/ingest";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);

    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        last_error_ = "CURL error: " + std::string(curl_easy_strerror(res));
        return false;
    }

    if (response_code < 200 || response_code >= 300) {
        last_error_ = "HTTP error: " + std::to_string(response_code) + " - " + response_data;
        return false;
    }

    return true;
}

