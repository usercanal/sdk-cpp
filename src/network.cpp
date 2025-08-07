// src/network.cpp
// Implementation of network layer with TCP client and connection management

#include "usercanal/network.hpp"
#include "usercanal/utils.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <cstring>
#include <algorithm>
#include <sstream>

#ifdef __APPLE__
#include <sys/types.h>
#include <sys/socket.h>
#endif

namespace usercanal {

// NetworkStats implementation
void NetworkStats::reset() {
    connections_created = 0;
    connections_failed = 0;
    bytes_sent = 0;
    bytes_received = 0;
    send_operations = 0;
    receive_operations = 0;
    reconnections = 0;
    timeouts = 0;
}

double NetworkStats::connection_success_rate() const {
    uint64_t total = connections_created.load();
    uint64_t failed = connections_failed.load();
    return total > 0 ? static_cast<double>(total - failed) / total : 1.0;
}

double NetworkStats::average_send_size() const {
    uint64_t operations = send_operations.load();
    uint64_t bytes = bytes_sent.load();
    return operations > 0 ? static_cast<double>(bytes) / operations : 0.0;
}

// Connection implementation
class Connection::Impl {
public:
    Impl(const std::string& host, uint16_t port, const NetworkConfig& config)
        : host_(host), port_(port), config_(config), socket_fd_(-1), 
          state_(ConnectionState::DISCONNECTED) {}

    ~Impl() {
        disconnect_internal();
    }

    std::future<void> connect_async() {
        return std::async(std::launch::async, [this]() {
            connect_internal();
        });
    }

    std::future<void> disconnect_async() {
        return std::async(std::launch::async, [this]() {
            disconnect_internal();
        });
    }

    void connect_internal() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ == ConnectionState::CONNECTED) {
            return;
        }
        
        state_ = ConnectionState::CONNECTING;
        stats_.connections_created++;
        
        try {
            // Create socket
            socket_fd_ = NetworkUtils::create_tcp_socket();
            if (socket_fd_ < 0) {
                throw NetworkUtils::create_network_error("create_socket", errno);
            }
            
            // Set socket options
            NetworkUtils::set_socket_timeout(socket_fd_, config_.connect_timeout);
            NetworkUtils::set_socket_nodelay(socket_fd_, true);
            
            if (config_.enable_keepalive) {
                NetworkUtils::set_socket_keepalive(socket_fd_, true, 
                    config_.keepalive_idle_time, config_.keepalive_interval, config_.keepalive_probes);
            }
            
            NetworkUtils::set_socket_buffer_size(socket_fd_, 
                config_.socket_buffer_size, config_.socket_buffer_size);
            
            // Resolve hostname and connect
            struct sockaddr_in server_addr{};
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(port_);
            
            if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0) {
                // Try to resolve hostname
                struct hostent* host_entry = gethostbyname(host_.c_str());
                if (!host_entry) {
                    throw NetworkUtils::create_network_error("dns_resolve", h_errno);
                }
                memcpy(&server_addr.sin_addr, host_entry->h_addr_list[0], host_entry->h_length);
            }
            
            // Perform connection
            if (::connect(socket_fd_, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
                if (errno == EINPROGRESS) {
                    // Non-blocking connect in progress
                    struct pollfd pfd{};
                    pfd.fd = socket_fd_;
                    pfd.events = POLLOUT;
                    
                    int poll_result = poll(&pfd, 1, config_.connect_timeout.count());
                    if (poll_result <= 0) {
                        throw NetworkUtils::create_timeout_error("connect", config_.connect_timeout);
                    }
                    
                    // Check for connection error
                    int socket_error;
                    socklen_t len = sizeof(socket_error);
                    if (getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &socket_error, &len) < 0 || socket_error != 0) {
                        throw NetworkUtils::create_network_error("connect", socket_error);
                    }
                } else {
                    throw NetworkUtils::create_network_error("connect", errno);
                }
            }
            
