// include/usercanal/client.hpp
// Purpose: Main client interface integrating network and batch systems
// This is the primary entry point for users of the C++ SDK

#pragma once

#include "types.hpp"
#include "config.hpp"
#include "errors.hpp"
#include "network.hpp"
#include "batch.hpp"
#include <memory>
#include <functional>
#include <future>
#include <chrono>
#include <atomic>

namespace usercanal {

// Forward declarations
class Client;
class ClientImpl;

// Client state enumeration
enum class ClientState : uint8_t {
    UNINITIALIZED = 0,
    INITIALIZING = 1,
    RUNNING = 2,
    SHUTTING_DOWN = 3,
    SHUTDOWN = 4,
    FAILED = 5
};

// Client statistics combining network and batch metrics
struct ClientStats {
    // Event statistics
    std::atomic<uint64_t> events_submitted{0};
    std::atomic<uint64_t> events_sent{0};
    std::atomic<uint64_t> events_failed{0};
    std::atomic<uint64_t> events_dropped{0};
    
    // Log statistics
    std::atomic<uint64_t> logs_submitted{0};
    std::atomic<uint64_t> logs_sent{0};
    std::atomic<uint64_t> logs_failed{0};
    std::atomic<uint64_t> logs_dropped{0};
    
    // Network statistics
    std::atomic<uint64_t> bytes_sent{0};
    std::atomic<uint64_t> network_errors{0};
    std::atomic<uint64_t> reconnections{0};
    
    // Batch statistics
    std::atomic<uint64_t> batches_sent{0};
    std::atomic<uint64_t> batches_failed{0};
    std::atomic<uint64_t> flush_operations{0};
    
    // Timing statistics
    std::atomic<uint64_t> total_processing_time_ms{0};
    std::atomic<uint64_t> last_successful_send_ms{0};
    
    void reset();
    double get_event_success_rate() const;
    double get_log_success_rate() const;
    double get_overall_success_rate() const;
    uint64_t get_total_items() const;
    
    // Create a copyable snapshot of the stats
    struct Snapshot {
        uint64_t events_submitted = 0;
        uint64_t events_sent = 0;
        uint64_t events_failed = 0;
        uint64_t events_dropped = 0;
        uint64_t logs_submitted = 0;
        uint64_t logs_sent = 0;
        uint64_t logs_failed = 0;
        uint64_t logs_dropped = 0;
        uint64_t bytes_sent = 0;
        uint64_t network_errors = 0;
        uint64_t reconnections = 0;
        uint64_t batches_sent = 0;
        uint64_t batches_failed = 0;
        uint64_t flush_operations = 0;
        uint64_t total_processing_time_ms = 0;
        uint64_t last_successful_send_ms = 0;
        
        double get_event_success_rate() const {
            uint64_t total = events_sent + events_failed;
            return total > 0 ? static_cast<double>(events_sent) / total : 1.0;
        }
        
        double get_log_success_rate() const {
            uint64_t total = logs_sent + logs_failed;
            return total > 0 ? static_cast<double>(logs_sent) / total : 1.0;
        }
        
        double get_overall_success_rate() const {
            uint64_t total_sent = events_sent + logs_sent;
            uint64_t total_failed = events_failed + logs_failed;
            uint64_t total = total_sent + total_failed;
            return total > 0 ? static_cast<double>(total_sent) / total : 1.0;
        }
        
        uint64_t get_total_items() const {
            return events_submitted + logs_submitted;
        }
    };
    
    Snapshot snapshot() const {
        Snapshot s;
        s.events_submitted = events_submitted.load();
        s.events_sent = events_sent.load();
        s.events_failed = events_failed.load();
        s.events_dropped = events_dropped.load();
        s.logs_submitted = logs_submitted.load();
        s.logs_sent = logs_sent.load();
        s.logs_failed = logs_failed.load();
        s.logs_dropped = logs_dropped.load();
        s.bytes_sent = bytes_sent.load();
        s.network_errors = network_errors.load();
        s.reconnections = reconnections.load();
        s.batches_sent = batches_sent.load();
        s.batches_failed = batches_failed.load();
        s.flush_operations = flush_operations.load();
        s.total_processing_time_ms = total_processing_time_ms.load();
        s.last_successful_send_ms = last_successful_send_ms.load();
        return s;
    }
};

// Event callback types for client monitoring
class ClientEventHandler {
public:
    virtual ~ClientEventHandler() = default;
    
