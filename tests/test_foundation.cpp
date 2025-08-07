// tests/test_foundation.cpp
// Basic tests for Phase 1 foundation components (types, config, errors)

#include <gtest/gtest.h>
#include "usercanal/usercanal.hpp"

using namespace usercanal;

// Test fixture for foundation tests
class FoundationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup before each test
    }

    void TearDown() override {
        // Cleanup after each test
    }
};

// Tests for Types and Utilities
TEST_F(FoundationTest, TestCurrencyConversion) {
    EXPECT_EQ(Utils::currency_to_string(Currency::USD), "USD");
    EXPECT_EQ(Utils::currency_to_string(Currency::EUR), "EUR");
    EXPECT_EQ(Utils::currency_to_string(Currency::GBP), "GBP");
    
    EXPECT_EQ(Utils::string_to_currency("USD"), Currency::USD);
    EXPECT_EQ(Utils::string_to_currency("eur"), Currency::EUR);
    EXPECT_EQ(Utils::string_to_currency("invalid"), Currency::USD); // fallback
}

TEST_F(FoundationTest, TestLogLevelConversion) {
    EXPECT_EQ(Utils::log_level_to_string(LogLevel::ERROR), "ERROR");
    EXPECT_EQ(Utils::log_level_to_string(LogLevel::INFO), "INFO");
    EXPECT_EQ(Utils::log_level_to_string(LogLevel::DEBUG), "DEBUG");
    EXPECT_EQ(Utils::log_level_to_string(LogLevel::TRACE), "TRACE");
}

TEST_F(FoundationTest, TestEventTypeConversion) {
    EXPECT_EQ(Utils::event_type_to_string(EventType::TRACK), "TRACK");
    EXPECT_EQ(Utils::event_type_to_string(EventType::IDENTIFY), "IDENTIFY");
    EXPECT_EQ(Utils::event_type_to_string(EventType::GROUP), "GROUP");
}

TEST_F(FoundationTest, TestApiKeyValidation) {
    // Valid API key (32 hex characters)
    std::string valid_key = "1234567890abcdef1234567890abcdef";
    EXPECT_TRUE(Utils::is_valid_api_key(valid_key));
    
    // Invalid cases
    EXPECT_FALSE(Utils::is_valid_api_key(""));
    EXPECT_FALSE(Utils::is_valid_api_key("too_short"));
    EXPECT_FALSE(Utils::is_valid_api_key("1234567890abcdef1234567890abcdef123")); // too long
    EXPECT_FALSE(Utils::is_valid_api_key("1234567890abcdef1234567890abcdeg")); // invalid hex
}

TEST_F(FoundationTest, TestUserIdValidation) {
    EXPECT_TRUE(Utils::is_valid_user_id("user123"));
    EXPECT_TRUE(Utils::is_valid_user_id("test@example.com"));
    
    // Invalid cases
    EXPECT_FALSE(Utils::is_valid_user_id(""));
    EXPECT_FALSE(Utils::is_valid_user_id("   ")); // only whitespace
    EXPECT_FALSE(Utils::is_valid_user_id(std::string(256, 'x'))); // too long
}

TEST_F(FoundationTest, TestTimestamp) {
    Timestamp now = Utils::now_milliseconds();
    EXPECT_GT(now, 0);
    
    // Should be reasonable timestamp (after 2020)
    EXPECT_GT(now, 1577836800000ULL); // 2020-01-01 00:00:00 UTC
}

TEST_F(FoundationTest, TestBatchIdGeneration) {
    BatchId id1 = Utils::generate_batch_id();
    BatchId id2 = Utils::generate_batch_id();
    
    // IDs should be non-zero and different
    EXPECT_NE(id1, 0);
    EXPECT_NE(id2, 0);
    EXPECT_NE(id1, id2);
}

// Tests for Properties
TEST_F(FoundationTest, TestPropertiesBasicOperations) {
    Properties props;
    
    // Test empty properties
    EXPECT_TRUE(props.empty());
    EXPECT_EQ(props.size(), 0);
    
    // Test adding properties
    props["string_prop"] = std::string("test");
    props["int_prop"] = int64_t(123);
    props["double_prop"] = 45.67;
    props["bool_prop"] = true;
    
    EXPECT_FALSE(props.empty());
    EXPECT_EQ(props.size(), 4);
    
    // Test contains
    EXPECT_TRUE(props.contains("string_prop"));
    EXPECT_FALSE(props.contains("missing_prop"));
}

