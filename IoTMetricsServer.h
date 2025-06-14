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

/// @brief Namespace aliases for OpenTelemetry metrics API and SDK.
namespace metrics_api = opentelemetry::metrics;
namespace metrics_sdk = opentelemetry::sdk::metrics;
namespace prometheus_exporter = opentelemetry::exporter::metrics;

/// @brief IoTMetricsServer provides an HTTP API for ingesting and exporting OpenTelemetry metrics.
/// 
/// This server supports Counter, UpDownCounter, Histogram, and Gauge instruments,
/// and exposes Prometheus-compatible endpoints.
class IoTMetricsServer {
public:
    /// @brief Construct a new IoTMetricsServer.
    /// @param port The port for the main API server (default: 8080).
    /// @param metrics_port The port for the Prometheus metrics endpoint (default: 9090).
    IoTMetricsServer(int port = 8080, int metrics_port = 9090);

    /// @brief Destructor. Stops the server if running.
    ~IoTMetricsServer();

    /// @brief Start the HTTP server (blocking call).
    /// @return true if the server started successfully, false otherwise.
    bool start();

    /// @brief Stop the HTTP server if running.
    void stop();

private:
    //==============================================================================
    // MEMBER VARIABLES
    //==============================================================================

    /// @brief API server port.
    int port_;

    /// @brief Prometheus metrics endpoint port.
    int metrics_port_;

    /// @brief Indicates if the server is running.
    bool server_running_;

    /// @brief Pointer to the HTTP server instance.
    std::unique_ptr<httplib::Server> http_server_;

    /// @brief OpenTelemetry MeterProvider.
    std::shared_ptr<metrics_api::MeterProvider> meter_provider_;

    /// @brief Default histogram bucket boundaries (OpenTelemetry spec).
    const std::vector<double> default_histogram_boundaries_ = {
        0, 5, 10, 25, 50, 75, 100, 250, 500, 750, 1000, 2500, 5000, 7500, 10000
    };

    //==============================================================================
    // HISTOGRAM STATE MANAGEMENT
    //==============================================================================

    /// @brief Tracks the state of a histogram for a given metric and attribute set.
    struct HistogramState {
        /// @brief Total number of recorded values.
        uint64_t count = 0;
        /// @brief Sum of all recorded values.
        double sum = 0.0;
        /// @brief Minimum recorded value.
        double min = std::numeric_limits<double>::max();
        /// @brief Maximum recorded value.
        double max = std::numeric_limits<double>::lowest();
        /// @brief Counts for each histogram bucket.
        std::vector<uint64_t> bucket_counts;
        /// @brief Histogram bucket boundaries.
        std::vector<double> boundaries;

        /// @brief Default constructor.
        HistogramState() = default;

        /// @brief Construct with custom bucket boundaries.
        /// @param bounds The histogram bucket boundaries.
        HistogramState(const std::vector<double>& bounds) : boundaries(bounds) {
            bucket_counts.resize(bounds.size() + 1, 0);
        }
    };

    //==============================================================================
    // METRIC STORAGE
    //==============================================================================

    /// @brief Storage for Counter metrics (name -> MetricData).
    std::map<std::string, std::unique_ptr<metrics_sdk::MetricData>> counter_metrics_;
    /// @brief Storage for UpDownCounter metrics (name -> MetricData).
    std::map<std::string, std::unique_ptr<metrics_sdk::MetricData>> updowncounter_metrics_;
    /// @brief Storage for Histogram metrics (name -> MetricData).
    std::map<std::string, std::unique_ptr<metrics_sdk::MetricData>> histogram_metrics_;
    /// @brief Storage for Gauge metrics (name -> MetricData).
    std::map<std::string, std::unique_ptr<metrics_sdk::MetricData>> gauge_metrics_;
    /// @brief Histogram state tracking (metric_name -> attribute_key -> state).
    std::map<std::string, std::map<std::string, HistogramState>> histogram_states_;

    //==============================================================================
    // CUSTOM PROMETHEUS EXPORT METHODS
    //==============================================================================

    /// @brief Handle the /metrics endpoint for custom Prometheus export.
    void handlePrometheusMetrics(const httplib::Request& req, httplib::Response& res);

    /// @brief Format all metrics for Prometheus exposition.
    /// @return Formatted Prometheus metrics as a string.
    std::string formatPrometheusMetrics();

    /// @brief Format a Counter metric for Prometheus.
    /// @param name Metric name.
    /// @param metric_data Metric data.
    /// @return Prometheus-formatted string.
    std::string formatCounterForPrometheus(const std::string& name,
        const metrics_sdk::MetricData& metric_data);

    /// @brief Format an UpDownCounter metric for Prometheus.
    /// @param name Metric name.
    /// @param metric_data Metric data.
    /// @return Prometheus-formatted string.
    std::string formatUpDownCounterForPrometheus(const std::string& name,
        const metrics_sdk::MetricData& metric_data);

