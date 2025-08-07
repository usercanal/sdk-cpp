// src/observability.cpp
// Server-optimized observability implementation for high-performance analytics
// Designed for production server environments with minimal overhead

#include "usercanal/observability.hpp"
#include "usercanal/utils.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <fstream>
#include <iostream>

namespace usercanal {

//=============================================================================
// MetricValue Implementation (Helper class for statistical calculations)
//=============================================================================

class MetricValue {
public:
    MetricValue() = default;
    explicit MetricValue(double value) : value_(value), timestamp_(Utils::now_milliseconds()) {}
    
    double get_value() const { return value_; }
    Timestamp get_timestamp() const { return timestamp_; }
    
    void add_sample(double sample) { samples_.push_back(sample); }
    const std::vector<double>& get_samples() const { return samples_; }
    
    double get_mean() const;
    double get_median() const;
    double get_percentile(double p) const;
    double get_min() const;
    double get_max() const;
    double get_stddev() const;

private:
    double value_ = 0.0;
    Timestamp timestamp_ = 0;
    std::vector<double> samples_;
};

double MetricValue::get_mean() const {
    if (samples_.empty()) return 0.0;
    return std::accumulate(samples_.begin(), samples_.end(), 0.0) / samples_.size();
}

double MetricValue::get_median() const {
    if (samples_.empty()) return 0.0;
    
    auto sorted = samples_;
    std::sort(sorted.begin(), sorted.end());
    
    size_t n = sorted.size();
    if (n % 2 == 0) {
        return (sorted[n/2 - 1] + sorted[n/2]) / 2.0;
    } else {
        return sorted[n/2];
    }
}

double MetricValue::get_percentile(double p) const {
    if (samples_.empty()) return 0.0;
    if (p < 0.0 || p > 1.0) return 0.0;
    
    auto sorted = samples_;
    std::sort(sorted.begin(), sorted.end());
    
    double index = p * (sorted.size() - 1);
    size_t lower = static_cast<size_t>(std::floor(index));
    size_t upper = static_cast<size_t>(std::ceil(index));
    
    if (lower == upper) {
        return sorted[lower];
    }
    
    double weight = index - lower;
    return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
}

double MetricValue::get_min() const {
    if (samples_.empty()) return 0.0;
    return *std::min_element(samples_.begin(), samples_.end());
}

double MetricValue::get_max() const {
    if (samples_.empty()) return 0.0;
    return *std::max_element(samples_.begin(), samples_.end());
}

double MetricValue::get_stddev() const {
    if (samples_.size() < 2) return 0.0;
    
    double mean = get_mean();
    double variance = 0.0;
    
    for (double sample : samples_) {
        double diff = sample - mean;
        variance += diff * diff;
    }
    
    variance /= (samples_.size() - 1);
    return std::sqrt(variance);
}

//=============================================================================
// ServerHealthMonitor Additional Methods
//=============================================================================

std::string health_status_to_string(HealthStatus status) {
    switch (status) {
        case HealthStatus::HEALTHY: return "HEALTHY";
        case HealthStatus::DEGRADED: return "DEGRADED";
        case HealthStatus::UNHEALTHY: return "UNHEALTHY";
        default: return "UNKNOWN";
    }
}

//=============================================================================
// NetworkHealthCheck Implementation
//=============================================================================

class NetworkHealthCheck : public HealthCheck {
public:
    explicit NetworkHealthCheck(const std::string& endpoint) : endpoint_(endpoint) {}
    
    std::string get_name() const override { return "network"; }
    
    HealthReport check() const override {
        HealthReport report;
        
        try {
            // Simple network connectivity check
            // In production, this would actually test network connectivity
            // For now, we'll simulate a basic check
            
            bool connection_ok = true; // Placeholder - would test actual connectivity
            
            if (connection_ok) {
                report.status = HealthStatus::HEALTHY;
                report.message = "Network connectivity OK";
                report.details["endpoint"] = endpoint_;
                report.details["last_check"] = std::to_string(Utils::now_milliseconds());
            } else {
                report.status = HealthStatus::UNHEALTHY;
                report.message = "Network connectivity failed";
                report.details["endpoint"] = endpoint_;
                report.details["error"] = "Connection timeout";
            }
            
        } catch (const std::exception& e) {
            report.status = HealthStatus::UNHEALTHY;
            report.message = "Network check failed: " + std::string(e.what());
            report.details["endpoint"] = endpoint_;
            report.details["exception"] = e.what();
        }
        
        return report;
    }
    
private:
    std::string endpoint_;
};

//=============================================================================
// BatchHealthCheck - Server-specific batch queue monitoring
//=============================================================================

class BatchHealthCheck : public HealthCheck {
public:
    BatchHealthCheck(const std::string& batch_name, 
                     std::function<size_t()> size_getter,
                     std::function<size_t()> pending_getter,
                     size_t warning_threshold,
                     size_t critical_threshold)
        : batch_name_(batch_name)
        , size_getter_(std::move(size_getter))
        , pending_getter_(std::move(pending_getter))
        , warning_threshold_(warning_threshold)
        , critical_threshold_(critical_threshold) {}
    