    // Client lifecycle events
    virtual void on_client_initialized() {}
    virtual void on_client_shutdown() {}
    virtual void on_client_error(const Error& /*error*/) {}
    
    // Data processing events
    virtual void on_event_sent(const std::string& /*user_id*/, const std::string& /*event_name*/) {}
    virtual void on_log_sent(LogLevel /*level*/, const std::string& /*service*/, const std::string& /*message*/) {}
    virtual void on_batch_sent(BatchId /*batch_id*/, size_t /*items*/, size_t /*bytes*/) {}
    
    // Error events
    virtual void on_event_failed(const std::string& /*user_id*/, const std::string& /*event_name*/, const Error& /*error*/) {}
    virtual void on_log_failed(LogLevel /*level*/, const std::string& /*service*/, const Error& /*error*/) {}
    virtual void on_batch_failed(BatchId /*batch_id*/, const Error& /*error*/) {}
    
    // Queue events
    virtual void on_queue_full(const std::string& /*queue_type*/) {}
    virtual void on_flush_triggered(const std::string& /*reason*/) {}
};

// Main UserCanal Client
class Client {
public:
    // Construction with configuration
    explicit Client(const Config& config);
    explicit Client(const std::string& api_key); // Uses production preset
    
    // Destructor ensures proper cleanup
    ~Client();
    
    // Non-copyable, movable
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;
    
    // Client lifecycle
    void initialize();
    void shutdown(std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));
    bool is_initialized() const;
    ClientState get_state() const;
    
    // Event tracking methods
    void track(const std::string& user_id, const std::string& event_name, const Properties& properties = {});
    void identify(const std::string& user_id, const Properties& traits);
    void group(const std::string& user_id, const std::string& group_id, const Properties& properties = {});
    void alias(const std::string& user_id, const std::string& previous_id);
    
    // Revenue tracking
    void track_revenue(const std::string& user_id, const std::string& order_id, 
                      double amount, Currency currency = Currency::USD,
                      const Properties& properties = {});
    
    // Convenience event methods with predefined names
    void track_user_signup(const std::string& user_id, const Properties& properties = {});
    void track_user_login(const std::string& user_id, const Properties& properties = {});
    void track_feature_used(const std::string& user_id, const std::string& feature_name, const Properties& properties = {});
    void track_page_view(const std::string& user_id, const std::string& page, const Properties& properties = {});
    
    // Structured logging methods
    void log_emergency(const std::string& service, const std::string& message, const Properties& data = {});
    void log_alert(const std::string& service, const std::string& message, const Properties& data = {});
    void log_critical(const std::string& service, const std::string& message, const Properties& data = {});
    void log_error(const std::string& service, const std::string& message, const Properties& data = {});
    void log_warning(const std::string& service, const std::string& message, const Properties& data = {});
    void log_notice(const std::string& service, const std::string& message, const Properties& data = {});
    void log_info(const std::string& service, const std::string& message, const Properties& data = {});
    void log_debug(const std::string& service, const std::string& message, const Properties& data = {});
    void log_trace(const std::string& service, const std::string& message, const Properties& data = {});
    
    // Generic log method
    void log(LogLevel level, const std::string& service, const std::string& message, 
             const Properties& data = {}, ContextId context_id = 0);
    
    // Async versions (return futures for completion)
    std::future<void> track_async(const std::string& user_id, const std::string& event_name, const Properties& properties = {});
    std::future<void> log_async(LogLevel level, const std::string& service, const std::string& message, 
                               const Properties& data = {}, ContextId context_id = 0);
    
    // Batch management
    void flush();
    std::future<void> flush_async();
    void flush_events();
    void flush_logs();
    
    // Queue status
    size_t get_event_queue_size() const;
    size_t get_log_queue_size() const;
    bool is_event_queue_full() const;
    bool is_log_queue_full() const;
    
    // Connection management
    bool test_connection();
    void force_reconnect();
    bool is_connected() const;
    
    // Configuration management
    const Config& get_config() const;
    void update_config(const Config& new_config);
    
    // Statistics and monitoring
    ClientStats::Snapshot get_stats() const;
    void reset_stats();
    
    // Event handler registration
    void set_event_handler(std::shared_ptr<ClientEventHandler> handler);
    void remove_event_handler();
    
    // Error handling
    using ErrorCallback = std::function<void(const Error&)>;
    void set_error_callback(ErrorCallback callback);
    void remove_error_callback();
    
    // Health checking
    struct HealthStatus {
        bool overall_healthy;
        bool network_healthy;
        bool batch_processing_healthy;
        std::vector<std::string> issues;
        std::chrono::system_clock::time_point last_check;
    };
    
