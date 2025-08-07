// src/pipeline.cpp
// Implementation of event processing pipeline with middleware support

#include "usercanal/pipeline.hpp"
#include "usercanal/utils.hpp"
#include <algorithm>
#include <thread>
#include <future>
#include <random>

namespace usercanal {

//=============================================================================
// EventContext Implementation
//=============================================================================

EventContext::EventContext(const std::string& user_id, const std::string& event_name, Properties properties)
    : user_id_(user_id)
    , event_name_(event_name)
    , properties_(std::move(properties))
    , event_type_(EventType::TRACK)
    , timestamp_(Utils::now_milliseconds())
    , start_time_(std::chrono::steady_clock::now()) {
}

void EventContext::add_property(const std::string& key, const PropertyValue& value) {
    properties_[key] = value;
}

void EventContext::remove_property(const std::string& key) {
    // Properties class doesn't have erase method, so we'll set to nullptr
    if (properties_.contains(key)) {
        properties_[key] = nullptr;
    }
}

bool EventContext::has_property(const std::string& key) const {
    return properties_.contains(key);
}

void EventContext::set_metadata(const std::string& key, const std::any& value) {
    metadata_[key] = value;
}

std::any EventContext::get_metadata(const std::string& key) const {
    auto it = metadata_.find(key);
    return it != metadata_.end() ? it->second : std::any{};
}

bool EventContext::has_metadata(const std::string& key) const {
    return metadata_.find(key) != metadata_.end();
}

std::chrono::milliseconds EventContext::get_processing_time() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
}

void EventContext::mark_invalid(const std::string& reason) {
    is_valid_ = false;
    validation_error_ = reason;
}

//=============================================================================
// LogContext Implementation
//=============================================================================

LogContext::LogContext(LogLevel level, const std::string& service, const std::string& message, 
                       Properties data, ContextId context_id)
    : level_(level)
    , service_(service)
    , message_(message)
    , data_(std::move(data))
    , context_id_(context_id)
    , timestamp_(Utils::now_milliseconds())
    , start_time_(std::chrono::steady_clock::now()) {
}

void LogContext::add_data(const std::string& key, const PropertyValue& value) {
    data_[key] = value;
}

void LogContext::remove_data(const std::string& key) {
    if (data_.contains(key)) {
        data_[key] = nullptr;
    }
}

bool LogContext::has_data(const std::string& key) const {
    return data_.contains(key);
}

void LogContext::set_metadata(const std::string& key, const std::any& value) {
    metadata_[key] = value;
}

std::any LogContext::get_metadata(const std::string& key) const {
    auto it = metadata_.find(key);
    return it != metadata_.end() ? it->second : std::any{};
}

bool LogContext::has_metadata(const std::string& key) const {
    return metadata_.find(key) != metadata_.end();
}

std::chrono::milliseconds LogContext::get_processing_time() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
}

void LogContext::mark_invalid(const std::string& reason) {
    is_valid_ = false;
    validation_error_ = reason;
}

//=============================================================================
// ValidationMiddleware Implementation
//=============================================================================

ValidationMiddleware::ValidationMiddleware() {
    // Add default validators
    require_user_id();
    require_event_name();
    require_service_name();
    require_log_message();
}

void ValidationMiddleware::add_event_validator(const std::string& name, EventValidator validator) {
    event_validators_.emplace_back(name, std::move(validator));
}

void ValidationMiddleware::add_log_validator(const std::string& name, LogValidator validator) {
    log_validators_.emplace_back(name, std::move(validator));
}

void ValidationMiddleware::require_user_id() {
    add_event_validator("require_user_id", [](const EventContext& context, std::string& error) {
        if (!Utils::is_valid_user_id(context.get_user_id())) {
            error = "Invalid or empty user ID";
            return false;
        }
        return true;
    });
}

