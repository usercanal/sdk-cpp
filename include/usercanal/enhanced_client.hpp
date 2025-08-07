// include/usercanal/enhanced_client.hpp
// Server-optimized enhanced client with advanced pipeline processing and monitoring
// Designed for high-performance production server environments

#pragma once

#include "usercanal/client.hpp"
#include "usercanal/pipeline.hpp"
#include "usercanal/observability.hpp"
#include "usercanal/hooks.hpp"
#include <memory>
#include <functional>
#include <future>
#include <optional>
#include <atomic>
#include <thread>

namespace usercanal {

//=============================================================================
// ENHANCED CLIENT CONFIGURATION
//=============================================================================

struct EnhancedClientConfig {
    Config base_config;
    
    // Constructor
    explicit EnhancedClientConfig(const Config& config) : base_config(config) {}
    
    // Pipeline processing
    bool enable_pipeline_processing = true;
    bool enable_parallel_processing = true;
    uint32_t pipeline_worker_threads = 4;
    std::chrono::milliseconds pipeline_timeout{2000};
    
    // Server observability
    bool enable_metrics = true;
    bool enable_health_monitoring = true;
    bool enable_performance_tracking = true;
    bool enable_resource_monitoring = true;
    std::chrono::seconds health_check_interval{30};
    std::chrono::seconds metrics_export_interval{60};
    
    // Hooks system
    bool enable_hooks = true;
    bool enable_validation_hooks = true;
    bool enable_enrichment_hooks = true;
    bool enable_filtering_hooks = false;
    bool fail_fast_validation = true;
    HookPriority default_hook_priority = HookPriority::NORMAL;
    
    // Server optimization
    bool enable_batch_optimization = true;
    bool enable_connection_pooling = true;
    bool enable_adaptive_batching = true;
    double cpu_usage_threshold = 0.8;
    size_t memory_limit_mb = 1024;
    
    // Performance features
    bool enable_event_sampling = false;
    double event_sample_rate = 1.0;
    bool enable_log_sampling = false;
    double log_sample_rate = 1.0;
    bool enable_rate_limiting = false;
    double max_events_per_second = 10000.0;
    double max_logs_per_second = 5000.0;
    
    // Alerting
    bool enable_alerting = false;
    double error_rate_alert_threshold = 0.05; // 5%
    size_t memory_alert_threshold_mb = 800;
    double cpu_alert_threshold = 0.9; // 90%
    
    // Factory methods
    static EnhancedClientConfig from_base(const Config& config) {
        EnhancedClientConfig enhanced(config);
        return enhanced;
    }
    
    static EnhancedClientConfig server_optimized(const std::string& api_key = "") {
        Config base_config(api_key.empty() ? "default_api_key" : api_key);
        EnhancedClientConfig config(base_config);
        config.enable_parallel_processing = true;
        config.pipeline_worker_threads = std::thread::hardware_concurrency();
        config.enable_batch_optimization = true;
        config.enable_adaptive_batching = true;
        config.enable_connection_pooling = true;
        config.enable_resource_monitoring = true;
        config.cpu_usage_threshold = 0.7;
        config.memory_limit_mb = 2048;
        return config;
    }
    
    static EnhancedClientConfig high_throughput(const std::string& api_key = "") {
        EnhancedClientConfig config = server_optimized(api_key);
        config.max_events_per_second = 50000.0;
        config.max_logs_per_second = 25000.0;
        config.pipeline_worker_threads = std::thread::hardware_concurrency() * 2;
        config.enable_event_sampling = true;
        config.event_sample_rate = 0.1; // Sample 10% for high throughput
        return config;
    }
    
    static EnhancedClientConfig development(const std::string& api_key = "") {
        Config base_config(api_key.empty() ? "dev_api_key" : api_key);
        EnhancedClientConfig config(base_config);
        config.enable_alerting = false;
        config.health_check_interval = std::chrono::seconds(60);
        config.metrics_export_interval = std::chrono::seconds(30);
        config.fail_fast_validation = true;
        return config;
    }
};

//=============================================================================
// ENHANCED CLIENT STATISTICS
//=============================================================================

struct EnhancedClientStats {
    // Processing statistics
    uint64_t total_events_processed = 0;
    uint64_t total_logs_processed = 0;
    uint64_t total_pipeline_executions = 0;
    uint64_t total_hook_executions = 0;
    
