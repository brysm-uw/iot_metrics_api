#include "IoTMetricsServer.h"
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/data/metric_data.h>
#include <opentelemetry/sdk/metrics/data/point_data.h>
#include <opentelemetry/exporters/prometheus/exporter_factory.h>
#include <opentelemetry/exporters/prometheus/exporter_options.h>
#include <opentelemetry/metrics/provider.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <limits>
#include <sstream>

using json = nlohmann::json;

namespace metrics_api = opentelemetry::metrics;
namespace metrics_sdk = opentelemetry::sdk::metrics;

//==============================================================================
// CONSTRUCTOR & DESTRUCTOR
//==============================================================================

IoTMetricsServer::IoTMetricsServer(int port, int metrics_port)
    : port_(port)
    , metrics_port_(metrics_port)
    , server_running_(false)
{
    http_server_ = std::make_unique<httplib::Server>();
    initializeMetrics();
    setupRoutes();
}

IoTMetricsServer::~IoTMetricsServer() {
    stop();
}

//==============================================================================
// INITIALIZATION METHODS
//==============================================================================

void IoTMetricsServer::initializeMetrics() {
    std::cout << "Initializing Custom OpenTelemetry metrics system..." << std::endl;

    // Note: We're keeping the OpenTelemetry setup for compatibility but using custom export
    // Create Prometheus exporter with options (for potential future use)
    opentelemetry::exporter::metrics::PrometheusExporterOptions options;
    options.url = "<your-server-ip>:" + std::to_string(metrics_port_);

    // Create the Prometheus exporter using factory
    auto prometheus_exporter = opentelemetry::exporter::metrics::PrometheusExporterFactory::Create(options);

    std::cout << "Created Prometheus exporter on port " << metrics_port_ << std::endl;

    // Create MeterProvider
    auto provider = std::shared_ptr<metrics_api::MeterProvider>(
        new metrics_sdk::MeterProvider()
    );

    // Cast to the SDK MeterProvider to add the metric reader
    auto p = std::static_pointer_cast<metrics_sdk::MeterProvider>(provider);
    p->AddMetricReader(std::move(prometheus_exporter));

    std::cout << "MeterProvider created and MetricReader added" << std::endl;

    // Set the global meter provider
    metrics_api::Provider::SetMeterProvider(provider);
    meter_provider_ = provider;

    std::cout << "Global MeterProvider set" << std::endl;
    std::cout << "Custom Prometheus metrics available at: http://<your-server-ip>:" << port_ << "/metrics" << std::endl;
    std::cout << "Standard Prometheus metrics also available at: http://<your-server-ip>:" << metrics_port_ << "/metrics" << std::endl;
    std::cout << "Supported OpenTelemetry instruments: Counter, UpDownCounter, Histogram, Gauge" << std::endl;
}

void IoTMetricsServer::setupRoutes() {
    // CORS headers for web clients
    http_server_->set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        return httplib::Server::HandlerResponse::Unhandled;
        });

    // Handle preflight OPTIONS requests
    http_server_->Options(".*", [](const httplib::Request&, httplib::Response& res) {
        return;
        });

    // OpenTelemetry-compliant metric submission endpoint
    http_server_->Post("/api/metrics", [this](const httplib::Request& req, httplib::Response& res) {
        handleMetric(req, res);
        });

    // Health check endpoint
    http_server_->Get("/health", [this](const httplib::Request& req, httplib::Response& res) {
        handleHealth(req, res);
        });

    // Status endpoint
    http_server_->Get("/api/status", [this](const httplib::Request& req, httplib::Response& res) {
        handleStatus(req, res);
        });

    // List registered metrics
    http_server_->Get("/api/metrics/list", [this](const httplib::Request& req, httplib::Response& res) {
        handleMetricsList(req, res);
        });

    // CUSTOM PROMETHEUS METRICS ENDPOINT
    http_server_->Get("/metrics", [this](const httplib::Request& req, httplib::Response& res) {
        handlePrometheusMetrics(req, res);
        });

    // Metrics endpoint info (for reference)
    http_server_->Get("/metrics/info", [this](const httplib::Request& req, httplib::Response& res) {
        json response;
        response["message"] = "Custom Prometheus metrics available at /metrics";
        response["metrics_url"] = "http://<your-server-ip>:" + std::to_string(port_) + "/metrics";
        response["standard_metrics_url"] = "http://<your-server-ip>:" + std::to_string(metrics_port_) + "/metrics";
        response["opentelemetry_instruments"] = {
            "counter", "updowncounter", "histogram"
        };
        res.set_content(response.dump(2), "application/json");
        });
}

