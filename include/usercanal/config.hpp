// include/usercanal/config.hpp
// Purpose: Configuration system for the UserCanal C++ SDK
// Optimized for high-performance logging with sensible defaults

#pragma once

#include "types.hpp"
#include "errors.hpp"
#include <string>
#include <chrono>
#include <memory>
#include <optional>

namespace usercanal {

// Network configuration
struct NetworkConfig {
    std::string endpoint = "collect.usercanal.com:50000";
    std::chrono::milliseconds connect_timeout{5000};    // 5 seconds
    std::chrono::milliseconds send_timeout{10000};      // 10 seconds
    std::chrono::milliseconds receive_timeout{5000};    // 5 seconds
    int max_retries = 3;
    std::chrono::milliseconds retry_delay{1000};        // 1 second base delay
    bool use_exponential_backoff = true;
    double backoff_multiplier = 2.0;
    std::chrono::milliseconds max_retry_delay{30000};   // 30 seconds max
    bool enable_keepalive = true;
    int keepalive_idle_time = 60;    // seconds
    int keepalive_interval = 30;     // seconds
    int keepalive_probes = 3;
    size_t socket_buffer_size = 65536; // 64KB
};

// Batching configuration - optimized for high throughput
struct BatchConfig {
    size_t event_batch_size = 100;                      // Events per batch
    size_t log_batch_size = 50;                         // Logs per batch (typically larger)
    std::chrono::milliseconds event_flush_interval{5000}; // 5 seconds
    std::chrono::milliseconds log_flush_interval{2000};   // 2 seconds (faster for logs)
    size_t max_batch_size_bytes = 1024 * 1024;         // 1MB max batch size
    size_t max_queue_size = 10000;                      // Max items in queue
    bool auto_flush = true;                             // Flush on interval
    bool flush_on_shutdown = true;                      // Ensure data delivery
};

// Performance configuration
struct PerformanceConfig {
    size_t thread_pool_size = 2;                       // Network + processing threads
    size_t max_memory_usage = 50 * 1024 * 1024;       // 50MB max memory
    bool enable_compression = false;                    // Disable by default (CPU vs network tradeoff)
    bool zero_copy_serialization = true;               // Use FlatBuffers zero-copy
    size_t buffer_pool_size = 100;                     // Reusable buffer pool
    bool enable_metrics = false;                       // Disabled by default for performance
};

// Logging configuration (for SDK internal logging, not user logs)
enum class SystemLogLevel : uint8_t {
    NONE = 0,       // No SDK logging
    ERROR = 1,      // Only errors
    WARN = 2,       // Warnings and errors
    INFO = 3,       // Informational + above
    DEBUG = 4,      // Debug + above
    TRACE = 5       // Everything
};

struct LoggingConfig {
    SystemLogLevel level = SystemLogLevel::ERROR;      // Conservative default
    bool log_to_console = true;
    bool log_to_file = false;
    std::string log_file_path;
    size_t max_log_file_size = 10 * 1024 * 1024;     // 10MB
    int max_log_files = 5;
    bool async_logging = true;                         // Don't block main thread
};

// Main configuration class
class Config {
public:
    // Constructor with API key (required)
    explicit Config(const std::string& api_key);

    // Copy constructor and assignment
    Config(const Config& other) = default;
    Config& operator=(const Config& other) = default;

    // Move constructor and assignment
    Config(Config&& other) noexcept = default;
    Config& operator=(Config&& other) noexcept = default;

    // Getters
    const std::string& api_key() const noexcept { return api_key_; }
    const NetworkConfig& network() const noexcept { return network_; }
    const BatchConfig& batch() const noexcept { return batch_; }
    const PerformanceConfig& performance() const noexcept { return performance_; }
    const LoggingConfig& logging() const noexcept { return logging_; }

    // Setters (fluent interface)
    Config& set_endpoint(const std::string& endpoint);
    Config& set_connect_timeout(std::chrono::milliseconds timeout);
    Config& set_send_timeout(std::chrono::milliseconds timeout);
    Config& set_max_retries(int retries);
    Config& set_event_batch_size(size_t size);
    Config& set_log_batch_size(size_t size);
    Config& set_event_flush_interval(std::chrono::milliseconds interval);
    Config& set_log_flush_interval(std::chrono::milliseconds interval);
    Config& set_max_queue_size(size_t size);
    Config& set_thread_pool_size(size_t size);
    Config& set_max_memory_usage(size_t bytes);
    Config& set_system_log_level(SystemLogLevel level);
    Config& set_log_to_file(const std::string& path);
    Config& enable_compression(bool enable = true);
    Config& enable_metrics(bool enable = true);
    Config& enable_keepalive(bool enable = true);

    // Validation
    void validate() const;
    bool is_valid() const noexcept;
    std::vector<std::string> validation_errors() const;

    // Preset configurations
    static Config development(const std::string& api_key);
    static Config production(const std::string& api_key);
    static Config high_throughput(const std::string& api_key);
    static Config low_latency(const std::string& api_key);
    static Config minimal_resource(const std::string& api_key);

    // Friend class to allow ConfigBuilder access to private members
    friend class ConfigBuilder;

private:
    std::string api_key_;
    NetworkConfig network_;
    BatchConfig batch_;
    PerformanceConfig performance_;
    LoggingConfig logging_;