            connect_time_ = std::chrono::system_clock::now();
            state_ = ConnectionState::CONNECTED;
            
        } catch (const NetworkError& e) {
            stats_.connections_failed++;
            state_ = ConnectionState::FAILED;
            disconnect_internal();
            throw;
        }
    }
    
    void disconnect_internal() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
        
        state_ = ConnectionState::DISCONNECTED;
    }
    
    std::future<size_t> send_async(const std::vector<uint8_t>& data) {
        return std::async(std::launch::async, [this, data]() {
            return send_internal(data);
        });
    }
    
    size_t send_internal(const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != ConnectionState::CONNECTED) {
            throw Errors::send_failed("Connection not established");
        }
        
        stats_.send_operations++;
        size_t total_sent = 0;
        const uint8_t* buffer = data.data();
        size_t remaining = data.size();
        
        auto start_time = std::chrono::steady_clock::now();
        
        while (remaining > 0) {
            ssize_t sent = ::send(socket_fd_, buffer + total_sent, remaining, MSG_NOSIGNAL);
            
            if (sent < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Would block, wait for socket to be ready
                    struct pollfd pfd{};
                    pfd.fd = socket_fd_;
                    pfd.events = POLLOUT;
                    
                    int poll_result = poll(&pfd, 1, config_.send_timeout.count());
                    if (poll_result <= 0) {
                        stats_.timeouts++;
                        throw NetworkUtils::create_timeout_error("send", config_.send_timeout);
                    }
                    continue;
                } else if (errno == EPIPE || errno == ECONNRESET) {
                    state_ = ConnectionState::FAILED;
                    throw NetworkUtils::create_network_error("send", errno);
                } else {
                    throw NetworkUtils::create_network_error("send", errno);
                }
            } else if (sent == 0) {
                // Connection closed by peer
                state_ = ConnectionState::FAILED;
                throw Errors::send_failed("Connection closed by peer");
            }
            
            total_sent += sent;
            remaining -= sent;
            
            // Check for timeout
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (elapsed > config_.send_timeout) {
                stats_.timeouts++;
                throw NetworkUtils::create_timeout_error("send", config_.send_timeout);
            }
        }
        
        stats_.bytes_sent += total_sent;
        return total_sent;
    }
    
    std::future<std::vector<uint8_t>> receive_async(size_t max_bytes) {
        return std::async(std::launch::async, [this, max_bytes]() {
            return receive_internal(max_bytes);
        });
    }
    
    std::vector<uint8_t> receive_internal(size_t max_bytes) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != ConnectionState::CONNECTED) {
            throw NetworkError(ErrorCode::RECEIVE_FAILED, "receive", "Connection not established");
        }
        
        stats_.receive_operations++;
        std::vector<uint8_t> buffer(max_bytes);
        
        struct pollfd pfd{};
        pfd.fd = socket_fd_;
        pfd.events = POLLIN;
        
        int poll_result = poll(&pfd, 1, config_.receive_timeout.count());
        if (poll_result <= 0) {
            stats_.timeouts++;
            throw NetworkUtils::create_timeout_error("receive", config_.receive_timeout);
        }
        
        ssize_t received = recv(socket_fd_, buffer.data(), max_bytes, 0);
        
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available
                return std::vector<uint8_t>();
            } else {
                throw NetworkUtils::create_network_error("receive", errno);
            }
        } else if (received == 0) {
            // Connection closed by peer
            state_ = ConnectionState::FAILED;
            throw NetworkError(ErrorCode::RECEIVE_FAILED, "receive", "Connection closed by peer");
        }
        
        buffer.resize(received);
        stats_.bytes_received += received;
        return buffer;
    }
    
    bool is_connected() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == ConnectionState::CONNECTED;
    }
    
    ConnectionState get_state() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }
    
    std::string get_endpoint() const {
        return host_ + ":" + std::to_string(port_);
    }
    
    std::chrono::system_clock::time_point get_connect_time() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return connect_time_;
    }
    
    std::chrono::milliseconds get_uptime() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != ConnectionState::CONNECTED) {
            return std::chrono::milliseconds(0);
        }
        
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - connect_time_);
    }
    
    NetworkStats::Snapshot get_stats() const { return stats_.snapshot(); }
    void reset_stats() { stats_.reset(); }

private:
    std::string host_;
    uint16_t port_;
    NetworkConfig config_;
    int socket_fd_;
    ConnectionState state_;
    std::chrono::system_clock::time_point connect_time_;
    NetworkStats stats_;
    mutable std::mutex mutex_;
};

// Connection public interface
Connection::Connection(const std::string& host, uint16_t port, const NetworkConfig& config)
    : pimpl_(std::make_unique<Impl>(host, port, config)) {}

Connection::~Connection() = default;
Connection::Connection(Connection&&) noexcept = default;
Connection& Connection::operator=(Connection&&) noexcept = default;

std::future<void> Connection::connect_async() { return pimpl_->connect_async(); }
std::future<void> Connection::disconnect_async() { return pimpl_->disconnect_async(); }
bool Connection::is_connected() const { return pimpl_->is_connected(); }
ConnectionState Connection::get_state() const { return pimpl_->get_state(); }

std::future<size_t> Connection::send_async(const std::vector<uint8_t>& data) {
    return pimpl_->send_async(data);
}

std::future<std::vector<uint8_t>> Connection::receive_async(size_t max_bytes) {
    return pimpl_->receive_async(max_bytes);
}

void Connection::connect(std::chrono::milliseconds timeout) {
    auto future = pimpl_->connect_async();
    if (future.wait_for(timeout) == std::future_status::timeout) {
        throw NetworkUtils::create_timeout_error("connect", timeout);
    }
    future.get(); // May throw
}

