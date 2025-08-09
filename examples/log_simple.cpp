// log_simple.cpp
// Simple Logging Example
// Shows basic structured logging with UserCanal C++ SDK

#include <iostream>
#include <chrono>
#include <thread>
#include "usercanal/usercanal.hpp"

using namespace usercanal;

int main() {
    std::cout << "📝 UserCanal C++ SDK - Simple Logging\n" << std::endl;
    
    std::string api_key = "000102030405060708090a0b0c0d0e0f";
    
    try {
        // Initialize client
        Config config(api_key);
        config.set_endpoint("localhost:50001");  // Configure endpoint
        Client client(config);
        client.initialize();

        std::cout << "✅ UserCanal SDK initialized" << std::endl;
        
        // Send various log messages:
        // client.LogInfo(ctx, "my-app", "Application started", nil)
        client.log_info("my-app", "Application started", {});
        
        // client.LogError(ctx, "my-app", "Login failed", map[string]interface{}{
        //     "user_id": "123",
        //     "reason":  "invalid_password",
        // })
        client.log_error("my-app", "Login failed", {
            {"user_id", std::string("123")},
            {"reason", std::string("invalid_password")}
        });
        std::cout << "📝 [ERROR] Login failed (my-app)" << std::endl;
        
        // client.LogDebug(ctx, "my-app", "Processing request", map[string]interface{}{
        //     "request_id": "req_456",
        //     "duration":   "45ms",
        // })
        client.log_debug("my-app", "Processing request", {
            {"request_id", std::string("req_456")},
            {"duration", std::string("45ms")}
        });
        std::cout << "📝 [DEBUG] Processing request (my-app)" << std::endl;
        
        client.flush();
        std::cout << "✅ Logs sent to UserCanal!" << std::endl;
        
        // Clean shutdown
        client.shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}