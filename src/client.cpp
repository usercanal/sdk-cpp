// src/client.cpp
// Implementation of main client integrating all Phase 2 components

#include "usercanal/client.hpp"
#include "usercanal/utils.hpp"
#include <algorithm>
#include <sstream>

namespace usercanal {

// ClientStats implementation
void ClientStats::reset() {
    events_submitted = 0;
    events_sent = 0;
    events_failed = 0;
    events_dropped = 0;
    logs_submitted = 0;
    logs_sent = 0;
    logs_failed = 0;
    logs_dropped = 0;
    bytes_sent = 0;
    network_errors = 0;
    reconnections = 0;
    batches_sent = 0;
    batches_failed = 0;
    flush_operations = 0;
    total_processing_time_ms = 0;
    last_successful_send_ms = 0;
}

double ClientStats::get_event_success_rate() const {
    uint64_t total = events_sent.load() + events_failed.load();
    return total > 0 ? static_cast<double>(events_sent.load()) / total : 1.0;
}

double ClientStats::get_log_success_rate() const {
    uint64_t total = logs_sent.load() + logs_failed.load();
    return total > 0 ? static_cast<double>(logs_sent.load()) / total : 1.0;
}

double ClientStats::get_overall_success_rate() const {
    uint64_t total_sent = events_sent.load() + logs_sent.load();
    uint64_t total_failed = events_failed.load() + logs_failed.load();
    uint64_t total = total_sent + total_failed;
    return total > 0 ? static_cast<double>(total_sent) / total : 1.0;
}

uint64_t ClientStats::get_total_items() const {
    return events_submitted.load() + logs_submitted.load();
}

// ClientImpl implementation
class ClientImpl {
public:
    ClientImpl(const Config& config) 
        : config_(config), state_(ClientState::UNINITIALIZED),
          batch_manager_(config.batch(), config.api_key()),
          network_client_(config) {
        
        // Set up batch callbacks
        batch_manager_.set_send_callback([this](const std::vector<uint8_t>& data) {
            send_batch_data(data);
        });
        
        batch_manager_.set_error_callback([this](const Error& error) {
            handle_error(error);
        });
    }

    ~ClientImpl() {
        if (state_ == ClientState::RUNNING) {
            shutdown(std::chrono::milliseconds(5000));
        }
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != ClientState::UNINITIALIZED) {
            throw Errors::client_already_initialized();
        }
        
        state_ = ClientState::INITIALIZING;
        
        try {
            // Initialize network client
            network_client_.initialize();
            
            // Start batch manager
            batch_manager_.start();
            
            state_ = ClientState::RUNNING;
            
            if (event_handler_) {
                event_handler_->on_client_initialized();
            }
            
        } catch (const Error& e) {
            state_ = ClientState::FAILED;
            throw;
        }
    }