TEST_F(FoundationTest, TestPropertiesInitializerList) {
    Properties props{
        {"name", std::string("John")},
        {"age", int64_t(30)},
        {"height", 5.9},
        {"active", true}
    };
    
    EXPECT_EQ(props.size(), 4);
    EXPECT_TRUE(props.contains("name"));
    EXPECT_TRUE(props.contains("age"));
}

// Tests for Error Handling
TEST_F(FoundationTest, TestErrorCreation) {
    auto error = Errors::invalid_api_key("bad_key");
    EXPECT_EQ(error.code(), ErrorCode::INVALID_API_KEY);
    EXPECT_NE(std::string(error.what()).find("bad_key"), std::string::npos);
}

TEST_F(FoundationTest, TestNetworkError) {
    auto error = Errors::connection_failed("localhost:8080");
    EXPECT_EQ(error.code(), ErrorCode::CONNECTION_FAILED);
    EXPECT_EQ(error.retries(), 0);
    
    // Test retry increment
    NetworkError network_error = error;
    network_error.increment_retries();
    EXPECT_EQ(network_error.retries(), 1);
}

TEST_F(FoundationTest, TestValidationError) {
    auto error = Errors::invalid_user_id("");
    EXPECT_EQ(error.code(), ErrorCode::INVALID_USER_ID);
    EXPECT_EQ(error.field(), "user_id");
}

TEST_F(FoundationTest, TestTimeoutError) {
    TimeoutError error("connect", std::chrono::milliseconds(5000));
    EXPECT_EQ(error.code(), ErrorCode::TIMEOUT);
    EXPECT_EQ(error.operation(), "connect");
    EXPECT_EQ(error.timeout(), std::chrono::milliseconds(5000));
}

// Tests for Configuration
TEST_F(FoundationTest, TestConfigBasic) {
    std::string api_key = "1234567890abcdef1234567890abcdef";
    Config config(api_key);
    
    EXPECT_EQ(config.api_key(), api_key);
    EXPECT_EQ(config.network().endpoint, "collect.usercanal.com:50000");
    EXPECT_GT(config.batch().event_batch_size, 0);
    EXPECT_GT(config.performance().thread_pool_size, 0);
}

TEST_F(FoundationTest, TestConfigValidation) {
    // Valid config
    std::string valid_api_key = "1234567890abcdef1234567890abcdef";
    Config valid_config(valid_api_key);
    EXPECT_TRUE(valid_config.is_valid());
    EXPECT_NO_THROW(valid_config.validate());
    
    // Invalid config
    Config invalid_config("invalid_key");
    EXPECT_FALSE(invalid_config.is_valid());
    EXPECT_THROW(invalid_config.validate(), ConfigError);
}

TEST_F(FoundationTest, TestConfigBuilder) {
    std::string api_key = "1234567890abcdef1234567890abcdef";
    
    auto config = ConfigBuilder(api_key)
        .endpoint("localhost:8080")
        .batching(50, 25, std::chrono::milliseconds(2000), std::chrono::milliseconds(1000))
        .system_logging(SystemLogLevel::DEBUG)
        .build();
    
    EXPECT_EQ(config.api_key(), api_key);
    EXPECT_EQ(config.network().endpoint, "localhost:8080");
    EXPECT_EQ(config.batch().event_batch_size, 50);
    EXPECT_EQ(config.logging().level, SystemLogLevel::DEBUG);
}

TEST_F(FoundationTest, TestConfigPresets) {
    std::string api_key = "1234567890abcdef1234567890abcdef";
    
    // Development preset
    auto dev_config = Config::development(api_key);
    EXPECT_EQ(dev_config.network().endpoint, "localhost:50000");
    EXPECT_EQ(dev_config.logging().level, SystemLogLevel::DEBUG);
    
    // Production preset
    auto prod_config = Config::production(api_key);
    EXPECT_EQ(prod_config.network().endpoint, "collect.usercanal.com:50000");
    EXPECT_EQ(prod_config.logging().level, SystemLogLevel::ERROR);
    
    // High throughput preset
    auto ht_config = Config::high_throughput(api_key);
    EXPECT_GT(ht_config.batch().event_batch_size, prod_config.batch().event_batch_size);
    EXPECT_GT(ht_config.performance().thread_pool_size, prod_config.performance().thread_pool_size);
}

// Tests for Utility Functions
TEST_F(FoundationTest, TestStringUtils) {
    EXPECT_EQ(Utils::trim("  hello  "), "hello");
    EXPECT_EQ(Utils::trim(""), "");
    EXPECT_EQ(Utils::trim("   "), "");
    
    EXPECT_EQ(Utils::to_lower("HELLO"), "hello");
    EXPECT_EQ(Utils::to_upper("hello"), "HELLO");
    
    EXPECT_TRUE(Utils::starts_with("hello world", "hello"));
    EXPECT_FALSE(Utils::starts_with("hello", "hello world"));
    
    EXPECT_TRUE(Utils::ends_with("hello world", "world"));
    EXPECT_FALSE(Utils::ends_with("world", "hello world"));
}