void Connection::disconnect() {
    pimpl_->disconnect_async().get();
}

size_t Connection::send(const std::vector<uint8_t>& data, std::chrono::milliseconds timeout) {
    auto future = pimpl_->send_async(data);
    if (future.wait_for(timeout) == std::future_status::timeout) {
        throw NetworkUtils::create_timeout_error("send", timeout);
    }
    return future.get();
}

std::vector<uint8_t> Connection::receive(size_t max_bytes, std::chrono::milliseconds timeout) {
    auto future = pimpl_->receive_async(max_bytes);
    if (future.wait_for(timeout) == std::future_status::timeout) {
        throw NetworkUtils::create_timeout_error("receive", timeout);
    }
    return future.get();
}

std::string Connection::get_endpoint() const { return pimpl_->get_endpoint(); }
std::chrono::system_clock::time_point Connection::get_connect_time() const { return pimpl_->get_connect_time(); }
std::chrono::milliseconds Connection::get_uptime() const { return pimpl_->get_uptime(); }
NetworkStats::Snapshot Connection::get_stats() const { return pimpl_->get_stats(); }
void Connection::reset_stats() { pimpl_->reset_stats(); }

// NetworkUtils implementation
namespace NetworkUtils {

std::vector<std::string> resolve_hostname(const std::string& hostname) {
    std::vector<std::string> addresses;
    struct hostent* host_entry = gethostbyname(hostname.c_str());
    
    if (host_entry) {
        for (int i = 0; host_entry->h_addr_list[i] != nullptr; ++i) {
            char* addr = inet_ntoa(*reinterpret_cast<struct in_addr*>(host_entry->h_addr_list[i]));
            if (addr) {
                addresses.emplace_back(addr);
            }
        }
    }
    
    return addresses;
}

bool is_hostname_reachable(const std::string& hostname, uint16_t port, std::chrono::milliseconds timeout) {
    try {
        NetworkConfig test_config;
        test_config.connect_timeout = timeout;
        
        Connection test_conn(hostname, port, test_config);
        test_conn.connect(timeout);
        test_conn.disconnect();
        return true;
    } catch (const NetworkError&) {
        return false;
    }
}

int create_tcp_socket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }
    
    // Set socket to non-blocking mode
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }
    
    return sock;
}

bool set_socket_keepalive(int socket, bool enable, int idle_time, int interval, int probes) {
    (void)idle_time;  // Suppress unused parameter warning
    int keepalive = enable ? 1 : 0;
    if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0) {
        return false;
    }
    
    if (enable) {
#ifdef TCP_KEEPIDLE
        if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, &idle_time, sizeof(idle_time)) < 0) {
            return false;
        }
#endif
#ifdef TCP_KEEPINTVL
        if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) < 0) {
            return false;
        }
#endif
#ifdef TCP_KEEPCNT
        if (setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, &probes, sizeof(probes)) < 0) {
            return false;
        }
#endif
    }
    
    return true;
}

bool set_socket_timeout(int socket, std::chrono::milliseconds timeout) {
    struct timeval tv;
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;
    
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        return false;
    }
    
    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        return false;
    }
    
    return true;
}

bool set_socket_buffer_size(int socket, size_t send_buffer_size, size_t receive_buffer_size) {
    int send_size = static_cast<int>(send_buffer_size);
    int recv_size = static_cast<int>(receive_buffer_size);
    
    if (setsockopt(socket, SOL_SOCKET, SO_SNDBUF, &send_size, sizeof(send_size)) < 0) {
        return false;
    }
    
    if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &recv_size, sizeof(recv_size)) < 0) {
        return false;
    }
    
    return true;
}

bool set_socket_nodelay(int socket, bool enable) {
    int nodelay = enable ? 1 : 0;
    return setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay)) >= 0;
}

ConnectionTest test_tcp_connection(const std::string& host, uint16_t port, std::chrono::milliseconds timeout) {
    ConnectionTest result;
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        NetworkConfig test_config;
        test_config.connect_timeout = timeout;
        
        Connection test_conn(host, port, test_config);
        test_conn.connect(timeout);
        test_conn.disconnect();
        
        result.success = true;
        result.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        
    } catch (const NetworkError& e) {
        result.success = false;
        result.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        result.error_message = e.what();
    }
    
    return result;
}

NetworkError create_network_error(const std::string& operation, int error_code) {
    std::string message = std::strerror(error_code);
    return NetworkError(ErrorCode::NETWORK_UNREACHABLE, operation, message);
}

NetworkError create_timeout_error(const std::string& operation, std::chrono::milliseconds timeout) {
    std::string message = "Operation timed out after " + std::to_string(timeout.count()) + "ms";
    return NetworkError(ErrorCode::CONNECTION_TIMEOUT, operation, message);
}

