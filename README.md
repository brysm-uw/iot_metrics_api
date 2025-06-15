# IoT Metrics API


An OpenTelemetry-compliant metrics API server for IoT and cloud-native environments.  
Collect, aggregate, and expose metrics with Prometheus support.

## Features

- **OpenTelemetry Synchronous Instruments:** Supports Counter, UpDownCounter, Histogram, and Gauge metrics.
- **Prometheus Exporter:** Exposes metrics in Prometheus format at `/metrics` and via standard OpenTelemetry exporter.
- **RESTful API:** Submit metrics via simple HTTP POST requests.
- **Customizable & Thread-Safe:** Designed for high-concurrency IoT and edge workloads.
- **Health & Status Endpoints:** Built-in endpoints for health checks and server status.
---
Quick Start
1.	Build & Run
<pre>git clone https://github.com/yourusername/iot-metrics-api.git
cd iot-metrics-api</pre>
Build with CMake or open in Visual Studio 2022
<pre>cmake -S . -B build
cmake --build build</pre>
Run the server (default: API on 8080, Prometheus on 9090)
<pre>./build/IoTMetricsServer</pre>
3.	Submit a Metric
<pre>curl -X POST http://localhost:8080/api/metrics 
-H "Content-Type: application/json" 
-d '{"metric_name":"http_requests_total","instrument_type":"counter","value":1,"attributes":{"method":"GET","status":"200"}}'</pre>
3.	View Metrics in Prometheus Format
<pre>http://localhost:8080/metrics</pre>
---
## API Endpoints

| Endpoint            | Method | Description             |
|---------------------|--------|-------------------------|
| /api/metrics        | POST   | Submit a metric         |
| /api/metrics/list   | GET    | List all metrics        |
| /metrics            | GET    | Prometheus metrics      |
| /health             | GET    | Health check            |
| /api/status         | GET    | Server status           |
| /metrics/info       | GET    | Metrics endpoint info   |
---
Example Metric Payloads
Counter:
<pre>{
  "metric_name": "http_requests_total",
  "instrument_type": "counter",
  "value": 1,
  "attributes": {"method": "GET", "status": "200"}
}</pre>
UpDownCounter:
<pre>{
  "metric_name": "queue_length",
  "instrument_type": "updowncounter",
  "value": 5,
  "unit": "items",
  "attributes": {"queue": "processing"}
</pre>
Histogram:
<pre>{
  "metric_name": "response_time",
  "instrument_type": "histogram",
  "value": 0.234,
  "unit": "s",
  "attributes": {"endpoint": "/api/data"}
}</pre>
---
## Integration

- **Prometheus:**  
  Add the `/metrics` endpoint to your Prometheus scrape config.

- **Grafana:**  
  Visualize your metrics with Grafana dashboards.

Health & Monitoring

•	Health: ```http://localhost:8080/health```

•	Status: ```http://localhost:8080/api/status```

---
