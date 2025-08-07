// basic_foundation.cpp
// Getting Started with UserCanal C++ SDK
// Your first example showing basic setup and usage

#include <iostream>
#include "usercanal/usercanal.hpp"

using namespace usercanal;

int main() {
    std::cout << "🚀 UserCanal C++ SDK - Getting Started\n" << std::endl;
    
    // Replace with your API key from https://app.usercanal.com/settings/api-keys
    std::string api_key = "your-api-key-here";
    
    if (api_key == "your-api-key-here") {
        std::cout << "⚠️  Please set your UserCanal API key in the source code" << std::endl;
        std::cout << "   Get your API key from: https://app.usercanal.com/settings/api-keys" << std::endl;
        return 1;
    }
    
    try {
        // Initialize UserCanal client
        Client client(api_key);
        client.initialize();
        
        std::cout << "✅ UserCanal SDK initialized successfully\n" << std::endl;
        
        // Track your first event
        Properties props;
        props["message"] = std::string("My first UserCanal event!");
        props["timestamp"] = static_cast<int64_t>(Utils::now_milliseconds());
        client.track("user_123", "hello_world", props);
        std::cout << "📊 Tracked: hello_world event" << std::endl;
        
        // Send your first log
        Properties log_props;
        log_props["sdk_version"] = std::string("1.0.0");
        log_props["language"] = std::string("cpp");
        client.log_info("my-app", "UserCanal SDK is working!", log_props);
        std::cout << "📝 Logged: SDK working message" << std::endl;
        
        // Make sure data is sent
        client.flush();
        std::cout << "\n✅ Data sent to UserCanal!" << std::endl;
        std::cout << "Check your UserCanal dashboard to see your first event and log." << std::endl;
        
        client.shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}