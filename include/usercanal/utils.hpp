// include/usercanal/utils.hpp
// Purpose: Utility functions and helpers for the UserCanal C++ SDK
// Provides common functionality used across the SDK

#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <thread>

namespace usercanal {
namespace Utils {

// String utilities
std::string trim(const std::string& str);
std::string to_lower(const std::string& str);
std::string to_upper(const std::string& str);
std::vector<std::string> split(const std::string& str, char delimiter);
bool starts_with(const std::string& str, const std::string& prefix);
bool ends_with(const std::string& str, const std::string& suffix);

// Network utilities
std::pair<std::string, uint16_t> parse_endpoint(const std::string& endpoint);
bool is_valid_hostname(const std::string& hostname);
bool is_valid_port(uint16_t port);
std::string get_hostname();
std::string get_local_ip();

// Time utilities
Timestamp current_timestamp_ms();
std::string timestamp_to_iso8601(Timestamp timestamp_ms);
std::string current_iso8601_timestamp();

// System utilities
size_t get_available_memory();
size_t get_total_memory();
double get_cpu_usage();
size_t get_thread_count();
std::string get_os_info();
std::string get_arch_info();

// Encoding utilities
std::string base64_encode(const std::vector<uint8_t>& data);
std::vector<uint8_t> base64_decode(const std::string& encoded);
std::string hex_encode(const std::vector<uint8_t>& data);
std::vector<uint8_t> hex_decode(const std::string& hex);

// Hash utilities
std::string sha256_hash(const std::string& data);
std::string md5_hash(const std::string& data);
uint32_t crc32_hash(const std::string& data);

// JSON utilities (if nlohmann/json is available)
#ifdef NLOHMANN_JSON_FOUND
#include <nlohmann/json.hpp>
nlohmann::json properties_to_json(const Properties& props);
Properties json_to_properties(const nlohmann::json& json);
std::string serialize_properties(const Properties& props);
Properties deserialize_properties(const std::string& json_str);
#endif

// File utilities
bool file_exists(const std::string& path);
bool directory_exists(const std::string& path);
bool create_directory(const std::string& path);
size_t get_file_size(const std::string& path);
std::string read_file_contents(const std::string& path);
bool write_file_contents(const std::string& path, const std::string& contents);

// Thread utilities
class ThreadSafeCounter {
public:
    ThreadSafeCounter(uint64_t initial_value = 0);
    
    uint64_t increment();
    uint64_t decrement();
    uint64_t get() const;
    void set(uint64_t value);
    void reset();

private:
    mutable std::mutex mutex_;
    uint64_t value_;
};

// RAII timer for performance measurement
class ScopedTimer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::milliseconds;
    
    explicit ScopedTimer(Duration& output);
    explicit ScopedTimer(std::function<void(Duration)> callback);
    ~ScopedTimer();

    Duration elapsed() const;

private:
    TimePoint start_time_;
    Duration* output_;
    std::function<void(Duration)> callback_;
};

// Memory pool for buffer reuse
template<typename T>
class ObjectPool {
public:
    explicit ObjectPool(size_t initial_size = 10);
    ~ObjectPool();

    std::unique_ptr<T> acquire();
    void release(std::unique_ptr<T> obj);
    size_t available_count() const;
    size_t total_count() const;

private:
    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<T>> available_objects_;
    size_t total_objects_;
    size_t max_objects_;
};

// Rate limiter for controlling request rates
class RateLimiter {
public:
    RateLimiter(double requests_per_second, size_t burst_size = 1);
    
    bool try_acquire(size_t tokens = 1);
    bool wait_for_tokens(size_t tokens = 1, std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
    double get_rate() const;
    void set_rate(double requests_per_second);

private:
    mutable std::mutex mutex_;
    double rate_;
    size_t burst_size_;
    double tokens_;
    std::chrono::steady_clock::time_point last_refill_;
    
    void refill_tokens();
};

// Exponential backoff calculator
class ExponentialBackoff {
public:
    ExponentialBackoff(std::chrono::milliseconds base_delay,
                       double multiplier = 2.0,
                       std::chrono::milliseconds max_delay = std::chrono::milliseconds(30000));
    
    std::chrono::milliseconds next_delay();
    void reset();
    int attempt_count() const;

private:
    std::chrono::milliseconds base_delay_;
    double multiplier_;
    std::chrono::milliseconds max_delay_;
    int attempts_;
    std::chrono::milliseconds current_delay_;
};

// Circuit breaker for fault tolerance
class CircuitBreaker {
public:
    enum class State {
        CLOSED,    // Normal operation
        OPEN,      // Failing fast
        HALF_OPEN  // Testing if service recovered
    };

    CircuitBreaker(int failure_threshold = 5,
                   std::chrono::milliseconds timeout = std::chrono::milliseconds(60000),
                   int success_threshold = 2);
    
    bool is_call_allowed();
    void record_success();
    void record_failure();
    State get_state() const;
    void reset();

private:
    mutable std::mutex mutex_;
    int failure_threshold_;
    int success_threshold_;
    std::chrono::milliseconds timeout_;
    
    State state_;
    int failure_count_;
    int success_count_;
    std::chrono::steady_clock::time_point last_failure_time_;
};

// System resource monitoring utilities
class SystemMonitor {
public:
    // Get current memory usage in bytes
    static size_t get_current_memory_usage();
    
    // Get current CPU usage percentage (0.0 - 1.0)
    static double get_current_cpu_usage();
    
    // Get number of active threads for current process
    static uint64_t get_thread_count();
    
    // Get available system memory in bytes
    static size_t get_available_memory();
    
    // Get total system memory in bytes
    static size_t get_total_memory();
};



// Convenience function for backward compatibility
inline size_t get_current_memory_usage() {
    return SystemMonitor::get_current_memory_usage();
}

} // namespace Utils
} // namespace usercanal