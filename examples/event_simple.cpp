// event_simple.cpp
// Simple Event Tracking Example
// Shows basic event tracking with UserCanal C++ SDK

#include <iostream>
#include "usercanal/usercanal.hpp"

using namespace usercanal;

int main() {
    std::cout << "🚀 UserCanal C++ SDK - Simple Event Tracking\n" << std::endl;
    
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
        
        // Track user signup
        Properties signup_props;
        signup_props["signup_method"] = std::string("email");
        signup_props["referral_source"] = std::string("google");
        client.track_user_signup("user_123", signup_props);
        std::cout << "📊 Tracked: User signup" << std::endl;
        
        // Track custom event
        Properties video_props;
        video_props["video_id"] = std::string("vid_123");
        video_props["duration"] = int64_t(120);
        video_props["quality"] = std::string("hd");
        client.track("user_123", "video_viewed", video_props);
        std::cout << "📊 Tracked: Video viewed" << std::endl;
        
        // Track feature usage
        Properties feature_props;
        feature_props["feature_name"] = std::string("dashboard");
        feature_props["section"] = std::string("analytics");
        client.track("user_123", "feature_used", feature_props);
        std::cout << "📊 Tracked: Feature used" << std::endl;
        
        // Track revenue
        Properties revenue_props;
        revenue_props["product"] = std::string("premium_plan");
        client.track_revenue("user_123", "order_456", 9.99, Currency::USD, revenue_props);
        std::cout << "💰 Tracked: Revenue ($9.99)" << std::endl;
        
        // Ensure all events are sent
        client.flush();
        std::cout << "\n✅ All events sent to UserCanal!" << std::endl;
        
        client.shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}