void ValidationMiddleware::require_event_name() {
    add_event_validator("require_event_name", [](const EventContext& context, std::string& error) {
        if (context.get_event_name().empty()) {
            error = "Empty event name";
            return false;
        }
        if (context.get_event_name().length() > 255) {
            error = "Event name too long (max 255 characters)";
            return false;
        }
        return true;
    });
}

void ValidationMiddleware::require_service_name() {
    add_log_validator("require_service_name", [](const LogContext& context, std::string& error) {
        if (context.get_service().empty()) {
            error = "Empty service name";
            return false;
        }
        if (context.get_service().length() > 100) {
            error = "Service name too long (max 100 characters)";
            return false;
        }
        return true;
    });
}

void ValidationMiddleware::require_log_message() {
    add_log_validator("require_log_message", [](const LogContext& context, std::string& error) {
        if (context.get_message().empty()) {
            error = "Empty log message";
            return false;
        }
        if (context.get_message().length() > 10000) {
            error = "Log message too long (max 10000 characters)";
            return false;
        }
        return true;
    });
}

void ValidationMiddleware::max_property_count(size_t max_count) {
    add_event_validator("max_property_count", [max_count](const EventContext& context, std::string& error) {
        if (context.get_properties().size() > max_count) {
            error = "Too many properties (max " + std::to_string(max_count) + ")";
            return false;
        }
        return true;
    });
}

void ValidationMiddleware::max_property_size(size_t max_size_bytes) {
    add_event_validator("max_property_size", [max_size_bytes](const EventContext& context, std::string& error) {
        // Simplified size calculation
        size_t total_size = 0;
        for (const auto& [key, value] : context.get_properties()) {
            total_size += key.size();
            // Approximate value size
            total_size += 50; // Rough estimate per value
        }
        
        if (total_size > max_size_bytes) {
            error = "Properties too large (max " + std::to_string(max_size_bytes) + " bytes)";
            return false;
        }
        return true;
    });
}

void ValidationMiddleware::validate_email_format(const std::string& property_name) {
    add_event_validator("validate_email_" + property_name, [property_name](const EventContext& context, std::string& error) {
        if (context.has_property(property_name)) {
            // Simplified email validation
            // In practice, would use proper regex or validation library
            error = "Email validation not implemented";
            return true; // Skip validation for now
        }
        return true;
    });
}

void ValidationMiddleware::validate_url_format(const std::string& property_name) {
    add_event_validator("validate_url_" + property_name, [property_name](const EventContext& context, std::string& error) {
        if (context.has_property(property_name)) {
            // Simplified URL validation
            error = "URL validation not implemented";
            return true; // Skip validation for now
        }
        return true;
    });
}

PipelineResult ValidationMiddleware::process_event(EventContext& context) {
    context.increment_middleware_count();
    
    for (const auto& [name, validator] : event_validators_) {
        std::string error;
        if (!validator(context, error)) {
            context.mark_invalid(name + ": " + error);
            return PipelineResult::SKIP;
        }
    }
    
    return PipelineResult::CONTINUE;
}

PipelineResult ValidationMiddleware::process_log(LogContext& context) {
    context.increment_middleware_count();
    
    for (const auto& [name, validator] : log_validators_) {
        std::string error;
        if (!validator(context, error)) {
            context.mark_invalid(name + ": " + error);
            return PipelineResult::SKIP;
        }
    }
    
    return PipelineResult::CONTINUE;
}

//=============================================================================
// EnrichmentMiddleware Implementation
//=============================================================================

EnrichmentMiddleware::EnrichmentMiddleware() {
    // Add default enrichers
    add_timestamp();
    add_hostname();
}

void EnrichmentMiddleware::add_event_enricher(const std::string& name, EventEnricher enricher) {
    event_enrichers_.emplace_back(name, std::move(enricher));
}

void EnrichmentMiddleware::add_log_enricher(const std::string& name, LogEnricher enricher) {
    log_enrichers_.emplace_back(name, std::move(enricher));
}