//==============================================================================
// SERVER LIFECYCLE METHODS
//==============================================================================

bool IoTMetricsServer::start() {
    if (server_running_) {
        std::cout << "Server is already running" << std::endl;
        return false;
    }

    std::cout << "Starting OpenTelemetry-compliant IoT Metrics API server..." << std::endl;
    std::cout << "API server: http://<your-server-ip>:" << port_ << std::endl;
    std::cout << "Custom Prometheus metrics: http://<your-server-ip>:" << port_ << "/metrics" << std::endl;
    std::cout << "Standard Prometheus metrics: http://<your-server-ip>:" << metrics_port_ << "/metrics" << std::endl;
    std::cout << "Health check: http://<your-server-ip>:" << port_ << "/health" << std::endl;
    std::cout << "Instruments list: http://<your-server-ip>:" << port_ << "/api/metrics/list" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "OpenTelemetry Synchronous Instruments Examples:" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Counter (monotonic - only increases):" << std::endl;
    std::cout << "curl -X POST http://<your-server-ip>:" << port_ << "/api/metrics \\" << std::endl;
    std::cout << "  -H \"Content-Type: application/json\" \\" << std::endl;
    std::cout << "  -d '{\"metric_name\":\"http_requests_total\",\"instrument_type\":\"counter\",\"value\":1,\"attributes\":{\"method\":\"GET\",\"status\":\"200\"}}'" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "# UpDownCounter (accumulates, can increase/decrease):" << std::endl;
    std::cout << "curl -X POST http://<your-server-ip>:" << port_ << "/api/metrics \\" << std::endl;
    std::cout << "  -H \"Content-Type: application/json\" \\" << std::endl;
    std::cout << "  -d '{\"metric_name\":\"queue_length\",\"instrument_type\":\"updowncounter\",\"value\":5,\"unit\":\"items\",\"attributes\":{\"queue\":\"processing\"}}'" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "# Histogram (value distribution):" << std::endl;
    std::cout << "curl -X POST http://<your-server-ip>:" << port_ << "/api/metrics \\" << std::endl;
    std::cout << "  -H \"Content-Type: application/json\" \\" << std::endl;
    std::cout << "  -d '{\"metric_name\":\"response_time\",\"instrument_type\":\"histogram\",\"value\":0.234,\"unit\":\"s\",\"attributes\":{\"endpoint\":\"/api/data\"}}'" << std::endl;
    std::cout << "" << std::endl;

    server_running_ = true;

    // Start server (this is blocking)
    bool success = http_server_->listen("0.0.0.0", port_);

    server_running_ = false;
    return success;
}

void IoTMetricsServer::stop() {
    if (server_running_) {
        std::cout << "Stopping OpenTelemetry IoT Metrics API server..." << std::endl;
        http_server_->stop();
        server_running_ = false;
    }
}

//==============================================================================
// HTTP ENDPOINT HANDLERS
//==============================================================================

void IoTMetricsServer::handleMetric(const httplib::Request& req, httplib::Response& res) {
    try {
        // Parse JSON body
        json request_data = json::parse(req.body);

        // Validate the request against OpenTelemetry standards
        std::string error_msg;
        if (!validateMetricRequest(request_data, error_msg)) {
            res.status = 400;
            res.set_content(createErrorResponse(error_msg).dump(2), "application/json");
            return;
        }

        // Extract metric data
        std::string metric_name = request_data["metric_name"];
        std::string instrument_type = request_data["instrument_type"];
        double value = request_data["value"];
        std::string unit = request_data.value("unit", "");
        std::string description = request_data.value("description", "");

        // Extract attributes/labels
        std::map<std::string, std::string> attributes;
        if (request_data.contains("attributes")) {
            for (auto& [key, val] : request_data["attributes"].items()) {
                attributes[key] = val.get<std::string>();
            }
        }

        // Record the metric using proper OpenTelemetry instrument
        recordMetric(metric_name, instrument_type, value, attributes, unit, description);

        // Log the measurement
        std::cout << "OpenTelemetry metric recorded: " << metric_name
            << " (" << instrument_type << ") = " << value;
        if (!unit.empty()) std::cout << " " << unit;
        std::cout << std::endl;

        // Create success response
        json response = createSuccessResponse("OpenTelemetry metric recorded successfully");
        response["data"] = {
            {"metric_name", metric_name},
            {"instrument_type", instrument_type},
            {"value", value},
            {"unit", unit},
            {"attributes", attributes},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };

        res.set_content(response.dump(2), "application/json");

    }
    catch (const json::parse_error& e) {
        res.status = 400;
        res.set_content(createErrorResponse("Invalid JSON: " + std::string(e.what())).dump(2), "application/json");
    }
    catch (const json::type_error& e) {
        res.status = 400;
        res.set_content(createErrorResponse("Invalid data type: " + std::string(e.what())).dump(2), "application/json");
    }
    catch (const std::exception& e) {
        res.status = 500;
        res.set_content(createErrorResponse("Internal server error: " + std::string(e.what())).dump(2), "application/json");
    }
}

