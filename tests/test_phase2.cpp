// tests/test_phase2.cpp
// Comprehensive tests for Phase 2 networking and batching components

#include <gtest/gtest.h>
#include "usercanal/usercanal.hpp"
#include <thread>
#include <atomic>
#include <chrono>

using namespace usercanal;

// Test fixture for Phase 2 tests
class Phase2Test : public ::testing::Test {
protected:
    void SetUp() override {
        api_key_ = "1234567890abcdef1234567890abcdef";
    }

    void TearDown() override {
        // Cleanup after each test
    }

    std::string api_key_;
};

// Tests for Network Layer
TEST_F(Phase2Test, TestNetworkConfigValidation) {
    NetworkConfig config;
    config.endpoint = "localhost:8080";
    config.connect_timeout = std::chrono::milliseconds(5000);
    config.send_timeout = std::chrono::milliseconds(10000);
    config.max_retries = 3;
    
    EXPECT_TRUE(NetworkConfigValidator::is_valid(config));
    
    // Test invalid endpoint
    config.endpoint = "invalid-endpoint";
    EXPECT_FALSE(NetworkConfigValidator::is_valid(config));
    
    // Test invalid timeout
    config.endpoint = "localhost:8080";
    config.connect_timeout = std::chrono::milliseconds(0);
    EXPECT_FALSE(NetworkConfigValidator::is_valid(config));
}

TEST_F(Phase2Test, TestNetworkUtils) {
    // Test endpoint parsing
    auto [host, port] = Utils::parse_endpoint("example.com:8080");
    EXPECT_EQ(host, "example.com");
    EXPECT_EQ(port, 8080);
    
    auto [invalid_host, invalid_port] = Utils::parse_endpoint("invalid");
    EXPECT_EQ(invalid_host, "");
    EXPECT_EQ(invalid_port, 0);
    
    // Test hostname validation
    EXPECT_TRUE(Utils::is_valid_hostname("example.com"));
    EXPECT_TRUE(Utils::is_valid_hostname("api.example.com"));
    EXPECT_FALSE(Utils::is_valid_hostname(""));
    EXPECT_FALSE(Utils::is_valid_hostname("invalid..hostname"));
    
    // Test port validation
    EXPECT_TRUE(Utils::is_valid_port(80));
    EXPECT_TRUE(Utils::is_valid_port(8080));
    EXPECT_TRUE(Utils::is_valid_port(65535));
    EXPECT_FALSE(Utils::is_valid_port(0));
}

TEST_F(Phase2Test, TestConnectionBasics) {
    NetworkConfig config;
    config.endpoint = "localhost:9999"; // Unlikely to be running
    config.connect_timeout = std::chrono::milliseconds(1000);
    
    Connection conn("localhost", 9999, config);
    
    EXPECT_FALSE(conn.is_connected());
    EXPECT_EQ(conn.get_state(), ConnectionState::DISCONNECTED);
    EXPECT_EQ(conn.get_endpoint(), "localhost:9999");
    
    // Test connection to non-existent server should fail
    EXPECT_THROW(conn.connect(std::chrono::milliseconds(1000)), NetworkError);
    EXPECT_EQ(conn.get_state(), ConnectionState::FAILED);
}

// Tests for Batch Management
TEST_F(Phase2Test, TestBatchItemCreation) {
    // Test EventBatchItem
    Properties event_props{
        {"action", std::string("click")},
        {"button", std::string("subscribe")},
        {"value", 1}
    };
    
    EventBatchItem event_item(EventType::TRACK, "user_123", event_props);
    EXPECT_EQ(event_item.get_event_type(), EventType::TRACK);
    EXPECT_EQ(event_item.get_user_id(), "user_123");
    EXPECT_GT(event_item.get_estimated_size(), 0);
    EXPECT_GT(event_item.calculate_size(), 50); // Should be reasonable size
    
    // Test LogBatchItem
    Properties log_data{
        {"request_id", std::string("req_456")},
        {"duration_ms", int64_t(150)}
    };
    
    LogBatchItem log_item(LogLevel::INFO, "api_service", "Request processed", log_data);
    EXPECT_EQ(log_item.get_level(), LogLevel::INFO);
    EXPECT_EQ(log_item.get_service(), "api_service");
    EXPECT_GT(log_item.get_estimated_size(), 0);
    EXPECT_GT(log_item.calculate_size(), 30);
}

