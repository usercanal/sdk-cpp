// include/usercanal/observability.hpp
// Server-optimized observability system for high-performance analytics
// Designed for production server environments with minimal overhead

#pragma once

#include "usercanal/types.hpp"
#include "usercanal/utils.hpp"
#include <atomic>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <mutex>
#include <functional>
#include <thread>
#include <condition_variable>

namespace usercanal {

//=============================================================================
// SERVER-OPTIMIZED METRICS SYSTEM
//=============================================================================

enum class MetricType {
    COUNTER = 0,        // Monotonically increasing (events, requests)
    GAUGE = 1,         // Current value (memory, connections)
    HISTOGRAM = 2,     // Value distribution (latencies, sizes)
    RATE = 3          // Operations per second
};

// Lockless metric value for high-performance server environments
class ServerMetric {
public:
    explicit ServerMetric(const std::string& name, MetricType type)
        : name_(name), type_(type), value_(0.0), count_(0), sum_(0.0) {}

    // High-performance operations (atomic)
    void increment(double delta = 1.0) {
        // Use mutex for atomic double operations since fetch_add isn't available
        std::lock_guard<std::mutex> lock(mutex_);
        value_ += delta;
        count_++;
    }

    void set(double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        value_ = value;
    }

    void observe(double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        sum_ += value;
        count_++;
    }

    // Fast read operations
    double get_value() const { 
        std::lock_guard<std::mutex> lock(mutex_);
        return value_; 
    }
    uint64_t get_count() const { 
        std::lock_guard<std::mutex> lock(mutex_);
        return count_; 
    }
    double get_average() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_ > 0 ? sum_ / count_ : 0.0;
    }

    const std::string& get_name() const { return name_; }
    MetricType get_type() const { return type_; }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        value_ = 0.0;
        count_ = 0;
        sum_ = 0.0;
    }

private:
    std::string name_;
    MetricType type_;
    mutable std::mutex mutex_;
    double value_;
    uint64_t count_;
    double sum_;
};

// Server metrics registry - optimized for concurrent access
class ServerMetricsRegistry {
public:
    static ServerMetricsRegistry& instance() {
        static ServerMetricsRegistry instance;
        return instance;
    }

    // Create metrics (thread-safe)
    std::shared_ptr<ServerMetric> create_counter(const std::string& name) {
        return create_metric(name, MetricType::COUNTER);
    }

    std::shared_ptr<ServerMetric> create_gauge(const std::string& name) {
        return create_metric(name, MetricType::GAUGE);
    }

    std::shared_ptr<ServerMetric> create_histogram(const std::string& name) {
        return create_metric(name, MetricType::HISTOGRAM);
    }

    std::shared_ptr<ServerMetric> create_rate(const std::string& name) {
        return create_metric(name, MetricType::RATE);
    }

    // Fast metric access
    std::shared_ptr<ServerMetric> get(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = metrics_.find(name);
        return it != metrics_.end() ? it->second : nullptr;
    }

    // Export all metrics efficiently
    std::vector<std::pair<std::string, double>> export_values() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::pair<std::string, double>> values;
        values.reserve(metrics_.size());
        
        for (const auto& pair : metrics_) {
            values.emplace_back(pair.first, pair.second->get_value());
        }
        return values;
    }

private:
    std::shared_ptr<ServerMetric> create_metric(const std::string& name, MetricType type) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = metrics_.find(name);
        if (it != metrics_.end()) {
            return it->second;
        }
        
        auto metric = std::make_shared<ServerMetric>(name, type);
        metrics_[name] = metric;
        return metric;
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<ServerMetric>> metrics_;
};

//=============================================================================
// HIGH-PERFORMANCE TIMER
//=============================================================================

class ServerTimer {
public:
    explicit ServerTimer(std::shared_ptr<ServerMetric> metric)
        : metric_(std::move(metric)), start_(std::chrono::steady_clock::now()) {}

    ~ServerTimer() {
        if (metric_) {
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start_).count();
            metric_->observe(static_cast<double>(duration));
        }
    }

    // Manual finish
    void finish() {
        if (metric_) {
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start_).count();
            metric_->observe(static_cast<double>(duration));
            metric_.reset();
        }
    }