void IoTMetricsServer::handleHealth(const httplib::Request& req, httplib::Response& res) {
    json response;
    response["status"] = "healthy";
    response["server"] = "OpenTelemetry IoT Metrics API";
    response["version"] = "1.0.0";
    response["opentelemetry"] = "enabled";
    response["supported_instruments"] = { "counter", "updowncounter", "histogram" };
    response["metrics_port"] = metrics_port_;
    response["custom_metrics_endpoint"] = "http://<your-server-ip>:" + std::to_string(port_) + "/metrics";
    response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    res.set_content(response.dump(2), "application/json");
}

void IoTMetricsServer::handleStatus(const httplib::Request& req, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(instruments_mutex_);

    json response;
    response["status"] = "running";
    response["api_port"] = port_;
    response["metrics_port"] = metrics_port_;
    response["custom_metrics_endpoint"] = "http://<your-server-ip>:" + std::to_string(port_) + "/metrics";
    response["standard_metrics_endpoint"] = "http://<your-server-ip>:" + std::to_string(metrics_port_) + "/metrics";
    response["opentelemetry"] = {
        {"meter_provider", "active"},
        {"prometheus_exporter", "active"},
        {"custom_exporter", "active"},
        {"meter_name", "iot_metrics_api"},
        {"meter_version", "1.0.0"},
        {"standard", "OpenTelemetry"}
    };
    response["registered_instruments"] = {
        {"counters", counter_metrics_.size()},
        {"updowncounters", updowncounter_metrics_.size()},
        {"histograms", histogram_metrics_.size()},
        {"gauges", gauge_metrics_.size()}
    };
    response["endpoints"] = {
        {"submit_metric", "POST /api/metrics"},
        {"list_metrics", "GET /api/metrics/list"},
        {"custom_prometheus_metrics", "GET /metrics"},
        {"health", "GET /health"},
        {"status", "GET /api/status"},
        {"metrics_info", "GET /metrics/info"}
    };

    res.set_content(response.dump(2), "application/json");
}

void IoTMetricsServer::handleMetricsList(const httplib::Request& req, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(instruments_mutex_);

    json response;
    response["opentelemetry_standard"] = true;

    json instruments_list;

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Helper lambda to extract value from SumPointData
    auto extract_sum_value = [](const metrics_sdk::MetricData* metric_data) -> double {
        if (metric_data && !metric_data->point_data_attr_.empty()) {
            const auto& point_data_attr = metric_data->point_data_attr_.front();
            const auto* sum_point = std::get_if<metrics_sdk::SumPointData>(&point_data_attr.point_data);
            if (sum_point) {
                double val = 0.0;
                std::visit([&val](const auto& v) { val = static_cast<double>(v); }, sum_point->value_);
                return val;
            }
        }
        return 0.0;
        };

    // Helper lambda for histogram
    auto extract_histogram = [](const metrics_sdk::MetricData* metric_data, json& j) {
        if (metric_data && !metric_data->point_data_attr_.empty()) {
            const auto& point_data_attr = metric_data->point_data_attr_.front();
            const auto* hist_point = std::get_if<metrics_sdk::HistogramPointData>(&point_data_attr.point_data);
            if (hist_point) {
                double sum = 0.0;
                std::visit([&sum](const auto& v) { sum = static_cast<double>(v); }, hist_point->sum_);
                j["value"] = sum;
                j["count"] = hist_point->count_;
            }
        }
        };

    // List Counters
    for (const auto& [name, metric_data_ptr] : counter_metrics_) {
        if (metric_data_ptr) {
            json j = {
                {"instrument_type", "counter"},
                {"description", metric_data_ptr->instrument_descriptor.description_},
                {"unit", metric_data_ptr->instrument_descriptor.unit_},
                {"semantic", "monotonically_increasing"},
                {"timestamp", now},
                {"value", extract_sum_value(metric_data_ptr.get())}
            };
            instruments_list[name] = j;
        }
    }

    // List UpDownCounters
    for (const auto& [name, metric_data_ptr] : updowncounter_metrics_) {
        if (metric_data_ptr) {
            json j = {
                {"instrument_type", "updowncounter"},
                {"description", metric_data_ptr->instrument_descriptor.description_},
                {"unit", metric_data_ptr->instrument_descriptor.unit_},
                {"semantic", "accumulates_can_increase_decrease"},
                {"timestamp", now},
                {"value", extract_sum_value(metric_data_ptr.get())}
            };
            instruments_list[name] = j;
        }
    }

    // List Histograms
    for (const auto& [name, metric_data_ptr] : histogram_metrics_) {
        if (metric_data_ptr) {
            json j = {
                {"instrument_type", "histogram"},
                {"description", metric_data_ptr->instrument_descriptor.description_},
                {"unit", metric_data_ptr->instrument_descriptor.unit_},
                {"semantic", "value_distribution"},
                {"timestamp", now}
            };
            extract_histogram(metric_data_ptr.get(), j);
            instruments_list[name] = j;
        }
    }

    // List Gauges
    for (const auto& [name, metric_data_ptr] : gauge_metrics_) {
        if (metric_data_ptr) {
            json j = {
                {"instrument_type", "gauge"},
                {"description", metric_data_ptr->instrument_descriptor.description_},
                {"unit", metric_data_ptr->instrument_descriptor.unit_},
                {"semantic", "absolute_value"},
                {"timestamp", now},
                {"value", extract_sum_value(metric_data_ptr.get())}
            };
            instruments_list[name] = j;
        }
    }

    response["instruments"] = instruments_list;
    response["total_instruments"] = instruments_list.size();
    res.set_content(response.dump(2), "application/json");
}