    void validate_api_key() const;
    void validate_network_config() const;
    void validate_batch_config() const;
    void validate_performance_config() const;
};

// Configuration builder for advanced use cases
class ConfigBuilder {
public:
    explicit ConfigBuilder(const std::string& api_key);

    // Network settings
    ConfigBuilder& endpoint(const std::string& endpoint);
    ConfigBuilder& timeouts(std::chrono::milliseconds connect,
                           std::chrono::milliseconds send,
                           std::chrono::milliseconds receive);
    ConfigBuilder& retries(int max_retries, std::chrono::milliseconds base_delay,
                          double backoff_multiplier = 2.0);
    ConfigBuilder& keepalive(bool enable, int idle_time = 60, int interval = 30, int probes = 3);

    // Batching settings
    ConfigBuilder& batching(size_t event_batch_size, size_t log_batch_size,
                           std::chrono::milliseconds event_interval,
                           std::chrono::milliseconds log_interval);
    ConfigBuilder& queue_limits(size_t max_queue_size, size_t max_batch_bytes);

    // Performance settings
    ConfigBuilder& performance(size_t thread_pool_size, size_t max_memory_usage);
    ConfigBuilder& optimizations(bool compression, bool zero_copy, bool metrics);

    // Logging settings
    ConfigBuilder& system_logging(SystemLogLevel level, bool console = true);
    ConfigBuilder& file_logging(const std::string& path, size_t max_size, int max_files);

    // Build final configuration
    Config build() const;

private:
    Config config_;
};

// Environment variable configuration loader
class EnvConfig {
public:
    // Load configuration from environment variables
    // UC_API_KEY, UC_ENDPOINT, UC_BATCH_SIZE, etc.
    static std::optional<Config> from_environment();
    
    // Load specific values from environment
    static std::optional<std::string> get_api_key();
    static std::optional<std::string> get_endpoint();
    static std::optional<size_t> get_batch_size();
    static std::optional<std::chrono::milliseconds> get_flush_interval();
    static std::optional<SystemLogLevel> get_log_level();
    
private:
    static std::optional<std::string> get_env(const std::string& name);
    static std::optional<int> get_env_int(const std::string& name);
    static std::optional<size_t> get_env_size_t(const std::string& name);
    static std::optional<bool> get_env_bool(const std::string& name);
};

// Configuration presets namespace
namespace Presets {

// Development configuration - verbose logging, smaller batches, local endpoint
inline Config development(const std::string& api_key) {
    return ConfigBuilder(api_key)
        .endpoint("localhost:50000")
        .batching(10, 5, std::chrono::milliseconds(1000), std::chrono::milliseconds(500))
        .system_logging(SystemLogLevel::DEBUG)
        .retries(2, std::chrono::milliseconds(500))
        .build();
}

// Production configuration - optimized for reliability and performance
inline Config production(const std::string& api_key) {
    return ConfigBuilder(api_key)
        .endpoint("collect.usercanal.com:50000")
        .batching(100, 50, std::chrono::milliseconds(5000), std::chrono::milliseconds(2000))
        .system_logging(SystemLogLevel::ERROR)
        .retries(3, std::chrono::milliseconds(1000), 2.0)
        .keepalive(true)
        .optimizations(false, true, false)
        .build();
}

// High throughput configuration - maximize data throughput
inline Config high_throughput(const std::string& api_key) {
    return ConfigBuilder(api_key)
        .endpoint("collect.usercanal.com:50000")
        .batching(500, 200, std::chrono::milliseconds(10000), std::chrono::milliseconds(5000))
        .performance(4, 100 * 1024 * 1024) // 4 threads, 100MB memory
        .queue_limits(50000, 5 * 1024 * 1024) // 50k items, 5MB batches
        .system_logging(SystemLogLevel::ERROR)
        .optimizations(true, true, true) // Enable all optimizations
        .build();
}

// Low latency configuration - minimize processing delay
inline Config low_latency(const std::string& api_key) {
    return ConfigBuilder(api_key)
        .endpoint("collect.usercanal.com:50000")
        .batching(1, 1, std::chrono::milliseconds(100), std::chrono::milliseconds(50))
        .timeouts(std::chrono::milliseconds(1000),
                 std::chrono::milliseconds(2000),
                 std::chrono::milliseconds(1000))
        .performance(2, 10 * 1024 * 1024) // Keep memory low
        .system_logging(SystemLogLevel::NONE) // No logging overhead
        .optimizations(false, true, false) // Zero-copy only
        .build();
}

// Minimal resource configuration - for constrained environments
inline Config minimal_resource(const std::string& api_key) {
    return ConfigBuilder(api_key)
        .endpoint("collect.usercanal.com:50000")
        .batching(20, 10, std::chrono::milliseconds(10000), std::chrono::milliseconds(5000))
        .performance(1, 5 * 1024 * 1024) // Single thread, 5MB memory
        .queue_limits(1000, 256 * 1024) // 1k items, 256KB batches
        .system_logging(SystemLogLevel::NONE)
        .optimizations(true, true, false) // Compression to save bandwidth
        .build();
}

} // namespace Presets

} // namespace usercanal