    void shutdown(std::chrono::milliseconds /*timeout*/) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ == ClientState::SHUTDOWN || state_ == ClientState::UNINITIALIZED) {
            return;
        }
        
        state_ = ClientState::SHUTTING_DOWN;
        
        try {
            // Flush remaining data
            batch_manager_.flush();
            
            // Stop batch manager
            batch_manager_.stop();
            
            // Shutdown network client
            network_client_.shutdown();
            
            state_ = ClientState::SHUTDOWN;
            
            if (event_handler_) {
                event_handler_->on_client_shutdown();
            }
            
        } catch (const Error& e) {
            state_ = ClientState::FAILED;
            handle_error(e);
        }
    }

    bool is_initialized() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == ClientState::RUNNING;
    }

    ClientState get_state() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

    void track(const std::string& user_id, const std::string& event_name, const Properties& properties) {
        if (!is_initialized()) {
            throw Errors::client_not_initialized();
        }

        if (!Utils::is_valid_user_id(user_id)) {
            throw Errors::invalid_user_id(user_id);
        }

        if (event_name.empty()) {
            throw Errors::invalid_event_name(event_name);
        }

        stats_.events_submitted++;

        // Create event item
        Properties event_props = properties;
        event_props[PropertyNames::EVENT_TYPE] = event_name;
        event_props[PropertyNames::TIMESTAMP] = int64_t(Utils::now_milliseconds());

        auto event_item = batch_manager_.create_event_item(EventType::TRACK, user_id, event_props);
        
        if (!batch_manager_.submit_event(std::move(event_item))) {
            stats_.events_dropped++;
            if (event_handler_) {
                event_handler_->on_queue_full("events");
            }
            throw Errors::queue_full(get_event_queue_size(), config_.batch().max_queue_size);
        }

        if (event_handler_) {
            event_handler_->on_event_sent(user_id, event_name);
        }
    }

    void identify(const std::string& user_id, const Properties& traits) {
        if (!is_initialized()) {
            throw Errors::client_not_initialized();
        }

        if (!Utils::is_valid_user_id(user_id)) {
            throw Errors::invalid_user_id(user_id);
        }

        stats_.events_submitted++;

        Properties event_props = traits;
        event_props[PropertyNames::TIMESTAMP] = int64_t(Utils::now_milliseconds());

        auto event_item = batch_manager_.create_event_item(EventType::IDENTIFY, user_id, event_props);
        
        if (!batch_manager_.submit_event(std::move(event_item))) {
            stats_.events_dropped++;
            throw Errors::queue_full(get_event_queue_size(), config_.batch().max_queue_size);
        }
    }

    void group(const std::string& user_id, const std::string& group_id, const Properties& properties) {
        if (!is_initialized()) {
            throw Errors::client_not_initialized();
        }

        if (!Utils::is_valid_user_id(user_id)) {
            throw Errors::invalid_user_id(user_id);
        }

        if (group_id.empty()) {
            throw ValidationError(ErrorCode::INVALID_USER_ID, "group_id", "Group ID cannot be empty");
        }

        stats_.events_submitted++;

        Properties event_props = properties;
        event_props[PropertyNames::GROUP_ID] = group_id;
        event_props[PropertyNames::TIMESTAMP] = int64_t(Utils::now_milliseconds());

        auto event_item = batch_manager_.create_event_item(EventType::GROUP, user_id, event_props);
        
        if (!batch_manager_.submit_event(std::move(event_item))) {
            stats_.events_dropped++;
            throw Errors::queue_full(get_event_queue_size(), config_.batch().max_queue_size);
        }
    }

    void track_revenue(const std::string& user_id, const std::string& order_id, 
                      double amount, Currency currency, const Properties& properties) {
        if (!is_initialized()) {
            throw Errors::client_not_initialized();
        }

        if (!Utils::is_valid_user_id(user_id)) {
            throw Errors::invalid_user_id(user_id);
        }

        if (order_id.empty()) {
            throw ValidationError(ErrorCode::INVALID_EVENT_NAME, "order_id", "Order ID cannot be empty");
        }

        if (amount < 0) {
            throw ValidationError(ErrorCode::INVALID_PROPERTIES, "amount", "Amount cannot be negative");
        }

        stats_.events_submitted++;

        Properties event_props = properties;
        event_props[PropertyNames::ORDER_ID] = order_id;
        event_props[PropertyNames::AMOUNT] = amount;
        event_props[PropertyNames::CURRENCY] = Utils::currency_to_string(currency);
        event_props[PropertyNames::TIMESTAMP] = int64_t(Utils::now_milliseconds());

        auto event_item = batch_manager_.create_event_item(EventType::TRACK, user_id, event_props);
        
        if (!batch_manager_.submit_event(std::move(event_item))) {
            stats_.events_dropped++;
            throw Errors::queue_full(get_event_queue_size(), config_.batch().max_queue_size);
        }
    }

    void log(LogLevel level, const std::string& service, const std::string& message, 
             const Properties& data, ContextId context_id) {
        if (!is_initialized()) {
            throw Errors::client_not_initialized();
        }

        if (service.empty()) {
            throw Errors::invalid_service_name(service);
        }

        if (message.empty()) {
            throw Errors::empty_message();
        }

        stats_.logs_submitted++;

        Properties log_data = data;
        log_data[PropertyNames::TIMESTAMP] = int64_t(Utils::now_milliseconds());
        if (context_id != 0) {
            log_data[PropertyNames::CORRELATION_ID] = int64_t(context_id);
        }

        auto log_item = batch_manager_.create_log_item(level, service, message, log_data);
        
        if (!batch_manager_.submit_log(std::move(log_item))) {
            stats_.logs_dropped++;
            if (event_handler_) {
                event_handler_->on_queue_full("logs");
            }
            throw Errors::queue_full(get_log_queue_size(), config_.batch().max_queue_size);
        }

        if (event_handler_) {
            event_handler_->on_log_sent(level, service, message);
        }
    }

    std::future<void> track_async(const std::string& user_id, const std::string& event_name, const Properties& properties) {
        return std::async(std::launch::async, [this, user_id, event_name, properties]() {
            track(user_id, event_name, properties);
        });
    }

    std::future<void> log_async(LogLevel level, const std::string& service, const std::string& message, 
                               const Properties& data, ContextId context_id) {
        return std::async(std::launch::async, [this, level, service, message, data, context_id]() {
            log(level, service, message, data, context_id);
        });
    }

    void flush() {
        if (!is_initialized()) {
            throw Errors::client_not_initialized();
        }

        stats_.flush_operations++;
        batch_manager_.flush();

        if (event_handler_) {
            event_handler_->on_flush_triggered("manual");
        }
    }

    std::future<void> flush_async() {
        return std::async(std::launch::async, [this]() {
            flush();
        });
    }

    bool test_connection() {
        if (!is_initialized()) {
            return false;
        }

        return network_client_.test_connection();
    }

    void force_reconnect() {
        if (is_initialized()) {
            network_client_.force_reconnect();
            stats_.reconnections++;
        }
    }

    bool is_connected() const {
        return network_client_.is_initialized();
    }

    size_t get_event_queue_size() const {
        return batch_manager_.get_event_queue_size();
    }

    size_t get_log_queue_size() const {
        return batch_manager_.get_log_queue_size();
    }

    const Config& get_config() const { return config_; }

    ClientStats::Snapshot get_stats() const {
        auto combined_stats = stats_.snapshot();
        
        // Add batch statistics
        auto batch_stats = batch_manager_.get_combined_stats();
        combined_stats.batches_sent = batch_stats.batches_sent;
        combined_stats.batches_failed = batch_stats.batches_failed;
        
        // Add network statistics
        auto network_stats = network_client_.get_stats();
        combined_stats.bytes_sent = network_stats.bytes_sent;
        combined_stats.network_errors = network_stats.connections_failed + network_stats.timeouts;
        
        return combined_stats;
    }

    void reset_stats() {
        stats_.reset();
        batch_manager_.reset_stats();
        network_client_.reset_stats();
    }

    void set_event_handler(std::shared_ptr<ClientEventHandler> handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        event_handler_ = std::move(handler);
    }

    void set_error_callback(Client::ErrorCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        error_callback_ = std::move(callback);
    }

    Client::HealthStatus get_health_status() const {
        Client::HealthStatus status;
        status.last_check = std::chrono::system_clock::now();
        
        // Check overall state
        bool state_healthy = (state_ == ClientState::RUNNING);
        
        // Check network health
        bool network_healthy = network_client_.is_initialized();
        
        // Check batch processing health
        bool batch_healthy = batch_manager_.is_running() && 
                            !batch_manager_.is_event_queue_full() && 
                            !batch_manager_.is_log_queue_full();
        
        status.network_healthy = network_healthy;
        status.batch_processing_healthy = batch_healthy;
        status.overall_healthy = state_healthy && network_healthy && batch_healthy;
        
        // Collect issues
        if (!state_healthy) {
            status.issues.push_back("Client not in running state");
        }
        if (!network_healthy) {
            status.issues.push_back("Network connection unhealthy");
        }
        if (!batch_healthy) {
            status.issues.push_back("Batch processing issues or queues full");
        }
        
        return status;
    }