//==============================================================================
// CUSTOM PROMETHEUS EXPORT METHODS
//==============================================================================

void IoTMetricsServer::handlePrometheusMetrics(const httplib::Request& req, httplib::Response& res) {
    try {
        std::string prometheus_output = formatPrometheusMetrics();
        res.set_header("Content-Type", "text/plain; version=0.0.4; charset=utf-8");
        res.set_content(prometheus_output, "text/plain");

        std::cout << "Served Custom Prometheus metrics (" << prometheus_output.length() << " bytes)" << std::endl;
    }
    catch (const std::exception& e) {
        res.status = 500;
        res.set_content("Error generating metrics: " + std::string(e.what()), "text/plain");
    }
}

std::string IoTMetricsServer::formatPrometheusMetrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    std::ostringstream output;

    // Add server info as comments
    output << "# OpenTelemetry IoT Metrics API - Custom Export\n";
    output << "# Server: http://<your-server-ip>:" << port_ << "\n";
    output << "# Generated: " << std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() << "\n\n";

    // Export Counters
    for (const auto& [name, metric_data_ptr] : counter_metrics_) {
        if (metric_data_ptr) {
            output << formatCounterForPrometheus(name, *metric_data_ptr);
        }
    }

    // Export UpDownCounters
    for (const auto& [name, metric_data_ptr] : updowncounter_metrics_) {
        if (metric_data_ptr) {
            output << formatUpDownCounterForPrometheus(name, *metric_data_ptr);
        }
    }

    // Export Histograms
    for (const auto& [name, metric_data_ptr] : histogram_metrics_) {
        if (metric_data_ptr) {
            output << formatHistogramForPrometheus(name, *metric_data_ptr);
        }
    }

    // Export Gauges
    for (const auto& [name, metric_data_ptr] : gauge_metrics_) {
        if (metric_data_ptr) {
            output << formatGaugeForPrometheus(name, *metric_data_ptr);
        }
    }
    return output.str();
}

std::string IoTMetricsServer::formatCounterForPrometheus(const std::string& name,
    const metrics_sdk::MetricData& metric_data) {

    std::ostringstream output;
    std::string sanitized_name = sanitizeMetricName(name);

    // HELP comment
    if (!metric_data.instrument_descriptor.description_.empty()) {
        output << "# HELP " << sanitized_name << " " << metric_data.instrument_descriptor.description_ << "\n";
    }

    // TYPE comment
    output << "# TYPE " << sanitized_name << " counter\n";

    // Metric data
    for (const auto& point_data_attr : metric_data.point_data_attr_) {
        const auto* sum_point = std::get_if<metrics_sdk::SumPointData>(&point_data_attr.point_data);
        if (sum_point) {
            std::string attributes_str = formatAttributes(point_data_attr.attributes);

            // Convert value variant to string safely
            std::string value_str;
            std::visit([&value_str](const auto& v) {
                value_str = std::to_string(v);
                }, sum_point->value_);

            output << sanitized_name << attributes_str << " " << value_str << "\n";
        }
    }

    output << "\n";
    return output.str();
}

