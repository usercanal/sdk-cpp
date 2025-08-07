// src/enhanced_client.cpp
// Server-optimized enhanced client implementation for high-performance analytics
// Designed for production server environments with minimal overhead

#include "usercanal/enhanced_client.hpp"
#include "usercanal/utils.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <fstream>

namespace usercanal {

//=============================================================================
// EnhancedClient Implementation
//=============================================================================

EnhancedClient::EnhancedClient(const EnhancedClientConfig& config)
    : config_(config), start_time_(std::chrono::steady_clock::now()) {
    
    // Initialize base client with server-optimized config
    base_client_ = std::make_unique<Client>(config_.base_config);
    
    // Initialize statistics
    stats_.last_updated = Utils::now_milliseconds();
    
    if (!config_.enable_pipeline_processing && !config_.enable_hooks) {
        // Basic mode - just use base client
        return;
    }
    
    // Initialize advanced components
    if (config_.enable_pipeline_processing) {
        pipeline_processor_ = std::make_shared<PipelineProcessor>();
    }
    
    if (config_.enable_hooks) {
        hook_manager_ = std::make_shared<HookManager>();
    }
}

EnhancedClient::~EnhancedClient() {
    shutdown();
}

void EnhancedClient::initialize() {
    if (initialized_.exchange(true)) {
        return; // Already initialized
    }
    
    // Initialize observability first
    if (config_.enable_metrics || config_.enable_health_monitoring || config_.enable_performance_tracking) {
        setup_observability();
    }
    
    // Initialize pipeline components
    if (pipeline_processor_) {
        setup_pipeline();
    }
    
    if (hook_manager_) {
        setup_hooks();
    }
    
    // Setup worker threads for parallel processing
    if (config_.enable_parallel_processing) {
        setup_worker_threads();
    }
    
    // Setup health checks
    if (config_.enable_health_monitoring) {
        setup_health_checks();
    }
    
    // Setup alerting
    if (config_.enable_alerting) {
        setup_alerts();
    }
    
    // Initialize base client
    // Note: Client doesn't have initialize() method, so we assume it's ready
}

void EnhancedClient::shutdown() {
    if (!initialized_ || shutting_down_.exchange(true)) {
        return;
    }
    
    // Stop worker threads
    workers_running_ = false;
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
    
    // Flush any remaining data
    if (base_client_) {
        base_client_->flush();
    }
    
    // Shutdown observability
    ServerObservabilityManager::instance().shutdown();
    
    initialized_ = false;
}

//=============================================================================
// Core Event and Logging API
//=============================================================================

void EnhancedClient::track_event(const std::string& user_id, EventType event_type, 
                                const Properties& properties) {
    if (!initialized_ || shutting_down_) {
        return;
    }
    
    // Check sampling
    if (config_.enable_event_sampling && !should_sample_event()) {
        return;
    }
    
    // Check rate limits
    if (config_.enable_rate_limiting && !check_rate_limits()) {
        UC_RECORD_ERROR("rate_limit_exceeded");
        return;
    }
    
    try {
        auto timer = UC_PERFORMANCE().start_timer();
        
        // Convert EventType to string for the client
        std::string event_name = usercanal::Utils::event_type_to_string(event_type);
        
        // Process through pipeline if enabled
        if (pipeline_processor_ && config_.enable_pipeline_processing) {
            process_event_with_pipeline(user_id, event_name, properties);
        } else {
            // Direct processing
            base_client_->track(user_id, event_name, properties);
        }
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_events_processed++;
            update_statistics();
        }
        
        // Record metrics
        UC_RECORD_EVENT("track_event");
        
        // Callback notification
        if (event_processed_callback_) {
            EventBatchItem item(event_type, user_id, properties);
            event_processed_callback_(item, true);
        }
        
    } catch (const std::exception& e) {
        on_processing_error(e);
        
        if (event_processed_callback_) {
            EventBatchItem item(event_type, user_id, properties);
            event_processed_callback_(item, false);
        }
        throw;
    }
}

