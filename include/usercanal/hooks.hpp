// include/usercanal/hooks.hpp
// Purpose: Validation and hooks system for advanced SDK control and extensibility
// Provides lifecycle hooks, event validation, custom processing, and plugin architecture

#pragma once

#include "types.hpp"
#include "errors.hpp"
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <any>

namespace usercanal {

// Forward declarations
class HookManager;
class ValidationEngine;
class EventHook;
class LogHook;

// Hook execution phases
enum class HookPhase {
    PRE_VALIDATION = 0,    // Before validation
    POST_VALIDATION = 1,   // After validation, before processing
    PRE_SERIALIZATION = 2, // Before serialization
    POST_SERIALIZATION = 3,// After serialization, before sending
    PRE_SEND = 4,          // Just before network send
    POST_SEND = 5,         // After successful send
    ON_ERROR = 6,          // On any error during processing
    ON_RETRY = 7           // On retry attempts
};

// Hook execution result
enum class HookResult {
    CONTINUE = 0,     // Continue normal processing
    SKIP = 1,         // Skip this item
    ABORT = 2,        // Abort entire operation
    RETRY = 3,        // Retry this item later
    TRANSFORM = 4     // Item was transformed, continue
};

// Hook priority levels (lower number = higher priority)
enum class HookPriority {
    HIGHEST = 0,
    HIGH = 25,
    NORMAL = 50,
    LOW = 75,
    LOWEST = 100
};

// Hook context for event processing
class EventHookContext {
public:
    EventHookContext(const std::string& user_id, const std::string& event_name, 
                     Properties properties, EventType type = EventType::TRACK);
    
    // Basic getters
    const std::string& get_user_id() const { return user_id_; }
    const std::string& get_event_name() const { return event_name_; }
    const Properties& get_properties() const { return properties_; }
    EventType get_event_type() const { return event_type_; }
    Timestamp get_timestamp() const { return timestamp_; }
    HookPhase get_current_phase() const { return current_phase_; }
    
    // Transformation methods (hooks can modify data)
    void set_user_id(const std::string& user_id) { user_id_ = user_id; modified_ = true; }
    void set_event_name(const std::string& event_name) { event_name_ = event_name; modified_ = true; }
    void set_properties(const Properties& properties) { properties_ = properties; modified_ = true; }
    void set_event_type(EventType type) { event_type_ = type; modified_ = true; }
    void set_timestamp(Timestamp timestamp) { timestamp_ = timestamp; modified_ = true; }
    
    // Property manipulation
    void add_property(const std::string& key, const PropertyValue& value);
    void remove_property(const std::string& key);
    void update_property(const std::string& key, const PropertyValue& value);
    bool has_property(const std::string& key) const;
    
    // Hook metadata (for passing data between hooks)
    void set_hook_data(const std::string& key, const std::any& value);
    std::any get_hook_data(const std::string& key) const;
    bool has_hook_data(const std::string& key) const;
    void clear_hook_data();
    
    // Execution tracking
    bool was_modified() const { return modified_; }
    void mark_modified() { modified_ = true; }
    void set_current_phase(HookPhase phase) { current_phase_ = phase; }
    
    // Error handling
    void add_error(const std::string& error);
    const std::vector<std::string>& get_errors() const { return errors_; }
    bool has_errors() const { return !errors_.empty(); }
    void clear_errors() { errors_.clear(); }
    
    // Performance tracking
    std::chrono::milliseconds get_processing_time() const;
    void start_timing();
    void stop_timing();

private:
    std::string user_id_;
    std::string event_name_;
    Properties properties_;
    EventType event_type_;
    Timestamp timestamp_;
    HookPhase current_phase_ = HookPhase::PRE_VALIDATION;
    
    std::unordered_map<std::string, std::any> hook_data_;
    std::vector<std::string> errors_;
    bool modified_ = false;
    
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point end_time_;
};

// Hook context for log processing
class LogHookContext {
public:
    LogHookContext(LogLevel level, const std::string& service, const std::string& message, 
                   Properties data = {}, ContextId context_id = 0);
    
    // Basic getters
    LogLevel get_level() const { return level_; }
    const std::string& get_service() const { return service_; }
    const std::string& get_message() const { return message_; }
    const Properties& get_data() const { return data_; }
    ContextId get_context_id() const { return context_id_; }
    Timestamp get_timestamp() const { return timestamp_; }
    HookPhase get_current_phase() const { return current_phase_; }
    
    // Transformation methods
    void set_level(LogLevel level) { level_ = level; modified_ = true; }
    void set_service(const std::string& service) { service_ = service; modified_ = true; }
    void set_message(const std::string& message) { message_ = message; modified_ = true; }
    void set_data(const Properties& data) { data_ = data; modified_ = true; }
    void set_context_id(ContextId context_id) { context_id_ = context_id; modified_ = true; }
    void set_timestamp(Timestamp timestamp) { timestamp_ = timestamp; modified_ = true; }
    