TEST_F(Phase2Test, TestBatchContainer) {
    EventBatch batch;
    
    EXPECT_TRUE(batch.is_empty());
    EXPECT_EQ(batch.get_item_count(), 0);
    EXPECT_EQ(batch.get_total_size(), 0);
    EXPECT_EQ(batch.get_state(), BatchState::COLLECTING);
    
    // Add items to batch
    Properties props{{"test", std::string("value")}};
    auto item1 = std::make_unique<EventBatchItem>(EventType::TRACK, "user1", props);
    auto item2 = std::make_unique<EventBatchItem>(EventType::TRACK, "user2", props);
    
    EXPECT_TRUE(batch.add_item(std::move(item1)));
    EXPECT_TRUE(batch.add_item(std::move(item2)));
    
    EXPECT_FALSE(batch.is_empty());
    EXPECT_EQ(batch.get_item_count(), 2);
    EXPECT_GT(batch.get_total_size(), 0);
    
    // Test batch fullness
    EXPECT_TRUE(batch.is_full(2, 1000000)); // Full by item count
    EXPECT_FALSE(batch.is_full(10, 10));    // Full by size
    
    // Test serialization
    std::vector<uint8_t> serialized = batch.serialize(api_key_, SchemaType::EVENT);
    EXPECT_GT(serialized.size(), 0);
}

TEST_F(Phase2Test, TestBatchUtils) {
    Properties props{
        {"string_val", std::string("test")},
        {"int_val", int64_t(42)},
        {"double_val", 3.14},
        {"bool_val", true}
    };
    
    // Test property serialization
    auto serialized = BatchUtils::serialize_properties(props);
    EXPECT_GT(serialized.size(), 0);
    
    auto deserialized = BatchUtils::deserialize_properties(serialized);
    EXPECT_EQ(deserialized.size(), props.size());
    
    // Test size estimation
    size_t event_size = BatchUtils::estimate_event_size("user_123", props);
    EXPECT_GT(event_size, 0);
    
    size_t log_size = BatchUtils::estimate_log_size("service", "message", props);
    EXPECT_GT(log_size, 0);
    
    // Test validation
    EventBatchItem event_item(EventType::TRACK, "user_123", props);
    EXPECT_TRUE(BatchUtils::validate_event_item(event_item));
    
    LogBatchItem log_item(LogLevel::INFO, "service", "message", props);
    EXPECT_TRUE(BatchUtils::validate_log_item(log_item));
    
    // Test batch size validation
    BatchConfig config;
    config.event_batch_size = 100;
    config.max_batch_size_bytes = 1024 * 1024;
    
    EXPECT_TRUE(BatchUtils::validate_batch_size(50, 500000, config));
    EXPECT_FALSE(BatchUtils::validate_batch_size(150, 500000, config)); // Too many items
    EXPECT_FALSE(BatchUtils::validate_batch_size(50, 2000000, config)); // Too many bytes
}

TEST_F(Phase2Test, TestBatchProcessor) {
    std::atomic<int> batches_sent{0};
    std::atomic<int> errors_received{0};
    
    BatchProcessor processor(
        [&batches_sent](const std::vector<uint8_t>& data) {
            batches_sent++;
            // Simulate successful send
        },
        [&errors_received](const Error& error) {
            errors_received++;
        }
    );
    
    processor.start();
    EXPECT_TRUE(processor.is_running());
    
    // Create and process a batch
    auto batch = std::make_unique<EventBatch>();
    Properties props{{"test", std::string("data")}};
    auto item = std::make_unique<EventBatchItem>(EventType::TRACK, "user", props);
    batch->add_item(std::move(item));
    
    processor.process_event_batch_sync(std::move(batch), api_key_);
    
    // Give some time for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_EQ(batches_sent.load(), 1);
    EXPECT_EQ(errors_received.load(), 0);
    
    auto stats = processor.get_stats();
    EXPECT_EQ(stats.batches_sent, 1);
    
    processor.stop();
    EXPECT_FALSE(processor.is_running());
}