bool is_retryable_error(const NetworkError& error) {
    ErrorCode code = error.code();
    return code == ErrorCode::CONNECTION_TIMEOUT ||
           code == ErrorCode::CONNECTION_FAILED ||
           code == ErrorCode::NETWORK_UNREACHABLE ||
           code == ErrorCode::CONNECTION_REFUSED;
}

} // namespace NetworkUtils

// NetworkConfigValidator implementation
std::vector<std::string> NetworkConfigValidator::validate(const NetworkConfig& config) {
    std::vector<std::string> errors;
    
    if (!validate_endpoint(config.endpoint)) {
        errors.push_back("Invalid endpoint format: " + config.endpoint);
    }
    
    if (!validate_timeout(config.connect_timeout, "connect_timeout")) {
        errors.push_back("Invalid connect timeout");
    }
    
    if (!validate_timeout(config.send_timeout, "send_timeout")) {
        errors.push_back("Invalid send timeout");
    }
    
    if (!validate_timeout(config.receive_timeout, "receive_timeout")) {
        errors.push_back("Invalid receive timeout");
    }
    
    if (!validate_retry_config(config.max_retries, config.retry_delay, config.backoff_multiplier)) {
        errors.push_back("Invalid retry configuration");
    }
    
    return errors;
}

bool NetworkConfigValidator::is_valid(const NetworkConfig& config) {
    return validate(config).empty();
}

void NetworkConfigValidator::validate_and_throw(const NetworkConfig& config) {
    auto errors = validate(config);
    if (!errors.empty()) {
        std::ostringstream oss;
        oss << "Network configuration validation failed:\n";
        for (const auto& error : errors) {
            oss << "  - " << error << "\n";
        }
        throw ConfigError(ErrorCode::INVALID_CONFIG, "network", oss.str());
    }
}

bool NetworkConfigValidator::validate_endpoint(const std::string& endpoint) {
    auto [host, port] = Utils::parse_endpoint(endpoint);
    return !host.empty() && port > 0 && Utils::is_valid_hostname(host) && Utils::is_valid_port(port);
}

bool NetworkConfigValidator::validate_timeout(std::chrono::milliseconds timeout, const std::string& name) {
    (void)name;  // Suppress unused parameter warning
    return timeout.count() >= 100 && timeout.count() <= 300000; // 100ms to 5 minutes
}

bool NetworkConfigValidator::validate_retry_config(int max_retries, std::chrono::milliseconds base_delay, double multiplier) {
    return max_retries >= 0 && max_retries <= 10 &&
           base_delay.count() >= 100 && base_delay.count() <= 60000 &&
           multiplier >= 1.0 && multiplier <= 10.0;
}

// NetworkClient implementation
class NetworkClient::Impl {
public:
    Impl(const Config& config) : config_(config), initialized_(false) {}
    
    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_) return;
        initialized_ = true;
        // Network initialization would go here
    }
    
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        initialized_ = false;
    }
    
    bool is_initialized() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return initialized_;
    }
    
    void send_data(const std::vector<uint8_t>& data) {
        (void)data; // Suppress unused warning
        // Implementation would send data over network
        stats_.bytes_sent += data.size();
        stats_.send_operations++;
    }
    
    bool test_connection() {
        // Simple test - would actually test network connection
        return true;
    }
    
    void force_reconnect() {
        stats_.reconnections++;
        // Would force reconnection logic here
    }
    
    NetworkStats::Snapshot get_stats() const { return stats_.snapshot(); }
    void reset_stats() { stats_.reset(); }

private:
    Config config_;
    bool initialized_;
    NetworkStats stats_;
    mutable std::mutex mutex_;
};

NetworkClient::NetworkClient(const Config& config) : pimpl_(std::make_unique<Impl>(config)) {}
NetworkClient::~NetworkClient() = default;
NetworkClient::NetworkClient(NetworkClient&&) noexcept = default;
NetworkClient& NetworkClient::operator=(NetworkClient&&) noexcept = default;

void NetworkClient::initialize() { pimpl_->initialize(); }
void NetworkClient::shutdown() { pimpl_->shutdown(); }
bool NetworkClient::is_initialized() const { return pimpl_->is_initialized(); }

void NetworkClient::send_data(const std::vector<uint8_t>& data) { pimpl_->send_data(data); }
bool NetworkClient::test_connection() { return pimpl_->test_connection(); }
void NetworkClient::force_reconnect() { pimpl_->force_reconnect(); }

NetworkStats::Snapshot NetworkClient::get_stats() const { return pimpl_->get_stats(); }
void NetworkClient::reset_stats() { pimpl_->reset_stats(); }

} // namespace usercanal