    // Data manipulation
    void add_data(const std::string& key, const PropertyValue& value);
    void remove_data(const std::string& key);
    void update_data(const std::string& key, const PropertyValue& value);
    bool has_data(const std::string& key) const;
    
    // Hook metadata
    void set_hook_data(const std::string& key, const std::any& value);
    std::any get_hook_data(const std::string& key) const;
    bool has_hook_data(const std::string& key) const;
    void clear_hook_data();
    
    // Execution tracking
    bool was_modified() const { return modified_; }
    void mark_modified() { modified_ = true; }
    void set_current_phase(HookPhase phase) { current_phase_ = phase; }
    
    // Error handling
    void add_error(const std::string& error);
    const std::vector<std::string>& get_errors() const { return errors_; }
    bool has_errors() const { return !errors_.empty(); }
    void clear_errors() { errors_.clear(); }
    
    // Performance tracking
    std::chrono::milliseconds get_processing_time() const;
    void start_timing();
    void stop_timing();

private:
    LogLevel level_;
    std::string service_;
    std::string message_;
    Properties data_;
    ContextId context_id_;
    Timestamp timestamp_;
    HookPhase current_phase_ = HookPhase::PRE_VALIDATION;
    
    std::unordered_map<std::string, std::any> hook_data_;
    std::vector<std::string> errors_;
    bool modified_ = false;
    
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point end_time_;
};

// Base hook interface
class Hook {
public:
    virtual ~Hook() = default;
    
    // Hook identification
    virtual std::string get_name() const = 0;
    virtual std::string get_version() const { return "1.0.0"; }
    virtual HookPriority get_priority() const { return HookPriority::NORMAL; }
    
    // Phase specification
    virtual std::vector<HookPhase> get_supported_phases() const = 0;
    
    // Configuration
    virtual void configure(const std::unordered_map<std::string, std::any>& /*config*/) {}
    
    // Lifecycle
    virtual void initialize() {}
    virtual void shutdown() {}
    virtual bool is_enabled() const { return enabled_; }
    virtual void set_enabled(bool enabled) { enabled_ = enabled; }

protected:
    bool enabled_ = true;
};

// Event hook interface
class EventHook : public Hook {
public:
    virtual HookResult execute(EventHookContext& context) = 0;
    
    // Hook can specify which event types it handles
    virtual std::vector<EventType> get_supported_event_types() const {
        return {EventType::TRACK, EventType::IDENTIFY, EventType::GROUP, EventType::ALIAS};
    }
    
    // Hook can specify which event names it handles (empty = all)
    virtual std::vector<std::string> get_supported_event_names() const { return {}; }
};

// Log hook interface  
class LogHook : public Hook {
public:
    virtual HookResult execute(LogHookContext& context) = 0;
    
    // Hook can specify which log levels it handles
    virtual std::vector<LogLevel> get_supported_log_levels() const {
        return {LogLevel::EMERGENCY, LogLevel::ALERT, LogLevel::CRITICAL, 
                LogLevel::ERROR, LogLevel::WARNING, LogLevel::NOTICE, 
                LogLevel::INFO, LogLevel::DEBUG, LogLevel::TRACE};
    }
    
    // Hook can specify which services it handles (empty = all)
    virtual std::vector<std::string> get_supported_services() const { return {}; }
};

// Validation rule interface
class ValidationRule {
public:
    virtual ~ValidationRule() = default;
    
    virtual std::string get_name() const = 0;
    virtual std::string get_description() const = 0;
    virtual bool is_enabled() const { return enabled_; }
    virtual void set_enabled(bool enabled) { enabled_ = enabled; }

protected:
    bool enabled_ = true;
};

// Event validation rule
class EventValidationRule : public ValidationRule {
public:
    virtual bool validate(const EventHookContext& context, std::string& error_message) const = 0;
    
    // Rule can specify which event types it validates
    virtual std::vector<EventType> get_applicable_event_types() const {
        return {EventType::TRACK, EventType::IDENTIFY, EventType::GROUP, EventType::ALIAS};
    }
};

// Log validation rule
class LogValidationRule : public ValidationRule {
public:
    virtual bool validate(const LogHookContext& context, std::string& error_message) const = 0;
    
    // Rule can specify which log levels it validates
    virtual std::vector<LogLevel> get_applicable_log_levels() const {
        return {LogLevel::EMERGENCY, LogLevel::ALERT, LogLevel::CRITICAL, 
                LogLevel::ERROR, LogLevel::WARNING, LogLevel::NOTICE, 
                LogLevel::INFO, LogLevel::DEBUG, LogLevel::TRACE};
    }
};

// Built-in validation rules
class RequiredFieldValidationRule : public EventValidationRule {
public:
    explicit RequiredFieldValidationRule(const std::vector<std::string>& required_fields);
    
