// examples/phase2_networking.cpp
// Example demonstrating Phase 2 networking and batch management features
// Shows how to use the complete client with real network operations

#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <vector>
#include <atomic>
#include "usercanal/usercanal.hpp"

using namespace usercanal;

// Custom event handler for monitoring client operations
class ExampleEventHandler : public ClientEventHandler {
public:
    void on_client_initialized() override {
        std::cout << "[CLIENT] Initialized successfully" << std::endl;
    }
    
    void on_client_shutdown() override {
        std::cout << "[CLIENT] Shutdown completed" << std::endl;
    }
    
    void on_client_error(const Error& error) override {
        std::cout << "[ERROR] " << error.what() << std::endl;
    }
    
    void on_event_sent(const std::string& user_id, const std::string& event_name) override {
        events_sent_++;
        if (events_sent_ % 50 == 0) {
            std::cout << "[EVENTS] Sent " << events_sent_ << " events (latest: " 
                      << event_name << " for " << user_id << ")" << std::endl;
        }
    }
    
    void on_log_sent(LogLevel level, const std::string& service, const std::string& message) override {
        logs_sent_++;
        if (logs_sent_ % 25 == 0) {
            std::cout << "[LOGS] Sent " << logs_sent_ << " logs (latest: " 
                      << Utils::log_level_to_string(level) << " from " << service << ")" << std::endl;
        }
    }
    
    void on_batch_sent(BatchId batch_id, size_t items, size_t bytes) override {
        batches_sent_++;
        std::cout << "[BATCH] Sent batch " << batch_id << " with " << items 
                  << " items (" << bytes << " bytes)" << std::endl;
    }
    
    void on_queue_full(const std::string& queue_type) override {
        std::cout << "[WARNING] " << queue_type << " queue is full!" << std::endl;
    }
    
    void on_flush_triggered(const std::string& reason) override {
        std::cout << "[BATCH] Flush triggered: " << reason << std::endl;
    }
    
    uint64_t get_events_sent() const { return events_sent_; }
    uint64_t get_logs_sent() const { return logs_sent_; }
    uint64_t get_batches_sent() const { return batches_sent_; }

private:
    std::atomic<uint64_t> events_sent_{0};
    std::atomic<uint64_t> logs_sent_{0};
    std::atomic<uint64_t> batches_sent_{0};
};

void demonstrate_basic_client_operations() {
    std::cout << "\n=== Basic Client Operations ===" << std::endl;
    
    std::string api_key = "1234567890abcdef1234567890abcdef";
    
    try {
        // Create client with development configuration
        auto client = ClientFactory::create_development(api_key, "localhost:50000");
        
        // Set up event handler
        auto handler = std::make_shared<ExampleEventHandler>();
        client->set_event_handler(handler);
        
        // Set up error callback
        client->set_error_callback([](const Error& error) {
            std::cout << "[ERROR CALLBACK] " << error.what() << std::endl;
        });
        
        // Initialize client
        std::cout << "Initializing client..." << std::endl;
        client->initialize();
        
        // Test basic operations
        std::cout << "Testing basic event tracking..." << std::endl;
        
        client->track_user_signup("user_001", {
            {"email", std::string("user001@example.com")},
            {"source", std::string("website")},
            {"campaign", std::string("summer2024")}
        });
        
        client->track_feature_used("user_001", "export_data", {
            {"format", std::string("csv")},
            {"size_mb", 2.5}
        });
        
        client->track_revenue("user_001", "order_12345", 99.99, Currency::USD, {
            {"product_id", std::string("premium_plan")},
            {"billing_cycle", std::string("monthly")}
        });
        
        // Test logging
        std::cout << "Testing structured logging..." << std::endl;
        
        client->log_info("payment_service", "Payment processed successfully", {
            {"user_id", std::string("user_001")},
            {"order_id", std::string("order_12345")},
            {"amount", 99.99},
            {"processing_time_ms", int64_t(245)}
        });
        
        client->log_warning("auth_service", "Multiple login attempts detected", {
            {"user_id", std::string("user_001")},
            {"attempts", int64_t(3)},
            {"ip_address", std::string("192.168.1.100")}
        });
        
        client->log_error("database_service", "Connection timeout", {
            {"database", std::string("users_db")},
            {"timeout_ms", int64_t(5000)},
            {"retry_count", int64_t(2)}
        });
        
        // Manual flush to ensure data is sent
        std::cout << "Flushing data..." << std::endl;
        client->flush();
        
        // Wait a moment for async operations
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // Show statistics
        auto stats = client->get_stats();
        std::cout << "Client Statistics:" << std::endl;
        std::cout << "  Events submitted: " << stats.events_submitted << std::endl;
        std::cout << "  Logs submitted: " << stats.logs_submitted << std::endl;
        std::cout << "  Total items: " << stats.get_total_items() << std::endl;
        std::cout << "  Event success rate: " << (stats.get_event_success_rate() * 100) << "%" << std::endl;
        
        // Shutdown client
        std::cout << "Shutting down client..." << std::endl;
        client->shutdown();
        
    } catch (const Error& e) {
        std::cerr << "UserCanal error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Standard error: " << e.what() << std::endl;
    }
}