private:
    void send_batch_data(const std::vector<uint8_t>& data) {
        try {
            network_client_.send_data(data);
            stats_.bytes_sent += data.size();
            stats_.last_successful_send_ms = Utils::now_milliseconds();
        } catch (const NetworkError& e) {
            stats_.network_errors++;
            handle_error(e);
        }
    }

    void handle_error(const Error& error) {
        if (error_callback_) {
            error_callback_(error);
        }
        if (event_handler_) {
            event_handler_->on_client_error(error);
        }
    }

private:
    Config config_;
    ClientState state_;
    ClientStats stats_;
    BatchManager batch_manager_;
    NetworkClient network_client_;
    
    std::shared_ptr<ClientEventHandler> event_handler_;
    Client::ErrorCallback error_callback_;
    
    mutable std::mutex mutex_;
};

// Client implementation
Client::Client(const Config& config) : pimpl_(std::make_unique<ClientImpl>(config)) {}

Client::Client(const std::string& api_key) : Client(Config::production(api_key)) {}

Client::~Client() = default;
Client::Client(Client&&) noexcept = default;
Client& Client::operator=(Client&&) noexcept = default;

void Client::initialize() { pimpl_->initialize(); }
void Client::shutdown(std::chrono::milliseconds timeout) { pimpl_->shutdown(timeout); }
bool Client::is_initialized() const { return pimpl_->is_initialized(); }
ClientState Client::get_state() const { return pimpl_->get_state(); }