private:
    std::shared_ptr<ServerMetric> metric_;
    std::chrono::steady_clock::time_point start_;
};

//=============================================================================
// SERVER HEALTH MONITORING
//=============================================================================

enum class HealthStatus {
    HEALTHY = 0,
    DEGRADED = 1,
    UNHEALTHY = 2
};

struct HealthReport {
    HealthStatus status;
    std::string message;
    std::unordered_map<std::string, std::string> details;
    Timestamp timestamp;
    
    HealthReport() : status(HealthStatus::HEALTHY), timestamp(Utils::now_milliseconds()) {}
};

// Lightweight health check interface
class HealthCheck {
public:
    virtual ~HealthCheck() = default;
    virtual std::string get_name() const = 0;
    virtual HealthReport check() const = 0;
};

// Server health monitor - optimized for server environments
class ServerHealthMonitor {
public:
    ServerHealthMonitor() : running_(false) {}
    ~ServerHealthMonitor() { stop(); }

    void add_check(std::shared_ptr<HealthCheck> check) {
        std::lock_guard<std::mutex> lock(checks_mutex_);
        checks_.push_back(std::move(check));
    }

    HealthReport get_health() const {
        std::lock_guard<std::mutex> lock(checks_mutex_);
        HealthReport overall;
        overall.status = HealthStatus::HEALTHY;
        overall.message = "All systems operational";
        overall.timestamp = Utils::now_milliseconds();
        
        std::vector<std::string> issues;
        
        for (const auto& check : checks_) {
            try {
                auto report = check->check();
                overall.details[check->get_name()] = 
                    (report.status == HealthStatus::HEALTHY) ? "OK" : report.message;
                
                if (report.status != HealthStatus::HEALTHY) {
                    issues.push_back(check->get_name() + ": " + report.message);
                    if (report.status == HealthStatus::UNHEALTHY) {
                        overall.status = HealthStatus::UNHEALTHY;
                    } else if (overall.status == HealthStatus::HEALTHY) {
                        overall.status = HealthStatus::DEGRADED;
                    }
                }
            } catch (const std::exception& e) {
                issues.push_back(check->get_name() + ": " + e.what());
                overall.status = HealthStatus::UNHEALTHY;
                overall.details[check->get_name()] = "ERROR: " + std::string(e.what());
            }
        }
        
        if (!issues.empty()) {
            overall.message = "Issues detected: ";
            for (size_t i = 0; i < issues.size(); ++i) {
                if (i > 0) overall.message += "; ";
                overall.message += issues[i];
            }
        }
        
        return overall;
    }

    bool is_healthy() const {
        return get_health().status == HealthStatus::HEALTHY;
    }

    void start_monitoring(std::chrono::seconds interval = std::chrono::seconds(30)) {
        if (running_) return;
        
        running_ = true;
        monitor_thread_ = std::thread([this, interval]() {
            while (running_) {
                try {
                    auto health = get_health();
                    last_health_ = health;
                    
                    // Log critical issues
                    if (health.status == HealthStatus::UNHEALTHY) {
                        // Could trigger alerts here in production
                    }
                } catch (...) {
                    // Continue monitoring even if checks fail
                }
                
                std::unique_lock<std::mutex> lock(monitor_mutex_);
                monitor_cv_.wait_for(lock, interval, [this] { return !running_; });
            }
        });
    }

    void stop() {
        if (!running_) return;
        
        running_ = false;
        monitor_cv_.notify_all();
        
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }
    }

private:
    mutable std::mutex checks_mutex_;
    std::vector<std::shared_ptr<HealthCheck>> checks_;
    
    std::atomic<bool> running_;
    std::thread monitor_thread_;
    std::mutex monitor_mutex_;
    std::condition_variable monitor_cv_;
    HealthReport last_health_;
};

//=============================================================================
// BUILT-IN HEALTH CHECKS FOR SERVERS
//=============================================================================

// Memory usage health check
class MemoryHealthCheck : public HealthCheck {
public:
    explicit MemoryHealthCheck(size_t max_mb = 1024) : max_bytes_(max_mb * 1024 * 1024) {}
    
    std::string get_name() const override { return "memory"; }
    