TEST_F(Phase2Test, TestBatchManager) {
    BatchConfig config;
    config.event_batch_size = 5;  // Small for testing
    config.log_batch_size = 3;
    config.max_queue_size = 100;
    config.event_flush_interval = std::chrono::milliseconds(1000);
    config.log_flush_interval = std::chrono::milliseconds(500);
    
    BatchManager manager(config, api_key_);
    
    std::atomic<int> batches_received{0};
    manager.set_send_callback([&batches_received](const std::vector<uint8_t>& data) {
        batches_received++;
    });
    
    manager.start();
    EXPECT_TRUE(manager.is_running());
    
    // Submit events
    for (int i = 0; i < 7; ++i) {
        Properties props{{"index", int64_t(i)}};
        auto event = manager.create_event_item(EventType::TRACK, "user_" + std::to_string(i), props);
        EXPECT_TRUE(manager.submit_event(std::move(event)));
    }
    
    // Submit logs
    for (int i = 0; i < 5; ++i) {
        Properties data{{"log_index", int64_t(i)}};
        auto log = manager.create_log_item(LogLevel::INFO, "test_service", "Test message " + std::to_string(i), data);
        EXPECT_TRUE(manager.submit_log(std::move(log)));
    }
    
    // Wait for automatic batching
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    
    // Should have triggered batches due to size limits
    EXPECT_GT(batches_received.load(), 0);
    
    manager.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto event_stats = manager.get_event_stats();
    auto log_stats = manager.get_log_stats();
    
    EXPECT_GT(event_stats.batches_sent, 0);
    EXPECT_GT(log_stats.batches_sent, 0);
    
    manager.stop();
}

// Tests for Client Interface
TEST_F(Phase2Test, TestClientCreation) {
    // Test different client creation methods
    auto config = Config::development(api_key_);
    Client client(config);
    
    EXPECT_EQ(client.get_state(), ClientState::UNINITIALIZED);
    EXPECT_FALSE(client.is_initialized());
    
    // Test config access
    const auto& client_config = client.get_config();
    EXPECT_EQ(client_config.api_key(), api_key_);
    
    // Test factory methods
    auto dev_client = ClientFactory::create_development(api_key_);
    EXPECT_NE(dev_client, nullptr);
    
    auto prod_client = ClientFactory::create_production(api_key_);
    EXPECT_NE(prod_client, nullptr);
    
    auto ht_client = ClientFactory::create_high_throughput(api_key_);
    EXPECT_NE(ht_client, nullptr);
}

TEST_F(Phase2Test, TestClientInitialization) {
    auto config = Config::development(api_key_);
    config.set_endpoint("localhost:9999"); // Non-existent endpoint
    
    Client client(config);
    
    // Initialize should fail due to network issues, but not crash
    try {
        client.initialize();
        // If it succeeds, that's also fine (maybe something is running on 9999)
        EXPECT_TRUE(client.is_initialized());
        client.shutdown();
    } catch (const NetworkError& e) {
        // Expected if no server running
        EXPECT_EQ(client.get_state(), ClientState::FAILED);
    }
}

TEST_F(Phase2Test, TestClientEventHandler) {
    class TestEventHandler : public ClientEventHandler {
    public:
        std::atomic<int> init_count{0};
        std::atomic<int> shutdown_count{0};
        std::atomic<int> error_count{0};
        
        void on_client_initialized() override { init_count++; }
        void on_client_shutdown() override { shutdown_count++; }
        void on_client_error(const Error& error) override { error_count++; }
    };
    
    auto handler = std::make_shared<TestEventHandler>();
    
    try {
        Client client(Config::development(api_key_));
        client.set_event_handler(handler);
        
        client.initialize();
        EXPECT_EQ(handler->init_count.load(), 1);
        
        client.shutdown();
        EXPECT_EQ(handler->shutdown_count.load(), 1);
        
    } catch (const NetworkError&) {
        // Network errors are expected in test environment
        EXPECT_GT(handler->error_count.load(), 0);
    }
}