    // Performance metrics
    double avg_processing_time_us = 0.0;
    double p95_processing_time_us = 0.0;
    double p99_processing_time_us = 0.0;
    uint64_t total_processing_errors = 0;
    
    // Pipeline metrics
    uint64_t validation_passes = 0;
    uint64_t validation_failures = 0;
    uint64_t enrichment_applications = 0;
    uint64_t filtering_applications = 0;
    
    // Resource usage
    double current_cpu_usage = 0.0;
    size_t current_memory_mb = 0;
    uint64_t active_threads = 0;
    uint64_t total_allocations = 0;
    
    // Network statistics
    uint64_t total_bytes_sent = 0;
    uint64_t total_bytes_received = 0;
    uint64_t active_connections = 0;
    
    Timestamp last_updated = 0;
    
    // Calculate derived metrics
    double get_processing_rate() const {
        return (total_events_processed + total_logs_processed) / 
               std::max(1.0, static_cast<double>(last_updated - 0) / 1000.0);
    }
    
    double get_error_rate() const {
        uint64_t total = total_events_processed + total_logs_processed;
        return total > 0 ? static_cast<double>(total_processing_errors) / total : 0.0;
    }
    
    double get_pipeline_success_rate() const {
        return total_pipeline_executions > 0 ? 
               static_cast<double>(validation_passes) / total_pipeline_executions : 0.0;
    }
};

//=============================================================================
// ENHANCED CLIENT CLASS
//=============================================================================

class EnhancedClient {
public:
    explicit EnhancedClient(const EnhancedClientConfig& config);
    ~EnhancedClient();
    
    // Initialization and lifecycle
    void initialize();
    void shutdown();
    bool is_initialized() const { return initialized_; }
    
    //=========================================================================
    // CORE EVENT AND LOGGING API (Enhanced)
    //=========================================================================
    
    // Enhanced event tracking with pipeline processing
    void track_event(const std::string& user_id, EventType event_type, 
                    const Properties& properties = {});
    
    void track_event_async(const std::string& user_id, EventType event_type, 
                          const Properties& properties = {},
                          std::function<void(bool success)> callback = nullptr);
    
    // Enhanced logging with structured metadata
    void log(LogLevel level, const std::string& message,
            const std::string& service = "", const Properties& data = {},
            const std::optional<ContextId>& context_id = std::nullopt);
    
    void log_async(LogLevel level, const std::string& message,
                  const std::string& service = "", const Properties& data = {},
                  const std::optional<ContextId>& context_id = std::nullopt,
                  std::function<void(bool success)> callback = nullptr);
    
    // Batch operations
    void track_events_batch(const std::vector<EventBatchItem>& events);
    void log_batch(const std::vector<LogBatchItem>& logs);
    
    //=========================================================================
    // PIPELINE AND HOOKS ACCESS
    //=========================================================================
    
    // Access pipeline components
    std::shared_ptr<PipelineProcessor> get_pipeline() const { return pipeline_processor_; }
    std::shared_ptr<HookManager> get_hooks() const { return hook_manager_; }
    
    // Pipeline configuration
    void configure_pipeline(const std::unordered_map<std::string, std::any>& config);
    void enable_pipeline_phase(HookPhase phase, bool enabled = true);
    void set_pipeline_timeout(std::chrono::milliseconds timeout);
    
    // Hook management
    void add_event_hook(std::shared_ptr<EventHook> hook);
    void add_log_hook(std::shared_ptr<LogHook> hook);
    void remove_hook(const std::string& hook_name);
    void clear_hooks();
    
    //=========================================================================
    // OBSERVABILITY AND MONITORING
    //=========================================================================
    