void demonstrate_high_volume_batching() {
    std::cout << "\n=== High-Volume Batching Demo ===" << std::endl;
    
    std::string api_key = "1234567890abcdef1234567890abcdef";
    
    try {
        // Create high-throughput client
        auto config = Config::high_throughput(api_key)
            .set_endpoint("localhost:50000")
            .set_event_batch_size(50)  // Smaller batches for demo
            .set_log_batch_size(25)
            .set_event_flush_interval(std::chrono::milliseconds(2000))
            .set_system_log_level(SystemLogLevel::INFO);
        
        Client client(config);
        
        auto handler = std::make_shared<ExampleEventHandler>();
        client.set_event_handler(handler);
        
        client.initialize();
        
        std::cout << "Starting high-volume data generation..." << std::endl;
        
        // Random number generator for varied data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> user_dist(1, 100);
        std::uniform_real_distribution<> amount_dist(9.99, 999.99);
        std::uniform_int_distribution<> level_dist(0, 8);
        
        const std::vector<std::string> features = {
            "dashboard", "reports", "export", "import", "settings", "profile", "billing", "support"
        };
        
        const std::vector<std::string> services = {
            "api_gateway", "auth_service", "payment_service", "notification_service", 
            "analytics_service", "user_service", "billing_service", "support_service"
        };
        
        auto start_time = std::chrono::steady_clock::now();
        
        // Generate events and logs rapidly
        for (int i = 0; i < 200; ++i) {
            std::string user_id = "user_" + std::to_string(user_dist(gen));
            
            // Generate random events
            if (i % 3 == 0) {
                std::string feature = features[i % features.size()];
                client.track_feature_used(user_id, feature, {
                    {"session_id", std::string("session_") + std::to_string(i)},
                    {"duration_ms", int64_t(gen() % 30000)},
                    {"success", true}
                });
            }
            
            if (i % 10 == 0) {
                client.track_revenue(user_id, "order_" + std::to_string(i), 
                                   amount_dist(gen), Currency::USD, {
                    {"product_type", std::string("subscription")},
                    {"tier", std::string("premium")}
                });
            }
            
            // Generate random logs
            std::string service = services[i % services.size()];
            LogLevel level = static_cast<LogLevel>(level_dist(gen));
            
            client.log(level, service, "Operation completed", {
                {"operation_id", std::string("op_") + std::to_string(i)},
                {"user_id", user_id},
                {"processing_time_ms", int64_t(gen() % 1000)},
                {"memory_usage_mb", double(gen() % 512) / 10.0}
            });
            
            // Small delay to avoid overwhelming
            if (i % 50 == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // Show queue status
                std::cout << "Progress: " << i << "/200 - Event queue: " 
                          << client.get_event_queue_size() << ", Log queue: " 
                          << client.get_log_queue_size() << std::endl;
            }
        }
        
        auto generation_time = std::chrono::steady_clock::now();
        auto generation_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            generation_time - start_time);
        
        std::cout << "Data generation completed in " << generation_duration.count() << "ms" << std::endl;
        std::cout << "Generated events: " << handler->get_events_sent() << std::endl;
        std::cout << "Generated logs: " << handler->get_logs_sent() << std::endl;
        
        // Final flush and wait for completion
        std::cout << "Performing final flush..." << std::endl;
        client.flush();
        
        // Wait for batches to be sent
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        
        auto final_stats = client.get_stats();
        std::cout << "\nFinal Statistics:" << std::endl;
        std::cout << "  Events submitted: " << final_stats.events_submitted << std::endl;
        std::cout << "  Events sent: " << final_stats.events_sent << std::endl;
        std::cout << "  Logs submitted: " << final_stats.logs_submitted << std::endl;
        std::cout << "  Logs sent: " << final_stats.logs_sent << std::endl;
        std::cout << "  Batches sent: " << final_stats.batches_sent << std::endl;
        std::cout << "  Bytes sent: " << final_stats.bytes_sent << std::endl;
        std::cout << "  Overall success rate: " << (final_stats.get_overall_success_rate() * 100) << "%" << std::endl;
        
        client.shutdown();
        
    } catch (const Error& e) {
        std::cerr << "UserCanal error: " << e.what() << std::endl;
    }
}