    /// @brief Format a Histogram metric for Prometheus.
    /// @param name Metric name.
    /// @param metric_data Metric data.
    /// @return Prometheus-formatted string.
    std::string formatHistogramForPrometheus(const std::string& name,
        const metrics_sdk::MetricData& metric_data);

    /// @brief Format a Gauge metric for Prometheus.
    /// @param name Metric name.
    /// @param metric_data Metric data.
    /// @return Prometheus-formatted string.
    std::string formatGaugeForPrometheus(const std::string& name,
        const metrics_sdk::MetricData& metric_data);

    /// @brief Format metric attributes for Prometheus.
    /// @param attributes Point attributes.
    /// @return Prometheus-formatted attribute string.
    std::string formatAttributes(const metrics_sdk::PointAttributes& attributes);

    /// @brief Sanitize a metric name for Prometheus compatibility.
    /// @param name Metric name.
    /// @return Sanitized metric name.
    std::string sanitizeMetricName(const std::string& name);

    //==============================================================================
    // THREAD SAFETY
    //==============================================================================

    /// @brief Mutex for protecting metric data.
    std::mutex metrics_mutex_;
    /// @brief Mutex for protecting instrument registration.
    std::mutex instruments_mutex_;

    //==============================================================================
    // INITIALIZATION METHODS
    //==============================================================================

    /// @brief Initialize OpenTelemetry metrics and exporters.
    void initializeMetrics();

    /// @brief Set up HTTP routes and endpoints.
    void setupRoutes();

    //==============================================================================
    // HTTP ENDPOINT HANDLERS
    //==============================================================================

    /// @brief Handle metric submission endpoint (/api/metrics).
    void handleMetric(const httplib::Request& req, httplib::Response& res);

    /// @brief Handle health check endpoint (/health).
    void handleHealth(const httplib::Request& req, httplib::Response& res);

    /// @brief Handle status endpoint (/api/status).
    void handleStatus(const httplib::Request& req, httplib::Response& res);

    /// @brief Handle metrics list endpoint (/api/metrics/list).
    void handleMetricsList(const httplib::Request& req, httplib::Response& res);

    //==============================================================================
    // METRIC RECORDING METHODS
    //==============================================================================

    /// @brief Record a metric of any supported type.
    /// @param metric_name Name of the metric.
    /// @param instrument_type Type of instrument ("counter", "updowncounter", "histogram", "gauge").
    /// @param value Value to record.
    /// @param attributes Key-value attributes for the metric.
    /// @param unit Unit of measurement.
    /// @param description Metric description.
    void recordMetric(const std::string& metric_name,
        const std::string& instrument_type,
        double value,
        const std::map<std::string, std::string>& attributes,
        const std::string& unit,
        const std::string& description);

    /// @brief Record a Counter metric.
    void recordCounterMetricData(const std::string& name,
        double value,
        const std::map<std::string, std::string>& attributes,
        const std::string& unit,
        const std::string& description);

    /// @brief Record an UpDownCounter metric.
    void recordUpDownCounterMetricData(const std::string& name,
        double value,
        const std::map<std::string, std::string>& attributes,
        const std::string& unit,
        const std::string& description);

    /// @brief Record a Histogram metric.
    void recordHistogramMetricData(const std::string& name,
        double value,
        const std::map<std::string, std::string>& attributes,
        const std::string& unit,
        const std::string& description);

    /// @brief Record a Gauge metric.
    void recordGaugeMetricData(const std::string& name,
        double value,
        const std::map<std::string, std::string>& attributes,
        const std::string& unit,
        const std::string& description);

    //==============================================================================
    // HELPER METHODS
    //==============================================================================

    /// @brief Create a unique key from metric attributes for storage.
    /// @param attributes Key-value attributes.
    /// @return Concatenated attribute key.
    std::string createAttributeKey(const std::map<std::string, std::string>& attributes);

    /// @brief Find the appropriate histogram bucket index for a value.
    /// @param value Value to bucket.
    /// @param boundaries Histogram bucket boundaries.
    /// @return Index of the bucket.
    size_t findBucketIndex(double value, const std::vector<double>& boundaries);

    /// @brief Calculate cumulative counts for histogram buckets.
    /// @param bucket_counts Per-bucket counts.
    /// @return Cumulative counts vector.
    std::vector<uint64_t> calculateCumulativeCounts(const std::vector<uint64_t>& bucket_counts);

    //==============================================================================
    // UTILITY METHODS
    //==============================================================================

    /// @brief Validate a metric submission request.
    /// @param request JSON request object.
    /// @param error_msg Output error message if invalid.
    /// @return true if valid, false otherwise.
    bool validateMetricRequest(const nlohmann::json& request, std::string& error_msg);

    /// @brief Create a JSON error response.
    /// @param error Error message.
    /// @param code HTTP error code (default: 400).
    /// @return JSON error response.
    nlohmann::json createErrorResponse(const std::string& error, int code = 400);

    /// @brief Create a JSON success response.
    /// @param message Success message.
    /// @return JSON success response.
    nlohmann::json createSuccessResponse(const std::string& message);
};