#include "IoTMetricsServer.h"
#include <iostream>

int main() {
    std::cout << "Starting OpenTelemetry IoT Metrics Server..." << std::endl;

    try {
        // Create server instance (API on port 8080, Prometheus on port 9090)
        IoTMetricsServer server(8080, 9090);

        std::cout << "Server initialized successfully!" << std::endl;
        std::cout << "Starting server (this will block)..." << std::endl;

        // Start server (this blocks until stopped)
        bool success = server.start();

        if (!success) {
            std::cerr << "Failed to start server!" << std::endl;
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception occurred: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown exception occurred!" << std::endl;
        return 1;
    }

    return 0;
}