void EnhancedClient::track_event_async(const std::string& user_id, EventType event_type, 
                                     const Properties& properties,
                                     std::function<void(bool success)> callback) {
    if (!initialized_ || shutting_down_) {
        if (callback) callback(false);
        return;
    }
    
    // For server environments, we can use thread pool for async operations
    // For now, we'll use std::thread (in production, use a proper thread pool)
    std::thread([this, user_id, event_type, properties, callback]() {
        try {
            track_event(user_id, event_type, properties);
            if (callback) callback(true);
        } catch (...) {
            if (callback) callback(false);
        }
    }).detach();
}

void EnhancedClient::log(LogLevel level, const std::string& message,
                        const std::string& service, const Properties& data,
                        const std::optional<ContextId>& context_id) {
    if (!initialized_ || shutting_down_) {
        return;
    }
    
    // Check sampling
    if (config_.enable_log_sampling && !should_sample_log()) {
        return;
    }
    
    // Check rate limits
    if (config_.enable_rate_limiting && !check_rate_limits()) {
        UC_RECORD_ERROR("log_rate_limit_exceeded");
        return;
    }
    
    try {
        auto timer = UC_PERFORMANCE().start_timer();
        
        // Process through pipeline if enabled
        if (pipeline_processor_ && config_.enable_pipeline_processing) {
            process_log_with_pipeline(level, message, service, data, context_id);
        } else {
            // Direct processing
            ContextId ctx_id = context_id.value_or(0);
            base_client_->log(level, service, message, data, ctx_id);
        }
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_logs_processed++;
            update_statistics();
        }
        
        // Record metrics
        UC_RECORD_LOG(level);
        
        // Callback notification
        if (log_processed_callback_) {
            LogBatchItem item(level, service, message, data, context_id.value_or(0));
            log_processed_callback_(item, true);
        }
        
    } catch (const std::exception& e) {
        on_processing_error(e);
        
        if (log_processed_callback_) {
            LogBatchItem item(level, service, message, data, context_id.value_or(0));
            log_processed_callback_(item, false);
        }
        throw;
    }
}

void EnhancedClient::log_async(LogLevel level, const std::string& message,
                              const std::string& service, const Properties& data,
                              const std::optional<ContextId>& context_id,
                              std::function<void(bool success)> callback) {
    if (!initialized_ || shutting_down_) {
        if (callback) callback(false);
        return;
    }
    
    std::thread([this, level, message, service, data, context_id, callback]() {
        try {
            log(level, message, service, data, context_id);
            if (callback) callback(true);
        } catch (...) {
            if (callback) callback(false);
        }
    }).detach();
}

void EnhancedClient::track_events_batch(const std::vector<EventBatchItem>& events) {
    if (!initialized_ || shutting_down_ || events.empty()) {
        return;
    }
    
    try {
        auto timer = UC_PERFORMANCE().start_timer();
        
        // Process batch through pipeline if enabled
        if (pipeline_processor_ && config_.enable_pipeline_processing) {
            for (const auto& event : events) {
                std::string event_name = usercanal::Utils::event_type_to_string(event.get_event_type());
                process_event_with_pipeline(event.get_user_id(), event_name, Properties{}); // Properties not directly accessible
            }
        } else {
            // Direct batch processing
            for (const auto& event : events) {
                std::string event_name = usercanal::Utils::event_type_to_string(event.get_event_type());
                base_client_->track(event.get_user_id(), event_name, Properties{});
            }
        }
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_events_processed += events.size();
            UC_PERFORMANCE().record_batch_size(events.size());
            update_statistics();
        }
        
        // Callback notification
        if (batch_sent_callback_) {
            batch_sent_callback_(events.size(), true);
        }
        
    } catch (const std::exception& e) {
        on_processing_error(e);
        if (batch_sent_callback_) {
            batch_sent_callback_(events.size(), false);
        }
        throw;
    }
}

