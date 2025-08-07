// include/usercanal/pipeline.hpp
// Purpose: Event processing pipeline with middleware support for advanced event transformation
// Provides hooks, validation, transformation, and enrichment capabilities

#pragma once

#include "types.hpp"
#include "errors.hpp"
#include <memory>
#include <vector>
#include <functional>
#include <string>
#include <chrono>
#include <any>
#include <unordered_map>
#include <shared_mutex>
#include <future>
#include <random>

namespace usercanal {

// Forward declarations
class EventContext;
class LogContext;
class PipelineProcessor;
class Middleware;

// Pipeline execution result
enum class PipelineResult {
    CONTINUE = 0,    // Continue processing
    SKIP = 1,        // Skip this event/log
    ABORT = 2,       // Abort entire pipeline
    RETRY = 3        // Retry processing later
};

// Context for event processing
class EventContext {
public:
    EventContext(const std::string& user_id, const std::string& event_name, Properties properties);
    
    // Getters
    const std::string& get_user_id() const { return user_id_; }
    const std::string& get_event_name() const { return event_name_; }
    const Properties& get_properties() const { return properties_; }
    EventType get_event_type() const { return event_type_; }
    Timestamp get_timestamp() const { return timestamp_; }
    
    // Setters for transformation
    void set_user_id(const std::string& user_id) { user_id_ = user_id; }
    void set_event_name(const std::string& event_name) { event_name_ = event_name; }
    void set_properties(const Properties& properties) { properties_ = properties; }
    void set_event_type(EventType type) { event_type_ = type; }
    void set_timestamp(Timestamp timestamp) { timestamp_ = timestamp; }
    
    // Property manipulation
    void add_property(const std::string& key, const PropertyValue& value);
    void remove_property(const std::string& key);
    bool has_property(const std::string& key) const;
    
    // Metadata (for middleware communication)
    void set_metadata(const std::string& key, const std::any& value);
    std::any get_metadata(const std::string& key) const;
    bool has_metadata(const std::string& key) const;
    
    // Execution context
    std::chrono::milliseconds get_processing_time() const;
    size_t get_middleware_count() const { return middleware_count_; }
    void increment_middleware_count() { middleware_count_++; }
    
    // Validation status
    bool is_valid() const { return is_valid_; }
    void mark_invalid(const std::string& reason);
    const std::string& get_validation_error() const { return validation_error_; }

private:
    std::string user_id_;
    std::string event_name_;
    Properties properties_;
    EventType event_type_;
    Timestamp timestamp_;
    
    std::unordered_map<std::string, std::any> metadata_;
    bool is_valid_ = true;
    std::string validation_error_;
    size_t middleware_count_ = 0;
    std::chrono::steady_clock::time_point start_time_;
};

// Context for log processing
class LogContext {
public:
    LogContext(LogLevel level, const std::string& service, const std::string& message, 
               Properties data = {}, ContextId context_id = 0);
    
    // Getters
    LogLevel get_level() const { return level_; }
    const std::string& get_service() const { return service_; }
    const std::string& get_message() const { return message_; }
    const Properties& get_data() const { return data_; }
    ContextId get_context_id() const { return context_id_; }
    Timestamp get_timestamp() const { return timestamp_; }
    
    // Setters for transformation
    void set_level(LogLevel level) { level_ = level; }
    void set_service(const std::string& service) { service_ = service; }
    void set_message(const std::string& message) { message_ = message; }
    void set_data(const Properties& data) { data_ = data; }
    void set_context_id(ContextId context_id) { context_id_ = context_id; }
    void set_timestamp(Timestamp timestamp) { timestamp_ = timestamp; }
    
    // Data manipulation
    void add_data(const std::string& key, const PropertyValue& value);
    void remove_data(const std::string& key);
    bool has_data(const std::string& key) const;
    
    // Metadata (for middleware communication)
    void set_metadata(const std::string& key, const std::any& value);
    std::any get_metadata(const std::string& key) const;
    bool has_metadata(const std::string& key) const;
    
    // Execution context
    std::chrono::milliseconds get_processing_time() const;
    size_t get_middleware_count() const { return middleware_count_; }
    void increment_middleware_count() { middleware_count_++; }
    
    // Validation status
    bool is_valid() const { return is_valid_; }
    void mark_invalid(const std::string& reason);
    const std::string& get_validation_error() const { return validation_error_; }

private:
    LogLevel level_;
    std::string service_;
    std::string message_;
    Properties data_;
    ContextId context_id_;
    Timestamp timestamp_;
    
    std::unordered_map<std::string, std::any> metadata_;
    bool is_valid_ = true;
    std::string validation_error_;
    size_t middleware_count_ = 0;
    std::chrono::steady_clock::time_point start_time_;
};

// Base middleware interface
class Middleware {
public:
    virtual ~Middleware() = default;
    
    // Process event (return CONTINUE to proceed, SKIP to drop, ABORT to stop pipeline)
    virtual PipelineResult process_event(EventContext& context) = 0;
    
