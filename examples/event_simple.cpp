// event_simple.cpp
// Simple Event Tracking Example
// Shows basic event tracking with UserCanal C++ SDK

#include <iostream>
#include "usercanal/usercanal.hpp"

using namespace usercanal;

int main() {
    std::cout << "🚀 UserCanal C++ SDK - Simple Event Tracking\n" << std::endl;
    
    std::string api_key = "000102030405060708090a0b0c0d0e0f";
    
    try {
        // Initialize client
        Config config(api_key);
        config.set_endpoint("localhost:50000");  // Configure endpoint
        Client client(config);
        client.initialize();
        
        std::cout << "✅ UserCanal SDK initialized" << std::endl;
        
        // Track user signup
        Properties signup_props;
        signup_props["signup_method"] = std::string("email");
        signup_props["referral_source"] = std::string("google");
        client.event("user_123", "user_signup", signup_props);
        
        // Track custom event
        Properties video_props;
        video_props["video_id"] = std::string("vid_123");
        video_props["duration"] = int64_t(120);
        video_props["quality"] = std::string("hd");
        client.event("user_123", "video_viewed", video_props);
        
        // Track feature usage
        Properties feature_props;
        feature_props["feature_name"] = std::string("dashboard");
        feature_props["section"] = std::string("analytics");
        client.event("user_123", "feature_used", feature_props);
        
        // Track revenue
        Properties revenue_props;
        revenue_props["product"] = std::string("premium_plan");
        client.event_revenue("user_123", "order_456", 9.99, "USD", revenue_props);
        
        // Ensure all events are sent
        client.flush();
        std::cout << "✅ Events sent to UserCanal!" << std::endl;
        
        client.shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}