void EnrichmentMiddleware::add_timestamp() {
    add_event_enricher("timestamp", [](EventContext& context) {
        context.add_property("_processed_at", static_cast<int64_t>(Utils::now_milliseconds()));
    });
    
    add_log_enricher("timestamp", [](LogContext& context) {
        context.add_data("_processed_at", static_cast<int64_t>(Utils::now_milliseconds()));
    });
}

void EnrichmentMiddleware::add_session_id(const std::string& session_id) {
    add_event_enricher("session_id", [session_id](EventContext& context) {
        context.add_property("session_id", session_id);
    });
    
    add_log_enricher("session_id", [session_id](LogContext& context) {
        context.add_data("session_id", session_id);
    });
}

void EnrichmentMiddleware::add_user_agent(const std::string& user_agent) {
    add_event_enricher("user_agent", [user_agent](EventContext& context) {
        context.add_property("user_agent", user_agent);
    });
    
    add_log_enricher("user_agent", [user_agent](LogContext& context) {
        context.add_data("user_agent", user_agent);
    });
}

void EnrichmentMiddleware::add_ip_address(const std::string& ip) {
    add_event_enricher("ip_address", [ip](EventContext& context) {
        context.add_property("ip_address", ip);
    });
    
    add_log_enricher("ip_address", [ip](LogContext& context) {
        context.add_data("ip_address", ip);
    });
}

void EnrichmentMiddleware::add_hostname() {
    std::string hostname = Utils::get_hostname();
    
    add_event_enricher("hostname", [hostname](EventContext& context) {
        context.add_property("_hostname", hostname);
    });
    
    add_log_enricher("hostname", [hostname](LogContext& context) {
        context.add_data("_hostname", hostname);
    });
}

void EnrichmentMiddleware::add_request_id() {
    add_event_enricher("request_id", [](EventContext& context) {
        context.add_property("request_id", static_cast<int64_t>(Utils::generate_context_id()));
    });
    
    add_log_enricher("request_id", [](LogContext& context) {
        context.add_data("request_id", static_cast<int64_t>(Utils::generate_context_id()));
    });
}

void EnrichmentMiddleware::add_correlation_id() {
    add_event_enricher("correlation_id", [](EventContext& context) {
        context.add_property("correlation_id", static_cast<int64_t>(Utils::generate_context_id()));
    });
    
    add_log_enricher("correlation_id", [](LogContext& context) {
        context.add_data("correlation_id", static_cast<int64_t>(Utils::generate_context_id()));
    });
}

PipelineResult EnrichmentMiddleware::process_event(EventContext& context) {
    context.increment_middleware_count();
    
    for (const auto& [name, enricher] : event_enrichers_) {
        try {
            enricher(context);
        } catch (const std::exception& e) {
            // Log error but continue processing
            context.set_metadata("enrichment_error_" + name, std::string(e.what()));
        }
    }
    
    return PipelineResult::CONTINUE;
}

PipelineResult EnrichmentMiddleware::process_log(LogContext& context) {
    context.increment_middleware_count();
    
    for (const auto& [name, enricher] : log_enrichers_) {
        try {
            enricher(context);
        } catch (const std::exception& e) {
            // Log error but continue processing
            context.set_metadata("enrichment_error_" + name, std::string(e.what()));
        }
    }
    
    return PipelineResult::CONTINUE;
}

//=============================================================================
// FilterMiddleware Implementation
//=============================================================================

FilterMiddleware::FilterMiddleware() {
    // No default filters
}

void FilterMiddleware::add_event_filter(const std::string& name, EventFilter filter) {
    event_filters_.emplace_back(name, std::move(filter));
}

void FilterMiddleware::add_log_filter(const std::string& name, LogFilter filter) {
    log_filters_.emplace_back(name, std::move(filter));
}