    // Metrics access
    ServerMetricsRegistry& get_metrics() { return ServerObservabilityManager::instance().metrics(); }
    ServerHealthMonitor& get_health() { return ServerObservabilityManager::instance().health(); }
    ServerPerformanceTracker& get_performance() { return ServerObservabilityManager::instance().performance(); }
    
    // Statistics and monitoring
    EnhancedClientStats get_statistics() const;
    std::string export_metrics_json() const;
    std::string export_metrics_prometheus() const;
    HealthReport get_health_report() const;
    
    // Performance monitoring
    void enable_performance_monitoring(bool enabled = true);
    void set_metrics_export_interval(std::chrono::seconds interval);
    void add_custom_health_check(std::shared_ptr<HealthCheck> check);
    
    //=========================================================================
    // SERVER OPTIMIZATION FEATURES
    //=========================================================================
    
    // Resource management
    void set_memory_limit(size_t limit_mb);
    void set_cpu_threshold(double threshold);
    void enable_adaptive_batching(bool enabled = true);
    void enable_connection_pooling(bool enabled = true);
    
    // Performance tuning
    void set_worker_thread_count(uint32_t count);
    void enable_parallel_processing(bool enabled = true);
    void set_processing_timeout(std::chrono::milliseconds timeout);
    
    // Sampling and rate limiting
    void enable_event_sampling(bool enabled, double rate = 1.0);
    void enable_log_sampling(bool enabled, double rate = 1.0);
    void set_rate_limits(double events_per_sec, double logs_per_sec);
    
    //=========================================================================
    // ALERTING AND NOTIFICATIONS
    //=========================================================================
    
    // Alert management
    void enable_alerting(bool enabled = true);
    void add_alert_handler(std::function<void(const std::string& alert)> handler);
    void set_error_rate_alert_threshold(double threshold);
    void set_memory_alert_threshold(size_t threshold_mb);
    void set_cpu_alert_threshold(double threshold);
    
    //=========================================================================
    // ADVANCED CONFIGURATION
    //=========================================================================
    
    // Runtime configuration updates
    void update_config(const EnhancedClientConfig& new_config);
    EnhancedClientConfig get_current_config() const { return config_; }
    
    // Feature toggles
    void enable_feature(const std::string& feature_name, bool enabled = true);
    bool is_feature_enabled(const std::string& feature_name) const;
    
    // Debug and diagnostics
    void enable_debug_mode(bool enabled = true);
    void dump_internal_state(const std::string& filename = "") const;
    std::vector<std::string> get_active_features() const;
    
    //=========================================================================
    // FLUSH AND SYNCHRONIZATION
    //=========================================================================
    
    // Enhanced flush operations
    void flush();
    bool flush_with_timeout(std::chrono::milliseconds timeout);
    std::future<bool> flush_async();
    
    // Graceful shutdown
    void shutdown_gracefully(std::chrono::milliseconds timeout = std::chrono::seconds(10));
    
    //=========================================================================
    // CALLBACK INTERFACES
    //=========================================================================
    
    // Processing callbacks
    void set_event_processed_callback(std::function<void(const EventBatchItem&, bool success)> callback);
    void set_log_processed_callback(std::function<void(const LogBatchItem&, bool success)> callback);
    void set_batch_sent_callback(std::function<void(size_t items, bool success)> callback);
    
    // Error callbacks
    void set_error_callback(std::function<void(const std::exception&)> callback);
    void set_validation_error_callback(std::function<void(const std::string& error)> callback);
    
    // Performance callbacks
    void set_performance_alert_callback(std::function<void(const std::string& metric, double value)> callback);

private:
    //=========================================================================
    // INTERNAL IMPLEMENTATION
    //=========================================================================
    
    // Configuration and state
    EnhancedClientConfig config_;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutting_down_{false};
    
    // Core components
    std::unique_ptr<Client> base_client_;
    std::shared_ptr<PipelineProcessor> pipeline_processor_;
    std::shared_ptr<HookManager> hook_manager_;
    
    // Processing components
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> workers_running_{false};
    