std::string IoTMetricsServer::formatUpDownCounterForPrometheus(const std::string& name,
    const metrics_sdk::MetricData& metric_data) {

    std::ostringstream output;
    std::string sanitized_name = sanitizeMetricName(name);

    // HELP comment
    if (!metric_data.instrument_descriptor.description_.empty()) {
        output << "# HELP " << sanitized_name << " " << metric_data.instrument_descriptor.description_ << "\n";
    }

    // TYPE comment (UpDownCounter becomes gauge in Prometheus)
    output << "# TYPE " << sanitized_name << " gauge\n";

    // Metric data
    for (const auto& point_data_attr : metric_data.point_data_attr_) {
        const auto* sum_point = std::get_if<metrics_sdk::SumPointData>(&point_data_attr.point_data);
        if (sum_point) {
            std::string attributes_str = formatAttributes(point_data_attr.attributes);

            // Convert value variant to string safely
            std::string value_str;
            std::visit([&value_str](const auto& v) {
                value_str = std::to_string(v);
                }, sum_point->value_);

            output << sanitized_name << attributes_str << " " << value_str << "\n";
        }
    }

    output << "\n";
    return output.str();
}

std::string IoTMetricsServer::formatHistogramForPrometheus(const std::string& name,
    const metrics_sdk::MetricData& metric_data) {

    std::ostringstream output;
    std::string sanitized_name = sanitizeMetricName(name);

    // HELP comment
    if (!metric_data.instrument_descriptor.description_.empty()) {
        output << "# HELP " << sanitized_name << " " << metric_data.instrument_descriptor.description_ << "\n";
    }

    // TYPE comment
    output << "# TYPE " << sanitized_name << " histogram\n";

    // Metric data
    for (const auto& point_data_attr : metric_data.point_data_attr_) {
        const auto* hist_point = std::get_if<metrics_sdk::HistogramPointData>(&point_data_attr.point_data);
        if (hist_point) {
            std::string attributes_str = formatAttributes(point_data_attr.attributes);

            // Histogram buckets
            for (size_t i = 0; i < hist_point->boundaries_.size(); ++i) {
                output << sanitized_name << "_bucket" << "{le=\"" << hist_point->boundaries_[i] << "\"";
                if (!attributes_str.empty()) {
                    // Remove the opening { and add comma
                    output << "," << attributes_str.substr(1);
                }
                else {
                    output << "}";
                }
                output << " " << hist_point->counts_[i] << "\n";
            }

            // +Inf bucket
            output << sanitized_name << "_bucket" << "{le=\"+Inf\"";
            if (!attributes_str.empty()) {
                output << "," << attributes_str.substr(1);
            }
            else {
                output << "}";
            }
            output << " " << hist_point->counts_.back() << "\n";

            // Count and sum - handle variant types
            output << sanitized_name << "_count" << attributes_str << " " << hist_point->count_ << "\n";

            // Convert sum variant to string safely
            std::string sum_str;
            std::visit([&sum_str](const auto& v) {
                sum_str = std::to_string(v);
                }, hist_point->sum_);

            output << sanitized_name << "_sum" << attributes_str << " " << sum_str << "\n";
        }
    }

    output << "\n";
    return output.str();
}
std::string IoTMetricsServer::formatGaugeForPrometheus(const std::string& name,
    const metrics_sdk::MetricData& metric_data) {

    std::ostringstream output;
    std::string sanitized_name = sanitizeMetricName(name);

    // HELP comment
    if (!metric_data.instrument_descriptor.description_.empty()) {
        output << "# HELP " << sanitized_name << " " << metric_data.instrument_descriptor.description_ << "\n";
    }

    // TYPE comment
    output << "# TYPE " << sanitized_name << " gauge\n";

    // Metric data
    for (const auto& point_data_attr : metric_data.point_data_attr_) {
        const auto* sum_point = std::get_if<metrics_sdk::SumPointData>(&point_data_attr.point_data);
        if (sum_point) {
            std::string attributes_str = formatAttributes(point_data_attr.attributes);

            // Convert value variant to string safely
            std::string value_str;
            std::visit([&value_str](const auto& v) {
                value_str = std::to_string(v);
                }, sum_point->value_);

            output << sanitized_name << attributes_str << " " << value_str << "\n";
        }
    }

    output << "\n";
    return output.str();
}
std::string IoTMetricsServer::formatAttributes(const metrics_sdk::PointAttributes& attributes) {
    if (attributes.empty()) {
        return "";
    }

    std::ostringstream output;
    output << "{";

    bool first = true;
    for (const auto& [key, value] : attributes) {
        if (!first) {
            output << ",";
        }

        // Convert value to string (handle each type in the variant explicitly)
        std::string value_str;
        std::visit([&value_str](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>) {
                value_str = v;
            }
            else if constexpr (std::is_same_v<T, const char*>) {
                value_str = std::string(v);
            }
            else if constexpr (std::is_same_v<T, bool>) {
                value_str = v ? "true" : "false";
            }
            else if constexpr (std::is_same_v<T, int32_t>) {
                value_str = std::to_string(v);
            }
            else if constexpr (std::is_same_v<T, int64_t>) {
                value_str = std::to_string(v);
            }
            else if constexpr (std::is_same_v<T, uint32_t>) {
                value_str = std::to_string(v);
            }
            else if constexpr (std::is_same_v<T, uint64_t>) {
                value_str = std::to_string(v);
            }
            else if constexpr (std::is_same_v<T, double>) {
                value_str = std::to_string(v);
            }
            else if constexpr (std::is_same_v<T, float>) {
                value_str = std::to_string(v);
            }
            else {
                // For any unknown types, just use a placeholder
                value_str = "unknown_type";
            }
            }, value);

        output << key << "=\"" << value_str << "\"";
        first = false;
    }

    output << "}";
    return output.str();
}