// Event tracking methods
void Client::track(const std::string& user_id, const std::string& event_name, const Properties& properties) {
    pimpl_->track(user_id, event_name, properties);
}

void Client::identify(const std::string& user_id, const Properties& traits) {
    pimpl_->identify(user_id, traits);
}

void Client::group(const std::string& user_id, const std::string& group_id, const Properties& properties) {
    pimpl_->group(user_id, group_id, properties);
}

void Client::alias(const std::string& user_id, const std::string& previous_id) {
    Properties props{{PropertyNames::USER_ID, previous_id}};
    track(user_id, "alias", props);
}

void Client::track_revenue(const std::string& user_id, const std::string& order_id, 
                          double amount, Currency currency, const Properties& properties) {
    pimpl_->track_revenue(user_id, order_id, amount, currency, properties);
}

// Convenience event methods
void Client::track_user_signup(const std::string& user_id, const Properties& properties) {
    track(user_id, EventNames::USER_SIGNED_UP, properties);
}

void Client::track_user_login(const std::string& user_id, const Properties& properties) {
    track(user_id, EventNames::USER_LOGGED_IN, properties);
}

void Client::track_feature_used(const std::string& user_id, const std::string& feature_name, const Properties& properties) {
    Properties props = properties;
    props["feature_name"] = feature_name;
    track(user_id, EventNames::FEATURE_USED, props);
}

void Client::track_page_view(const std::string& user_id, const std::string& page, const Properties& properties) {
    Properties props = properties;
    props["page"] = page;
    track(user_id, EventNames::PAGE_VIEWED, props);
}

// Logging methods
void Client::log_emergency(const std::string& service, const std::string& message, const Properties& data) {
    pimpl_->log(LogLevel::EMERGENCY, service, message, data, 0);
}

void Client::log_alert(const std::string& service, const std::string& message, const Properties& data) {
    pimpl_->log(LogLevel::ALERT, service, message, data, 0);
}

void Client::log_critical(const std::string& service, const std::string& message, const Properties& data) {
    pimpl_->log(LogLevel::CRITICAL, service, message, data, 0);
}

void Client::log_error(const std::string& service, const std::string& message, const Properties& data) {
    pimpl_->log(LogLevel::ERROR, service, message, data, 0);
}

void Client::log_warning(const std::string& service, const std::string& message, const Properties& data) {
    pimpl_->log(LogLevel::WARNING, service, message, data, 0);
}

void Client::log_notice(const std::string& service, const std::string& message, const Properties& data) {
    pimpl_->log(LogLevel::NOTICE, service, message, data, 0);
}

void Client::log_info(const std::string& service, const std::string& message, const Properties& data) {
    pimpl_->log(LogLevel::INFO, service, message, data, 0);
}

void Client::log_debug(const std::string& service, const std::string& message, const Properties& data) {
    pimpl_->log(LogLevel::DEBUG, service, message, data, 0);
}