    HealthReport check() const override {
        HealthReport report;
        
        // Simple memory check (platform-specific implementations would be better)
        size_t current_memory = Utils::get_current_memory_usage();
        
        if (current_memory > max_bytes_) {
            report.status = HealthStatus::UNHEALTHY;
            report.message = "Memory usage exceeds limit";
        } else if (current_memory > max_bytes_ * 0.8) {
            report.status = HealthStatus::DEGRADED;
            report.message = "Memory usage high";
        } else {
            report.status = HealthStatus::HEALTHY;
            report.message = "Memory usage normal";
        }
        
        report.details["current_mb"] = std::to_string(current_memory / (1024 * 1024));
        report.details["limit_mb"] = std::to_string(max_bytes_ / (1024 * 1024));
        
        return report;
    }

private:
    size_t max_bytes_;
};

// Queue size health check
class QueueHealthCheck : public HealthCheck {
public:
    QueueHealthCheck(const std::string& name, std::function<size_t()> size_getter, size_t max_size)
        : name_(name), size_getter_(std::move(size_getter)), max_size_(max_size) {}
    
    std::string get_name() const override { return "queue_" + name_; }
    
    HealthReport check() const override {
        HealthReport report;
        
        try {
            size_t current_size = size_getter_();
            
            if (current_size > max_size_) {
                report.status = HealthStatus::UNHEALTHY;
                report.message = "Queue size exceeds limit";
            } else if (current_size > max_size_ * 0.8) {
                report.status = HealthStatus::DEGRADED;
                report.message = "Queue size high";
            } else {
                report.status = HealthStatus::HEALTHY;
                report.message = "Queue size normal";
            }
            
            report.details["current_size"] = std::to_string(current_size);
            report.details["max_size"] = std::to_string(max_size_);
            
        } catch (const std::exception& e) {
            report.status = HealthStatus::UNHEALTHY;
            report.message = "Failed to check queue: " + std::string(e.what());
        }
        
        return report;
    }

private:
    std::string name_;
    std::function<size_t()> size_getter_;
    size_t max_size_;
};

//=============================================================================
// SERVER PERFORMANCE TRACKER
//=============================================================================

class ServerPerformanceTracker {
public:
    struct PerformanceSnapshot {
        double events_per_second = 0.0;
        double logs_per_second = 0.0;
        double avg_batch_size = 0.0;
        double avg_processing_time_us = 0.0;
        uint64_t total_events = 0;
        uint64_t total_logs = 0;
        uint64_t total_errors = 0;
        double error_rate = 0.0;
        Timestamp timestamp = 0;
    };

    ServerPerformanceTracker() {
        auto& registry = ServerMetricsRegistry::instance();
        events_counter_ = registry.create_counter("sdk.events.total");
        logs_counter_ = registry.create_counter("sdk.logs.total");
        errors_counter_ = registry.create_counter("sdk.errors.total");
        processing_timer_ = registry.create_histogram("sdk.processing.duration_us");
        batch_size_gauge_ = registry.create_gauge("sdk.batch.size");
    }

    void record_event() {
        events_counter_->increment();
    }

    void record_log() {
        logs_counter_->increment();
    }

    void record_error() {
        errors_counter_->increment();
    }

    void record_batch_size(size_t size) {
        batch_size_gauge_->set(static_cast<double>(size));
    }

    std::unique_ptr<ServerTimer> start_timer() {
        return std::make_unique<ServerTimer>(processing_timer_);
    }

    PerformanceSnapshot get_snapshot() const {
        PerformanceSnapshot snapshot;
        snapshot.timestamp = Utils::now_milliseconds();
        
        snapshot.total_events = events_counter_->get_count();
        snapshot.total_logs = logs_counter_->get_count();
        snapshot.total_errors = errors_counter_->get_count();
        snapshot.avg_batch_size = batch_size_gauge_->get_value();
        snapshot.avg_processing_time_us = processing_timer_->get_average();
        
        // Calculate rates (simplified - would use time windows in production)
        uint64_t total_operations = snapshot.total_events + snapshot.total_logs;
        if (total_operations > 0) {
            snapshot.error_rate = static_cast<double>(snapshot.total_errors) / total_operations;
        }
        
        return snapshot;
    }

private:
    std::shared_ptr<ServerMetric> events_counter_;
    std::shared_ptr<ServerMetric> logs_counter_;
    std::shared_ptr<ServerMetric> errors_counter_;
    std::shared_ptr<ServerMetric> processing_timer_;
    std::shared_ptr<ServerMetric> batch_size_gauge_;
};