void demonstrate_network_resilience() {
    std::cout << "\n=== Network Resilience Demo ===" << std::endl;
    
    std::string api_key = "1234567890abcdef1234567890abcdef";
    
    try {
        // Create client with custom retry settings
        auto config = ConfigBuilder(api_key)
            .endpoint("localhost:50000")  // This may not be running
            .timeouts(std::chrono::milliseconds(2000), 
                     std::chrono::milliseconds(3000),
                     std::chrono::milliseconds(2000))
            .retries(3, std::chrono::milliseconds(1000), 2.0)
            .batching(10, 5, std::chrono::milliseconds(1000), std::chrono::milliseconds(500))
            .system_logging(SystemLogLevel::DEBUG)
            .build();
        
        Client client(config);
        
        // Set up detailed error callback
        client.set_error_callback([](const Error& error) {
            std::cout << "[NETWORK ERROR] " << error.category() << ": " << error.what() << std::endl;
        });
        
        auto handler = std::make_shared<ExampleEventHandler>();
        client.set_event_handler(handler);
        
        std::cout << "Attempting to initialize client with potentially unreachable endpoint..." << std::endl;
        
        try {
            client.initialize();
            std::cout << "Client initialized successfully!" << std::endl;
            
            // Test connection
            if (client.test_connection()) {
                std::cout << "Connection test passed!" << std::endl;
            } else {
                std::cout << "Connection test failed, but client is still functional" << std::endl;
            }
            
        } catch (const NetworkError& e) {
            std::cout << "Network initialization failed (expected): " << e.what() << std::endl;
            std::cout << "In a real scenario, this would trigger retry logic" << std::endl;
            return;
        }
        
        // Send some data to test network operations
        std::cout << "Sending test data..." << std::endl;
        
        for (int i = 0; i < 20; ++i) {
            client.track("test_user", "network_test", {
                {"attempt", int64_t(i)},
                {"timestamp", int64_t(Utils::now_milliseconds())}
            });
            
            client.log_info("test_service", "Network resilience test log", {
                {"test_id", int64_t(i)},
                {"client_state", static_cast<int>(client.get_state())}
            });
        }
        
        // Force flush and see what happens
        client.flush();
        
        // Check health status
        auto health = client.get_health_status();
        std::cout << "\nHealth Status:" << std::endl;
        std::cout << "  Overall healthy: " << (health.overall_healthy ? "Yes" : "No") << std::endl;
        std::cout << "  Network healthy: " << (health.network_healthy ? "Yes" : "No") << std::endl;
        std::cout << "  Batch processing healthy: " << (health.batch_processing_healthy ? "Yes" : "No") << std::endl;
        
        if (!health.issues.empty()) {
            std::cout << "  Issues:" << std::endl;
            for (const auto& issue : health.issues) {
                std::cout << "    - " << issue << std::endl;
            }
        }
        
        client.shutdown();
        
    } catch (const Error& e) {
        std::cout << "Expected error during network resilience test: " << e.what() << std::endl;
    }
}