    HealthStatus get_health_status() const;
    bool is_healthy() const;

private:
    std::unique_ptr<ClientImpl> pimpl_;
};

// Client factory functions
namespace ClientFactory {

// Create client with automatic configuration detection
std::unique_ptr<Client> create_auto(const std::string& api_key);

// Create client from environment variables
std::unique_ptr<Client> create_from_env();

// Create client with preset configurations
std::unique_ptr<Client> create_development(const std::string& api_key, const std::string& endpoint = "localhost:50000");
std::unique_ptr<Client> create_production(const std::string& api_key);
std::unique_ptr<Client> create_high_throughput(const std::string& api_key);
std::unique_ptr<Client> create_low_latency(const std::string& api_key);

} // namespace ClientFactory

// Global client instance management (optional singleton pattern)
namespace GlobalClient {

// Initialize global client instance
void initialize(const Config& config);
void initialize(const std::string& api_key);

// Get global client instance
Client& instance();
bool is_initialized();

// Shutdown global client
void shutdown();

// Convenience methods using global instance
void track(const std::string& user_id, const std::string& event_name, const Properties& properties = {});
void log_info(const std::string& service, const std::string& message, const Properties& data = {});
void log_error(const std::string& service, const std::string& message, const Properties& data = {});
void flush();

} // namespace GlobalClient

// Client builder for advanced configuration
class ClientBuilder {
public:
    ClientBuilder();
    
    // Configuration methods
    ClientBuilder& with_api_key(const std::string& api_key);
    ClientBuilder& with_endpoint(const std::string& endpoint);
    ClientBuilder& with_batch_config(size_t event_batch_size, size_t log_batch_size,
                                    std::chrono::milliseconds event_interval,
                                    std::chrono::milliseconds log_interval);
    ClientBuilder& with_network_timeouts(std::chrono::milliseconds connect_timeout,
                                        std::chrono::milliseconds send_timeout);
    ClientBuilder& with_retry_config(int max_retries, std::chrono::milliseconds base_delay);
    ClientBuilder& with_performance_config(size_t thread_pool_size, size_t max_memory_usage);
    ClientBuilder& with_logging(SystemLogLevel level, bool console_output = true);
    ClientBuilder& with_event_handler(std::shared_ptr<ClientEventHandler> handler);
    ClientBuilder& with_error_callback(Client::ErrorCallback callback);
    
    // Build the client
    std::unique_ptr<Client> build();

private:
    Config config_;
    std::shared_ptr<ClientEventHandler> event_handler_;
    Client::ErrorCallback error_callback_;
    bool has_api_key_ = false;
};

// Utility classes for common use cases
class EventTracker {
public:
    EventTracker(Client& client, const std::string& user_id);
    
    void track(const std::string& event_name, const Properties& properties = {});
    void track_signup(const Properties& properties = {});
    void track_login(const Properties& properties = {});
    void track_feature_use(const std::string& feature_name, const Properties& properties = {});
    void track_revenue(const std::string& order_id, double amount, Currency currency = Currency::USD, const Properties& properties = {});
    
    void set_user_properties(const Properties& traits);
    void set_group(const std::string& group_id, const Properties& properties = {});

private:
    Client& client_;
    std::string user_id_;
};

class Logger {
public:
    Logger(Client& client, const std::string& service_name);
    
    void emergency(const std::string& message, const Properties& data = {});
    void alert(const std::string& message, const Properties& data = {});
    void critical(const std::string& message, const Properties& data = {});
    void error(const std::string& message, const Properties& data = {});
    void warning(const std::string& message, const Properties& data = {});
    void notice(const std::string& message, const Properties& data = {});
    void info(const std::string& message, const Properties& data = {});
    void debug(const std::string& message, const Properties& data = {});
    void trace(const std::string& message, const Properties& data = {});
    
    void log(LogLevel level, const std::string& message, const Properties& data = {}, ContextId context_id = 0);
    
    // Context management for distributed tracing
    void set_context(ContextId context_id);
    void clear_context();
    ContextId get_context() const;

private:
    Client& client_;
    std::string service_name_;
    ContextId context_id_ = 0;
};

// RAII helper for automatic flushing
class ScopedFlush {
public:
    explicit ScopedFlush(Client& client);
    ~ScopedFlush();
    
    void flush_now();

private:
    Client& client_;
    bool flushed_ = false;
};

} // namespace usercanal