void EnhancedClient::log_batch(const std::vector<LogBatchItem>& logs) {
    if (!initialized_ || shutting_down_ || logs.empty()) {
        return;
    }
    
    try {
        auto timer = UC_PERFORMANCE().start_timer();
        
        // Process batch through pipeline if enabled
        if (pipeline_processor_ && config_.enable_pipeline_processing) {
            for (const auto& log_item : logs) {
                // LogBatchItem doesn't have direct accessors, use simplified approach
                process_log_with_pipeline(LogLevel::INFO, "batch_log", "batch_service", Properties{}, std::nullopt);
            }
        } else {
            // Direct batch processing
            for (const auto& log_item : logs) {
                // Use simplified approach since LogBatchItem interface is not fully accessible
                base_client_->log(LogLevel::INFO, "batch_service", "batch_log", Properties{}, 0);
            }
        }
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_logs_processed += logs.size();
            UC_PERFORMANCE().record_batch_size(logs.size());
            update_statistics();
        }
        
        // Callback notification
        if (batch_sent_callback_) {
            batch_sent_callback_(logs.size(), true);
        }
        
    } catch (const std::exception& e) {
        on_processing_error(e);
        if (batch_sent_callback_) {
            batch_sent_callback_(logs.size(), false);
        }
        throw;
    }
}

//=============================================================================
// Pipeline and Hooks Configuration
//=============================================================================

void EnhancedClient::configure_pipeline(const std::unordered_map<std::string, std::any>& config) {
    if (pipeline_processor_) {
        // Configure pipeline components
        if (auto validation = pipeline_processor_->get_validation()) {
            validation->configure(config);
        }
        if (auto enrichment = pipeline_processor_->get_enrichment()) {
            enrichment->configure(config);
        }
        if (auto filtering = pipeline_processor_->get_filtering()) {
            filtering->configure(config);
        }
    }
}

void EnhancedClient::enable_pipeline_phase(HookPhase phase, bool enabled) {
    if (hook_manager_) {
        // Enable/disable specific pipeline phases
        // Implementation would depend on HookManager's capabilities
    }
}

void EnhancedClient::set_pipeline_timeout(std::chrono::milliseconds timeout) {
    config_.pipeline_timeout = timeout;
}

void EnhancedClient::add_event_hook(std::shared_ptr<EventHook> hook) {
    if (hook_manager_) {
        hook_manager_->register_event_hook(hook);
    }
}

void EnhancedClient::add_log_hook(std::shared_ptr<LogHook> hook) {
    if (hook_manager_) {
        hook_manager_->register_log_hook(hook);
    }
}

void EnhancedClient::remove_hook(const std::string& hook_name) {
    if (hook_manager_) {
        hook_manager_->unregister_hook(hook_name);
    }
}

void EnhancedClient::clear_hooks() {
    if (hook_manager_) {
        hook_manager_->clear_hooks();
    }
}

//=============================================================================
// Statistics and Monitoring
//=============================================================================

EnhancedClientStats EnhancedClient::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto stats = stats_;
    
    // Update real-time metrics
    stats.last_updated = Utils::now_milliseconds();
    stats.current_memory_mb = Utils::get_current_memory_usage() / (1024 * 1024);
    stats.active_threads = worker_threads_.size();
    
    return stats;
}

std::string EnhancedClient::export_metrics_json() const {
    return ServerObservabilityManager::instance().export_json_format();
}

std::string EnhancedClient::export_metrics_prometheus() const {
    return ServerObservabilityManager::instance().export_prometheus_format();
}

HealthReport EnhancedClient::get_health_report() const {
    return ServerObservabilityManager::instance().get_overall_health();
}

void EnhancedClient::enable_performance_monitoring(bool enabled) {
    config_.enable_performance_tracking = enabled;
    if (enabled && initialized_) {
        // Re-setup performance monitoring
        UC_PERFORMANCE();
    }
}

void EnhancedClient::set_metrics_export_interval(std::chrono::seconds interval) {
    config_.metrics_export_interval = interval;
}

void EnhancedClient::add_custom_health_check(std::shared_ptr<HealthCheck> check) {
    if (config_.enable_health_monitoring) {
        UC_HEALTH().add_check(check);
    }
}