void demonstrate_utility_classes() {
    std::cout << "\n=== Utility Classes Demo ===" << std::endl;
    
    std::string api_key = "1234567890abcdef1234567890abcdef";
    
    try {
        Client client(Config::development(api_key));
        client.initialize();
        
        // EventTracker utility
        std::cout << "Testing EventTracker utility..." << std::endl;
        EventTracker tracker(client, "demo_user_123");
        
        tracker.track_signup({
            {"method", std::string("email")},
            {"source", std::string("landing_page")}
        });
        
        tracker.track_login({
            {"method", std::string("password")},
            {"remember_me", true}
        });
        
        tracker.track_feature_use("data_export", {
            {"format", std::string("json")},
            {"records", int64_t(15000)}
        });
        
        tracker.track_revenue("premium_upgrade", 29.99, Currency::USD, {
            {"billing_period", std::string("monthly")},
            {"discount_applied", false}
        });
        
        tracker.set_user_properties({
            {"plan", std::string("premium")},
            {"signup_date", std::string("2024-01-15")},
            {"active", true}
        });
        
        // Logger utility
        std::cout << "Testing Logger utility..." << std::endl;
        Logger logger(client, "demo_service");
        
        // Set context for distributed tracing
        ContextId context = Utils::generate_context_id();
        logger.set_context(context);
        
        logger.info("User operation started", {
            {"operation", std::string("data_export")},
            {"user_id", std::string("demo_user_123")}
        });
        
        logger.debug("Processing user data", {
            {"records_processed", int64_t(5000)},
            {"progress", 33.3}
        });
        
        logger.warning("Large dataset detected", {
            {"total_records", int64_t(15000)},
            {"estimated_time_minutes", 5}
        });
        
        logger.info("User operation completed", {
            {"operation", std::string("data_export")},
            {"duration_ms", int64_t(4520)},
            {"success", true}
        });
        
        // Test RAII flushing
        std::cout << "Testing ScopedFlush utility..." << std::endl;
        {
            ScopedFlush scoped_flush(client);
            
            // This data will be automatically flushed when scope exits
            client.track("scope_test_user", "scoped_operation", {
                {"auto_flush", true}
            });
            
            client.log_info("scope_test", "This will be auto-flushed");
            
            std::cout << "Exiting scope - auto-flush will trigger..." << std::endl;
        }
        std::cout << "Scope exited - data should be flushed" << std::endl;
        
        // Wait a moment for operations to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        auto final_stats = client.get_stats();
        std::cout << "Utility demo completed - Total items processed: " << final_stats.get_total_items() << std::endl;
        
        client.shutdown();
        
    } catch (const Error& e) {
        std::cerr << "Error in utility demo: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "UserCanal C++ SDK - Phase 2 Networking & Batching Demo" << std::endl;
    std::cout << "Version: " << VERSION << std::endl;
    std::cout << "========================================================" << std::endl;
    
    try {
        demonstrate_basic_client_operations();
        demonstrate_high_volume_batching();
        demonstrate_network_resilience();
        demonstrate_utility_classes();
        
        std::cout << "\n=== Phase 2 Demo Completed Successfully! ===" << std::endl;
        std::cout << "All networking and batch management components are working." << std::endl;
        std::cout << "\nNote: Some network operations may show errors if no collector is running," << std::endl;
        std::cout << "but this demonstrates the error handling and resilience features." << std::endl;
        
    } catch (const Error& e) {
        std::cerr << "\nDemo failed with UserCanal error: " << e.what() << std::endl;
        std::cerr << "Error category: " << e.category() << std::endl;
        return 1;
        
    } catch (const std::exception& e) {
        std::cerr << "\nDemo failed with standard exception: " << e.what() << std::endl;
        return 1;
        
    } catch (...) {
        std::cerr << "\nDemo failed with unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
}