void FilterMiddleware::filter_by_user_id(const std::vector<std::string>& allowed_users) {
    add_event_filter("user_id_filter", [allowed_users](const EventContext& context) {
        return std::find(allowed_users.begin(), allowed_users.end(), 
                        context.get_user_id()) != allowed_users.end();
    });
}

void FilterMiddleware::filter_by_event_name(const std::vector<std::string>& allowed_events) {
    add_event_filter("event_name_filter", [allowed_events](const EventContext& context) {
        return std::find(allowed_events.begin(), allowed_events.end(), 
                        context.get_event_name()) != allowed_events.end();
    });
}

void FilterMiddleware::filter_by_log_level(LogLevel min_level) {
    add_log_filter("log_level_filter", [min_level](const LogContext& context) {
        return context.get_level() <= min_level; // Lower numeric values = higher priority
    });
}

void FilterMiddleware::filter_by_service(const std::vector<std::string>& allowed_services) {
    add_log_filter("service_filter", [allowed_services](const LogContext& context) {
        return std::find(allowed_services.begin(), allowed_services.end(),
                        context.get_service()) != allowed_services.end();
    });
}

void FilterMiddleware::sample_events(double sample_rate) {
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    thread_local std::uniform_real_distribution<double> dis(0.0, 1.0);
    
    add_event_filter("event_sampling", [sample_rate](const EventContext& /*context*/) {
        return dis(gen) <= sample_rate;
    });
}

void FilterMiddleware::sample_logs(double sample_rate) {
    thread_local std::random_device rd;
    thread_local std::mt19937 gen(rd());
    thread_local std::uniform_real_distribution<double> dis(0.0, 1.0);
    
    add_log_filter("log_sampling", [sample_rate](const LogContext& context) {
        // Always preserve critical logs
        if (context.get_level() <= LogLevel::ERROR) {
            return true;
        }
        return dis(gen) <= sample_rate;
    });
}

PipelineResult FilterMiddleware::process_event(EventContext& context) {
    context.increment_middleware_count();
    
    for (const auto& [name, filter] : event_filters_) {
        try {
            if (!filter(context)) {
                context.set_metadata("filtered_by", name);
                return PipelineResult::SKIP;
            }
        } catch (const std::exception& e) {
            context.set_metadata("filter_error_" + name, std::string(e.what()));
            // Continue processing on filter error
        }
    }
    
    return PipelineResult::CONTINUE;
}

PipelineResult FilterMiddleware::process_log(LogContext& context) {
    context.increment_middleware_count();
    
    for (const auto& [name, filter] : log_filters_) {
        try {
            if (!filter(context)) {
                context.set_metadata("filtered_by", name);
                return PipelineResult::SKIP;
            }
        } catch (const std::exception& e) {
            context.set_metadata("filter_error_" + name, std::string(e.what()));
            // Continue processing on filter error
        }
    }
    
    return PipelineResult::CONTINUE;
}

//=============================================================================
// TransformationMiddleware Implementation
//=============================================================================

TransformationMiddleware::TransformationMiddleware() {
    // No default transformers
}

void TransformationMiddleware::add_event_transformer(const std::string& name, EventTransformer transformer) {
    event_transformers_.emplace_back(name, std::move(transformer));
}

void TransformationMiddleware::add_log_transformer(const std::string& name, LogTransformer transformer) {
    log_transformers_.emplace_back(name, std::move(transformer));
}

void TransformationMiddleware::normalize_user_id() {
    add_event_transformer("normalize_user_id", [](EventContext& context) {
        std::string normalized = context.get_user_id();
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
        context.set_user_id(normalized);
    });
}

void TransformationMiddleware::normalize_event_names() {
    add_event_transformer("normalize_event_names", [](EventContext& context) {
        std::string normalized = context.get_event_name();
        // Convert to lowercase and replace spaces with underscores
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
        std::replace(normalized.begin(), normalized.end(), ' ', '_');
        context.set_event_name(normalized);
    });
}