    // Process log (return CONTINUE to proceed, SKIP to drop, ABORT to stop pipeline)  
    virtual PipelineResult process_log(LogContext& context) = 0;
    
    // Middleware identification
    virtual std::string get_name() const = 0;
    virtual std::string get_version() const { return "1.0.0"; }
    virtual int get_priority() const { return 100; } // Lower = higher priority
    
    // Configuration
    virtual void configure(const std::unordered_map<std::string, std::any>& /*config*/) {}
    
    // Lifecycle
    virtual void initialize() {}
    virtual void shutdown() {}
};

// Validation middleware
class ValidationMiddleware : public Middleware {
public:
    using EventValidator = std::function<bool(const EventContext&, std::string&)>;
    using LogValidator = std::function<bool(const LogContext&, std::string&)>;
    
    ValidationMiddleware();
    
    // Add validation rules
    void add_event_validator(const std::string& name, EventValidator validator);
    void add_log_validator(const std::string& name, LogValidator validator);
    
    // Built-in validators
    void require_user_id();
    void require_event_name();
    void require_service_name();
    void require_log_message();
    void max_property_count(size_t max_count);
    void max_property_size(size_t max_size_bytes);
    void validate_email_format(const std::string& property_name);
    void validate_url_format(const std::string& property_name);
    
    PipelineResult process_event(EventContext& context) override;
    PipelineResult process_log(LogContext& context) override;
    
    std::string get_name() const override { return "validation"; }
    int get_priority() const override { return 10; } // High priority

private:
    std::vector<std::pair<std::string, EventValidator>> event_validators_;
    std::vector<std::pair<std::string, LogValidator>> log_validators_;
};

// Enrichment middleware
class EnrichmentMiddleware : public Middleware {
public:
    using EventEnricher = std::function<void(EventContext&)>;
    using LogEnricher = std::function<void(LogContext&)>;
    
    EnrichmentMiddleware();
    
    // Add enrichment functions
    void add_event_enricher(const std::string& name, EventEnricher enricher);
    void add_log_enricher(const std::string& name, LogEnricher enricher);
    
    // Built-in enrichers
    void add_timestamp();
    void add_session_id(const std::string& session_id);
    void add_user_agent(const std::string& user_agent);
    void add_ip_address(const std::string& ip);
    void add_hostname();
    void add_request_id();
    void add_correlation_id();
    
    PipelineResult process_event(EventContext& context) override;
    PipelineResult process_log(LogContext& context) override;
    
    std::string get_name() const override { return "enrichment"; }
    int get_priority() const override { return 50; } // Medium priority

private:
    std::vector<std::pair<std::string, EventEnricher>> event_enrichers_;
    std::vector<std::pair<std::string, LogEnricher>> log_enrichers_;
};

// Filtering middleware
class FilterMiddleware : public Middleware {
public:
    using EventFilter = std::function<bool(const EventContext&)>;
    using LogFilter = std::function<bool(const LogContext&)>;
    
    FilterMiddleware();
    
    // Add filters (return true to keep, false to skip)
    void add_event_filter(const std::string& name, EventFilter filter);
    void add_log_filter(const std::string& name, LogFilter filter);
    
    // Built-in filters
    void filter_by_user_id(const std::vector<std::string>& allowed_users);
    void filter_by_event_name(const std::vector<std::string>& allowed_events);
    void filter_by_log_level(LogLevel min_level);
    void filter_by_service(const std::vector<std::string>& allowed_services);
    void sample_events(double sample_rate); // 0.0 - 1.0
    void sample_logs(double sample_rate);
    
    PipelineResult process_event(EventContext& context) override;
    PipelineResult process_log(LogContext& context) override;
    
    std::string get_name() const override { return "filter"; }
    int get_priority() const override { return 20; } // High priority

private:
    std::vector<std::pair<std::string, EventFilter>> event_filters_;
    std::vector<std::pair<std::string, LogFilter>> log_filters_;
    // Sample rates - reserved for future use
    // double event_sample_rate_ = 1.0;
    // double log_sample_rate_ = 1.0;
};

// Transformation middleware  
class TransformationMiddleware : public Middleware {
public:
    using EventTransformer = std::function<void(EventContext&)>;
    using LogTransformer = std::function<void(LogContext&)>;
    
    TransformationMiddleware();
    
    // Add transformation functions
    void add_event_transformer(const std::string& name, EventTransformer transformer);
    void add_log_transformer(const std::string& name, LogTransformer transformer);
    
    // Built-in transformers
    void normalize_user_id();
    void normalize_event_names();
    void anonymize_property(const std::string& property_name);
    void hash_property(const std::string& property_name);
    void truncate_strings(size_t max_length);
    void convert_currency(const std::string& target_currency);
    void format_timestamps();
    
    PipelineResult process_event(EventContext& context) override;
    PipelineResult process_log(LogContext& context) override;
    