    std::string get_name() const override { return "batch_" + batch_name_; }
    
    HealthReport check() const override {
        HealthReport report;
        
        try {
            size_t queue_size = size_getter_();
            size_t pending_batches = pending_getter_();
            
            if (queue_size >= critical_threshold_) {
                report.status = HealthStatus::UNHEALTHY;
                report.message = "Batch queue critically full";
            } else if (queue_size >= warning_threshold_) {
                report.status = HealthStatus::DEGRADED;
                report.message = "Batch queue size elevated";
            } else {
                report.status = HealthStatus::HEALTHY;
                report.message = "Batch processing normal";
            }
            
            report.details["queue_size"] = std::to_string(queue_size);
            report.details["pending_batches"] = std::to_string(pending_batches);
            report.details["warning_threshold"] = std::to_string(warning_threshold_);
            report.details["critical_threshold"] = std::to_string(critical_threshold_);
            
        } catch (const std::exception& e) {
            report.status = HealthStatus::UNHEALTHY;
            report.message = "Batch health check failed: " + std::string(e.what());
        }
        
        return report;
    }
    
private:
    std::string batch_name_;
    std::function<size_t()> size_getter_;
    std::function<size_t()> pending_getter_;
    size_t warning_threshold_;
    size_t critical_threshold_;
};

//=============================================================================
// Connection Pool Health Check
//=============================================================================

class ConnectionPoolHealthCheck : public HealthCheck {
public:
    ConnectionPoolHealthCheck(std::function<size_t()> active_getter,
                              std::function<size_t()> idle_getter,
                              size_t max_connections)
        : active_getter_(std::move(active_getter))
        , idle_getter_(std::move(idle_getter))
        , max_connections_(max_connections) {}
    
    std::string get_name() const override { return "connection_pool"; }
    
    HealthReport check() const override {
        HealthReport report;
        
        try {
            size_t active = active_getter_();
            size_t idle = idle_getter_();
            size_t total = active + idle;
            
            double utilization = static_cast<double>(active) / max_connections_;
            
            if (utilization > 0.9) {
                report.status = HealthStatus::UNHEALTHY;
                report.message = "Connection pool critically utilized";
            } else if (utilization > 0.7) {
                report.status = HealthStatus::DEGRADED;
                report.message = "Connection pool highly utilized";
            } else {
                report.status = HealthStatus::HEALTHY;
                report.message = "Connection pool normal";
            }
            
            report.details["active_connections"] = std::to_string(active);
            report.details["idle_connections"] = std::to_string(idle);
            report.details["total_connections"] = std::to_string(total);
            report.details["max_connections"] = std::to_string(max_connections_);
            report.details["utilization"] = std::to_string(utilization * 100.0) + "%";
            
        } catch (const std::exception& e) {
            report.status = HealthStatus::UNHEALTHY;
            report.message = "Connection pool check failed: " + std::string(e.what());
        }
        
        return report;
    }
    
private:
    std::function<size_t()> active_getter_;
    std::function<size_t()> idle_getter_;
    size_t max_connections_;
};

//=============================================================================
// Server-Specific Performance Metrics
//=============================================================================

class ServerMetricsCollector {
public:
    struct ServerMetrics {
        // Throughput metrics
        double events_per_second = 0.0;
        double logs_per_second = 0.0;
        double batches_per_second = 0.0;
        
        // Latency metrics
        double avg_processing_latency_ms = 0.0;
        double p95_processing_latency_ms = 0.0;
        double p99_processing_latency_ms = 0.0;
        
        // Resource metrics
        double cpu_utilization_percent = 0.0;
        double memory_usage_mb = 0.0;
        double network_bytes_per_second = 0.0;
        
        // Error metrics
        double error_rate = 0.0;
        uint64_t total_errors = 0;
        
        // Batch metrics
        double avg_batch_size = 0.0;
        double batch_utilization = 0.0;
        
        Timestamp timestamp = 0;
    };
    