void TransformationMiddleware::anonymize_property(const std::string& property_name) {
    add_event_transformer("anonymize_" + property_name, [property_name](EventContext& context) {
        if (context.has_property(property_name)) {
            context.add_property(property_name, std::string("[ANONYMIZED]"));
        }
    });
    
    add_log_transformer("anonymize_" + property_name, [property_name](LogContext& context) {
        if (context.has_data(property_name)) {
            context.add_data(property_name, std::string("[ANONYMIZED]"));
        }
    });
}

void TransformationMiddleware::hash_property(const std::string& property_name) {
    add_event_transformer("hash_" + property_name, [property_name](EventContext& context) {
        if (context.has_property(property_name)) {
            // Simple hash transformation (in practice would use proper hashing)
            context.add_property(property_name + "_hash", std::string("[HASHED]"));
        }
    });
    
    add_log_transformer("hash_" + property_name, [property_name](LogContext& context) {
        if (context.has_data(property_name)) {
            context.add_data(property_name + "_hash", std::string("[HASHED]"));
        }
    });
}

void TransformationMiddleware::truncate_strings(size_t max_length) {
    add_event_transformer("truncate_strings", [max_length](EventContext& context) {
        // Truncate event name if too long
        std::string event_name = context.get_event_name();
        if (event_name.length() > max_length) {
            context.set_event_name(event_name.substr(0, max_length));
        }
    });
    
    add_log_transformer("truncate_strings", [max_length](LogContext& context) {
        // Truncate message if too long
        std::string message = context.get_message();
        if (message.length() > max_length) {
            context.set_message(message.substr(0, max_length));
        }
    });
}

void TransformationMiddleware::convert_currency(const std::string& target_currency) {
    add_event_transformer("convert_currency_" + target_currency, [target_currency](EventContext& context) {
        // Currency conversion placeholder - would implement with real exchange rates
        if (context.has_property("currency")) {
            context.add_property("original_currency", context.get_properties().at("currency"));
            context.add_property("currency", target_currency);
        }
    });
}

void TransformationMiddleware::format_timestamps() {
    add_event_transformer("format_timestamps", [](EventContext& context) {
        // Ensure timestamp is properly formatted
        context.set_timestamp(Utils::now_milliseconds());
    });
    
    add_log_transformer("format_timestamps", [](LogContext& context) {
        // Ensure timestamp is properly formatted
        context.set_timestamp(Utils::now_milliseconds());
    });
}

PipelineResult TransformationMiddleware::process_event(EventContext& context) {
    context.increment_middleware_count();
    
    for (const auto& [name, transformer] : event_transformers_) {
        try {
            transformer(context);
        } catch (const std::exception& e) {
            context.set_metadata("transformation_error_" + name, std::string(e.what()));
            // Continue processing on transformation error
        }
    }
    
    return PipelineResult::CONTINUE;
}

PipelineResult TransformationMiddleware::process_log(LogContext& context) {
    context.increment_middleware_count();
    
    for (const auto& [name, transformer] : log_transformers_) {
        try {
            transformer(context);
        } catch (const std::exception& e) {
            context.set_metadata("transformation_error_" + name, std::string(e.what()));
            // Continue processing on transformation error
        }
    }
    
    return PipelineResult::CONTINUE;
}

//=============================================================================
// PipelineProcessor Implementation
//=============================================================================

class PipelineProcessor::Impl {
public:
    std::vector<std::shared_ptr<Middleware>> middleware_;
    PipelineStats stats_;
    mutable std::shared_mutex stats_mutex_;
    std::chrono::milliseconds timeout_{5000};
    bool parallel_processing_ = false;
    bool initialized_ = false;
    
    void add_middleware(std::shared_ptr<Middleware> middleware) {
        middleware_.push_back(middleware);
        
        // Sort by priority (lower values = higher priority)
        std::sort(middleware_.begin(), middleware_.end(),
            [](const std::shared_ptr<Middleware>& a, const std::shared_ptr<Middleware>& b) {
                return a->get_priority() < b->get_priority();
            });
    }
    
