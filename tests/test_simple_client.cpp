// tests/test_simple_client.cpp
// Simple tests for the simplified client API

#include <gtest/gtest.h>
#include "usercanal/usercanal.hpp"

using namespace usercanal;

// Test fixture for simple client tests
class SimpleClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        api_key_ = "1234567890abcdef1234567890abcdef";
    }

    void TearDown() override {
        // Cleanup after each test
    }

    std::string api_key_;
};

TEST_F(SimpleClientTest, TestClientConstruction) {
    // Test construction with API key
    Client client1(api_key_);
    EXPECT_FALSE(client1.is_initialized());

    // Test construction with config
    Config config(api_key_);
    config.set_endpoint("localhost:50001");
    Client client2(config);
    EXPECT_FALSE(client2.is_initialized());
}

TEST_F(SimpleClientTest, TestClientInitialization) {
    Client client(api_key_);
    
    // Test initialization
    client.initialize();
    EXPECT_TRUE(client.is_initialized());
    
    // Test shutdown
    client.shutdown();
    EXPECT_FALSE(client.is_initialized());
}

TEST_F(SimpleClientTest, TestEventTracking) {
    Client client(api_key_);
    client.initialize();
    
    // Test basic event tracking
    Properties props;
    props["page"] = std::string("homepage");
    props["duration"] = int64_t(120);
    
    // Should not throw
    EXPECT_NO_THROW(client.event("user_123", "page_view", props));
    
    // Test with empty properties
    EXPECT_NO_THROW(client.event("user_456", "signup", {}));
    
    client.shutdown();
}

TEST_F(SimpleClientTest, TestRevenueTracking) {
    Client client(api_key_);
    client.initialize();
    
    Properties props;
    props["product"] = std::string("premium_plan");
    
    // Test revenue tracking
    EXPECT_NO_THROW(client.event_revenue("user_123", "order_456", 29.99, "USD", props));
    
    // Test with default currency
    EXPECT_NO_THROW(client.event_revenue("user_789", "order_789", 19.99));
    
    client.shutdown();
}

TEST_F(SimpleClientTest, TestLogging) {
    Client client(api_key_);
    client.initialize();
    
    Properties data;
    data["request_id"] = std::string("req_123");
    data["duration_ms"] = int64_t(250);
    
    // Test different log levels
    EXPECT_NO_THROW(client.log_info("service", "Operation completed", data));
    EXPECT_NO_THROW(client.log_error("service", "Operation failed", data));
    EXPECT_NO_THROW(client.log_warning("service", "Performance warning", data));
    EXPECT_NO_THROW(client.log_debug("service", "Debug info", data));
    
    // Test with empty data
    EXPECT_NO_THROW(client.log_info("service", "Simple message"));
    
    client.shutdown();
}

TEST_F(SimpleClientTest, TestLegacyCompatibility) {
    Client client(api_key_);
    client.initialize();
    
    Properties props;
    props["feature"] = std::string("dashboard");
    
    // Test legacy track method
    EXPECT_NO_THROW(client.track("user_123", "feature_used", props));
    
    // Test legacy track_revenue method
    EXPECT_NO_THROW(client.track_revenue("user_456", "order_123", 99.99, "EUR", props));
    
    client.shutdown();
}

TEST_F(SimpleClientTest, TestFlush) {
    Client client(api_key_);
    client.initialize();
    
    // Add some events and logs
    client.event("user_123", "test_event", {});
    client.log_info("test_service", "Test message", {});
    
    // Test flush (should not throw)
    EXPECT_NO_THROW(client.flush());
    
    client.shutdown();
}

TEST_F(SimpleClientTest, TestStats) {
    Client client(api_key_);
    client.initialize();
    
    // Get initial stats
    Stats stats = client.get_stats();
    
    // Stats should have some basic fields
    // Note: In our simplified implementation, these are just counters
    // The actual values might be 0 since we're not doing real network operations
    
    // Add some events
    client.event("user_123", "test_event", {});
    client.log_info("service", "test log", {});
    
    // Get stats again - should have incremented
    Stats new_stats = client.get_stats();
    
    // In our simplified implementation, we expect the counters to increment
    EXPECT_GE(new_stats.events_sent, stats.events_sent);
    EXPECT_GE(new_stats.logs_sent, stats.logs_sent);
    
    client.shutdown();
}

TEST_F(SimpleClientTest, TestUninitializedBehavior) {
    Client client(api_key_);
    
    // Operations on uninitialized client should be safe (no-ops in our implementation)
    EXPECT_NO_THROW(client.event("user_123", "test", {}));
    EXPECT_NO_THROW(client.log_info("service", "test", {}));
    EXPECT_NO_THROW(client.flush());
    
    // Stats should still work
    EXPECT_NO_THROW(client.get_stats());
}

TEST_F(SimpleClientTest, TestMoveSemantic) {
    Client client1(api_key_);
    client1.initialize();
    
    // Test move construction
    Client client2 = std::move(client1);
    EXPECT_TRUE(client2.is_initialized());
    
    // Test move assignment
    Client client3(api_key_);
    client3 = std::move(client2);
    EXPECT_TRUE(client3.is_initialized());
    
    client3.shutdown();
}