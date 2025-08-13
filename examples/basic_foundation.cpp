// basic_foundation.cpp
// Getting Started with UserCanal C++ SDK

#include <iostream>
#include "usercanal/usercanal.hpp"

using namespace usercanal;

int main() {
    std::cout << "🚀 UserCanal C++ SDK - Getting Started\n" << std::endl;

    // Use test API key
    std::string api_key = "000102030405060708090a0b0c0d0e0f";

    try {
        // Initialize UserCanal client
        Config config(api_key);
        config.set_endpoint("localhost:50000");  // Configure endpoint
        Client client(config);
        client.initialize();

        std::cout << "✅ UserCanal SDK initialized successfully\n" << std::endl;

        // Track a signup event using predefined constant
        Properties props;
        props["signup_method"] = std::string("email");
        props["referral_source"] = std::string("google");
        client.event("user_123", EventNames::USER_SIGNED_UP, props);

        // Send your first log  
        client.log_info("my-app", "Application started", {});

        // Make sure data is sent
        client.flush();
        std::cout << "✅ Data sent to UserCanal!" << std::endl;

        client.shutdown();

    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