    std::string get_name() const override { return "transformation"; }
    int get_priority() const override { return 70; } // Lower priority

private:
    std::vector<std::pair<std::string, EventTransformer>> event_transformers_;
    std::vector<std::pair<std::string, LogTransformer>> log_transformers_;
};

// Pipeline statistics
struct PipelineStats {
    std::atomic<uint64_t> events_processed{0};
    std::atomic<uint64_t> events_skipped{0};
    std::atomic<uint64_t> events_failed{0};
    std::atomic<uint64_t> logs_processed{0};
    std::atomic<uint64_t> logs_skipped{0};
    std::atomic<uint64_t> logs_failed{0};
    std::atomic<uint64_t> total_processing_time_ms{0};
    
    void reset();
    
    struct Snapshot {
        uint64_t events_processed = 0;
        uint64_t events_skipped = 0;
        uint64_t events_failed = 0;
        uint64_t logs_processed = 0;
        uint64_t logs_skipped = 0;
        uint64_t logs_failed = 0;
        uint64_t total_processing_time_ms = 0;
        
        double get_event_success_rate() const {
            uint64_t total = events_processed + events_failed;
            return total > 0 ? static_cast<double>(events_processed) / total : 1.0;
        }
        
        double get_log_success_rate() const {
            uint64_t total = logs_processed + logs_failed;
            return total > 0 ? static_cast<double>(logs_processed) / total : 1.0;
        }
        
        double get_average_processing_time_ms() const {
            uint64_t total = events_processed + logs_processed;
            return total > 0 ? static_cast<double>(total_processing_time_ms) / total : 0.0;
        }
    };
    
    Snapshot snapshot() const;
};

// Main pipeline processor
class PipelineProcessor {
public:
    PipelineProcessor();
    ~PipelineProcessor();
    
    // Non-copyable, movable
    PipelineProcessor(const PipelineProcessor&) = delete;
    PipelineProcessor& operator=(const PipelineProcessor&) = delete;
    PipelineProcessor(PipelineProcessor&&) noexcept;
    PipelineProcessor& operator=(PipelineProcessor&&) noexcept;
    
    // Middleware management
    void add_middleware(std::shared_ptr<Middleware> middleware);
    void remove_middleware(const std::string& name);
    void clear_middleware();
    
    // Built-in middleware setup
    std::shared_ptr<ValidationMiddleware> add_validation();
    std::shared_ptr<EnrichmentMiddleware> add_enrichment();
    std::shared_ptr<FilterMiddleware> add_filtering();
    std::shared_ptr<TransformationMiddleware> add_transformation();
    
    // Pipeline execution
    PipelineResult process_event(EventContext& context);
    PipelineResult process_log(LogContext& context);
    
    // Async processing
    std::future<PipelineResult> process_event_async(std::unique_ptr<EventContext> context);
    std::future<PipelineResult> process_log_async(std::unique_ptr<LogContext> context);
    
    // Configuration
    void set_timeout(std::chrono::milliseconds timeout);
    void enable_parallel_processing(bool enabled);
    void set_max_processing_time(std::chrono::milliseconds max_time);
    
    // Statistics and monitoring
    PipelineStats::Snapshot get_stats() const;
    void reset_stats();
    
    // Middleware information
    std::vector<std::string> get_middleware_names() const;
    std::shared_ptr<Middleware> get_middleware(const std::string& name) const;
    
    // Lifecycle
    void initialize();
    void shutdown();
    bool is_initialized() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

// Pipeline event callbacks
class PipelineEventHandler {
public:
    virtual ~PipelineEventHandler() = default;
    
    // Processing events
    virtual void on_event_processing_started(const EventContext& /*context*/) {}
    virtual void on_event_processing_completed(const EventContext& /*context*/, PipelineResult /*result*/) {}
    virtual void on_log_processing_started(const LogContext& /*context*/) {}
    virtual void on_log_processing_completed(const LogContext& /*context*/, PipelineResult /*result*/) {}
    
    // Middleware events
    virtual void on_middleware_executed(const std::string& /*middleware_name*/, 
                                       std::chrono::milliseconds /*duration*/) {}
    virtual void on_middleware_error(const std::string& /*middleware_name*/, 
                                    const std::string& /*error*/) {}
    
    // Pipeline events
    virtual void on_pipeline_timeout(std::chrono::milliseconds /*timeout*/) {}
    virtual void on_pipeline_error(const Error& /*error*/) {}
};

// Factory for common middleware configurations
namespace PipelineFactory {

// Create a basic validation pipeline
std::unique_ptr<PipelineProcessor> create_basic_pipeline();

// Create a comprehensive pipeline with all middleware
std::unique_ptr<PipelineProcessor> create_full_pipeline();

// Create a performance-optimized pipeline
std::unique_ptr<PipelineProcessor> create_performance_pipeline();

// Create a compliance-focused pipeline (GDPR, data privacy)
std::unique_ptr<PipelineProcessor> create_compliance_pipeline();

// Create a debugging pipeline with extensive logging
std::unique_ptr<PipelineProcessor> create_debug_pipeline();

} // namespace PipelineFactory

} // namespace usercanal