TEST_F(FoundationTest, TestNetworkUtils) {
    auto [host, port] = Utils::parse_endpoint("example.com:8080");
    EXPECT_EQ(host, "example.com");
    EXPECT_EQ(port, 8080);
    
    auto [invalid_host, invalid_port] = Utils::parse_endpoint("invalid");
    EXPECT_EQ(invalid_host, "");
    EXPECT_EQ(invalid_port, 0);
    
    EXPECT_TRUE(Utils::is_valid_hostname("example.com"));
    EXPECT_TRUE(Utils::is_valid_hostname("sub.example.com"));
    EXPECT_FALSE(Utils::is_valid_hostname(""));
    EXPECT_FALSE(Utils::is_valid_hostname("invalid..hostname"));
    
    EXPECT_TRUE(Utils::is_valid_port(80));
    EXPECT_TRUE(Utils::is_valid_port(65535));
    EXPECT_FALSE(Utils::is_valid_port(0));
    EXPECT_FALSE(Utils::is_valid_port(65536));
}

TEST_F(FoundationTest, TestThreadSafeCounter) {
    Utils::ThreadSafeCounter counter;
    
    EXPECT_EQ(counter.get(), 0);
    EXPECT_EQ(counter.increment(), 1);
    EXPECT_EQ(counter.increment(), 2);
    EXPECT_EQ(counter.decrement(), 1);
    EXPECT_EQ(counter.get(), 1);
    
    counter.reset();
    EXPECT_EQ(counter.get(), 0);
}

TEST_F(FoundationTest, TestRateLimiter) {
    Utils::RateLimiter limiter(2.0, 2); // 2 requests per second, burst of 2
    
    // Should allow initial burst
    EXPECT_TRUE(limiter.try_acquire());
    EXPECT_TRUE(limiter.try_acquire());
    
    // Should be rate limited now
    EXPECT_FALSE(limiter.try_acquire());
}

TEST_F(FoundationTest, TestExponentialBackoff) {
    Utils::ExponentialBackoff backoff(std::chrono::milliseconds(100), 2.0);
    
    EXPECT_EQ(backoff.attempt_count(), 0);
    
    auto delay1 = backoff.next_delay();
    EXPECT_EQ(delay1, std::chrono::milliseconds(100));
    EXPECT_EQ(backoff.attempt_count(), 1);
    
    auto delay2 = backoff.next_delay();
    EXPECT_EQ(delay2, std::chrono::milliseconds(200));
    EXPECT_EQ(backoff.attempt_count(), 2);
    
    backoff.reset();
    EXPECT_EQ(backoff.attempt_count(), 0);
}

TEST_F(FoundationTest, TestCircuitBreaker) {
    Utils::CircuitBreaker breaker(2, std::chrono::milliseconds(1000));
    
    // Initially closed
    EXPECT_EQ(breaker.get_state(), Utils::CircuitBreaker::State::CLOSED);
    EXPECT_TRUE(breaker.is_call_allowed());
    
    // Record failures
    breaker.record_failure();
    EXPECT_TRUE(breaker.is_call_allowed()); // Still closed
    
    breaker.record_failure();
    EXPECT_EQ(breaker.get_state(), Utils::CircuitBreaker::State::OPEN);
    EXPECT_FALSE(breaker.is_call_allowed());
    
    breaker.reset();
    EXPECT_EQ(breaker.get_state(), Utils::CircuitBreaker::State::CLOSED);
}

// Tests for SDK Information
TEST_F(FoundationTest, TestSDKVersion) {
    EXPECT_EQ(std::string(VERSION), "1.0.0");
    EXPECT_EQ(VERSION_MAJOR, 1);
    EXPECT_EQ(VERSION_MINOR, 0);
    EXPECT_EQ(VERSION_PATCH, 0);
}

// Performance test
TEST_F(FoundationTest, TestPerformance) {
    // Test that basic operations are fast
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 10000; ++i) {
        Properties props{
            {"test", std::string("value")},
            {"number", int64_t(i)},
            {"flag", true}
        };
        
        BatchId id = Utils::generate_batch_id();
        (void)id; // Suppress unused variable warning
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete 10k operations in reasonable time (< 1 second)
    EXPECT_LT(duration.count(), 1000);
}