// log_severities.cpp
// Log Severity Levels Example
// Shows all available log levels with UserCanal C++ SDK

#include <iostream>
#include "usercanal/usercanal.hpp"

using namespace usercanal;

int main() {
    std::cout << "🔍 UserCanal C++ SDK - Log Severity Levels\n" << std::endl;
    
    // Replace with your API key
    std::string api_key = "your-api-key-here";
    
    if (api_key == "your-api-key-here") {
        std::cout << "⚠️  Please set your UserCanal API key in the source code" << std::endl;
        std::cout << "   Get your API key from: https://app.usercanal.com/settings/api-keys" << std::endl;
        return 1;
    }
    
    try {
        // Initialize client
        Client client(api_key);
        client.initialize();
        
        std::cout << "✅ UserCanal SDK initialized\n" << std::endl;
        
        // EMERGENCY - System is unusable
        client.log(LogLevel::EMERGENCY, "system", "Database cluster down", {
            {"affected_users", int64_t(50000)},
            {"estimated_downtime", std::string("30 minutes")}
        });
        std::cout << "🚨 Logged: EMERGENCY - Database cluster down" << std::endl;
        
        // ALERT - Action must be taken immediately  
        client.log(LogLevel::ALERT, "security", "Multiple failed login attempts", {
            {"user_id", std::string("user_123")},
            {"attempts", int64_t(10)},
            {"source_ip", std::string("192.168.1.100")}
        });
        std::cout << "🔴 Logged: ALERT - Security breach attempt" << std::endl;
        
        // CRITICAL - Critical conditions
        client.log(LogLevel::CRITICAL, "payment-service", "Payment processor unavailable", {
            {"processor", std::string("stripe")},
            {"error_rate", 100.0},
            {"revenue_impact", 15000.0}
        });
        std::cout << "💥 Logged: CRITICAL - Payment system down" << std::endl;
        
        // ERROR - Error conditions
        client.log_error("user-service", "Failed to create user account", {
            {"user_email", std::string("user@example.com")},
            {"error_code", std::string("DB_CONNECTION_FAILED")},
            {"retry_count", int64_t(3)}
        });
        std::cout << "❌ Logged: ERROR - User creation failed" << std::endl;
        
        // WARNING - Warning conditions
        client.log_warning("api-gateway", "High response time detected", {
            {"endpoint", std::string("/api/users")},
            {"avg_response_ms", int64_t(2500)},
            {"threshold_ms", int64_t(1000)}
        });
        std::cout << "⚠️  Logged: WARNING - Performance degradation" << std::endl;
        
        // NOTICE - Normal but significant condition
        client.log(LogLevel::NOTICE, "auth-service", "New user registration", {
            {"user_id", std::string("user_456")},
            {"signup_method", std::string("oauth")},
            {"referrer", std::string("google")}
        });
        std::cout << "🔔 Logged: NOTICE - User registered" << std::endl;
        
        // INFO - Informational messages
        client.log_info("scheduler", "Daily backup completed", {
            {"backup_size_mb", int64_t(2048)},
            {"duration_seconds", int64_t(180)},
            {"status", std::string("success")}
        });
        std::cout << "ℹ️  Logged: INFO - Backup completed" << std::endl;
        
        // DEBUG - Debug-level messages
        client.log_debug("cache-service", "Cache hit ratio statistics", {
            {"hit_ratio", 0.85},
            {"total_requests", int64_t(10000)},
            {"cache_hits", int64_t(8500)},
            {"cache_misses", int64_t(1500)}
        });
        std::cout << "🐛 Logged: DEBUG - Cache statistics" << std::endl;
        
        // TRACE - Very detailed debug information
        client.log(LogLevel::TRACE, "request-handler", "HTTP request details", {
            {"method", std::string("POST")},
            {"path", std::string("/api/users/123/profile")},
            {"headers", std::string("Content-Type: application/json")},
            {"body_size", int64_t(256)},
            {"user_agent", std::string("UserCanal-Client/1.0")}
        });
        std::cout << "🔬 Logged: TRACE - Request details" << std::endl;
        
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