    std::string get_name() const override { return "required_fields"; }
    std::string get_description() const override;
    bool validate(const EventHookContext& context, std::string& error_message) const override;

private:
    std::vector<std::string> required_fields_;
};

class MaxLengthValidationRule : public EventValidationRule {
public:
    MaxLengthValidationRule(const std::string& field_name, size_t max_length);
    
    std::string get_name() const override { return "max_length_" + field_name_; }
    std::string get_description() const override;
    bool validate(const EventHookContext& context, std::string& error_message) const override;

private:
    std::string field_name_;
    size_t max_length_;
};

class RegexValidationRule : public EventValidationRule {
public:
    RegexValidationRule(const std::string& field_name, const std::string& pattern, 
                       const std::string& pattern_description = "");
    
    std::string get_name() const override { return "regex_" + field_name_; }
    std::string get_description() const override;
    bool validate(const EventHookContext& context, std::string& error_message) const override;

private:
    std::string field_name_;
    std::string pattern_;
    std::string pattern_description_;
};

class LogLevelValidationRule : public LogValidationRule {
public:
    LogLevelValidationRule(LogLevel min_level, LogLevel max_level = LogLevel::TRACE);
    
    std::string get_name() const override { return "log_level_range"; }
    std::string get_description() const override;
    bool validate(const LogHookContext& context, std::string& error_message) const override;

private:
    LogLevel min_level_;
    LogLevel max_level_;
};

// Validation engine
class ValidationEngine {
public:
    ValidationEngine();
    ~ValidationEngine();
    
    // Rule management
    void add_event_rule(std::shared_ptr<EventValidationRule> rule);
    void add_log_rule(std::shared_ptr<LogValidationRule> rule);
    void remove_rule(const std::string& rule_name);
    void clear_rules();
    
    // Built-in rules
    void add_required_fields_rule(const std::vector<std::string>& fields);
    void add_max_length_rule(const std::string& field, size_t max_length);
    void add_regex_rule(const std::string& field, const std::string& pattern);
    void add_email_validation_rule(const std::string& field);
    void add_url_validation_rule(const std::string& field);
    void add_log_level_rule(LogLevel min_level, LogLevel max_level = LogLevel::TRACE);
    
    // Validation execution
    bool validate_event(EventHookContext& context) const;
    bool validate_log(LogHookContext& context) const;
    
    // Rule information
    std::vector<std::string> get_event_rule_names() const;
    std::vector<std::string> get_log_rule_names() const;
    std::shared_ptr<ValidationRule> get_rule(const std::string& name) const;
    
    // Enable/disable rules
    void enable_rule(const std::string& name);
    void disable_rule(const std::string& name);
    void enable_all_rules();
    void disable_all_rules();
    
    // Configuration
    void set_fail_fast(bool fail_fast) { fail_fast_ = fail_fast; }
    bool get_fail_fast() const { return fail_fast_; }

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
    bool fail_fast_ = true; // Stop validation on first failure
};

// Hook execution statistics
struct HookStats {
    std::atomic<uint64_t> events_processed{0};
    std::atomic<uint64_t> logs_processed{0};
    std::atomic<uint64_t> hooks_executed{0};
    std::atomic<uint64_t> hooks_skipped{0};
    std::atomic<uint64_t> hooks_failed{0};
    std::atomic<uint64_t> validations_passed{0};
    std::atomic<uint64_t> validations_failed{0};
    std::atomic<uint64_t> total_execution_time_ms{0};
    
    void reset();
    
    struct Snapshot {
        uint64_t events_processed = 0;
        uint64_t logs_processed = 0;
        uint64_t hooks_executed = 0;
        uint64_t hooks_skipped = 0;
        uint64_t hooks_failed = 0;
        uint64_t validations_passed = 0;
        uint64_t validations_failed = 0;
        uint64_t total_execution_time_ms = 0;
        
        double get_hook_success_rate() const {
            uint64_t total = hooks_executed + hooks_failed;
            return total > 0 ? static_cast<double>(hooks_executed) / total : 1.0;
        }
        
        double get_validation_success_rate() const {
            uint64_t total = validations_passed + validations_failed;
            return total > 0 ? static_cast<double>(validations_passed) / total : 1.0;
        }
        
        double get_average_execution_time_ms() const {
            uint64_t total = hooks_executed;
            return total > 0 ? static_cast<double>(total_execution_time_ms) / total : 0.0;
        }
    };
    
    Snapshot snapshot() const;
};

// Main hook manager
class HookManager {
public:
    HookManager();
    ~HookManager();
    
