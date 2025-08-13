// examples/event_advanced.cpp
// Example demonstrating EventAdvanced functionality with optional device/session ID overrides

#include "usercanal/usercanal.hpp"
#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>

using namespace usercanal;

void print_separator(const std::string& title) {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << title << std::endl;
    std::cout << std::string(50, '=') << std::endl;
}

// Helper to create a 16-byte UUID vector (simplified for demo)
std::unique_ptr<std::vector<uint8_t>> create_uuid(const std::string& hex_string) {
    auto uuid = std::make_unique<std::vector<uint8_t>>();
    uuid->reserve(16);
    
    // Convert hex string to bytes (simplified - assumes valid 32-char hex)
    for (size_t i = 0; i < hex_string.length() && i < 32; i += 2) {
        std::string byte_string = hex_string.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(strtol(byte_string.c_str(), nullptr, 16));
        uuid->push_back(byte);
    }
    
    // Pad to 16 bytes if needed
    while (uuid->size() < 16) {
        uuid->push_back(0);
    }
    
    return uuid;
}

int main() {
    try {
        print_separator("EventAdvanced Demo - C++ SDK Schema Updates");
        
        // Initialize client
        Config config("your-api-key-here");
        config.set_endpoint("https://api.usercanal.com/collect");
        config.set_system_log_level(SystemLogLevel::DEBUG);
        
        Client client(config);
        client.initialize();
        
        if (!client.is_initialized()) {
            std::cerr << "Failed to initialize client" << std::endl;
            return 1;
        }
        
        std::cout << "Client initialized successfully!" << std::endl;
        
        // 1. Regular event (server-side behavior - nil device_id and session_id)
        print_separator("1. Regular Event (Server-Side Behavior)");
        std::cout << "Sending regular event with nil device_id/session_id..." << std::endl;
        
        Properties signup_props;
        signup_props["signup_method"] = std::string("email");
        signup_props["referral_source"] = std::string("google");
        signup_props["email_domain"] = std::string("gmail.com");
        
        client.event("user_12345", EventNames::USER_SIGNED_UP, signup_props);
        
        // 2. EventAdvanced without overrides (same as regular event)
        print_separator("2. EventAdvanced - Basic Usage");
        std::cout << "Using EventAdvanced without device/session ID overrides..." << std::endl;
        
        Properties login_props;
        login_props["login_method"] = std::string("password");
        login_props["remember_me"] = true;
        login_props["ip_address"] = std::string("192.168.1.100");
        
        EventAdvanced login_event("user_12345", EventNames::USER_SIGNED_IN, login_props);
        client.event_advanced(login_event);
        
        // 3. EventAdvanced with custom device_id
        print_separator("3. EventAdvanced - Custom Device ID");
        std::cout << "Using EventAdvanced with custom device_id..." << std::endl;
        
        Properties purchase_props;
        purchase_props["product_id"] = std::string("premium_plan");
        purchase_props["amount"] = 29.99;
        purchase_props["currency"] = std::string(Currencies::USD);
        purchase_props["payment_method"] = std::string(PaymentMethods::STRIPE);
        
        EventAdvanced purchase_event("user_123", EventNames::ORDER_COMPLETED, purchase_props);
        purchase_event.device_id = create_uuid("a1b2c3d4e5f6789012345678901234ab");
        
        client.event_advanced(purchase_event);
        
        // 4. EventAdvanced with custom device_id and session_id
        print_separator("4. EventAdvanced - Custom Device ID & Session ID");
        std::cout << "Using EventAdvanced with both device_id and session_id..." << std::endl;
        
        Properties feature_props;
        feature_props["feature_name"] = std::string("advanced_analytics");
        feature_props["feature_tier"] = std::string("premium");
        feature_props["usage_count"] = static_cast<int64_t>(42);
        
        EventAdvanced feature_event("user_12345", EventNames::FEATURE_USED, feature_props);
        feature_event.device_id = create_uuid("a1b2c3d4e5f6789012345678901234ab");
        feature_event.session_id = create_uuid("11223344556677889900112233445566");
        
        client.event_advanced(feature_event);
        
        // 5. EventAdvanced with custom timestamp
        print_separator("5. EventAdvanced - Custom Timestamp");
        std::cout << "Using EventAdvanced with custom timestamp..." << std::endl;
        
        Properties page_props;
        page_props["page_name"] = std::string("/dashboard");
        page_props["referrer"] = std::string("/login");
        page_props["load_time_ms"] = static_cast<int64_t>(245);
        
        auto custom_timestamp = Utils::now_milliseconds() - 60000; // 1 minute ago
        EventAdvanced page_event("user_12345", EventNames::PAGE_VIEWED, page_props);
        page_event.timestamp = std::make_unique<Timestamp>(custom_timestamp);
        
        client.event_advanced(page_event);
        
        // 6. Revenue event (using simplified payload structure)
        print_separator("6. Revenue Event");
        std::cout << "Sending revenue event with new schema..." << std::endl;
        
        Properties revenue_props;
        revenue_props["payment_method"] = std::string(PaymentMethods::STRIPE);
        revenue_props["plan_type"] = std::string(SubscriptionIntervals::ANNUAL);
        revenue_props["discount_applied"] = true;
        revenue_props["channel"] = std::string(Channels::DIRECT);
        
        client.event_revenue("user_12345", "order_789", 299.99, Currencies::USD, revenue_props);
        
        // 7. Multiple events with different configurations
        print_separator("7. Mixed Event Types");
        std::cout << "Sending multiple events with different configurations..." << std::endl;
        
        // Server event (no device context)
        Properties server_props;
        server_props["source"] = std::string(Sources::DIRECT);
        server_props["endpoint"] = std::string("/api/v1/users");
        server_props["device_type"] = std::string(DeviceTypes::BOT);
        client.event("system", "api_request_completed", server_props);
        
        // Mobile event (with device context)
        Properties mobile_props;
        mobile_props["platform"] = std::string(OperatingSystems::IOS);
        mobile_props["app_version"] = std::string("2.1.0");
        mobile_props["device_type"] = std::string(DeviceTypes::MOBILE);
        
        EventAdvanced mobile_event("user_67890", EventNames::PAGE_VIEWED, mobile_props);
        mobile_event.device_id = create_uuid("b2c3d4e5f6a7890123456789012345bc");
        mobile_event.session_id = create_uuid("22334455667788990011223344556677");
        
        client.event_advanced(mobile_event);
        
        // Wait a moment for batching
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Flush all pending events
        print_separator("Flushing Events");
        std::cout << "Flushing all pending events..." << std::endl;
        client.flush();
        
        // Display statistics
        print_separator("Statistics");
        auto stats = client.get_stats();
        std::cout << "Events sent: " << stats.events_sent << std::endl;
        std::cout << "Batches sent: " << stats.batches_sent << std::endl;
        std::cout << "Network errors: " << stats.network_errors << std::endl;
        std::cout << "Success rate: " << (stats.success_rate() * 100) << "%" << std::endl;
        
        // Key improvements summary
        print_separator("Key Schema Improvements");
        std::cout << "✓ Event payload simplified - properties directly in payload" << std::endl;
        std::cout << "✓ Event name field added for ~80x faster event name access" << std::endl;
        std::cout << "✓ Device ID & Session ID optional (nil by default for servers)" << std::endl;
        std::cout << "✓ EventAdvanced allows manual ID overrides when needed" << std::endl;
        std::cout << "✓ Server-first design with override capability" << std::endl;
        std::cout << "✓ Backward compatibility maintained" << std::endl;
        
        client.shutdown();
        std::cout << "\nDemo completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}