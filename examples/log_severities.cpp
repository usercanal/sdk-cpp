// log_severities.cpp
// Log Severity Levels Example
// Shows all available log levels with UserCanal C++ SDK

#include <iostream>
#include "usercanal/usercanal.hpp"

using namespace usercanal;

int main() {
    std::cout << "🔍 UserCanal C++ SDK - Log Severity Levels\n" << std::endl;
    
    std::string api_key = "000102030405060708090a0b0c0d0e0f";
    
    try {
        // Initialize client
        Config config(api_key);
        config.set_endpoint("localhost:50001");  // Configure endpoint
        Client client(config);
        client.initialize();
        
        std::cout << "✅ UserCanal SDK initialized" << std::endl;
        
        // EMERGENCY - System is unusable (mapped to error)
        client.log_error("system", "Database cluster down", {
            {"affected_users", int64_t(50000)},
            {"estimated_downtime", std::string("30 minutes")},
            {"severity", std::string("EMERGENCY")}
        });
        
        // ALERT - Action must be taken immediately (mapped to error)
        client.log_error("security", "Multiple failed login attempts", {
            {"user_id", std::string("user_123")},
            {"attempts", int64_t(10)},
            {"source_ip", std::string("192.168.1.100")},
            {"severity", std::string("ALERT")}
        });
        
        // CRITICAL - Critical conditions (mapped to error)
        client.log_error("payment-service", "Payment processor unavailable", {
            {"processor", std::string("stripe")},
            {"error_rate", 100.0},
            {"revenue_impact", 15000.0},
            {"severity", std::string("CRITICAL")}
        });
        
        // ERROR - Error conditions
        client.log_error("user-service", "Failed to create user account", {
            {"user_email", std::string("user@example.com")},
            {"error_code", std::string("DB_CONNECTION_FAILED")},
            {"retry_count", int64_t(3)}
        });
        
        // WARNING - Warning conditions
        client.log_warning("api-gateway", "High response time detected", {
            {"endpoint", std::string("/api/users")},
            {"avg_response_ms", int64_t(2500)},
            {"threshold_ms", int64_t(1000)}
        });
        
        // NOTICE - Normal but significant condition (mapped to info)
        client.log_info("auth-service", "New user registration", {
            {"user_id", std::string("user_456")},
            {"email", std::string("newuser@example.com")},
            {"referral", std::string("google")},
            {"severity", std::string("NOTICE")}
        });
        
        // INFO - Informational messages
        client.log_info("scheduler", "Daily backup completed", {
            {"backup_size_mb", int64_t(2048)},
            {"duration_seconds", int64_t(180)},
            {"status", std::string("success")}
        });
        
        // DEBUG - Debug-level messages
        client.log_debug("cache-service", "Cache hit ratio statistics", {
            {"hit_ratio", 0.85},
            {"total_requests", int64_t(10000)},
            {"cache_hits", int64_t(8500)},
            {"cache_misses", int64_t(1500)}
        });
        
        // TRACE - Very detailed debug information (mapped to debug)
        client.log_debug("request-handler", "HTTP request details", {
            {"method", std::string("POST")},
            {"path", std::string("/api/users/123/profile")},
            {"headers", std::string("Content-Type: application/json")},
            {"body_size", int64_t(256)},
            {"user_agent", std::string("UserCanal-Client/1.0")},
            {"severity", std::string("TRACE")}
        });
        
        // Ensure all logs are sent
        client.flush();
        std::cout << "\n✅ All severity levels demonstrated and sent to UserCanal!" << std::endl;
        std::cout << "📊 Check your UserCanal dashboard to see the different log levels" << std::endl;
        
        client.shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}