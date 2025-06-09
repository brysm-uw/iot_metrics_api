# CMakeLists.txt - Fixed runtime library linking
cmake_minimum_required(VERSION 3.15)
project(iot-metrics-api VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/permissive- /bigobj)
    add_compile_options(/wd4996 /wd4267 /wd4244 /wd4251 /wd4275)
endif()

find_package(Threads REQUIRED)
include(FetchContent)

# HTTP and JSON
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.15.3)
FetchContent_MakeAvailable(httplib)

# OpenTelemetry with Prometheus enabled
FetchContent_Declare(opentelemetry-cpp
    GIT_REPOSITORY https://github.com/open-telemetry/opentelemetry-cpp.git
    GIT_TAG v1.16.1)

set(BUILD_TESTING OFF CACHE BOOL "")
set(WITH_EXAMPLES OFF CACHE BOOL "")
set(WITH_PROMETHEUS ON CACHE BOOL "")  # ENABLE PROMETHEUS
set(WITH_OTLP_GRPC OFF CACHE BOOL "")
set(WITH_OTLP_HTTP OFF CACHE BOOL "")
set(WITH_ABSEIL OFF CACHE BOOL "")
set(WITH_STL ON CACHE BOOL "")

FetchContent_MakeAvailable(opentelemetry-cpp)

add_executable(iot-metrics-api
    main.cpp IoTMetricsServer.cpp IoTMetricsServer.h)

# FIXED: Link ALL the required OpenTelemetry libraries
target_link_libraries(iot-metrics-api PRIVATE
    # OpenTelemetry API
    opentelemetry_api
    
    # OpenTelemetry SDK components (REQUIRED for your code)
    opentelemetry_sdk
    opentelemetry_common
    opentelemetry_resources
    opentelemetry_metrics
    opentelemetry_version
    
    # Prometheus exporter (REQUIRED for your code)
    opentelemetry_exporter_prometheus
    
    # HTTP and JSON
    httplib::httplib
    nlohmann_json::nlohmann_json
    
    # System
    Threads::Threads)

target_compile_definitions(iot-metrics-api PRIVATE
    OPENTELEMETRY_SDK_ENABLED
    PROMETHEUS_EXPORTER_ENABLED
    WIN32_LEAN_AND_MEAN
    NOMINMAX)

if(MSVC)
    target_compile_options(iot-metrics-api PRIVATE /W3 /utf-8)
    
    # FIXED: Use dynamic runtime library to match OpenTelemetry
    set_property(TARGET iot-metrics-api PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>")
    
    # Alternative: If you prefer to set this globally for all targets
    # set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>")
endif()

message(STATUS "🎉 IoT Metrics API configured with full OpenTelemetry + Prometheus support")