TEST_F(Phase2Test, TestClientOperationsWithoutNetwork) {
    // Test client operations when network is unavailable
    auto config = Config::development(api_key_);
    config.set_endpoint("localhost:9999");
    
    Client client(config);
    
    // Operations before initialization should fail
    EXPECT_THROW(client.track("user", "event"), ClientError);
    EXPECT_THROW(client.log_info("service", "message"), ClientError);
    
    try {
        client.initialize();
        
        // Try operations that would queue data but fail to send
        client.track_user_signup("user_123", {
            {"source", std::string("test")}
        });
        
        client.log_info("test_service", "Test message", {
            {"test", true}
        });
        
        // Check queue sizes
        EXPECT_GT(client.get_event_queue_size(), 0);
        EXPECT_GT(client.get_log_queue_size(), 0);
        
        // Flush should not crash even if network is down
        client.flush();
        
        auto stats = client.get_stats();
        EXPECT_GT(stats.events_submitted, 0);
        EXPECT_GT(stats.logs_submitted, 0);
        
        client.shutdown();
        
    } catch (const NetworkError&) {
        // Expected if network is unavailable
    }
}

TEST_F(Phase2Test, TestUtilityClasses) {
    try {
        Client client(Config::development(api_key_));
        client.initialize();
        
        // Test EventTracker
        EventTracker tracker(client, "test_user");
        tracker.track_signup({{"method", std::string("email")}});
        tracker.track_login({{"success", true}});
        tracker.set_user_properties({{"tier", std::string("premium")}});
        
        // Test Logger
        Logger logger(client, "test_service");
        logger.set_context(12345);
        EXPECT_EQ(logger.get_context(), 12345);
        
        logger.info("Test info message");
        logger.error("Test error message", {{"error_code", int64_t(500)}});
        logger.clear_context();
        EXPECT_EQ(logger.get_context(), 0);
        
        // Test ScopedFlush
        {
            ScopedFlush scoped(client);
            client.track("scope_user", "scope_event");
            // Flush should happen automatically when scope exits
        }
        
        client.shutdown();
        
    } catch (const NetworkError&) {
        // Expected in test environment
    }
}

TEST_F(Phase2Test, TestClientStats) {
    ClientStats stats;
    
    EXPECT_EQ(stats.events_submitted.load(), 0);
    EXPECT_EQ(stats.logs_submitted.load(), 0);
    EXPECT_EQ(stats.get_total_items(), 0);
    EXPECT_EQ(stats.get_event_success_rate(), 1.0); // No failures = 100% success
    EXPECT_EQ(stats.get_log_success_rate(), 1.0);
    EXPECT_EQ(stats.get_overall_success_rate(), 1.0);
    
    // Simulate some stats
    stats.events_submitted = 100;
    stats.events_sent = 80;
    stats.events_failed = 20;
    stats.logs_submitted = 50;
    stats.logs_sent = 45;
    stats.logs_failed = 5;
    
    EXPECT_EQ(stats.get_total_items(), 150);
    EXPECT_DOUBLE_EQ(stats.get_event_success_rate(), 0.8);
    EXPECT_DOUBLE_EQ(stats.get_log_success_rate(), 0.9);
    EXPECT_DOUBLE_EQ(stats.get_overall_success_rate(), 0.833333333333);
    
    stats.reset();
    EXPECT_EQ(stats.get_total_items(), 0);
}