//=============================================================================
// UNIFIED OBSERVABILITY MANAGER
//=============================================================================

class ServerObservabilityManager {
public:
    static ServerObservabilityManager& instance() {
        static ServerObservabilityManager instance;
        return instance;
    }

    void initialize() {
        if (initialized_) return;
        
        // Setup SDK-specific metrics and health checks
        setup_sdk_monitoring();
        
        initialized_ = true;
    }

    void shutdown() {
        if (!initialized_) return;
        
        health_monitor_.stop();
        initialized_ = false;
    }

    // Access components
    ServerMetricsRegistry& metrics() const { return ServerMetricsRegistry::instance(); }
    ServerHealthMonitor& health() { return health_monitor_; }
    ServerPerformanceTracker& performance() { return performance_tracker_; }

    // High-level operations
    void record_sdk_event(const std::string& event_name) {
        performance_tracker_.record_event();
        auto metric = metrics().get("sdk.events." + event_name);
        if (!metric) {
            metric = metrics().create_counter("sdk.events." + event_name);
        }
        metric->increment();
    }

    void record_sdk_log(LogLevel level) {
        performance_tracker_.record_log();
        auto metric = metrics().get("sdk.logs.level_" + std::to_string(static_cast<int>(level)));
        if (!metric) {
            metric = metrics().create_counter("sdk.logs.level_" + std::to_string(static_cast<int>(level)));
        }
        metric->increment();
    }

    void record_sdk_error(const std::string& error_type) {
        performance_tracker_.record_error();
        auto metric = metrics().get("sdk.errors." + error_type);
        if (!metric) {
            metric = metrics().create_counter("sdk.errors." + error_type);
        }
        metric->increment();
    }

    // Export capabilities for monitoring systems
    std::string export_prometheus_format() const {
        std::string output;
        auto values = metrics().export_values();
        
        for (const auto& pair : values) {
            output += pair.first + " " + std::to_string(pair.second) + "\n";
        }
        
        return output;
    }

    std::string export_json_format() const;
    
    void add_batch_health_check(
        const std::string& batch_name,
        std::function<size_t()> size_getter,
        std::function<size_t()> pending_getter,
        size_t warning_threshold,
        size_t critical_threshold);
    
    void add_connection_pool_health_check(
        std::function<size_t()> active_getter,
        std::function<size_t()> idle_getter,
        size_t max_connections);
    
    bool save_metrics_to_file(const std::string& filename) const;

    HealthReport get_overall_health() const {
        return health_monitor_.get_health();
    }

private:
    ServerObservabilityManager() = default;
    
    void setup_sdk_monitoring() {
        // Add memory health check
        health_monitor_.add_check(std::make_shared<MemoryHealthCheck>(512)); // 512MB limit
        
        // Could add more checks here (network, disk, etc.)
        
        // Start health monitoring
        health_monitor_.start_monitoring(std::chrono::seconds(30));
    }

    bool initialized_ = false;
    ServerHealthMonitor health_monitor_;
    ServerPerformanceTracker performance_tracker_;
};

//=============================================================================
// CONVENIENCE MACROS FOR HIGH-PERFORMANCE USAGE
//=============================================================================

#define UC_METRICS() ServerObservabilityManager::instance().metrics()
#define UC_HEALTH() ServerObservabilityManager::instance().health()
#define UC_PERFORMANCE() ServerObservabilityManager::instance().performance()

#define UC_COUNTER(name) UC_METRICS().create_counter(name)
#define UC_GAUGE(name) UC_METRICS().create_gauge(name)
#define UC_HISTOGRAM(name) UC_METRICS().create_histogram(name)

#define UC_TIMER_START(name) auto timer_##name = UC_PERFORMANCE().start_timer()
#define UC_RECORD_EVENT(name) ServerObservabilityManager::instance().record_sdk_event(name)
#define UC_RECORD_LOG(level) ServerObservabilityManager::instance().record_sdk_log(level)
#define UC_RECORD_ERROR(type) ServerObservabilityManager::instance().record_sdk_error(type)

} // namespace usercanal