std::string IoTMetricsServer::sanitizeMetricName(const std::string& name) {
    std::string sanitized = name;

    // Replace invalid characters with underscores
    for (char& c : sanitized) {
        if (!std::isalnum(c) && c != '_' && c != ':') {
            c = '_';
        }
    }

    return sanitized;
}

//==============================================================================
// METRIC RECORDING METHODS - USING UNIQUE_PTR STORAGE
//==============================================================================

void IoTMetricsServer::recordMetric(const std::string& metric_name,
    const std::string& instrument_type,
    double value,
    const std::map<std::string, std::string>& attributes,
    const std::string& unit,
    const std::string& description) {

    // Log what we're recording
    std::cout << "Creating MetricData: " << metric_name << " (" << instrument_type << ") = " << value;
    if (!unit.empty()) std::cout << " " << unit;
    if (!attributes.empty()) {
        std::cout << " with attributes: {";
        bool first = true;
        for (const auto& [key, val] : attributes) {
            if (!first) std::cout << ", ";
            std::cout << key << "=" << val;
            first = false;
        }
        std::cout << "}";
    }
    std::cout << std::endl;

    if (instrument_type == "counter") {
        recordCounterMetricData(metric_name, value, attributes, unit, description);
    }
    else if (instrument_type == "updowncounter") {
        recordUpDownCounterMetricData(metric_name, value, attributes, unit, description);
    }
    else if (instrument_type == "histogram") {
        recordHistogramMetricData(metric_name, value, attributes, unit, description);
    }
    else if (instrument_type == "gauge") {
        recordGaugeMetricData(metric_name, value, attributes, unit, description);
    }
    else {
        throw std::invalid_argument("Unsupported OpenTelemetry instrument type: " + instrument_type);
    }
}

void IoTMetricsServer::recordCounterMetricData(const std::string& name, double value, const std::map<std::string, std::string>& attributes, const std::string& unit, const std::string& description) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    // Create MetricData on the heap to avoid copy issues
    auto metric_data = std::make_unique<metrics_sdk::MetricData>();
    metrics_sdk::InstrumentDescriptor descriptor;

    descriptor.name_ = name;
    descriptor.description_ = description;
    descriptor.unit_ = unit;
    descriptor.type_ = metrics_sdk::InstrumentType::kCounter;
    metric_data->instrument_descriptor = descriptor;

    metrics_sdk::SumPointData point_data;
    point_data.value_ = value;
    point_data.is_monotonic_ = true;

    metrics_sdk::PointAttributes point_attributes;
    for (const auto& [key, val] : attributes) {
        point_attributes[key] = val;
    }

    // Reserve space to avoid reallocation during emplace_back
    metric_data->point_data_attr_.reserve(1);
    metric_data->point_data_attr_.emplace_back();
    auto& point_data_attributes = metric_data->point_data_attr_.back();
    point_data_attributes.attributes = std::move(point_attributes);
    point_data_attributes.point_data = std::move(point_data);

    // Store the unique_ptr - this avoids copying MetricData
    counter_metrics_[name] = std::move(metric_data);

    std::cout << "Counter PointDataAttributes created with " << attributes.size() << " attributes in PointAttributes" << std::endl;
}