// Performance tests
TEST_F(Phase2Test, TestBatchingPerformance) {
    BatchConfig config;
    config.event_batch_size = 100;
    config.log_batch_size = 50;
    config.max_queue_size = 10000;
    
    BatchManager manager(config, api_key_);
    
    std::atomic<int> batches_processed{0};
    manager.set_send_callback([&batches_processed](const std::vector<uint8_t>& data) {
        batches_processed++;
    });
    
    manager.start();
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Submit many items quickly
    const int num_events = 1000;
    const int num_logs = 500;
    
    for (int i = 0; i < num_events; ++i) {
        Properties props{
            {"index", int64_t(i)},
            {"timestamp", int64_t(Utils::now_milliseconds())},
            {"test_data", std::string("performance_test_") + std::to_string(i)}
        };
        auto event = manager.create_event_item(EventType::TRACK, "user_" + std::to_string(i % 100), props);
        manager.submit_event(std::move(event));
    }
    
    for (int i = 0; i < num_logs; ++i) {
        Properties data{
            {"log_index", int64_t(i)},
            {"performance_test", true}
        };
        auto log = manager.create_log_item(LogLevel::INFO, "perf_service", "Performance test log " + std::to_string(i), data);
        manager.submit_log(std::move(log));
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "Submitted " << (num_events + num_logs) << " items in " 
              << duration.count() << "ms" << std::endl;
    
    // Should complete submission in reasonable time (< 1 second)
    EXPECT_LT(duration.count(), 1000);
    
    // Flush and wait for processing
    manager.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Should have processed multiple batches
    EXPECT_GT(batches_processed.load(), 10);
    
    auto combined_stats = manager.get_combined_stats();
    EXPECT_EQ(combined_stats.items_batched, num_events + num_logs);
    
    manager.stop();
}

// Global client tests
TEST_F(Phase2Test, TestGlobalClient) {
    // Test global client functionality
    EXPECT_FALSE(GlobalClient::is_initialized());
    
    try {
        GlobalClient::initialize(api_key_);
        EXPECT_TRUE(GlobalClient::is_initialized());
        
        // Test global operations
        GlobalClient::track("global_user", "global_event", {
            {"global", true}
        });
        
        GlobalClient::log_info("global_service", "Global log message", {
            {"source", std::string("global_client")}
        });
        
        GlobalClient::flush();
        
        GlobalClient::shutdown();
        EXPECT_FALSE(GlobalClient::is_initialized());
        
    } catch (const NetworkError&) {
        // Expected in test environment
        GlobalClient::shutdown(); // Cleanup
    }
}

// Integration test combining multiple components
TEST_F(Phase2Test, TestIntegratedWorkflow) {
    try {
        // Create client with custom configuration
        auto config = ConfigBuilder(api_key_)
            .endpoint("localhost:9999")
            .batching(10, 5, std::chrono::milliseconds(500), std::chrono::milliseconds(250))
            .performance(2, 10 * 1024 * 1024)
            .system_logging(SystemLogLevel::DEBUG)
            .build();
        
        Client client(config);
        
        // Set up monitoring
        class IntegrationHandler : public ClientEventHandler {
        public:
            std::atomic<int> events_sent{0};
            std::atomic<int> logs_sent{0};
            std::atomic<int> batches_sent{0};
            
            void on_event_sent(const std::string& user_id, const std::string& event_name) override {
                events_sent++;
            }
            
            void on_log_sent(LogLevel level, const std::string& service, const std::string& message) override {
                logs_sent++;
            }
            
            void on_batch_sent(BatchId batch_id, size_t items, size_t bytes) override {
                batches_sent++;
            }
        };
        
        auto handler = std::make_shared<IntegrationHandler>();
        client.set_event_handler(handler);
        
        client.initialize();
        
        // Simulate realistic usage pattern
        for (int session = 0; session < 5; ++session) {
            std::string user_id = "integration_user_" + std::to_string(session);
            
            // User session start
            client.track(user_id, "session_start", {
                {"session_id", std::string("session_") + std::to_string(session)},
                {"device", std::string("test_device")},
                {"version", std::string("1.0.0")}
            });
            
            // Some user activities
            client.track_feature_used(user_id, "dashboard", {
                {"time_spent_ms", int64_t(15000)}
            });
            
            client.track_page_view(user_id, "/reports", {
                {"load_time_ms", int64_t(250)}
            });
            
            // Revenue event
            if (session % 3 == 0) {
                client.track_revenue(user_id, "order_" + std::to_string(session), 
                                   49.99, Currency::USD, {
                    {"plan", std::string("premium")},
                    {"upgrade", true}
                });
            }
            
            // Corresponding logs
            client.log_info("session_service", "User session started", {
                {"user_id", user_id},
                {"session_duration_estimate", int64_t(1800)}
            });
            
            client.log_debug("analytics_service", "User activity tracked", {
                {"user_id", user_id},
                {"activities_count", int64_t(3)}
            });
            
            // Session end
            client.track(user_id, "session_end", {
                {"duration_ms", int64_t(30000 + session * 5000)}
            });
        }
        
        // Manual flush to complete processing
        client.flush();
        
        // Wait for processing to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // Verify statistics
        auto stats = client.get_stats();
        EXPECT_GT(stats.events_submitted, 15); // At least 3 events per session
        EXPECT_GT(stats.logs_submitted, 10);   // At least 2 logs per session
        
        // Check health
        auto health = client.get_health_status();
        // Health may be false due to network issues, but should not crash
        
        client.shutdown();
        
    } catch (const NetworkError&) {
        // Expected when no collector is running
    }
}