    // Non-copyable, movable
    HookManager(const HookManager&) = delete;
    HookManager& operator=(const HookManager&) = delete;
    HookManager(HookManager&&) noexcept;
    HookManager& operator=(HookManager&&) noexcept;
    
    // Hook registration
    void register_event_hook(std::shared_ptr<EventHook> hook);
    void register_log_hook(std::shared_ptr<LogHook> hook);
    void unregister_hook(const std::string& hook_name);
    void clear_hooks();
    
    // Validation engine access
    ValidationEngine& get_validation_engine() { return *validation_engine_; }
    const ValidationEngine& get_validation_engine() const { return *validation_engine_; }
    
    // Hook execution
    HookResult execute_event_hooks(EventHookContext& context, HookPhase phase);
    HookResult execute_log_hooks(LogHookContext& context, HookPhase phase);
    
    // Process complete pipelines (all phases)
    HookResult process_event(EventHookContext& context);
    HookResult process_log(LogHookContext& context);
    
    // Hook information
    std::vector<std::string> get_event_hook_names() const;
    std::vector<std::string> get_log_hook_names() const;
    std::vector<std::string> get_hooks_for_phase(HookPhase phase) const;
    
    // Configuration
    void set_timeout(std::chrono::milliseconds timeout);
    void set_parallel_execution(bool enabled);
    void set_continue_on_error(bool enabled);
    
    // Statistics
    HookStats::Snapshot get_stats() const;
    void reset_stats();
    
    // Lifecycle
    void initialize();
    void shutdown();
    bool is_initialized() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
    std::unique_ptr<ValidationEngine> validation_engine_;
};

// Built-in hooks
namespace BuiltInHooks {

// Enrichment hook - adds common properties
class EnrichmentEventHook : public EventHook {
public:
    EnrichmentEventHook();
    
    std::string get_name() const override { return "enrichment"; }
    std::vector<HookPhase> get_supported_phases() const override {
        return {HookPhase::POST_VALIDATION};
    }
    
    HookResult execute(EventHookContext& context) override;
    
    // Configuration
    void set_add_timestamp(bool enabled) { add_timestamp_ = enabled; }
    void set_add_hostname(bool enabled) { add_hostname_ = enabled; }
    void set_session_id(const std::string& session_id) { session_id_ = session_id; }

private:
    bool add_timestamp_ = true;
    bool add_hostname_ = true;
    std::string session_id_;
};

// Privacy hook - removes or hashes PII
class PrivacyEventHook : public EventHook {
public:
    PrivacyEventHook();
    
    std::string get_name() const override { return "privacy"; }
    std::vector<HookPhase> get_supported_phases() const override {
        return {HookPhase::PRE_SERIALIZATION};
    }
    
    HookResult execute(EventHookContext& context) override;
    
    // Configuration
    void add_pii_field(const std::string& field_name);
    void remove_pii_field(const std::string& field_name);
    void set_hash_pii(bool hash) { hash_pii_ = hash; }
    void set_remove_pii(bool remove) { remove_pii_ = remove; }

private:
    std::vector<std::string> pii_fields_;
    bool hash_pii_ = false;
    bool remove_pii_ = true;
};

// Sampling hook - samples events based on rules
class SamplingEventHook : public EventHook {
public:
    SamplingEventHook(double sample_rate = 1.0);
    
    std::string get_name() const override { return "sampling"; }
    std::vector<HookPhase> get_supported_phases() const override {
        return {HookPhase::PRE_VALIDATION};
    }
    
    HookResult execute(EventHookContext& context) override;
    
    // Configuration
    void set_sample_rate(double rate) { sample_rate_ = rate; }
    void set_event_specific_rate(const std::string& event_name, double rate);

private:
    double sample_rate_;
    std::unordered_map<std::string, double> event_rates_;
};

// Rate limiting hook
class RateLimitingLogHook : public LogHook {
public:
    RateLimitingLogHook(double logs_per_second = 100.0, size_t burst_size = 10);
    
    std::string get_name() const override { return "rate_limiting"; }
    std::vector<HookPhase> get_supported_phases() const override {
        return {HookPhase::PRE_VALIDATION};
    }
    
    HookResult execute(LogHookContext& context) override;
    
    // Configuration
    void set_rate_limit(double logs_per_second, size_t burst_size);
    void set_service_specific_limit(const std::string& service, double logs_per_second);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace BuiltInHooks

// Hook factory for common configurations
namespace HookFactory {

// Create a basic validation setup
std::unique_ptr<HookManager> create_basic_hook_manager();

// Create a privacy-compliant setup
std::unique_ptr<HookManager> create_privacy_compliant_manager();

// Create a performance-optimized setup
std::unique_ptr<HookManager> create_performance_optimized_manager();

// Create a comprehensive setup with all features
std::unique_ptr<HookManager> create_full_featured_manager();

} // namespace HookFactory

} // namespace usercanal