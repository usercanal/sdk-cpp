// src/hooks.cpp
// Implementation of hooks system for event and log processing callbacks

#include "usercanal/hooks.hpp"
#include "usercanal/utils.hpp"
#include <algorithm>
#include <thread>
#include <future>
#include <unordered_set>
#include <shared_mutex>

namespace usercanal {

//=============================================================================
// EventHookContext Implementation
//=============================================================================

EventHookContext::EventHookContext(const std::string& user_id, const std::string& event_name, 
                                   Properties properties, EventType type)
    : user_id_(user_id)
    , event_name_(event_name)
    , properties_(std::move(properties))
    , event_type_(type)
    , timestamp_(Utils::now_milliseconds())
    , start_time_(std::chrono::steady_clock::now()) {
}

//=============================================================================
// EventHookContext Method Implementations
//=============================================================================

void EventHookContext::add_property(const std::string& key, const PropertyValue& value) {
    properties_[key] = value;
    modified_ = true;
}

void EventHookContext::remove_property(const std::string& key) {
    if (properties_.contains(key)) {
        properties_[key] = nullptr;
        modified_ = true;
    }
}

void EventHookContext::update_property(const std::string& key, const PropertyValue& value) {
    if (properties_.contains(key)) {
        properties_[key] = value;
        modified_ = true;
    }
}

bool EventHookContext::has_property(const std::string& key) const {
    return properties_.contains(key);
}

void EventHookContext::set_hook_data(const std::string& key, const std::any& value) {
    hook_data_[key] = value;
}

std::any EventHookContext::get_hook_data(const std::string& key) const {
    auto it = hook_data_.find(key);
    return it != hook_data_.end() ? it->second : std::any{};
}

bool EventHookContext::has_hook_data(const std::string& key) const {
    return hook_data_.find(key) != hook_data_.end();
}

void EventHookContext::clear_hook_data() {
    hook_data_.clear();
}

void EventHookContext::add_error(const std::string& error) {
    errors_.push_back(error);
}

std::chrono::milliseconds EventHookContext::get_processing_time() const {
    if (end_time_ == std::chrono::steady_clock::time_point{}) {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(end_time_ - start_time_);
}

void EventHookContext::start_timing() {
    start_time_ = std::chrono::steady_clock::now();
}

void EventHookContext::stop_timing() {
    end_time_ = std::chrono::steady_clock::now();
}

//=============================================================================
// LogHookContext Implementation
//=============================================================================

LogHookContext::LogHookContext(LogLevel level, const std::string& service, const std::string& message,
                               Properties data, ContextId context_id)
    : level_(level)
    , service_(service)
    , message_(message)
    , data_(std::move(data))
    , context_id_(context_id)
    , timestamp_(Utils::now_milliseconds())
    , start_time_(std::chrono::steady_clock::now()) {
}

//=============================================================================
// LogHookContext Method Implementations
//=============================================================================

void LogHookContext::add_data(const std::string& key, const PropertyValue& value) {
    data_[key] = value;
    modified_ = true;
}

void LogHookContext::remove_data(const std::string& key) {
    if (data_.contains(key)) {
        data_[key] = nullptr;
        modified_ = true;
    }
}

void LogHookContext::update_data(const std::string& key, const PropertyValue& value) {
    if (data_.contains(key)) {
        data_[key] = value;
        modified_ = true;
    }
}

bool LogHookContext::has_data(const std::string& key) const {
    return data_.contains(key);
}

void LogHookContext::set_hook_data(const std::string& key, const std::any& value) {
    hook_data_[key] = value;
}

std::any LogHookContext::get_hook_data(const std::string& key) const {
    auto it = hook_data_.find(key);
    return it != hook_data_.end() ? it->second : std::any{};
}

bool LogHookContext::has_hook_data(const std::string& key) const {
    return hook_data_.find(key) != hook_data_.end();
}

void LogHookContext::clear_hook_data() {
    hook_data_.clear();
}

void LogHookContext::add_error(const std::string& error) {
    errors_.push_back(error);
}

std::chrono::milliseconds LogHookContext::get_processing_time() const {
    if (end_time_ == std::chrono::steady_clock::time_point{}) {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(end_time_ - start_time_);
}

void LogHookContext::start_timing() {
    start_time_ = std::chrono::steady_clock::now();
}

void LogHookContext::stop_timing() {
    end_time_ = std::chrono::steady_clock::now();
}

//=============================================================================
// Built-in Hook Implementations
//=============================================================================

// Basic validation hook
class BasicValidationHook : public EventHook {
public:
    std::string get_name() const override { return "basic_validation"; }
    HookPriority get_priority() const override { return HookPriority::HIGHEST; }
    
    std::vector<HookPhase> get_supported_phases() const override {
        return {HookPhase::PRE_VALIDATION};
    }
    
    HookResult execute(EventHookContext& context) override {
        // Validate user ID
        if (!Utils::is_valid_user_id(context.get_user_id())) {
            context.add_error("Invalid user ID");
            return HookResult::SKIP;
        }
        
        // Validate event name
        if (context.get_event_name().empty()) {
            context.add_error("Empty event name");
            return HookResult::SKIP;
        }
        
        return HookResult::CONTINUE;
    }
};

// Basic log validation hook
class BasicLogValidationHook : public LogHook {
public:
    std::string get_name() const override { return "basic_log_validation"; }
    HookPriority get_priority() const override { return HookPriority::HIGHEST; }
    
    std::vector<HookPhase> get_supported_phases() const override {
        return {HookPhase::PRE_VALIDATION};
    }
    
    HookResult execute(LogHookContext& context) override {
        // Validate service name
        if (context.get_service().empty()) {
            context.add_error("Empty service name");
            return HookResult::SKIP;
        }
        
        // Validate message
        if (context.get_message().empty()) {
            context.add_error("Empty log message");
            return HookResult::SKIP;
        }
        
        return HookResult::CONTINUE;
    }
};

// Event enrichment hook
class EventEnrichmentHook : public EventHook {
public:
    std::string get_name() const override { return "event_enrichment"; }
    HookPriority get_priority() const override { return HookPriority::NORMAL; }
    
    std::vector<HookPhase> get_supported_phases() const override {
        return {HookPhase::POST_VALIDATION};
    }
    
    HookResult execute(EventHookContext& context) override {
        // Add automatic enrichment data
        context.add_property("_enriched_at", static_cast<int64_t>(Utils::now_milliseconds()));
        context.add_property("_sdk_version", std::string("1.0.0"));
        context.add_property("_hostname", Utils::get_hostname());
        
        return HookResult::CONTINUE;
    }
};

// Log enrichment hook
class LogEnrichmentHook : public LogHook {
public:
    std::string get_name() const override { return "log_enrichment"; }
    HookPriority get_priority() const override { return HookPriority::NORMAL; }
    
    std::vector<HookPhase> get_supported_phases() const override {
        return {HookPhase::POST_VALIDATION};
    }
    
    HookResult execute(LogHookContext& context) override {
        // Add automatic enrichment data
        context.add_data("_enriched_at", static_cast<int64_t>(Utils::now_milliseconds()));
        context.add_data("_sdk_version", std::string("1.0.0"));
        context.add_data("_hostname", Utils::get_hostname());
        context.add_data("_level_string", Utils::log_level_to_string(context.get_level()));
        
        return HookResult::CONTINUE;
    }
};

// Privacy compliance hook
class PrivacyComplianceHook : public EventHook {
private:
    std::unordered_set<std::string> pii_fields_ = {
        "email", "phone", "ssn", "credit_card", "address", "ip_address"
    };
    
public:
    std::string get_name() const override { return "privacy_compliance"; }
    HookPriority get_priority() const override { return HookPriority::HIGH; }
    
    std::vector<HookPhase> get_supported_phases() const override {
        return {HookPhase::PRE_SERIALIZATION};
    }
    
    HookResult execute(EventHookContext& context) override {
        // Check for PII in properties
        for (const auto& [key, value] : context.get_properties()) {
            if (pii_fields_.find(key) != pii_fields_.end()) {
                // Hash or redact PII fields
                context.add_property(key + "_hash", std::string("[REDACTED]"));
                context.remove_property(key);
            }
        }
        
        return HookResult::CONTINUE;
    }
};

//=============================================================================
// ValidationEngine Simple Implementation
//=============================================================================

class ValidationEngine::Impl {
public:
    bool fail_fast_ = true;
};

ValidationEngine::ValidationEngine() : pimpl_(std::make_unique<Impl>()) {}
ValidationEngine::~ValidationEngine() = default;

void ValidationEngine::add_event_rule(std::shared_ptr<EventValidationRule> /*rule*/) {
    // Stub implementation
}

void ValidationEngine::add_log_rule(std::shared_ptr<LogValidationRule> /*rule*/) {
    // Stub implementation
}

void ValidationEngine::remove_rule(const std::string& /*rule_name*/) {
    // Stub implementation
}

void ValidationEngine::clear_rules() {
    // Stub implementation
}

void ValidationEngine::add_required_fields_rule(const std::vector<std::string>& /*fields*/) {
    // Stub implementation
}

void ValidationEngine::add_max_length_rule(const std::string& /*field*/, size_t /*max_length*/) {
    // Stub implementation
}

void ValidationEngine::add_regex_rule(const std::string& /*field*/, const std::string& /*pattern*/) {
    // Stub implementation
}

void ValidationEngine::add_email_validation_rule(const std::string& /*field*/) {
    // Stub implementation
}

void ValidationEngine::add_url_validation_rule(const std::string& /*field*/) {
    // Stub implementation
}

void ValidationEngine::add_log_level_rule(LogLevel /*min_level*/, LogLevel /*max_level*/) {
    // Stub implementation
}

bool ValidationEngine::validate_event(EventHookContext& context) const {
    return !context.has_errors();
}

bool ValidationEngine::validate_log(LogHookContext& context) const {
    return !context.has_errors();
}

std::vector<std::string> ValidationEngine::get_event_rule_names() const {
    return {};
}

std::vector<std::string> ValidationEngine::get_log_rule_names() const {
    return {};
}

std::shared_ptr<ValidationRule> ValidationEngine::get_rule(const std::string& /*name*/) const {
    return nullptr;
}

void ValidationEngine::enable_rule(const std::string& /*name*/) {
    // Stub implementation
}

void ValidationEngine::disable_rule(const std::string& /*name*/) {
    // Stub implementation
}

void ValidationEngine::enable_all_rules() {
    // Stub implementation
}

void ValidationEngine::disable_all_rules() {
    // Stub implementation
}




//=============================================================================
// HookManager Implementation
//=============================================================================

class HookManager::Impl {
public:
    std::vector<std::shared_ptr<EventHook>> event_hooks_;
    std::vector<std::shared_ptr<LogHook>> log_hooks_;
    HookStats stats_;
    mutable std::shared_mutex stats_mutex_;
    std::chrono::milliseconds timeout_{5000};
    bool parallel_execution_ = false;
    bool continue_on_error_ = false;
    bool initialized_ = false;
    
    Impl() {
        // Add default hooks
        register_default_hooks();
    }
    
    void register_default_hooks() {
        // Add basic validation hooks
        event_hooks_.push_back(std::make_shared<BasicValidationHook>());
        log_hooks_.push_back(std::make_shared<BasicLogValidationHook>());
        
        // Add enrichment hooks
        event_hooks_.push_back(std::make_shared<EventEnrichmentHook>());
        log_hooks_.push_back(std::make_shared<LogEnrichmentHook>());
        
        sort_hooks();
    }
    
    void sort_hooks() {
        // Sort by priority (lower values = higher priority)
        std::sort(event_hooks_.begin(), event_hooks_.end(),
            [](const std::shared_ptr<EventHook>& a, const std::shared_ptr<EventHook>& b) {
                return static_cast<int>(a->get_priority()) < static_cast<int>(b->get_priority());
            });
        
        std::sort(log_hooks_.begin(), log_hooks_.end(),
            [](const std::shared_ptr<LogHook>& a, const std::shared_ptr<LogHook>& b) {
                return static_cast<int>(a->get_priority()) < static_cast<int>(b->get_priority());
            });
    }
    
    HookResult execute_event_hooks(EventHookContext& context, HookPhase phase) {
        context.set_current_phase(phase);
        
        for (auto& hook : event_hooks_) {
            auto supported_phases = hook->get_supported_phases();
            if (std::find(supported_phases.begin(), supported_phases.end(), phase) == supported_phases.end()) {
                continue;
            }
            
            auto start_time = std::chrono::steady_clock::now();
            
            try {
                HookResult result = hook->execute(context);
                update_hook_stats(start_time, true);
                
                if (result != HookResult::CONTINUE) {
                    if (result == HookResult::SKIP || result == HookResult::ABORT) {
                        return result;
                    }
                }
                
            } catch (const std::exception& e) {
                update_hook_stats(start_time, false);
                context.set_hook_data("hook_error_" + hook->get_name(), std::string(e.what()));
                
                if (!continue_on_error_) {
                    return HookResult::ABORT;
                }
            }
        }
        
        return HookResult::CONTINUE;
    }
    
    HookResult execute_log_hooks(LogHookContext& context, HookPhase phase) {
        context.set_current_phase(phase);
        
        for (auto& hook : log_hooks_) {
            auto supported_phases = hook->get_supported_phases();
            if (std::find(supported_phases.begin(), supported_phases.end(), phase) == supported_phases.end()) {
                continue;
            }
            
            auto start_time = std::chrono::steady_clock::now();
            
            try {
                HookResult result = hook->execute(context);
                update_hook_stats(start_time, true);
                
                if (result != HookResult::CONTINUE) {
                    if (result == HookResult::SKIP || result == HookResult::ABORT) {
                        return result;
                    }
                }
                
            } catch (const std::exception& e) {
                update_hook_stats(start_time, false);
                context.set_hook_data("hook_error_" + hook->get_name(), std::string(e.what()));
                
                if (!continue_on_error_) {
                    return HookResult::ABORT;
                }
            }
        }
        
        return HookResult::CONTINUE;
    }
    
private:
    void update_hook_stats(std::chrono::steady_clock::time_point start_time, bool success) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        
        std::unique_lock lock(stats_mutex_);
        stats_.hooks_executed.fetch_add(1);
        stats_.total_execution_time_ms.fetch_add(duration.count());
        
        if (!success) {
            stats_.hooks_failed.fetch_add(1);
        }
    }
};

//=============================================================================
// HookManager Public Interface
//=============================================================================

HookManager::HookManager() : pimpl_(std::make_unique<Impl>()) {
    validation_engine_ = std::make_unique<ValidationEngine>();
}

HookManager::~HookManager() = default;

HookManager::HookManager(HookManager&&) noexcept = default;
HookManager& HookManager::operator=(HookManager&&) noexcept = default;

void HookManager::register_event_hook(std::shared_ptr<EventHook> hook) {
    pimpl_->event_hooks_.push_back(std::move(hook));
    pimpl_->sort_hooks();
}

void HookManager::register_log_hook(std::shared_ptr<LogHook> hook) {
    pimpl_->log_hooks_.push_back(std::move(hook));
    pimpl_->sort_hooks();
}

void HookManager::unregister_hook(const std::string& hook_name) {
    // Remove from event hooks
    pimpl_->event_hooks_.erase(
        std::remove_if(pimpl_->event_hooks_.begin(), pimpl_->event_hooks_.end(),
            [&hook_name](const std::shared_ptr<EventHook>& hook) {
                return hook->get_name() == hook_name;
            }),
        pimpl_->event_hooks_.end());
    
    // Remove from log hooks
    pimpl_->log_hooks_.erase(
        std::remove_if(pimpl_->log_hooks_.begin(), pimpl_->log_hooks_.end(),
            [&hook_name](const std::shared_ptr<LogHook>& hook) {
                return hook->get_name() == hook_name;
            }),
        pimpl_->log_hooks_.end());
}

void HookManager::clear_hooks() {
    pimpl_->event_hooks_.clear();
    pimpl_->log_hooks_.clear();
}



HookResult HookManager::execute_event_hooks(EventHookContext& context, HookPhase phase) {
    return pimpl_->execute_event_hooks(context, phase);
}

HookResult HookManager::execute_log_hooks(LogHookContext& context, HookPhase phase) {
    return pimpl_->execute_log_hooks(context, phase);
}

HookResult HookManager::process_event(EventHookContext& context) {
    // Execute all phases in sequence
    std::vector<HookPhase> phases = {
        HookPhase::PRE_VALIDATION,
        HookPhase::POST_VALIDATION,
        HookPhase::PRE_SERIALIZATION,
        HookPhase::POST_SERIALIZATION,
        HookPhase::PRE_SEND
    };
    
    for (auto phase : phases) {
        auto result = execute_event_hooks(context, phase);
        if (result != HookResult::CONTINUE) {
            return result;
        }
    }
    
    return HookResult::CONTINUE;
}

HookResult HookManager::process_log(LogHookContext& context) {
    // Execute all phases in sequence
    std::vector<HookPhase> phases = {
        HookPhase::PRE_VALIDATION,
        HookPhase::POST_VALIDATION,
        HookPhase::PRE_SERIALIZATION,
        HookPhase::POST_SERIALIZATION,
        HookPhase::PRE_SEND
    };
    
    for (auto phase : phases) {
        auto result = execute_log_hooks(context, phase);
        if (result != HookResult::CONTINUE) {
            return result;
        }
    }
    
    return HookResult::CONTINUE;
}

std::vector<std::string> HookManager::get_event_hook_names() const {
    std::vector<std::string> names;
    for (const auto& hook : pimpl_->event_hooks_) {
        names.push_back(hook->get_name());
    }
    return names;
}

std::vector<std::string> HookManager::get_log_hook_names() const {
    std::vector<std::string> names;
    for (const auto& hook : pimpl_->log_hooks_) {
        names.push_back(hook->get_name());
    }
    return names;
}

std::vector<std::string> HookManager::get_hooks_for_phase(HookPhase phase) const {
    std::vector<std::string> names;
    
    for (const auto& hook : pimpl_->event_hooks_) {
        auto supported_phases = hook->get_supported_phases();
        if (std::find(supported_phases.begin(), supported_phases.end(), phase) != supported_phases.end()) {
            names.push_back(hook->get_name());
        }
    }
    
    for (const auto& hook : pimpl_->log_hooks_) {
        auto supported_phases = hook->get_supported_phases();
        if (std::find(supported_phases.begin(), supported_phases.end(), phase) != supported_phases.end()) {
            names.push_back(hook->get_name());
        }
    }
    
    return names;
}

void HookManager::set_timeout(std::chrono::milliseconds timeout) {
    pimpl_->timeout_ = timeout;
}

void HookManager::set_parallel_execution(bool enabled) {
    pimpl_->parallel_execution_ = enabled;
}

void HookManager::set_continue_on_error(bool enabled) {
    pimpl_->continue_on_error_ = enabled;
}

void HookManager::initialize() {
    pimpl_->initialized_ = true;
}

void HookManager::shutdown() {
    pimpl_->initialized_ = false;
}

bool HookManager::is_initialized() const {
    return pimpl_->initialized_;
}

HookStats::Snapshot HookManager::get_stats() const {
    std::shared_lock lock(pimpl_->stats_mutex_);
    return pimpl_->stats_.snapshot();
}

void HookManager::reset_stats() {
    std::unique_lock lock(pimpl_->stats_mutex_);
    pimpl_->stats_.reset();
}

//=============================================================================
// HookStats Implementation
//=============================================================================

void HookStats::reset() {
    events_processed.store(0);
    logs_processed.store(0);
    hooks_executed.store(0);
    hooks_skipped.store(0);
    hooks_failed.store(0);
    validations_passed.store(0);
    validations_failed.store(0);
    total_execution_time_ms.store(0);
}

HookStats::Snapshot HookStats::snapshot() const {
    Snapshot s;
    s.events_processed = events_processed.load();
    s.logs_processed = logs_processed.load();
    s.hooks_executed = hooks_executed.load();
    s.hooks_skipped = hooks_skipped.load();
    s.hooks_failed = hooks_failed.load();
    s.validations_passed = validations_passed.load();
    s.validations_failed = validations_failed.load();
    s.total_execution_time_ms = total_execution_time_ms.load();
    return s;
}

} // namespace usercanal