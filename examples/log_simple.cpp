// log_simple.cpp
// Simple Logging Example
// Shows basic structured logging with UserCanal C++ SDK

#include <iostream>
#include "usercanal/usercanal.hpp"

using namespace usercanal;

int main() {
    std::cout << "📝 UserCanal C++ SDK - Simple Logging\n" << std::endl;
    
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
        
        // Simple info logging
        client.log_info("my-app", "Application started", {
            {"version", std::string("1.0.0")},
            {"port", int64_t(8080)}
        });
        std::cout << "📝 Logged: Application started (INFO)" << std::endl;
        
        // Error logging with context
        client.log_error("auth-service", "Login failed", {
            {"user_id", std::string("user_123")},
            {"reason", std::string("invalid_password")},
            {"attempt_count", int64_t(3)}
        });
        std::cout << "📝 Logged: Login failed (ERROR)" << std::endl;
        
        // Debug logging
        client.log_debug("api-service", "Processing request", {
            {"request_id", std::string("req_456")},
            {"endpoint", std::string("/api/users")},
            {"duration_ms", int64_t(45)}
        });
        std::cout << "📝 Logged: Request processed (DEBUG)" << std::endl;
        
        // Warning logging
        client.log_warning("payment-service", "Rate limit approaching", {
            {"current_rate", int64_t(95)},
            {"limit", int64_t(100)},
            {"client_ip", std::string("192.168.1.100")}
        });
        std::cout << "📝 Logged: Rate limit warning (WARNING)" << std::endl;
        
        // Ensure all logs are sent
        client.flush();
        std::cout << "\n✅ All logs sent to UserCanal!" << std::endl;
        
        client.shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}