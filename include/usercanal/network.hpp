// include/usercanal/network.hpp
// Purpose: Network layer for TCP client and connection management
// Optimized for high-performance logging and event delivery

#pragma once

#include "types.hpp"
#include "config.hpp"
#include "errors.hpp"
#include <memory>
#include <functional>
#include <future>
#include <vector>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>

namespace usercanal {

// Forward declarations
class NetworkClient;
class Connection;
class ConnectionPool;

// Network callback types
using ConnectCallback = std::function<void(const std::error_code& error)>;
using SendCallback = std::function<void(const std::error_code& error, size_t bytes_sent)>;
using ReceiveCallback = std::function<void(const std::error_code& error, const std::vector<uint8_t>& data)>;

// Connection state enumeration
enum class ConnectionState {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    RECONNECTING = 3,
    FAILED = 4,
    SHUTDOWN = 5
};

// Network statistics
struct NetworkStats {
    std::atomic<uint64_t> connections_created{0};
    std::atomic<uint64_t> connections_failed{0};
    std::atomic<uint64_t> bytes_sent{0};
    std::atomic<uint64_t> bytes_received{0};
    std::atomic<uint64_t> send_operations{0};
    std::atomic<uint64_t> receive_operations{0};
    std::atomic<uint64_t> reconnections{0};
    std::atomic<uint64_t> timeouts{0};
    
    // Reset all statistics
    void reset();
    
    // Get success rate for connections
    double connection_success_rate() const;
    
    // Get average bytes per operation
    double average_send_size() const;
    
    // Create a copyable snapshot of the stats
    struct Snapshot {
        uint64_t connections_created = 0;
        uint64_t connections_failed = 0;
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        uint64_t send_operations = 0;
        uint64_t receive_operations = 0;
        uint64_t reconnections = 0;
        uint64_t timeouts = 0;
        
        double connection_success_rate() const {
            uint64_t total = connections_created;
            uint64_t failed = connections_failed;
            return total > 0 ? static_cast<double>(total - failed) / total : 1.0;
        }
        
        double average_send_size() const {
            uint64_t operations = send_operations;
            uint64_t bytes = bytes_sent;
            return operations > 0 ? static_cast<double>(bytes) / operations : 0.0;
        }
    };
    
    Snapshot snapshot() const {
        Snapshot s;
        s.connections_created = connections_created.load();
        s.connections_failed = connections_failed.load();
        s.bytes_sent = bytes_sent.load();
        s.bytes_received = bytes_received.load();
        s.send_operations = send_operations.load();
        s.receive_operations = receive_operations.load();
        s.reconnections = reconnections.load();
        s.timeouts = timeouts.load();
        return s;
    }
};

// Single TCP connection with automatic reconnection
class Connection {
public:
    Connection(const std::string& host, uint16_t port, const NetworkConfig& config);
    ~Connection();
    
    // Non-copyable, movable
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) noexcept;
    Connection& operator=(Connection&&) noexcept;
    
    // Connection management
    std::future<void> connect_async();
    std::future<void> disconnect_async();
    bool is_connected() const;
    ConnectionState get_state() const;
    
    // Data transmission
    std::future<size_t> send_async(const std::vector<uint8_t>& data);
    std::future<std::vector<uint8_t>> receive_async(size_t max_bytes = 4096);
    