    ServerMetrics collect_metrics() const {
        ServerMetrics metrics;
        metrics.timestamp = Utils::now_milliseconds();
        
        auto& registry = ServerMetricsRegistry::instance();
        
        // Get core counters
        if (auto events = registry.get("sdk.events.total")) {
            // Calculate rate based on time windows (simplified)
            metrics.events_per_second = calculate_rate(events->get_count());
        }
        
        if (auto logs = registry.get("sdk.logs.total")) {
            metrics.logs_per_second = calculate_rate(logs->get_count());
        }
        
        if (auto processing_time = registry.get("sdk.processing.duration_us")) {
            metrics.avg_processing_latency_ms = processing_time->get_average() / 1000.0;
        }
        
        if (auto batch_size = registry.get("sdk.batch.size")) {
            metrics.avg_batch_size = batch_size->get_value();
        }
        
        // System metrics (simplified - would use platform-specific APIs)
        metrics.memory_usage_mb = Utils::SystemMonitor::get_current_memory_usage() / (1024.0 * 1024.0);
        
        return metrics;
    }
    
private:
    double calculate_rate(uint64_t total_count) const {
        // Simplified rate calculation
        // In production, would maintain time windows for accurate rates
        static auto start_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time).count();
        
        if (elapsed > 0) {
            return static_cast<double>(total_count) / elapsed;
        }
        return 0.0;
    }
};

//=============================================================================
// ServerObservabilityManager Additional Methods
//=============================================================================

std::string ServerObservabilityManager::export_json_format() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"timestamp\": " << Utils::now_milliseconds() << ",\n";
    json << "  \"metrics\": {\n";
    
    auto values = metrics().export_values();
    size_t i = 0;
    for (const auto& pair : values) {
        json << "    \"" << pair.first << "\": " << pair.second;
        if (++i < values.size()) {
            json << ",";
        }
        json << "\n";
    }
    
    json << "  },\n";
    json << "  \"health\": {\n";
    
    auto health_report = get_overall_health();
    json << "    \"status\": \"" << health_status_to_string(health_report.status) << "\",\n";
    json << "    \"message\": \"" << health_report.message << "\",\n";
    json << "    \"details\": {\n";
    
    size_t j = 0;
    for (const auto& detail : health_report.details) {
        json << "      \"" << detail.first << "\": \"" << detail.second << "\"";
        if (++j < health_report.details.size()) {
            json << ",";
        }
        json << "\n";
    }
    
    json << "    }\n";
    json << "  },\n";
    json << "  \"performance\": {\n";
    
    auto perf = performance_tracker_.get_snapshot();
    json << "    \"events_per_second\": " << perf.events_per_second << ",\n";
    json << "    \"logs_per_second\": " << perf.logs_per_second << ",\n";
    json << "    \"avg_processing_time_us\": " << perf.avg_processing_time_us << ",\n";
    json << "    \"total_events\": " << perf.total_events << ",\n";
    json << "    \"total_logs\": " << perf.total_logs << ",\n";
    json << "    \"total_errors\": " << perf.total_errors << ",\n";
    json << "    \"error_rate\": " << perf.error_rate << "\n";
    json << "  }\n";
    json << "}\n";
    
    return json.str();
}

void ServerObservabilityManager::add_batch_health_check(
    const std::string& batch_name,
    std::function<size_t()> size_getter,
    std::function<size_t()> pending_getter,
    size_t warning_threshold,
    size_t critical_threshold) {
    
    auto check = std::make_shared<BatchHealthCheck>(
        batch_name, std::move(size_getter), std::move(pending_getter),
        warning_threshold, critical_threshold);
    
    health_monitor_.add_check(check);
}

void ServerObservabilityManager::add_connection_pool_health_check(
    std::function<size_t()> active_getter,
    std::function<size_t()> idle_getter,
    size_t max_connections) {
    
    auto check = std::make_shared<ConnectionPoolHealthCheck>(
        std::move(active_getter), std::move(idle_getter), max_connections);
    
    health_monitor_.add_check(check);
}

bool ServerObservabilityManager::save_metrics_to_file(const std::string& filename) const {
    try {
        std::ofstream file(filename);
        if (!file) return false;
        
        file << export_json_format();
        return true;
    } catch (...) {
        return false;
    }
}

//=============================================================================
// Resource Monitoring Utilities
//=============================================================================

class ResourceMonitor {
public:
    struct ResourceUsage {
        double cpu_percent = 0.0;
        size_t memory_bytes = 0;
        size_t network_bytes_sent = 0;
        size_t network_bytes_received = 0;
        size_t disk_bytes_read = 0;
        size_t disk_bytes_written = 0;
        uint64_t thread_count = 0;
        uint64_t file_descriptors = 0;
    };
    