    PipelineResult process_event_sync(EventContext& context) {
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            for (auto& middleware : middleware_) {
                auto result = middleware->process_event(context);
                
                if (result != PipelineResult::CONTINUE) {
                    update_stats_for_event(result, start_time);
                    return result;
                }
            }
            
            update_stats_for_event(PipelineResult::CONTINUE, start_time);
            return PipelineResult::CONTINUE;
            
        } catch (const std::exception& e) {
            context.mark_invalid(std::string("Pipeline error: ") + e.what());
            update_stats_for_event(PipelineResult::ABORT, start_time);
            return PipelineResult::ABORT;
        }
    }
    
    PipelineResult process_log_sync(LogContext& context) {
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            for (auto& middleware : middleware_) {
                auto result = middleware->process_log(context);
                
                if (result != PipelineResult::CONTINUE) {
                    update_stats_for_log(result, start_time);
                    return result;
                }
            }
            
            update_stats_for_log(PipelineResult::CONTINUE, start_time);
            return PipelineResult::CONTINUE;
            
        } catch (const std::exception& e) {
            context.mark_invalid(std::string("Pipeline error: ") + e.what());
            update_stats_for_log(PipelineResult::ABORT, start_time);
            return PipelineResult::ABORT;
        }
    }
    
private:
    void update_stats_for_event(PipelineResult result, std::chrono::steady_clock::time_point start_time) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        
        std::unique_lock lock(stats_mutex_);
        stats_.total_processing_time_ms.fetch_add(duration.count());
        
        switch (result) {
            case PipelineResult::CONTINUE:
                stats_.events_processed.fetch_add(1);
                break;
            case PipelineResult::SKIP:
                stats_.events_skipped.fetch_add(1);
                break;
            case PipelineResult::ABORT:
            case PipelineResult::RETRY:
                stats_.events_failed.fetch_add(1);
                break;
        }
    }
    
    void update_stats_for_log(PipelineResult result, std::chrono::steady_clock::time_point start_time) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        
        std::unique_lock lock(stats_mutex_);
        stats_.total_processing_time_ms.fetch_add(duration.count());
        
        switch (result) {
            case PipelineResult::CONTINUE:
                stats_.logs_processed.fetch_add(1);
                break;
            case PipelineResult::SKIP:
                stats_.logs_skipped.fetch_add(1);
                break;
            case PipelineResult::ABORT:
            case PipelineResult::RETRY:
                stats_.logs_failed.fetch_add(1);
                break;
        }
    }
};

PipelineProcessor::PipelineProcessor() : pimpl_(std::make_unique<Impl>()) {
    // Add default middleware
    add_middleware(std::make_shared<ValidationMiddleware>());
    add_middleware(std::make_shared<EnrichmentMiddleware>());
}

PipelineProcessor::~PipelineProcessor() = default;

PipelineProcessor::PipelineProcessor(PipelineProcessor&&) noexcept = default;
PipelineProcessor& PipelineProcessor::operator=(PipelineProcessor&&) noexcept = default;

void PipelineProcessor::add_middleware(std::shared_ptr<Middleware> middleware) {
    pimpl_->add_middleware(middleware);
}

void PipelineProcessor::remove_middleware(const std::string& name) {
    auto& middleware_vec = pimpl_->middleware_;
    middleware_vec.erase(
        std::remove_if(middleware_vec.begin(), middleware_vec.end(),
            [&name](const std::shared_ptr<Middleware>& middleware) {
                return middleware->get_name() == name;
            }),
        middleware_vec.end());
}

void PipelineProcessor::clear_middleware() {
    pimpl_->middleware_.clear();
}

std::shared_ptr<ValidationMiddleware> PipelineProcessor::add_validation() {
    auto middleware = std::make_shared<ValidationMiddleware>();
    add_middleware(middleware);
    return middleware;
}