    // Synchronous operations with timeout
    void connect(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    void disconnect();
    size_t send(const std::vector<uint8_t>& data, std::chrono::milliseconds timeout = std::chrono::milliseconds(10000));
    std::vector<uint8_t> receive(size_t max_bytes = 4096, std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    
    // Connection info
    std::string get_endpoint() const;
    std::chrono::system_clock::time_point get_connect_time() const;
    std::chrono::milliseconds get_uptime() const;
    
    // Statistics
    NetworkStats::Snapshot get_stats() const;
    void reset_stats();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

// Connection pool for managing multiple connections
class ConnectionPool {
public:
    explicit ConnectionPool(const NetworkConfig& config, size_t pool_size = 5);
    ~ConnectionPool();
    
    // Non-copyable, movable
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&) noexcept;
    ConnectionPool& operator=(ConnectionPool&&) noexcept;
    
    // Pool management
    void initialize();
    void shutdown();
    bool is_initialized() const;
    
    // Get connection from pool (blocking with timeout)
    std::shared_ptr<Connection> acquire_connection(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));
    void release_connection(std::shared_ptr<Connection> connection);
    
    // Pool statistics
    size_t total_connections() const;
    size_t available_connections() const;
    size_t active_connections() const;
    NetworkStats get_aggregated_stats() const;
    
    // Health checking
    void health_check();
    void remove_failed_connections();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

// High-level network client with automatic retry and error handling
class NetworkClient {
public:
    explicit NetworkClient(const Config& config);
    ~NetworkClient();
    
    // Non-copyable, movable
    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;
    NetworkClient(NetworkClient&&) noexcept;
    NetworkClient& operator=(NetworkClient&&) noexcept;
    
    // Client lifecycle
    void initialize();
    void shutdown();
    bool is_initialized() const;
    
    // High-level data transmission with retry
    std::future<void> send_data_async(const std::vector<uint8_t>& data);
    void send_data(const std::vector<uint8_t>& data);
    
    // Batch operations
    std::future<void> send_batch_async(const std::vector<std::vector<uint8_t>>& batch);
    void send_batch(const std::vector<std::vector<uint8_t>>& batch);
    
    // Connection management
    void force_reconnect();
    bool test_connection();
    
    // Statistics and monitoring
    NetworkStats::Snapshot get_stats() const;
    void reset_stats();
    
    // Configuration
    const Config& get_config() const;
    void update_config(const Config& config);

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

// Network utilities and helpers
namespace NetworkUtils {

// DNS resolution
std::vector<std::string> resolve_hostname(const std::string& hostname);
bool is_hostname_reachable(const std::string& hostname, uint16_t port, std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

// Network interface information
std::vector<std::string> get_local_ip_addresses();
std::string get_default_gateway();
bool is_network_available();

// Socket utilities
int create_tcp_socket();
bool set_socket_keepalive(int socket, bool enable, int idle_time = 60, int interval = 30, int probes = 3);
bool set_socket_timeout(int socket, std::chrono::milliseconds timeout);
bool set_socket_buffer_size(int socket, size_t send_buffer_size, size_t receive_buffer_size);
bool set_socket_nodelay(int socket, bool enable = true);

// Connection testing
struct ConnectionTest {
    bool success;
    std::chrono::milliseconds latency;
    std::string error_message;
};

ConnectionTest test_tcp_connection(const std::string& host, uint16_t port, std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

// Network error handling
NetworkError create_network_error(const std::string& operation, int error_code);
NetworkError create_timeout_error(const std::string& operation, std::chrono::milliseconds timeout);
bool is_retryable_error(const NetworkError& error);

} // namespace NetworkUtils

// Network event callbacks for monitoring
class NetworkEventHandler {
public:
    virtual ~NetworkEventHandler() = default;
    
    // Connection events
    virtual void on_connection_established(const std::string& /*endpoint*/) {}
    virtual void on_connection_lost(const std::string& /*endpoint*/, const std::error_code& /*error*/) {}
    virtual void on_connection_failed(const std::string& /*endpoint*/, const std::error_code& /*error*/) {}
    virtual void on_reconnection_attempt(const std::string& /*endpoint*/, int /*attempt*/) {}
    
    // Data transmission events
    virtual void on_data_sent(const std::string& /*endpoint*/, size_t /*bytes*/) {}
    virtual void on_data_send_failed(const std::string& /*endpoint*/, const std::error_code& /*error*/) {}
    virtual void on_data_received(const std::string& /*endpoint*/, size_t /*bytes*/) {}
    
    // Error events
    virtual void on_network_error(const NetworkError& /*error*/) {}
    virtual void on_timeout(const std::string& /*operation*/, std::chrono::milliseconds /*timeout*/) {}
};

// Async network operation result
template<typename T>
class NetworkResult {
public:
    NetworkResult() : success_(false) {}
    NetworkResult(T result) : result_(std::move(result)), success_(true) {}
    NetworkResult(NetworkError error) : error_(std::move(error)), success_(false) {}
    
    bool is_success() const { return success_; }
    bool is_error() const { return !success_; }
    
    const T& get_result() const {
        if (!success_) {
            throw std::runtime_error("Cannot get result from failed operation");
        }
        return result_;
    }
    
    const NetworkError& get_error() const {
        if (success_) {
            throw std::runtime_error("Cannot get error from successful operation");
        }
        return error_;
    }
    
    // Convenience methods
    T value_or(const T& default_value) const {
        return success_ ? result_ : default_value;
    }

private:
    T result_;
    NetworkError error_;
    bool success_;
};

// Network configuration validator
class NetworkConfigValidator {
public:
    static std::vector<std::string> validate(const NetworkConfig& config);
    static bool is_valid(const NetworkConfig& config);
    static void validate_and_throw(const NetworkConfig& config);

private:
    static bool validate_endpoint(const std::string& endpoint);
    static bool validate_timeout(std::chrono::milliseconds timeout, const std::string& name);
    static bool validate_retry_config(int max_retries, std::chrono::milliseconds base_delay, double multiplier);
};

} // namespace usercanal