    // Statistics and monitoring
    mutable std::mutex stats_mutex_;
    EnhancedClientStats stats_;
    std::chrono::steady_clock::time_point start_time_;
    
    // Callbacks
    std::function<void(const EventBatchItem&, bool)> event_processed_callback_;
    std::function<void(const LogBatchItem&, bool)> log_processed_callback_;
    std::function<void(size_t, bool)> batch_sent_callback_;
    std::function<void(const std::exception&)> error_callback_;
    std::function<void(const std::string&)> validation_error_callback_;
    std::function<void(const std::string&, double)> performance_alert_callback_;
    
    // Internal methods
    void setup_observability();
    void setup_hooks();
    void setup_pipeline();
    void setup_worker_threads();
    void setup_health_checks();
    void setup_alerts();
    
    void worker_thread_main();
    void update_statistics();
    void check_resource_limits();
    void handle_performance_alert(const std::string& metric, double value);
    
    bool should_sample_event() const;
    bool should_sample_log() const;
    bool check_rate_limits() const;
    
    void process_event_with_pipeline(const std::string& user_id, const std::string& event_name, 
                                   const Properties& properties);
    void process_log_with_pipeline(LogLevel level, const std::string& message,
                                 const std::string& service, const Properties& data,
                                 const std::optional<ContextId>& context_id);
    
    void on_processing_error(const std::exception& error);
    void on_validation_error(const std::string& error);
};

//=============================================================================
// ENHANCED CLIENT FACTORY
//=============================================================================

class EnhancedClientFactory {
public:
    // Create clients with different configurations
    static std::unique_ptr<EnhancedClient> create_server_optimized(const std::string& api_key);
    static std::unique_ptr<EnhancedClient> create_high_throughput(const std::string& api_key);
    static std::unique_ptr<EnhancedClient> create_development(const std::string& api_key);
    static std::unique_ptr<EnhancedClient> create_custom(const EnhancedClientConfig& config);
    
    // Create with automatic configuration detection
    static std::unique_ptr<EnhancedClient> create_auto_configured(const std::string& api_key);
};

//=============================================================================
// SCOPED CLIENT MANAGER (RAII)
//=============================================================================

class ScopedEnhancedClient {
public:
    explicit ScopedEnhancedClient(std::unique_ptr<EnhancedClient> client)
        : client_(std::move(client)) {
        if (client_) {
            client_->initialize();
        }
    }
    
    ~ScopedEnhancedClient() {
        if (client_) {
            client_->shutdown_gracefully();
        }
    }
    
    EnhancedClient& operator*() { return *client_; }
    EnhancedClient* operator->() { return client_.get(); }
    const EnhancedClient& operator*() const { return *client_; }
    const EnhancedClient* operator->() const { return client_.get(); }
    
    EnhancedClient* get() { return client_.get(); }
    const EnhancedClient* get() const { return client_.get(); }
    
private:
    std::unique_ptr<EnhancedClient> client_;
};

//=============================================================================
// CONVENIENCE MACROS FOR SERVER ENVIRONMENTS
//=============================================================================

#define UC_ENHANCED_CLIENT_TRACK(client, user_id, event_type, props) \
    do { \
        try { \
            (client).track_event((user_id), (event_type), (props)); \
        } catch (const std::exception& e) { \
            UC_RECORD_ERROR("event_tracking"); \
        } \
    } while(0)

#define UC_ENHANCED_CLIENT_LOG(client, level, message, service, data) \
    do { \
        try { \
            (client).log((level), (message), (service), (data)); \
        } catch (const std::exception& e) { \
            UC_RECORD_ERROR("logging"); \
        } \
    } while(0)

#define UC_ENHANCED_CLIENT_TRACK_ASYNC(client, user_id, event_type, props, callback) \
    (client).track_event_async((user_id), (event_type), (props), (callback))

#define UC_ENHANCED_CLIENT_SCOPED_TIMER(client, operation) \
    auto timer = (client).get_performance().start_timer()

} // namespace usercanal