std::shared_ptr<EnrichmentMiddleware> PipelineProcessor::add_enrichment() {
    auto middleware = std::make_shared<EnrichmentMiddleware>();
    add_middleware(middleware);
    return middleware;
}

std::shared_ptr<FilterMiddleware> PipelineProcessor::add_filtering() {
    auto middleware = std::make_shared<FilterMiddleware>();
    add_middleware(middleware);
    return middleware;
}

std::shared_ptr<TransformationMiddleware> PipelineProcessor::add_transformation() {
    auto middleware = std::make_shared<TransformationMiddleware>();
    add_middleware(middleware);
    return middleware;
}

void PipelineProcessor::set_timeout(std::chrono::milliseconds timeout) {
    pimpl_->timeout_ = timeout;
}

void PipelineProcessor::enable_parallel_processing(bool enable) {
    pimpl_->parallel_processing_ = enable;
}

PipelineResult PipelineProcessor::process_event(EventContext& context) {
    return pimpl_->process_event_sync(context);
}

PipelineResult PipelineProcessor::process_log(LogContext& context) {
    return pimpl_->process_log_sync(context);
}

std::future<PipelineResult> PipelineProcessor::process_event_async(std::unique_ptr<EventContext> context) {
    return std::async(std::launch::async, [this, context = std::move(context)]() {
        return this->process_event(*context);
    });
}

std::future<PipelineResult> PipelineProcessor::process_log_async(std::unique_ptr<LogContext> context) {
    return std::async(std::launch::async, [this, context = std::move(context)]() {
        return this->process_log(*context);
    });
}

PipelineStats::Snapshot PipelineProcessor::get_stats() const {
    std::shared_lock lock(pimpl_->stats_mutex_);
    return pimpl_->stats_.snapshot();
}

void PipelineProcessor::reset_stats() {
    std::unique_lock lock(pimpl_->stats_mutex_);
    pimpl_->stats_.reset();
}

void PipelineProcessor::set_max_processing_time(std::chrono::milliseconds max_time) {
    pimpl_->timeout_ = max_time;
}

std::vector<std::string> PipelineProcessor::get_middleware_names() const {
    std::vector<std::string> names;
    for (const auto& middleware : pimpl_->middleware_) {
        names.push_back(middleware->get_name());
    }
    return names;
}

std::shared_ptr<Middleware> PipelineProcessor::get_middleware(const std::string& name) const {
    for (const auto& middleware : pimpl_->middleware_) {
        if (middleware->get_name() == name) {
            return middleware;
        }
    }
    return nullptr;
}

void PipelineProcessor::initialize() {
    pimpl_->initialized_ = true;
    for (auto& middleware : pimpl_->middleware_) {
        middleware->initialize();
    }
}

void PipelineProcessor::shutdown() {
    for (auto& middleware : pimpl_->middleware_) {
        middleware->shutdown();
    }
    pimpl_->initialized_ = false;
}

bool PipelineProcessor::is_initialized() const {
    return pimpl_->initialized_;
}

//=============================================================================
// PipelineStats Implementation
//=============================================================================

void PipelineStats::reset() {
    events_processed.store(0);
    events_skipped.store(0);
    events_failed.store(0);
    logs_processed.store(0);
    logs_skipped.store(0);
    logs_failed.store(0);
    total_processing_time_ms.store(0);
}

PipelineStats::Snapshot PipelineStats::snapshot() const {
    Snapshot s;
    s.events_processed = events_processed.load();
    s.events_skipped = events_skipped.load();
    s.events_failed = events_failed.load();
    s.logs_processed = logs_processed.load();
    s.logs_skipped = logs_skipped.load();
    s.logs_failed = logs_failed.load();
    s.total_processing_time_ms = total_processing_time_ms.load();
    return s;
}

} // namespace usercanal