    static ResourceUsage get_current_usage() {
        ResourceUsage usage;
        
        // Platform-specific resource monitoring would go here
        // For now, we'll provide basic memory monitoring
        usage.memory_bytes = Utils::SystemMonitor::get_current_memory_usage();
        usage.thread_count = std::thread::hardware_concurrency();
        
        return usage;
    }
    
    static void setup_resource_metrics() {
        auto& registry = ServerMetricsRegistry::instance();
        
        // Create system resource metrics
        registry.create_gauge("system.cpu.percent");
        registry.create_gauge("system.memory.bytes");
        registry.create_counter("system.network.bytes_sent");
        registry.create_counter("system.network.bytes_received");
        registry.create_gauge("system.threads.count");
        registry.create_gauge("system.file_descriptors.count");
    }
    
    static void update_resource_metrics() {
        auto usage = get_current_usage();
        auto& registry = ServerMetricsRegistry::instance();
        
        if (auto cpu = registry.get("system.cpu.percent")) {
            cpu->set(usage.cpu_percent);
        }
        
        if (auto memory = registry.get("system.memory.bytes")) {
            memory->set(static_cast<double>(usage.memory_bytes));
        }
        
        if (auto threads = registry.get("system.threads.count")) {
            threads->set(static_cast<double>(usage.thread_count));
        }
        
        if (auto fds = registry.get("system.file_descriptors.count")) {
            fds->set(static_cast<double>(usage.file_descriptors));
        }
    }
};

//=============================================================================
// Alert System (Simplified for Server Environment)
//=============================================================================

class SimpleAlertManager {
public:
    enum class AlertLevel {
        INFO,
        WARNING,
        ERROR,
        CRITICAL
    };
    
    struct Alert {
        std::string id;
        std::string message;
        AlertLevel level;
        Timestamp timestamp;
        std::unordered_map<std::string, std::string> metadata;
    };
    
    using AlertHandler = std::function<void(const Alert&)>;
    
    void add_handler(const std::string& name, AlertHandler handler) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        handlers_[name] = std::move(handler);
    }
    
    void fire_alert(const std::string& message, AlertLevel level, 
                    const std::unordered_map<std::string, std::string>& metadata = {}) {
        Alert alert;
        alert.id = generate_alert_id();
        alert.message = message;
        alert.level = level;
        alert.timestamp = Utils::now_milliseconds();
        alert.metadata = metadata;
        
        // Store alert
        {
            std::lock_guard<std::mutex> lock(alerts_mutex_);
            recent_alerts_.push_back(alert);
            
            // Keep only recent alerts
            if (recent_alerts_.size() > max_recent_alerts_) {
                recent_alerts_.erase(recent_alerts_.begin());
            }
        }
        
        // Notify handlers
        {
            std::lock_guard<std::mutex> lock(handlers_mutex_);
            for (const auto& pair : handlers_) {
                try {
                    pair.second(alert);
                } catch (...) {
                    // Continue with other handlers
                }
            }
        }
    }
    
    std::vector<Alert> get_recent_alerts(size_t limit = 100) const {
        std::lock_guard<std::mutex> lock(alerts_mutex_);
        
        if (limit >= recent_alerts_.size()) {
            return recent_alerts_;
        }
        
        return std::vector<Alert>(
            recent_alerts_.end() - limit,
            recent_alerts_.end()
        );
    }
    
    void add_console_handler() {
        add_handler("console", [](const Alert& alert) {
            std::string level_str;
            switch (alert.level) {
                case AlertLevel::INFO: level_str = "INFO"; break;
                case AlertLevel::WARNING: level_str = "WARNING"; break;
                case AlertLevel::ERROR: level_str = "ERROR"; break;
                case AlertLevel::CRITICAL: level_str = "CRITICAL"; break;
            }
            
            std::cout << "[ALERT " << level_str << "] " << alert.message << std::endl;
        });
    }
    
private:
    std::string generate_alert_id() const {
        return "alert_" + std::to_string(Utils::now_milliseconds()) + 
               "_" + std::to_string(alert_counter_++);
    }
    
    mutable std::mutex handlers_mutex_;
    mutable std::mutex alerts_mutex_;
    
    std::unordered_map<std::string, AlertHandler> handlers_;
    std::vector<Alert> recent_alerts_;
    size_t max_recent_alerts_ = 1000;
    mutable std::atomic<uint64_t> alert_counter_{0};
};

//=============================================================================
// Global Observability Instance
//=============================================================================

// Initialize global alert manager
static SimpleAlertManager g_alert_manager;

SimpleAlertManager& get_alert_manager() {
    return g_alert_manager;
}

} // namespace usercanal