void IoTMetricsServer::recordUpDownCounterMetricData(const std::string& name, double value, const std::map<std::string, std::string>& attributes, const std::string& unit, const std::string& description) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    std::string attr_key = createAttributeKey(attributes);
    static std::map<std::string, std::map<std::string, double>> updown_states;
    updown_states[name][attr_key] += value;
    double current_value = updown_states[name][attr_key];

    // Create MetricData on the heap to avoid copy issues
    auto metric_data = std::make_unique<metrics_sdk::MetricData>();
    metrics_sdk::InstrumentDescriptor descriptor;

    descriptor.name_ = name;
    descriptor.description_ = description;
    descriptor.unit_ = unit;
    descriptor.type_ = metrics_sdk::InstrumentType::kUpDownCounter;
    metric_data->instrument_descriptor = descriptor;

    metrics_sdk::SumPointData point_data;
    point_data.value_ = current_value;
    point_data.is_monotonic_ = false;

    metrics_sdk::PointAttributes point_attributes;
    for (const auto& [key, val] : attributes) {
        point_attributes[key] = val;
    }

    // Reserve space to avoid reallocation during emplace_back
    metric_data->point_data_attr_.reserve(1);
    metric_data->point_data_attr_.emplace_back();
    auto& point_data_attributes = metric_data->point_data_attr_.back();
    point_data_attributes.attributes = std::move(point_attributes);
    point_data_attributes.point_data = std::move(point_data);

    // Store the unique_ptr - this avoids copying MetricData
    updowncounter_metrics_[name] = std::move(metric_data);

    std::cout << "UpDownCounter updated: " << name << " += " << value
        << " (current=" << current_value << ")" << std::endl;
}

void IoTMetricsServer::recordHistogramMetricData(const std::string& name, double value, const std::map<std::string, std::string>& attributes, const std::string& unit, const std::string& description) {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    std::string attr_key = createAttributeKey(attributes);

    // Initialize histogram state if it doesn't exist
    if (histogram_states_[name].find(attr_key) == histogram_states_[name].end()) {
        // Use emplace to construct HistogramState with proper boundaries
        histogram_states_[name].emplace(attr_key, HistogramState(default_histogram_boundaries_));
        std::cout << "Created new histogram state for: " << name << " with attributes: " << attr_key << std::endl;
    }

    // Update histogram state
    HistogramState& state = histogram_states_[name][attr_key];
    state.count++;
    state.sum += value;
    state.min = std::min(state.min, value);
    state.max = std::max(state.max, value);

    // Find appropriate bucket and increment count
    size_t bucket_index = findBucketIndex(value, state.boundaries);
    state.bucket_counts[bucket_index]++;

    // Create MetricData on the heap to avoid copy issues
    auto metric_data = std::make_unique<metrics_sdk::MetricData>();
    metrics_sdk::InstrumentDescriptor descriptor;

    descriptor.name_ = name;
    descriptor.description_ = description;
    descriptor.unit_ = unit;
    descriptor.type_ = metrics_sdk::InstrumentType::kHistogram;
    metric_data->instrument_descriptor = descriptor;

    // Create HistogramPointData
    metrics_sdk::HistogramPointData histogram_point;
    histogram_point.count_ = state.count;
    histogram_point.sum_ = state.sum;

    if (state.count > 0) {
        histogram_point.min_ = state.min;
        histogram_point.max_ = state.max;
    }

    // Set bucket counts (cumulative)
    histogram_point.counts_ = calculateCumulativeCounts(state.bucket_counts);
    histogram_point.boundaries_ = state.boundaries;

    metrics_sdk::PointAttributes point_attributes;
    for (const auto& [key, val] : attributes) {
        point_attributes[key] = val;
    }

    // Reserve space to avoid reallocation during emplace_back
    metric_data->point_data_attr_.reserve(1);
    metric_data->point_data_attr_.emplace_back();
    auto& point_data_attributes = metric_data->point_data_attr_.back();
    point_data_attributes.attributes = std::move(point_attributes);
    point_data_attributes.point_data = std::move(histogram_point);

    // Store the unique_ptr - this avoids copying MetricData
    histogram_metrics_[name] = std::move(metric_data);

    std::cout << "Histogram recorded: " << name << " = " << value
        << " (count=" << state.count << ", sum=" << state.sum
        << ", bucket=" << bucket_index << ")" << std::endl;
}