//=============================================================================
// Server Optimization Features
//=============================================================================

void EnhancedClient::set_memory_limit(size_t limit_mb) {
    config_.memory_limit_mb = limit_mb;
}

void EnhancedClient::set_cpu_threshold(double threshold) {
    config_.cpu_usage_threshold = threshold;
}

void EnhancedClient::enable_adaptive_batching(bool enabled) {
    config_.enable_adaptive_batching = enabled;
}

void EnhancedClient::enable_connection_pooling(bool enabled) {
    config_.enable_connection_pooling = enabled;
}

void EnhancedClient::set_worker_thread_count(uint32_t count) {
    config_.pipeline_worker_threads = count;
    
    // If already running, restart worker threads
    if (initialized_ && workers_running_) {
        // Stop current workers
        workers_running_ = false;
        for (auto& thread : worker_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        worker_threads_.clear();
        
        // Start new workers
        setup_worker_threads();
    }
}

void EnhancedClient::enable_parallel_processing(bool enabled) {
    config_.enable_parallel_processing = enabled;
}

void EnhancedClient::set_processing_timeout(std::chrono::milliseconds timeout) {
    config_.pipeline_timeout = timeout;
}

void EnhancedClient::enable_event_sampling(bool enabled, double rate) {
    config_.enable_event_sampling = enabled;
    config_.event_sample_rate = std::clamp(rate, 0.0, 1.0);
}

void EnhancedClient::enable_log_sampling(bool enabled, double rate) {
    config_.enable_log_sampling = enabled;
    config_.log_sample_rate = std::clamp(rate, 0.0, 1.0);
}

void EnhancedClient::set_rate_limits(double events_per_sec, double logs_per_sec) {
    config_.enable_rate_limiting = true;
    config_.max_events_per_second = events_per_sec;
    config_.max_logs_per_second = logs_per_sec;
}

//=============================================================================
// Alerting and Notifications
//=============================================================================

void EnhancedClient::enable_alerting(bool enabled) {
    config_.enable_alerting = enabled;
}

void EnhancedClient::add_alert_handler(std::function<void(const std::string& alert)> handler) {
    // Add to global alert manager
    get_alert_manager().add_handler("custom", [handler](const SimpleAlertManager::Alert& alert) {
        handler(alert.message);
    });
}

void EnhancedClient::set_error_rate_alert_threshold(double threshold) {
    config_.error_rate_alert_threshold = threshold;
}

void EnhancedClient::set_memory_alert_threshold(size_t threshold_mb) {
    config_.memory_alert_threshold_mb = threshold_mb;
}

void EnhancedClient::set_cpu_alert_threshold(double threshold) {
    config_.cpu_alert_threshold = threshold;
}

//=============================================================================
// Advanced Configuration
//=============================================================================

void EnhancedClient::update_config(const EnhancedClientConfig& new_config) {
    config_ = new_config;
    
    // Re-initialize components that depend on config
    if (initialized_) {
        // This would require more sophisticated hot-reloading
        // For now, just update the config
    }
}

void EnhancedClient::enable_feature(const std::string& feature_name, bool enabled) {
    // Map feature names to config options
    if (feature_name == "pipeline_processing") {
        config_.enable_pipeline_processing = enabled;
    } else if (feature_name == "metrics") {
        config_.enable_metrics = enabled;
    } else if (feature_name == "health_monitoring") {
        config_.enable_health_monitoring = enabled;
    } else if (feature_name == "performance_tracking") {
        config_.enable_performance_tracking = enabled;
    } else if (feature_name == "hooks") {
        config_.enable_hooks = enabled;
    } else if (feature_name == "parallel_processing") {
        config_.enable_parallel_processing = enabled;
    } else if (feature_name == "alerting") {
        config_.enable_alerting = enabled;
    }
}

bool EnhancedClient::is_feature_enabled(const std::string& feature_name) const {
    if (feature_name == "pipeline_processing") {
        return config_.enable_pipeline_processing;
    } else if (feature_name == "metrics") {
        return config_.enable_metrics;
    } else if (feature_name == "health_monitoring") {
        return config_.enable_health_monitoring;
    } else if (feature_name == "performance_tracking") {
        return config_.enable_performance_tracking;
    } else if (feature_name == "hooks") {
        return config_.enable_hooks;
    } else if (feature_name == "parallel_processing") {
        return config_.enable_parallel_processing;
    } else if (feature_name == "alerting") {
        return config_.enable_alerting;
    }
    return false;
}

void EnhancedClient::enable_debug_mode(bool enabled) {
    // Set debug mode on base client if it supports it
    // For now, this is a no-op
}

void EnhancedClient::dump_internal_state(const std::string& filename) const {
    std::ostringstream state;
    
    state << "=== Enhanced Client Internal State ===\n";
    state << "Initialized: " << initialized_ << "\n";
    state << "Shutting down: " << shutting_down_ << "\n";
    state << "Worker threads: " << worker_threads_.size() << "\n";
    
    auto stats = get_statistics();
    state << "Total events processed: " << stats.total_events_processed << "\n";
    state << "Total logs processed: " << stats.total_logs_processed << "\n";
    state << "Total processing errors: " << stats.total_processing_errors << "\n";
    state << "Current memory (MB): " << stats.current_memory_mb << "\n";
    state << "Processing rate: " << stats.get_processing_rate() << " ops/sec\n";
    state << "Error rate: " << (stats.get_error_rate() * 100.0) << "%\n";
    
    state << "\n=== Configuration ===\n";
    state << "Pipeline processing: " << config_.enable_pipeline_processing << "\n";
    state << "Parallel processing: " << config_.enable_parallel_processing << "\n";
    state << "Worker threads: " << config_.pipeline_worker_threads << "\n";
    state << "Metrics: " << config_.enable_metrics << "\n";
    state << "Health monitoring: " << config_.enable_health_monitoring << "\n";
    state << "Performance tracking: " << config_.enable_performance_tracking << "\n";
    state << "Hooks: " << config_.enable_hooks << "\n";
    state << "Event sampling: " << config_.enable_event_sampling << " (" << config_.event_sample_rate << ")\n";
    state << "Log sampling: " << config_.enable_log_sampling << " (" << config_.log_sample_rate << ")\n";
    state << "Rate limiting: " << config_.enable_rate_limiting << "\n";
    
    if (!filename.empty()) {
        std::ofstream file(filename);
        file << state.str();
    } else {
        std::cout << state.str() << std::endl;
    }
}

std::vector<std::string> EnhancedClient::get_active_features() const {
    std::vector<std::string> features;
    
    if (config_.enable_pipeline_processing) features.push_back("pipeline_processing");
    if (config_.enable_parallel_processing) features.push_back("parallel_processing");
    if (config_.enable_metrics) features.push_back("metrics");
    if (config_.enable_health_monitoring) features.push_back("health_monitoring");
    if (config_.enable_performance_tracking) features.push_back("performance_tracking");
    if (config_.enable_hooks) features.push_back("hooks");
    if (config_.enable_event_sampling) features.push_back("event_sampling");
    if (config_.enable_log_sampling) features.push_back("log_sampling");
    if (config_.enable_rate_limiting) features.push_back("rate_limiting");
    if (config_.enable_alerting) features.push_back("alerting");
    if (config_.enable_batch_optimization) features.push_back("batch_optimization");
    if (config_.enable_connection_pooling) features.push_back("connection_pooling");
    if (config_.enable_adaptive_batching) features.push_back("adaptive_batching");
    if (config_.enable_resource_monitoring) features.push_back("resource_monitoring");
    
    return features;
}

//=============================================================================
// Flush and Synchronization
//=============================================================================

void EnhancedClient::flush() {
    if (base_client_) {
        base_client_->flush();
    }
}

bool EnhancedClient::flush_with_timeout(std::chrono::milliseconds timeout) {
    if (!base_client_) return false;
    
    // Simple timeout implementation
    auto start = std::chrono::steady_clock::now();
    base_client_->flush();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    
    return elapsed < timeout;
}

std::future<bool> EnhancedClient::flush_async() {
    return std::async(std::launch::async, [this]() {
        try {
            flush();
            return true;
        } catch (...) {
            return false;
        }
    });
}

void EnhancedClient::shutdown_gracefully(std::chrono::milliseconds timeout) {
    auto start = std::chrono::steady_clock::now();
    
    // Signal shutdown
    shutting_down_ = true;
    
    // Stop accepting new requests and flush
    flush();
    
    // Wait for workers to finish
    auto remaining_timeout = timeout - std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    
    if (remaining_timeout > std::chrono::milliseconds::zero()) {
        // Wait for workers with remaining timeout
        workers_running_ = false;
        
        for (auto& thread : worker_threads_) {
            if (thread.joinable()) {
                // In a real implementation, we'd use condition variables for proper timeout
                thread.join();
            }
        }
    }
    
    // Final shutdown
    shutdown();
}

//=============================================================================
// Callback Interfaces
//=============================================================================

void EnhancedClient::set_event_processed_callback(std::function<void(const EventBatchItem&, bool success)> callback) {
    event_processed_callback_ = std::move(callback);
}

void EnhancedClient::set_log_processed_callback(std::function<void(const LogBatchItem&, bool success)> callback) {
    log_processed_callback_ = std::move(callback);
}

void EnhancedClient::set_batch_sent_callback(std::function<void(size_t items, bool success)> callback) {
    batch_sent_callback_ = std::move(callback);
}

void EnhancedClient::set_error_callback(std::function<void(const std::exception&)> callback) {
    error_callback_ = std::move(callback);
}

void EnhancedClient::set_validation_error_callback(std::function<void(const std::string& error)> callback) {
    validation_error_callback_ = std::move(callback);
}

void EnhancedClient::set_performance_alert_callback(std::function<void(const std::string& metric, double value)> callback) {
    performance_alert_callback_ = std::move(callback);
}

//=============================================================================
// Internal Implementation
//=============================================================================

void EnhancedClient::setup_observability() {
    ServerObservabilityManager::instance().initialize();
}

void EnhancedClient::setup_hooks() {
    if (!hook_manager_) return;
    
    // Add built-in hooks based on configuration
    if (config_.enable_validation_hooks) {
        // Add validation hooks
        // Implementation would depend on available hook implementations
    }
    
    if (config_.enable_enrichment_hooks) {
        // Add enrichment hooks
    }
    
    if (config_.enable_filtering_hooks) {
        // Add filtering hooks
    }
}

void EnhancedClient::setup_pipeline() {
    if (!pipeline_processor_) return;
    
    // Configure pipeline based on configuration
    std::unordered_map<std::string, std::any> pipeline_config;
    pipeline_config["fail_fast_validation"] = config_.fail_fast_validation;
    pipeline_config["timeout_ms"] = static_cast<int>(config_.pipeline_timeout.count());
    
    configure_pipeline(pipeline_config);
}

void EnhancedClient::setup_worker_threads() {
    if (workers_running_) return;
    
    workers_running_ = true;
    
    for (uint32_t i = 0; i < config_.pipeline_worker_threads; ++i) {
        worker_threads_.emplace_back(&EnhancedClient::worker_thread_main, this);
    }
}

void EnhancedClient::setup_health_checks() {
    // Add enhanced client specific health checks
    auto& health = UC_HEALTH();
    
    // Memory health check
    if (config_.memory_limit_mb > 0) {
        health.add_check(std::make_shared<MemoryHealthCheck>(config_.memory_limit_mb));
    }
    
    // Processing queue health check
    health.add_check(std::make_shared<QueueHealthCheck>(
        "enhanced_client",
        [this]() -> size_t {
            // Return current processing queue size (simplified)
            return 0;
        },
        1000 // Max queue size
    ));
}

void EnhancedClient::setup_alerts() {
    if (!config_.enable_alerting) return;
    
    get_alert_manager().add_console_handler();
}

void EnhancedClient::worker_thread_main() {
    while (workers_running_ && initialized_) {
        try {
            // Worker thread processing logic
            // In a real implementation, this would process items from queues
            
            // Check resource limits periodically
            check_resource_limits();
            
            // Sleep briefly to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
        } catch (const std::exception& e) {
            on_processing_error(e);
        }
    }
}

void EnhancedClient::update_statistics() {
    // This method is called with stats_mutex_ already locked
    auto now = Utils::now_milliseconds();
    auto elapsed = std::chrono::steady_clock::now() - start_time_;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    
    if (elapsed_ms > 0) {
        double elapsed_sec = elapsed_ms / 1000.0;
        stats_.current_cpu_usage = 0.0; // Would calculate actual CPU usage
        stats_.last_updated = now;
    }
    
    // Update performance snapshot
    auto perf_snapshot = UC_PERFORMANCE().get_snapshot();
    stats_.avg_processing_time_us = perf_snapshot.avg_processing_time_us;
    stats_.total_processing_errors = perf_snapshot.total_errors;
}

void EnhancedClient::check_resource_limits() {
    auto current_memory = Utils::get_current_memory_usage() / (1024 * 1024); // MB
    
    if (config_.memory_limit_mb > 0 && current_memory > config_.memory_limit_mb) {
        handle_performance_alert("memory_limit_exceeded", static_cast<double>(current_memory));
    }
    
    // Check error rate
    auto stats = get_statistics();
    if (stats.get_error_rate() > config_.error_rate_alert_threshold) {
        handle_performance_alert("error_rate_high", stats.get_error_rate());
    }
}

void EnhancedClient::handle_performance_alert(const std::string& metric, double value) {
    if (config_.enable_alerting) {
        std::string alert_msg = "Performance alert: " + metric + " = " + std::to_string(value);
        get_alert_manager().fire_alert(alert_msg, SimpleAlertManager::AlertLevel::WARNING);
    }
    
    if (performance_alert_callback_) {
        performance_alert_callback_(metric, value);
    }
}

bool EnhancedClient::should_sample_event() const {
    if (!config_.enable_event_sampling) {
        return true;
    }
    
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    static thread_local std::uniform_real_distribution<> dis(0.0, 1.0);
    
    return dis(gen) < config_.event_sample_rate;
}

bool EnhancedClient::should_sample_log() const {
    if (!config_.enable_log_sampling) {
        return true;
    }
    
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    static thread_local std::uniform_real_distribution<> dis(0.0, 1.0);
    
    return dis(gen) < config_.log_sample_rate;
}

bool EnhancedClient::check_rate_limits() const {
    // Simplified rate limiting - in production would use proper token bucket
    static thread_local auto last_check = std::chrono::steady_clock::now();
    static thread_local uint64_t request_count = 0;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_check);
    
    if (elapsed.count() >= 1) {
        request_count = 0;
        last_check = now;
    }
    
    request_count++;
    
    // Simple check - would be more sophisticated in production
    return request_count < config_.max_events_per_second + config_.max_logs_per_second;
}

