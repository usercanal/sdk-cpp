// src/config.cpp
// Implementation of configuration system with validation and presets

#include "usercanal/config.hpp"
#include "usercanal/utils.hpp"
#include <regex>
#include <sstream>
#include <cstdlib>

namespace usercanal {

// Config implementation
Config::Config(const std::string& api_key) : api_key_(api_key) {
    // Set reasonable defaults optimized for C++ backend use
    // Network defaults - optimized for server environments
    network_.endpoint = "collect.usercanal.com:50000";
    network_.connect_timeout = std::chrono::milliseconds(5000);
    network_.send_timeout = std::chrono::milliseconds(10000);
    network_.receive_timeout = std::chrono::milliseconds(5000);
    network_.max_retries = 3;
    network_.retry_delay = std::chrono::milliseconds(1000);
    network_.use_exponential_backoff = true;
    network_.backoff_multiplier = 2.0;
    network_.max_retry_delay = std::chrono::milliseconds(30000);
    network_.enable_keepalive = true;
    network_.keepalive_idle_time = 60;
    network_.keepalive_interval = 30;
    network_.keepalive_probes = 3;
    network_.socket_buffer_size = 65536;

    // Batch defaults - optimized for logging performance
    batch_.event_batch_size = 100;
    batch_.log_batch_size = 50;
    batch_.event_flush_interval = std::chrono::milliseconds(5000);
    batch_.log_flush_interval = std::chrono::milliseconds(2000);
    batch_.max_batch_size_bytes = 1024 * 1024;
    batch_.max_queue_size = 10000;
    batch_.auto_flush = true;
    batch_.flush_on_shutdown = true;

    // Performance defaults - conservative but efficient
    performance_.thread_pool_size = 2;
    performance_.max_memory_usage = 50 * 1024 * 1024;
    performance_.enable_compression = false;
    performance_.zero_copy_serialization = true;
    performance_.buffer_pool_size = 100;
    performance_.enable_metrics = false;

    // Logging defaults - minimal by default for production
    logging_.level = SystemLogLevel::ERROR;
    logging_.log_to_console = true;
    logging_.log_to_file = false;
    logging_.max_log_file_size = 10 * 1024 * 1024;
    logging_.max_log_files = 5;
    logging_.async_logging = true;
}

Config& Config::set_endpoint(const std::string& endpoint) {
    network_.endpoint = endpoint;
    return *this;
}

Config& Config::set_connect_timeout(std::chrono::milliseconds timeout) {
    network_.connect_timeout = timeout;
    return *this;
}

Config& Config::set_send_timeout(std::chrono::milliseconds timeout) {
    network_.send_timeout = timeout;
    return *this;
}

Config& Config::set_max_retries(int retries) {
    network_.max_retries = retries;
    return *this;
}

Config& Config::set_event_batch_size(size_t size) {
    batch_.event_batch_size = size;
    return *this;
}

Config& Config::set_log_batch_size(size_t size) {
    batch_.log_batch_size = size;
    return *this;
}

Config& Config::set_event_flush_interval(std::chrono::milliseconds interval) {
    batch_.event_flush_interval = interval;
    return *this;
}

Config& Config::set_log_flush_interval(std::chrono::milliseconds interval) {
    batch_.log_flush_interval = interval;
    return *this;
}

Config& Config::set_max_queue_size(size_t size) {
    batch_.max_queue_size = size;
    return *this;
}

Config& Config::set_thread_pool_size(size_t size) {
    performance_.thread_pool_size = size;
    return *this;
}

Config& Config::set_max_memory_usage(size_t bytes) {
    performance_.max_memory_usage = bytes;
    return *this;
}

Config& Config::set_system_log_level(SystemLogLevel level) {
    logging_.level = level;
    return *this;
}

Config& Config::set_log_to_file(const std::string& path) {
    logging_.log_to_file = true;
    logging_.log_file_path = path;
    return *this;
}

Config& Config::enable_compression(bool enable) {
    performance_.enable_compression = enable;
    return *this;
}

Config& Config::enable_metrics(bool enable) {
    performance_.enable_metrics = enable;
    return *this;
}

Config& Config::enable_keepalive(bool enable) {
    network_.enable_keepalive = enable;
    return *this;
}

void Config::validate() const {
    std::vector<std::string> errors = validation_errors();
    if (!errors.empty()) {
        std::ostringstream oss;
        oss << "Configuration validation failed:\n";
        for (const auto& error : errors) {
            oss << "  - " << error << "\n";
        }
        throw ConfigError(ErrorCode::INVALID_CONFIG, "config", oss.str());
    }
}

bool Config::is_valid() const noexcept {
    return validation_errors().empty();
}

std::vector<std::string> Config::validation_errors() const {
    std::vector<std::string> errors;

    try {
        validate_api_key();
    } catch (const ConfigError& e) {
        errors.push_back(e.what());
    }

    try {
        validate_network_config();
    } catch (const ConfigError& e) {
        errors.push_back(e.what());
    }

    try {
        validate_batch_config();
    } catch (const ConfigError& e) {
        errors.push_back(e.what());
    }

    try {
        validate_performance_config();
    } catch (const ConfigError& e) {
        errors.push_back(e.what());
    }

    return errors;
}

void Config::validate_api_key() const {
    if (!Utils::is_valid_api_key(api_key_)) {
        throw Errors::invalid_api_key(api_key_);
    }
}

void Config::validate_network_config() const {
    // Validate endpoint format (host:port)
    static const std::regex endpoint_regex(R"(^[a-zA-Z0-9.-]+:\d+$)");
    if (!std::regex_match(network_.endpoint, endpoint_regex)) {
        throw Errors::invalid_endpoint(network_.endpoint);
    }

    // Validate timeouts
    if (network_.connect_timeout.count() < 100 || network_.connect_timeout.count() > 300000) {
        throw ConfigError(ErrorCode::INVALID_CONFIG, "connect_timeout", 
            "Connect timeout must be between 100ms and 300s");
    }
    if (network_.send_timeout.count() < 100 || network_.send_timeout.count() > 300000) {
        throw ConfigError(ErrorCode::INVALID_CONFIG, "send_timeout", 
            "Send timeout must be between 100ms and 300s");
    }

    // Validate retry settings
    if (network_.max_retries < 0 || network_.max_retries > 10) {
        throw ConfigError(ErrorCode::INVALID_CONFIG, "max_retries", 
            "Max retries must be between 0 and 10");
    }
    if (network_.backoff_multiplier < 1.0 || network_.backoff_multiplier > 10.0) {
        throw ConfigError(ErrorCode::INVALID_CONFIG, "backoff_multiplier", 
            "Backoff multiplier must be between 1.0 and 10.0");
    }
}

void Config::validate_batch_config() const {
    // Validate batch sizes
    if (batch_.event_batch_size == 0 || batch_.event_batch_size > 10000) {
        throw Errors::invalid_batch_size(static_cast<int>(batch_.event_batch_size));
    }
    if (batch_.log_batch_size == 0 || batch_.log_batch_size > 10000) {
        throw Errors::invalid_batch_size(static_cast<int>(batch_.log_batch_size));
    }

    // Validate flush intervals
    if (batch_.event_flush_interval.count() < 100 || batch_.event_flush_interval.count() > 300000) {
        throw Errors::invalid_flush_interval(batch_.event_flush_interval);
    }
    if (batch_.log_flush_interval.count() < 50 || batch_.log_flush_interval.count() > 300000) {
        throw Errors::invalid_flush_interval(batch_.log_flush_interval);
    }

    // Validate queue size
    if (batch_.max_queue_size == 0 || batch_.max_queue_size > 1000000) {
        throw ConfigError(ErrorCode::INVALID_CONFIG, "max_queue_size", 
            "Max queue size must be between 1 and 1,000,000");
    }

    // Validate max batch size
    if (batch_.max_batch_size_bytes < 1024 || batch_.max_batch_size_bytes > 100 * 1024 * 1024) {
        throw ConfigError(ErrorCode::INVALID_CONFIG, "max_batch_size_bytes", 
            "Max batch size must be between 1KB and 100MB");
    }
}

void Config::validate_performance_config() const {
    // Validate thread pool size
    if (performance_.thread_pool_size == 0 || performance_.thread_pool_size > 64) {
        throw ConfigError(ErrorCode::INVALID_CONFIG, "thread_pool_size", 
            "Thread pool size must be between 1 and 64");
    }

    // Validate memory usage
    if (performance_.max_memory_usage < 1024 * 1024 || performance_.max_memory_usage > 10ULL * 1024 * 1024 * 1024) {
        throw ConfigError(ErrorCode::INVALID_CONFIG, "max_memory_usage", 
            "Max memory usage must be between 1MB and 10GB");
    }

    // Validate buffer pool size
    if (performance_.buffer_pool_size > 10000) {
        throw ConfigError(ErrorCode::INVALID_CONFIG, "buffer_pool_size", 
            "Buffer pool size must not exceed 10,000");
    }
}

Config Config::development(const std::string& api_key) {
    return Presets::development(api_key);
}

Config Config::production(const std::string& api_key) {
    return Presets::production(api_key);
}

Config Config::high_throughput(const std::string& api_key) {
    return Presets::high_throughput(api_key);
}

Config Config::low_latency(const std::string& api_key) {
    return Presets::low_latency(api_key);
}

Config Config::minimal_resource(const std::string& api_key) {
    return Presets::minimal_resource(api_key);
}

// ConfigBuilder implementation
ConfigBuilder::ConfigBuilder(const std::string& api_key) : config_(api_key) {}

ConfigBuilder& ConfigBuilder::endpoint(const std::string& endpoint) {
    config_.network_.endpoint = endpoint;
    return *this;
}

ConfigBuilder& ConfigBuilder::timeouts(std::chrono::milliseconds connect,
                                      std::chrono::milliseconds send,
                                      std::chrono::milliseconds receive) {
    config_.network_.connect_timeout = connect;
    config_.network_.send_timeout = send;
    config_.network_.receive_timeout = receive;
    return *this;
}

ConfigBuilder& ConfigBuilder::retries(int max_retries, std::chrono::milliseconds base_delay,
                                     double backoff_multiplier) {
    config_.network_.max_retries = max_retries;
    config_.network_.retry_delay = base_delay;
    config_.network_.backoff_multiplier = backoff_multiplier;
    config_.network_.use_exponential_backoff = (backoff_multiplier > 1.0);
    return *this;
}

ConfigBuilder& ConfigBuilder::keepalive(bool enable, int idle_time, int interval, int probes) {
    config_.network_.enable_keepalive = enable;
    config_.network_.keepalive_idle_time = idle_time;
    config_.network_.keepalive_interval = interval;
    config_.network_.keepalive_probes = probes;
    return *this;
}

ConfigBuilder& ConfigBuilder::batching(size_t event_batch_size, size_t log_batch_size,
                                       std::chrono::milliseconds event_interval,
                                       std::chrono::milliseconds log_interval) {
    config_.batch_.event_batch_size = event_batch_size;
    config_.batch_.log_batch_size = log_batch_size;
    config_.batch_.event_flush_interval = event_interval;
    config_.batch_.log_flush_interval = log_interval;
    return *this;
}

ConfigBuilder& ConfigBuilder::queue_limits(size_t max_queue_size, size_t max_batch_bytes) {
    config_.batch_.max_queue_size = max_queue_size;
    config_.batch_.max_batch_size_bytes = max_batch_bytes;
    return *this;
}

ConfigBuilder& ConfigBuilder::performance(size_t thread_pool_size, size_t max_memory_usage) {
    config_.performance_.thread_pool_size = thread_pool_size;
    config_.performance_.max_memory_usage = max_memory_usage;
    return *this;
}

ConfigBuilder& ConfigBuilder::optimizations(bool compression, bool zero_copy, bool metrics) {
    config_.performance_.enable_compression = compression;
    config_.performance_.zero_copy_serialization = zero_copy;
    config_.performance_.enable_metrics = metrics;
    return *this;
}

ConfigBuilder& ConfigBuilder::system_logging(SystemLogLevel level, bool console) {
    config_.logging_.level = level;
    config_.logging_.log_to_console = console;
    return *this;
}

ConfigBuilder& ConfigBuilder::file_logging(const std::string& path, size_t max_size, int max_files) {
    config_.logging_.log_to_file = true;
    config_.logging_.log_file_path = path;
    config_.logging_.max_log_file_size = max_size;
    config_.logging_.max_log_files = max_files;
    return *this;
}

Config ConfigBuilder::build() const {
    Config config = config_;
    config.validate(); // Ensure the built config is valid
    return config;
}

// EnvConfig implementation
std::optional<Config> EnvConfig::from_environment() {
    auto api_key = get_api_key();
    if (!api_key) {
        return std::nullopt;
    }

    ConfigBuilder builder(*api_key);

    if (auto endpoint = get_endpoint()) {
        builder.endpoint(*endpoint);
    }

    if (auto batch_size = get_batch_size()) {
        builder.batching(*batch_size, *batch_size / 2,
                        std::chrono::milliseconds(5000),
                        std::chrono::milliseconds(2000));
    }

    if (auto flush_interval = get_flush_interval()) {
        builder.batching(100, 50, *flush_interval, *flush_interval / 2);
    }

    if (auto log_level = get_log_level()) {
        builder.system_logging(*log_level);
    }

    try {
        return builder.build();
    } catch (const Error&) {
        return std::nullopt;
    }
}

std::optional<std::string> EnvConfig::get_api_key() {
    return get_env("UC_API_KEY");
}

std::optional<std::string> EnvConfig::get_endpoint() {
    return get_env("UC_ENDPOINT");
}

std::optional<size_t> EnvConfig::get_batch_size() {
    return get_env_size_t("UC_BATCH_SIZE");
}

std::optional<std::chrono::milliseconds> EnvConfig::get_flush_interval() {
    if (auto ms = get_env_int("UC_FLUSH_INTERVAL_MS")) {
        return std::chrono::milliseconds(*ms);
    }
    return std::nullopt;
}

std::optional<SystemLogLevel> EnvConfig::get_log_level() {
    if (auto level_str = get_env("UC_LOG_LEVEL")) {
        std::string upper = *level_str;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        
        if (upper == "NONE") return SystemLogLevel::NONE;
        if (upper == "ERROR") return SystemLogLevel::ERROR;
        if (upper == "WARN" || upper == "WARNING") return SystemLogLevel::WARN;
        if (upper == "INFO") return SystemLogLevel::INFO;
        if (upper == "DEBUG") return SystemLogLevel::DEBUG;
        if (upper == "TRACE") return SystemLogLevel::TRACE;
    }
    return std::nullopt;
}

std::optional<std::string> EnvConfig::get_env(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    if (value && strlen(value) > 0) {
        return std::string(value);
    }
    return std::nullopt;
}

std::optional<int> EnvConfig::get_env_int(const std::string& name) {
    if (auto str = get_env(name)) {
        try {
            return std::stoi(*str);
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<size_t> EnvConfig::get_env_size_t(const std::string& name) {
    if (auto str = get_env(name)) {
        try {
            return std::stoull(*str);
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<bool> EnvConfig::get_env_bool(const std::string& name) {
    if (auto str = get_env(name)) {
        std::string lower = *str;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return (lower == "true" || lower == "1" || lower == "yes" || lower == "on");
    }
    return std::nullopt;
}

} // namespace usercanal