void IoTMetricsServer::recordGaugeMetricData(const std::string& name, double value,
    const std::map<std::string, std::string>& attributes, const std::string& unit,
    const std::string& description) {

    std::lock_guard<std::mutex> lock(metrics_mutex_);

    // Create MetricData on the heap
    auto metric_data = std::make_unique<metrics_sdk::MetricData>();
    metrics_sdk::InstrumentDescriptor descriptor;

    descriptor.name_ = name;
    descriptor.description_ = description;
    descriptor.unit_ = unit;
    descriptor.type_ = metrics_sdk::InstrumentType::kUpDownCounter;
    metric_data->instrument_descriptor = descriptor;

    metrics_sdk::SumPointData point_data;
    point_data.value_ = value;  // Set absolute value (no accumulation)
    point_data.is_monotonic_ = false;

    metrics_sdk::PointAttributes point_attributes;
    for (const auto& [key, val] : attributes) {
        point_attributes[key] = val;
    }

    metric_data->point_data_attr_.reserve(1);
    metric_data->point_data_attr_.emplace_back();
    auto& point_data_attributes = metric_data->point_data_attr_.back();
    point_data_attributes.attributes = std::move(point_attributes);
    point_data_attributes.point_data = std::move(point_data);

    // Store the unique_ptr (overwrites previous value for same name+attributes)
    gauge_metrics_[name] = std::move(metric_data);

    std::cout << "Gauge set: " << name << " = " << value << std::endl;
}

//==============================================================================
// HELPER METHODS
//==============================================================================

std::string IoTMetricsServer::createAttributeKey(const std::map<std::string, std::string>& attributes) {
    if (attributes.empty()) {
        return "__default__";
    }
    std::string key;
    for (const auto& [k, v] : attributes) {
        if (!key.empty()) key += "|";
        key += k + "=" + v;
    }
    return key;
}

size_t IoTMetricsServer::findBucketIndex(double value, const std::vector<double>& boundaries) {
    for (size_t i = 0; i < boundaries.size(); ++i) {
        if (value <= boundaries[i]) {
            return i;
        }
    }
    // If value is larger than all boundaries, it goes in the +Inf bucket
    return boundaries.size();
}

std::vector<uint64_t> IoTMetricsServer::calculateCumulativeCounts(const std::vector<uint64_t>& bucket_counts) {
    std::vector<uint64_t> cumulative_counts;
    cumulative_counts.reserve(bucket_counts.size());

    uint64_t cumulative = 0;
    for (uint64_t count : bucket_counts) {
        cumulative += count;
        cumulative_counts.push_back(cumulative);
    }

    return cumulative_counts;
}

//==============================================================================
// UTILITY METHODS
//==============================================================================

bool IoTMetricsServer::validateMetricRequest(const nlohmann::json& request, std::string& error_msg) {
    // Check required fields
    if (!request.contains("metric_name")) {
        error_msg = "Missing required field: metric_name";
        return false;
    }

    if (!request.contains("instrument_type")) {
        error_msg = "Missing required field: instrument_type";
        return false;
    }

    if (!request.contains("value")) {
        error_msg = "Missing required field: value";
        return false;
    }

    // Validate OpenTelemetry instrument_type
    std::string instrument_type = request["instrument_type"];
    if (instrument_type != "counter" &&
        instrument_type != "updowncounter" &&
        instrument_type != "histogram" &&
        instrument_type != "gauge") {
        error_msg = "instrument_type must be one of the OpenTelemetry synchronous instruments: counter, updowncounter, histogram, gauge";
        return false;
    }

    // Validate value is numeric
    if (!request["value"].is_number()) {
        error_msg = "value must be a number";
        return false;
    }

    double value = request["value"].get<double>();

    // Validate Counter semantic rules (must be non-negative and monotonic)
    if (instrument_type == "counter" && value < 0) {
        error_msg = "Counter values must be non-negative (OpenTelemetry rule)";
        return false;
    }

    // Validate Histogram values (must be finite)
    if (instrument_type == "histogram" && !std::isfinite(value)) {
        error_msg = "Histogram values must be finite (no NaN or infinity)";
        return false;
    }

    return true;
}

nlohmann::json IoTMetricsServer::createErrorResponse(const std::string& error, int code) {
    json response;
    response["success"] = false;
    response["error"] = error;
    response["code"] = code;
    response["opentelemetry_compliant"] = true;
    response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return response;
}

nlohmann::json IoTMetricsServer::createSuccessResponse(const std::string& message) {
    json response;
    response["success"] = true;
    response["message"] = message;
    response["opentelemetry_compliant"] = true;
    response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return response;
}