void Client::log_trace(const std::string& service, const std::string& message, const Properties& data) {
    pimpl_->log(LogLevel::TRACE, service, message, data, 0);
}

void Client::log(LogLevel level, const std::string& service, const std::string& message, 
                 const Properties& data, ContextId context_id) {
    pimpl_->log(level, service, message, data, context_id);
}

// Async methods
std::future<void> Client::track_async(const std::string& user_id, const std::string& event_name, const Properties& properties) {
    return pimpl_->track_async(user_id, event_name, properties);
}

std::future<void> Client::log_async(LogLevel level, const std::string& service, const std::string& message, 
                                   const Properties& data, ContextId context_id) {
    return pimpl_->log_async(level, service, message, data, context_id);
}

// Batch management
void Client::flush() { pimpl_->flush(); }
std::future<void> Client::flush_async() { return pimpl_->flush_async(); }
void Client::flush_events() { flush(); }  // Combined flush for now
void Client::flush_logs() { flush(); }    // Combined flush for now

// Queue status
size_t Client::get_event_queue_size() const { return pimpl_->get_event_queue_size(); }
size_t Client::get_log_queue_size() const { return pimpl_->get_log_queue_size(); }
bool Client::is_event_queue_full() const { return get_event_queue_size() >= get_config().batch().max_queue_size; }
bool Client::is_log_queue_full() const { return get_log_queue_size() >= get_config().batch().max_queue_size; }

// Connection management
bool Client::test_connection() { return pimpl_->test_connection(); }
void Client::force_reconnect() { pimpl_->force_reconnect(); }
bool Client::is_connected() const { return pimpl_->is_connected(); }

// Configuration and stats
const Config& Client::get_config() const { return pimpl_->get_config(); }
void Client::update_config(const Config& /*new_config*/) {
    // Would need to implement config updates - complex for Phase 2
    throw std::runtime_error("Config updates not yet implemented");
}

ClientStats::Snapshot Client::get_stats() const { return pimpl_->get_stats(); }
void Client::reset_stats() { pimpl_->reset_stats(); }

// Event handling
void Client::set_event_handler(std::shared_ptr<ClientEventHandler> handler) {
    pimpl_->set_event_handler(std::move(handler));
}

void Client::remove_event_handler() {
    pimpl_->set_event_handler(nullptr);
}

void Client::set_error_callback(ErrorCallback callback) {
    pimpl_->set_error_callback(std::move(callback));
}

void Client::remove_error_callback() {
    pimpl_->set_error_callback(nullptr);
}

// Health checking
Client::HealthStatus Client::get_health_status() const {
    return pimpl_->get_health_status();
}

bool Client::is_healthy() const {
    return get_health_status().overall_healthy;
}

// ClientFactory implementation
namespace ClientFactory {

std::unique_ptr<Client> create_auto(const std::string& api_key) {
    return std::make_unique<Client>(Config::production(api_key));
}

std::unique_ptr<Client> create_from_env() {
    auto config = EnvConfig::from_environment();
    if (!config) {
        throw ConfigError(ErrorCode::INVALID_CONFIG, "environment", "No valid configuration found in environment variables");
    }
    return std::make_unique<Client>(*config);
}

std::unique_ptr<Client> create_development(const std::string& api_key, const std::string& endpoint) {
    auto config = Config::development(api_key);
    if (endpoint != "localhost:50000") {
        config.set_endpoint(endpoint);
    }
    return std::make_unique<Client>(config);
}

std::unique_ptr<Client> create_production(const std::string& api_key) {
    return std::make_unique<Client>(Config::production(api_key));
}

std::unique_ptr<Client> create_high_throughput(const std::string& api_key) {
    return std::make_unique<Client>(Config::high_throughput(api_key));
}

std::unique_ptr<Client> create_low_latency(const std::string& api_key) {
    return std::make_unique<Client>(Config::low_latency(api_key));
}

} // namespace ClientFactory