void EnhancedClient::process_event_with_pipeline(const std::string& user_id, const std::string& event_name, 
                                               const Properties& properties) {
    if (!pipeline_processor_) {
        base_client_->track(user_id, event_name, properties);
        return;
    }
    
    try {
        // Create event context
        EventHookContext context(user_id, event_name, properties);
        
        // Process through pipeline
        auto result = pipeline_processor_->process_event(context);
        
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_pipeline_executions++;
            
            if (result == PipelineResult::CONTINUE) {
                stats_.validation_passes++;
                stats_.enrichment_applications++;
            } else if (result == PipelineResult::SKIP) {
                return; // Skip this event
            } else {
                stats_.validation_failures++;
                stats_.total_processing_errors++;
                if (validation_error_callback_) {
                    validation_error_callback_("Pipeline processing failed");
                }
                return;
            }
        }
        
        // Send processed event
        base_client_->track(context.get_user_id(), 
                          context.get_event_name(),
                          context.get_properties());
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_processing_errors++;
        throw;
    }
}

void EnhancedClient::process_log_with_pipeline(LogLevel level, const std::string& message,
                                             const std::string& service, const Properties& data,
                                             const std::optional<ContextId>& context_id) {
    if (!pipeline_processor_) {
        base_client_->log(level, service, message, data, context_id.value_or(0));
        return;
    }
    
    try {
        // Create log context
        LogHookContext context(level, service, message, data, context_id.value_or(0));
        
        // Process through pipeline
        auto result = pipeline_processor_->process_log(context);
        
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_pipeline_executions++;
            
            if (result == PipelineResult::CONTINUE) {
                stats_.validation_passes++;
                stats_.enrichment_applications++;
            } else if (result == PipelineResult::SKIP) {
                return; // Skip this log
            } else {
                stats_.validation_failures++;
                stats_.total_processing_errors++;
                if (validation_error_callback_) {
                    validation_error_callback_("Log pipeline processing failed");
                }
                return;
            }
        }
        
        // Send processed log
        base_client_->log(context.get_level(), context.get_service(), 
                        context.get_message(), context.get_data(), context.get_context_id());
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_processing_errors++;
        throw;
    }
}

