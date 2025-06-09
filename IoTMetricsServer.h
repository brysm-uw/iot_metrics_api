#pragma once

#include <memory>
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <limits>
#include <httplib.h>
#include <nlohmann/json.hpp>

// OpenTelemetry includes
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/data/metric_data.h>
#include <opentelemetry/sdk/metrics/data/point_data.h>
#include <opentelemetry/exporters/prometheus/exporter_factory.h>
#include <opentelemetry/exporters/prometheus/exporter_options.h>
#include <opentelemetry/metrics/provider.h>

// Namespace aliases for cleaner code
namespace metrics_api = opentelemetry::metrics;
namespace metrics_sdk = opentelemetry::sdk::metrics;
namespace prometheus_exporter = opentelemetry::exporter::metrics;

class IoTMetricsServer {
public:
    //==============================================================================
    // CONSTRUCTOR & DESTRUCTOR
    //==============================================================================
    IoTMetricsServer(int port = 8080, int metrics_port = 9090);
    ~IoTMetricsServer();

    //==============================================================================
    // SERVER LIFECYCLE METHODS
    //==============================================================================
    bool start();
    void stop();

private:
    //==============================================================================
    // MEMBER VARIABLES
    //==============================================================================

    // Server configuration
    int port_;
    int metrics_port_;
    bool server_running_;

    // HTTP server
    std::unique_ptr<httplib::Server> http_server_;

    // OpenTelemetry components
    std::shared_ptr<metrics_api::MeterProvider> meter_provider_;

    // Default histogram boundaries (OpenTelemetry spec)
    const std::vector<double> default_histogram_boundaries_ = {
        0, 5, 10, 25, 50, 75, 100, 250, 500, 750, 1000, 2500, 5000, 7500, 10000
    };

    //==============================================================================
    // HISTOGRAM STATE MANAGEMENT
    //==============================================================================

    struct HistogramState {
        uint64_t count = 0;
        double sum = 0.0;
        double min = std::numeric_limits<double>::max();
        double max = std::numeric_limits<double>::lowest();
        std::vector<uint64_t> bucket_counts;
        std::vector<double> boundaries;

        // Default constructor (required for std::map operations)
        HistogramState() = default;

        // Parameterized constructor
        HistogramState(const std::vector<double>& bounds) : boundaries(bounds) {
            bucket_counts.resize(bounds.size() + 1, 0);
        }
    };

    //==============================================================================
    // METRIC STORAGE
    //==============================================================================

    // Storage for different metric types (MetricData objects as unique_ptr to avoid copying)
    std::map<std::string, std::unique_ptr<metrics_sdk::MetricData>> counter_metrics_;
    std::map<std::string, std::unique_ptr<metrics_sdk::MetricData>> updowncounter_metrics_;
    std::map<std::string, std::unique_ptr<metrics_sdk::MetricData>> histogram_metrics_;
    std::map<std::string, std::unique_ptr<metrics_sdk::MetricData>> gauge_metrics_;
    // Histogram state tracking (metric_name -> attribute_key -> state)
    std::map<std::string, std::map<std::string, HistogramState>> histogram_states_;

    //==============================================================================
    // CUSTOM PROMETHEUS EXPORT METHODS
    //==============================================================================

    void handlePrometheusMetrics(const httplib::Request& req, httplib::Response& res);
    std::string formatPrometheusMetrics();
    std::string formatCounterForPrometheus(const std::string& name,
        const metrics_sdk::MetricData& metric_data);
    std::string formatUpDownCounterForPrometheus(const std::string& name,
        const metrics_sdk::MetricData& metric_data);
    std::string formatHistogramForPrometheus(const std::string& name,
        const metrics_sdk::MetricData& metric_data);
    std::string formatGaugeForPrometheus(const std::string& name,
        const metrics_sdk::MetricData& metric_data);
    std::string formatAttributes(const metrics_sdk::PointAttributes& attributes);
    std::string sanitizeMetricName(const std::string& name);

    //==============================================================================
    // THREAD SAFETY
    //==============================================================================

    std::mutex metrics_mutex_;
    std::mutex instruments_mutex_;

    //==============================================================================
    // INITIALIZATION METHODS
    //==============================================================================

    void initializeMetrics();
    void setupRoutes();

    //==============================================================================
    // HTTP ENDPOINT HANDLERS
    //==============================================================================

    void handleMetric(const httplib::Request& req, httplib::Response& res);
    void handleHealth(const httplib::Request& req, httplib::Response& res);
    void handleStatus(const httplib::Request& req, httplib::Response& res);
    void handleMetricsList(const httplib::Request& req, httplib::Response& res);

    //==============================================================================
    // METRIC RECORDING METHODS
    //==============================================================================

    void recordMetric(const std::string& metric_name,
        const std::string& instrument_type,
        double value,
        const std::map<std::string, std::string>& attributes,
        const std::string& unit,
        const std::string& description);

    void recordCounterMetricData(const std::string& name,
        double value,
        const std::map<std::string, std::string>& attributes,
        const std::string& unit,
        const std::string& description);

    void recordUpDownCounterMetricData(const std::string& name,
        double value,
        const std::map<std::string, std::string>& attributes,
        const std::string& unit,
        const std::string& description);

    void recordHistogramMetricData(const std::string& name,
        double value,
        const std::map<std::string, std::string>& attributes,
        const std::string& unit,
        const std::string& description);

    void recordGaugeMetricData(const std::string& name,
        double value,
        const std::map<std::string, std::string>& attributes,
        const std::string& unit,
        const std::string& description);
    //==============================================================================
    // HELPER METHODS
    //==============================================================================

    std::string createAttributeKey(const std::map<std::string, std::string>& attributes);
    size_t findBucketIndex(double value, const std::vector<double>& boundaries);
    std::vector<uint64_t> calculateCumulativeCounts(const std::vector<uint64_t>& bucket_counts);

    //==============================================================================
    // UTILITY METHODS
    //==============================================================================

    bool validateMetricRequest(const nlohmann::json& request, std::string& error_msg);
    nlohmann::json createErrorResponse(const std::string& error, int code = 400);
    nlohmann::json createSuccessResponse(const std::string& message);
};