// Global client implementation
namespace GlobalClient {

static std::unique_ptr<Client> global_client;
static std::mutex global_mutex;

void initialize(const Config& config) {
    std::lock_guard<std::mutex> lock(global_mutex);
    global_client = std::make_unique<Client>(config);
    global_client->initialize();
}

void initialize(const std::string& api_key) {
    initialize(Config::production(api_key));
}

Client& instance() {
    std::lock_guard<std::mutex> lock(global_mutex);
    if (!global_client) {
        throw Errors::client_not_initialized();
    }
    return *global_client;
}

bool is_initialized() {
    std::lock_guard<std::mutex> lock(global_mutex);
    return global_client && global_client->is_initialized();
}

void shutdown() {
    std::lock_guard<std::mutex> lock(global_mutex);
    if (global_client) {
        global_client->shutdown();
        global_client.reset();
    }
}

void track(const std::string& user_id, const std::string& event_name, const Properties& properties) {
    instance().track(user_id, event_name, properties);
}

void log_info(const std::string& service, const std::string& message, const Properties& data) {
    instance().log_info(service, message, data);
}

void log_error(const std::string& service, const std::string& message, const Properties& data) {
    instance().log_error(service, message, data);
}

void flush() {
    instance().flush();
}

} // namespace GlobalClient

// Utility classes
EventTracker::EventTracker(Client& client, const std::string& user_id) 
    : client_(client), user_id_(user_id) {}

void EventTracker::track(const std::string& event_name, const Properties& properties) {
    client_.track(user_id_, event_name, properties);
}

void EventTracker::track_signup(const Properties& properties) {
    client_.track_user_signup(user_id_, properties);
}

void EventTracker::track_login(const Properties& properties) {
    client_.track_user_login(user_id_, properties);
}

void EventTracker::track_feature_use(const std::string& feature_name, const Properties& properties) {
    client_.track_feature_used(user_id_, feature_name, properties);
}

void EventTracker::track_revenue(const std::string& order_id, double amount, Currency currency, const Properties& properties) {
    client_.track_revenue(user_id_, order_id, amount, currency, properties);
}

void EventTracker::set_user_properties(const Properties& traits) {
    client_.identify(user_id_, traits);
}

void EventTracker::set_group(const std::string& group_id, const Properties& properties) {
    client_.group(user_id_, group_id, properties);
}

Logger::Logger(Client& client, const std::string& service_name) 
    : client_(client), service_name_(service_name) {}

void Logger::emergency(const std::string& message, const Properties& data) {
    client_.log_emergency(service_name_, message, data);
}

void Logger::alert(const std::string& message, const Properties& data) {
    client_.log_alert(service_name_, message, data);
}

void Logger::critical(const std::string& message, const Properties& data) {
    client_.log_critical(service_name_, message, data);
}

void Logger::error(const std::string& message, const Properties& data) {
    client_.log_error(service_name_, message, data);
}

void Logger::warning(const std::string& message, const Properties& data) {
    client_.log_warning(service_name_, message, data);
}

void Logger::notice(const std::string& message, const Properties& data) {
    client_.log_notice(service_name_, message, data);
}

void Logger::info(const std::string& message, const Properties& data) {
    client_.log_info(service_name_, message, data);
}

void Logger::debug(const std::string& message, const Properties& data) {
    client_.log_debug(service_name_, message, data);
}

void Logger::trace(const std::string& message, const Properties& data) {
    client_.log_trace(service_name_, message, data);
}

void Logger::log(LogLevel level, const std::string& message, const Properties& data, ContextId context_id) {
    ContextId actual_context = context_id != 0 ? context_id : context_id_;
    client_.log(level, service_name_, message, data, actual_context);
}

void Logger::set_context(ContextId context_id) {
    context_id_ = context_id;
}

void Logger::clear_context() {
    context_id_ = 0;
}

ContextId Logger::get_context() const {
    return context_id_;
}

ScopedFlush::ScopedFlush(Client& client) : client_(client) {}

ScopedFlush::~ScopedFlush() {
    if (!flushed_) {
        flush_now();
    }
}

void ScopedFlush::flush_now() {
    if (!flushed_) {
        client_.flush();
        flushed_ = true;
    }
}

} // namespace usercanal