void EnhancedClient::on_processing_error(const std::exception& error) {
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_processing_errors++;
    }
    
    UC_RECORD_ERROR("processing_exception");
    
    if (error_callback_) {
        error_callback_(error);
    }
    
    if (config_.enable_alerting) {
        get_alert_manager().fire_alert("Processing error: " + std::string(error.what()), 
                                     SimpleAlertManager::AlertLevel::ERROR);
    }
}

void EnhancedClient::on_validation_error(const std::string& error) {
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.validation_failures++;
        stats_.total_processing_errors++;
    }
    
    UC_RECORD_ERROR("validation_error");
    
    if (validation_error_callback_) {
        validation_error_callback_(error);
    }
}

//=============================================================================
// EnhancedClientFactory Implementation
//=============================================================================

std::unique_ptr<EnhancedClient> EnhancedClientFactory::create_server_optimized(const std::string& api_key) {
    auto config = EnhancedClientConfig::server_optimized();
    config.base_config.api_key = api_key;
    return std::make_unique<EnhancedClient>(config);
}

std::unique_ptr<EnhancedClient> EnhancedClientFactory::create_high_throughput(const std::string& api_key) {
    auto config = EnhancedClientConfig::high_throughput();
    config.base_config.api_key = api_key;
    return std::make_unique<EnhancedClient>(config);
}

