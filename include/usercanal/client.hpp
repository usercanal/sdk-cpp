// include/usercanal/client.hpp
// Purpose: Simplified main client interface matching Go SDK scope
// This is the primary entry point for users of the C++ SDK

#pragma once

#include "types.hpp"
#include "config.hpp"
#include "errors.hpp"
#include "network.hpp"
#include "batch.hpp"
#include <memory>
#include <atomic>
#include <string>

namespace usercanal {

// Forward declaration
class ClientImpl;

// Forward declaration - using Stats from types.hpp
struct Stats;

// Main UserCanal Client (simplified to match Go SDK)
class Client {
public:
    // Construction
    explicit Client(const Config& config);
    explicit Client(const std::string& api_key);
    
    // Destructor
    ~Client();
    
    // Non-copyable, movable
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;
    
    // Lifecycle (simplified)
    void initialize();
    void shutdown();
    bool is_initialized() const;
    
    // Event tracking (matching Go SDK Event method)
    void event(const std::string& user_id, const std::string& event_name, const Properties& properties = {});
    
    // Revenue tracking (matching Go SDK EventRevenue method)
    void event_revenue(const std::string& user_id, const std::string& order_id, 
                      double amount, const std::string& currency = "USD",
                      const Properties& properties = {});
    
    // Structured logging (matching Go SDK LogInfo, LogError, etc.)
    void log_info(const std::string& service, const std::string& message, const Properties& data = {});
    void log_error(const std::string& service, const std::string& message, const Properties& data = {});
    void log_warning(const std::string& service, const std::string& message, const Properties& data = {});
    void log_debug(const std::string& service, const std::string& message, const Properties& data = {});
    
    // Explicit flush (matching Go SDK Flush method)
    void flush();
    
    // Statistics (matching Go SDK GetStats method)
    Stats get_stats() const;
    
    // Legacy methods for backward compatibility
    void track(const std::string& user_id, const std::string& event_name, const Properties& properties = {}) {
        event(user_id, event_name, properties);
    }
    
    void track_revenue(const std::string& user_id, const std::string& order_id, 
                      double amount, const std::string& currency = "USD",
                      const Properties& properties = {}) {
        event_revenue(user_id, order_id, amount, currency, properties);
    }

private:
    std::unique_ptr<ClientImpl> impl_;
};

} // namespace usercanal