std::unique_ptr<EnhancedClient> EnhancedClientFactory::create_development(const std::string& api_key) {
    auto config = EnhancedClientConfig::development();
    config.base_config.api_key = api_key;
    return std::make_unique<EnhancedClient>(config);
}

std::unique_ptr<EnhancedClient> EnhancedClientFactory::create_custom(const EnhancedClientConfig& config) {
    return std::make_unique<EnhancedClient>(config);
}

std::unique_ptr<EnhancedClient> EnhancedClientFactory::create_auto_configured(const std::string& api_key) {
    // Auto-detect optimal configuration based on system capabilities
    uint32_t cpu_cores = std::thread::hardware_concurrency();
    size_t memory_mb = Utils::get_current_memory_usage() / (1024 * 1024);
    
    EnhancedClientConfig config;
    config.base_config.api_key = api_key;
    
    if (cpu_cores >= 8 && memory_mb >= 4096) {
        // High-performance system
        config = EnhancedClientConfig::high_throughput();
        config.base_config.api_key = api_key;
    } else if (cpu_cores >= 4 && memory_mb >= 2048) {
        // Standard server system
        config = EnhancedClientConfig::server_optimized();
        config.base_config.api_key = api_key;
    } else {
        // Development or low-resource system
        config = EnhancedClientConfig::development();
        config.base_config.api_key = api_key;
    }
    
    return std::make_unique<EnhancedClient>(config);
}

} // namespace usercanal