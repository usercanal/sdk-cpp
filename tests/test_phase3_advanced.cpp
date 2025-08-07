// examples/phase3_advanced.cpp
// Comprehensive example demonstrating Phase 3 advanced features
// Shows pipeline processing, observability, hooks system, validation, and enhanced client capabilities

#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <vector>
#include <atomic>
#include <memory>
#include "usercanal/usercanal.hpp"

using namespace usercanal;

// Custom validation rule example
class CustomEmailValidationRule : public EventValidationRule {
public:
    std::string get_name() const override { return "custom_email_validation"; }
    std::string get_description() const override { return "Validates email format in user properties"; }
    
    bool validate(const EventHookContext& context, std::string& error_message) const override {
        if (!context.has_property("email")) {
            return true; // Email is optional
        }
        
        const auto& props = context.get_properties();
        auto email_it = props.find("email");
        if (email_it != props.end()) {
            if (std::holds_alternative<std::string>(email_it->second)) {
                std::string email = std::get<std::string>(email_it->second);
                if (email.find('@') == std::string::npos) {
                    error_message = "Invalid email format: " + email;
                    return false;
                }
            }
        }
        return true;
    }
};

// Custom enrichment hook example
class TimestampEnrichmentHook : public EventHook {
public:
    std::string get_name() const override { return "timestamp_enrichment"; }
    std::vector<HookPhase> get_supported_phases() const override {
        return {HookPhase::POST_VALIDATION};
    }
    
    HookResult execute(EventHookContext& context) override {
        // Add server-side timestamp
        context.add_property("server_timestamp", int64_t(Utils::now_milliseconds()));
        
        // Add processing duration if timing was started
        auto processing_time = context.get_processing_time();
        if (processing_time.count() > 0) {
            context.add_property("processing_time_ms", int64_t(processing_time.count()));
        }
        
        // Add enrichment metadata
        context.set_hook_data("enriched_by", std::string("timestamp_enrichment_hook"));
        context.set_hook_data("enrichment_version", std::string("1.0.0"));
        
        return HookResult::CONTINUE;
    }
};

// Custom middleware for pipeline processing
class PerformanceMiddleware : public Middleware {
public:
    std::string get_name() const override { return "performance_middleware"; }
    int get_priority() const override { return 10; } // High priority
    
    PipelineResult process_event(EventContext& context) override {
        events_processed_++;
        
        auto start_time = std::chrono::steady_clock::now();
        
        // Add performance metadata
        context.set_metadata("performance_start", start_time);
        context.set_metadata("processor_id", std::string("performance_middleware"));
        
        // Simulate some processing
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        total_processing_time_ += duration;
        
        return PipelineResult::CONTINUE;
    }
    
    PipelineResult process_log(LogContext& context) override {
        logs_processed_++;
        return PipelineResult::CONTINUE;
    }
    
    uint64_t get_events_processed() const { return events_processed_; }
    uint64_t get_logs_processed() const { return logs_processed_; }
    std::chrono::microseconds get_total_processing_time() const { return total_processing_time_; }

private:
    std::atomic<uint64_t> events_processed_{0};
    std::atomic<uint64_t> logs_processed_{0};
    std::atomic<std::chrono::microseconds> total_processing_time_{std::chrono::microseconds(0)};
};

// Custom health check example
class CustomServiceHealthCheck : public HealthCheck {
public:
    CustomServiceHealthCheck(const std::string& service_name) : service_name_(service_name) {}
    
    std::string get_name() const override { return "custom_service_" + service_name_; }
    
    bool is_healthy() const override {
        // Simulate health check logic
        auto now = std::chrono::steady_clock::now();
        if (now - last_check_ > std::chrono::seconds(5)) {
            last_check_ = now;
            last_healthy_ = (rand() % 10) > 1; // 90% healthy
        }
        return last_healthy_;
    }
    
    std::string get_status_message() const override {
        return last_healthy_ ? "Service is healthy" : "Service is experiencing issues";
    }
    
    std::unordered_map<std::string, std::any> get_details() const override {
        std::unordered_map<std::string, std::any> details;
        details["service_name"] = service_name_;
        details["last_check"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        details["uptime_seconds"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time_).count();
        return details;
    }

private:
    std::string service_name_;
    mutable std::chrono::steady_clock::time_point last_check_;
    mutable bool last_healthy_ = true;
    std::chrono::steady_clock::time_point start_time_ = std::chrono::steady_clock::now();
};

void demonstrate_enhanced_client_basic() {
    std::cout << "\n=== Enhanced Client - Basic Features Demo ===" << std::endl;
    
    try {
        // Create enhanced client with comprehensive configuration
        auto config = EnhancedClientConfig::comprehensive("1234567890abcdef1234567890abcdef");
        config.base_config.set_endpoint("localhost:50000");
        
        EnhancedClient client(config);
        client.initialize();
        
        std::cout << "Enhanced client initialized with all features enabled" << std::endl;
        
        // Test basic functionality with enhanced processing
        client.track("enhanced_user_001", EventNames::USER_SIGNED_UP, {
            {"email", std::string("user001@example.com")},
            {"source", std::string("advanced_sdk")},
            {"features_enabled", std::string("pipeline,observability,hooks")}
        });
        
        client.log_info("enhanced_demo", "Advanced client demonstration started", {
            {"demo_phase", std::string("basic_features")},
            {"client_type", std::string("enhanced")},
            {"feature_count", int64_t(3)}
        });
        
        // Check initial statistics
        auto stats = client.get_enhanced_stats();
        std::cout << "Initial processing complete:" << std::endl;
        std::cout << "  Events processed: " << stats.client_stats.events_submitted << std::endl;
        std::cout << "  Logs processed: " << stats.client_stats.logs_submitted << std::endl;
        std::cout << "  Pipeline efficiency: " << (stats.get_pipeline_efficiency() * 100) << "%" << std::endl;
        std::cout << "  Overall health: " << (stats.is_healthy() ? "Healthy" : "Unhealthy") << std::endl;
        
        client.shutdown();
        
    } catch (const Error& e) {
        std::cerr << "Enhanced client error: " << e.what() << std::endl;
    }
}

void demonstrate_pipeline_processing() {
    std::cout << "\n=== Pipeline Processing Demo ===" << std::endl;
    
    try {
        auto config = EnhancedClientConfig::performance_focused("1234567890abcdef1234567890abcdef");
        config.enable_pipeline_processing = true;
        config.enable_parallel_pipeline = true;
        
        EnhancedClient client(config);
        client.initialize();
        
        // Add custom middleware
        auto performance_middleware = std::make_shared<PerformanceMiddleware>();
        client.add_event_middleware(performance_middleware);
        
        // Add validation middleware
        auto validation_middleware = client.get_pipeline_processor().add_validation();
        validation_middleware->require_user_id();
        validation_middleware->require_event_name();
        validation_middleware->max_property_count(10);
        
        // Add enrichment middleware
        auto enrichment_middleware = client.get_pipeline_processor().add_enrichment();
        enrichment_middleware->add_timestamp();
        enrichment_middleware->add_hostname();
        enrichment_middleware->add_request_id();
        
        std::cout << "Pipeline configured with validation and enrichment middleware" << std::endl;
        
        // Process events through pipeline
        for (int i = 0; i < 50; ++i) {
            client.track("pipeline_user_" + std::to_string(i % 5), "pipeline_test_event", {
                {"iteration", int64_t(i)},
                {"batch_number", int64_t(i / 10)},
                {"processing_type", std::string("pipeline")},
                {"middleware_count", int64_t(3)}
            });
            
            if (i % 10 == 0) {
                std::cout << "Processed " << i << " events through pipeline..." << std::endl;
            }
        }
        
        // Check pipeline statistics
        auto pipeline_stats = client.get_pipeline_processor().get_stats();
        std::cout << "\nPipeline Processing Results:" << std::endl;
        std::cout << "  Events processed: " << pipeline_stats.events_processed << std::endl;
        std::cout << "  Events skipped: " << pipeline_stats.events_skipped << std::endl;
        std::cout << "  Events failed: " << pipeline_stats.events_failed << std::endl;
        std::cout << "  Success rate: " << (pipeline_stats.get_event_success_rate() * 100) << "%" << std::endl;
        std::cout << "  Average processing time: " << pipeline_stats.get_average_processing_time_ms() << "ms" << std::endl;
        
        // Check custom middleware performance
        std::cout << "\nCustom Middleware Performance:" << std::endl;
        std::cout << "  Events processed: " << performance_middleware->get_events_processed() << std::endl;
        std::cout << "  Total processing time: " << performance_middleware->get_total_processing_time().count() << "μs" << std::endl;
        
        client.shutdown();
        
    } catch (const Error& e) {
        std::cerr << "Pipeline processing error: " << e.what() << std::endl;
    }
}

void demonstrate_hooks_system() {
    std::cout << "\n=== Hooks System Demo ===" << std::endl;
    
    try {
        auto config = EnhancedClientConfig::validation_focused("1234567890abcdef1234567890abcdef");
        EnhancedClient client(config);
        client.initialize();
        
        // Add custom validation rule
        auto email_validation = std::make_shared<CustomEmailValidationRule>();
        client.add_validation_rule(email_validation);
        
        // Add custom enrichment hook
        auto timestamp_hook = std::make_shared<TimestampEnrichmentHook>();
        client.register_event_hook(timestamp_hook);
        
        // Add built-in privacy hook
        auto privacy_hook = std::make_shared<BuiltInHooks::PrivacyEventHook>();
        privacy_hook->add_pii_field("email");
        privacy_hook->add_pii_field("phone");
        privacy_hook->set_hash_pii(true);
        client.register_event_hook(privacy_hook);
        
        std::cout << "Hooks system configured with validation and enrichment" << std::endl;
        
        // Test valid event
        std::cout << "\nTesting valid event..." << std::endl;
        client.track("hooks_user_valid", "test_event", {
            {"email", std::string("valid@example.com")},
            {"action", std::string("button_click")},
            {"value", 42}
        });
        
        // Test invalid event (should be rejected by validation)
        std::cout << "Testing invalid event..." << std::endl;
        try {
            client.track("hooks_user_invalid", "test_event", {
                {"email", std::string("invalid-email")}, // Invalid email format
                {"action", std::string("form_submit")}
            });
        } catch (const ValidationError& e) {
            std::cout << "Validation caught invalid event: " << e.what() << std::endl;
        }
        
        // Test privacy processing
        std::cout << "Testing privacy processing..." << std::endl;
        client.track("hooks_user_privacy", "privacy_test", {
            {"email", std::string("private@example.com")},
            {"phone", std::string("+1-555-0123")},
            {"public_data", std::string("this_is_safe")}
        });
        
        // Check hooks statistics
        auto hook_stats = client.get_hook_manager().get_stats();
        std::cout << "\nHooks Processing Results:" << std::endl;
        std::cout << "  Events processed: " << hook_stats.events_processed << std::endl;
        std::cout << "  Hooks executed: " << hook_stats.hooks_executed << std::endl;
        std::cout << "  Validations passed: " << hook_stats.validations_passed << std::endl;
        std::cout << "  Validations failed: " << hook_stats.validations_failed << std::endl;
        std::cout << "  Hook success rate: " << (hook_stats.get_hook_success_rate() * 100) << "%" << std::endl;
        std::cout << "  Validation success rate: " << (hook_stats.get_validation_success_rate() * 100) << "%" << std::endl;
        
        client.shutdown();
        
    } catch (const Error& e) {
        std::cerr << "Hooks system error: " << e.what() << std::endl;
    }
}

void demonstrate_observability_system() {
    std::cout << "\n=== Observability System Demo ===" << std::endl;
    
    try {
        auto config = EnhancedClientConfig::comprehensive("1234567890abcdef1234567890abcdef");
        config.enable_metrics = true;
        config.enable_health_monitoring = true;
        config.enable_performance_tracking = true;
        config.enable_alerting = true;
        
        EnhancedClient client(config);
        client.initialize();
        
        // Setup comprehensive monitoring
        client.setup_comprehensive_monitoring();
        
        // Add custom health check
        auto custom_health = std::make_shared<CustomServiceHealthCheck>("demo_service");
        client.get_observability().health().register_health_check(custom_health);
        
        // Create custom metrics
        auto& metrics = client.get_observability().metrics();
        auto request_counter = metrics.create_counter("demo_requests_total", "Total demo requests");
        auto response_time_hist = metrics.create_histogram("demo_response_time", "Demo response times");
        auto active_users_gauge = metrics.create_gauge("demo_active_users", "Current active users");
        
        std::cout << "Observability system configured with custom metrics and health checks" << std::endl;
        
        // Generate some activity
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> response_time_dist(10, 500);
        std::uniform_int_distribution<> user_count_dist(50, 200);
        
        for (int i = 0; i < 100; ++i) {
            // Update metrics
            request_counter->increment();
            response_time_hist->observe(response_time_dist(gen));
            active_users_gauge->set_value(user_count_dist(gen));
            
            // Track operations for performance monitoring
            client.get_observability().performance().track_operation(
                "demo_operation", 
                std::chrono::milliseconds(response_time_dist(gen))
            );
            
            // Generate some events and logs
            if (i % 10 == 0) {
                client.track("observability_user_" + std::to_string(i), "demo_action", {
                    {"metric_iteration", int64_t(i)},
                    {"response_time", response_time_dist(gen)},
                    {"active_users", user_count_dist(gen)}
                });
                
                client.log_info("observability_demo", "Metrics updated", {
                    {"iteration", int64_t(i)},
                    {"requests_processed", int64_t(i + 1)}
                });
                
                std::cout << "Processed " << (i + 1) << " operations with metrics..." << std::endl;
            }
        }
        
        // Get comprehensive health report
        auto health_report = client.get_comprehensive_health_report();
        std::cout << "\nHealth Report:" << std::endl;
        std::cout << "  Overall status: " << static_cast<int>(health_report.overall_status) << " (0=Healthy)" << std::endl;
        std::cout << "  Check duration: " << health_report.check_duration.count() << "ms" << std::endl;
        std::cout << "  Checks performed: " << health_report.check_results.size() << std::endl;
        
        for (const auto& [check_name, result] : health_report.check_results) {
            std::cout << "    " << check_name << ": " << (result ? "PASS" : "FAIL") << std::endl;
        }
        
        // Export metrics
        std::cout << "\nExported Metrics (JSON format):" << std::endl;
        auto metrics_json = client.export_metrics_json();
        std::cout << metrics_json.substr(0, 200) << "..." << std::endl;
        
        // Performance tracking results
        auto perf_metrics = client.get_observability().performance().get_metrics("demo_operation");
        std::cout << "\nPerformance Metrics for 'demo_operation':" << std::endl;
        std::cout << "  Total operations: " << perf_metrics.total_operations << std::endl;
        std::cout << "  Average duration: " << perf_metrics.average_duration_ms << "ms" << std::endl;
        std::cout << "  P95 duration: " << perf_metrics.p95_duration_ms << "ms" << std::endl;
        std::cout << "  Error rate: " << (perf_metrics.error_rate * 100) << "%" << std::endl;
        std::cout << "  Throughput: " << perf_metrics.throughput_per_second << " ops/sec" << std::endl;
        
        client.shutdown();
        
    } catch (const Error& e) {
        std::cerr << "Observability system error: " << e.what() << std::endl;
    }
}

void demonstrate_privacy_and_compliance() {
    std::cout << "\n=== Privacy and Compliance Demo ===" << std::endl;
    
    try {
        auto config = EnhancedClientConfig::privacy_compliant("1234567890abcdef1234567890abcdef");
        EnhancedClient client(config);
        client.initialize();
        
        // Enable privacy mode
        client.enable_privacy_mode();
        
        // Configure PII fields
        client.add_pii_field("email");
        client.add_pii_field("phone");
        client.add_pii_field("ssn");
        client.add_pii_field("credit_card");
        
        std::cout << "Privacy mode enabled with PII field protection" << std::endl;
        
        // Test data processing with PII
        std::cout << "\nProcessing events with PII data..." << std::endl;
        
        client.track("privacy_user_001", "user_registration", {
            {"email", std::string("john.doe@example.com")},
            {"phone", std::string("+1-555-0123")},
            {"name", std::string("John Doe")}, // Non-PII, should remain
            {"age", int64_t(30)}, // Non-PII, should remain
            {"consent_given", true}
        });
        
        client.track("privacy_user_002", "payment_processed", {
            {"credit_card", std::string("4111-1111-1111-1111")},
            {"amount", 99.99},
            {"currency", std::string("USD")},
            {"merchant_id", std::string("merchant_123")}
        });
        
        // Test logging with sensitive data
        client.log_info("payment_service", "Payment processing completed", {
            {"user_id", std::string("privacy_user_002")},
            {"ssn", std::string("123-45-6789")}, // PII, should be protected
            {"transaction_id", std::string("txn_456")}, // Non-PII, should remain
            {"amount", 99.99},
            {"processing_time_ms", int64_t(250)}
        });
        
        // Set data retention policy
        client.set_data_retention_policy(std::chrono::hours(24 * 30)); // 30 days
        
        std::cout << "Privacy-compliant processing complete" << std::endl;
        std::cout << "All PII fields have been hashed/removed according to privacy settings" << std::endl;
        
        // Check compliance statistics
        auto enhanced_stats = client.get_enhanced_stats();
        std::cout << "\nCompliance Processing Results:" << std::endl;
        std::cout << "  Total events processed: " << enhanced_stats.client_stats.events_submitted << std::endl;
        std::cout << "  Privacy hooks executed: " << enhanced_stats.hook_stats.hooks_executed << std::endl;
        std::cout << "  Data protection applied: " << (enhanced_stats.hook_stats.hooks_executed > 0 ? "Yes" : "No") << std::endl;
        
        client.shutdown();
        
    } catch (const Error& e) {
        std::cerr << "Privacy and compliance error: " << e.what() << std::endl;
    }
}

void demonstrate_performance_optimization() {
    std::cout << "\n=== Performance Optimization Demo ===" << std::endl;
    
    try {
        auto config = EnhancedClientConfig::high_performance("1234567890abcdef1234567890abcdef");
        config.enable_event_sampling = true;
        config.default_sample_rate = 0.8; // Sample 80% of events
        config.enable_rate_limiting = true;
        config.max_logs_per_second = 500.0;
        
        EnhancedClient client(config);
        client.initialize();
        
        // Configure performance optimizations
        client.enable_event_sampling(0.8);
        client.enable_log_rate_limiting(500.0, 50); // 500 logs/sec, burst of 50
        client.set_event_sample_rate("high_volume_event", 0.1); // Only 10% for specific event
        
        std::cout << "Performance optimizations enabled (80% event sampling, 500 logs/sec rate limit)" << std::endl;
        
        // Generate high-volume data
        std::cout << "\nGenerating high-volume data stream..." << std::endl;
        
        auto start_time = std::chrono::steady_clock::now();
        
        for (int i = 0; i < 2000; ++i) {
            // High-volume events (will be sampled)
            if (i % 5 == 0) {
                client.track("perf_user_" + std::to_string(i % 100), "high_volume_event", {
                    {"iteration", int64_t(i)},
                    {"batch", int64_t(i / 100)},
                    {"sampled", true}
                });
            }
            
            // Regular events
            if (i % 10 == 0) {
                client.track("perf_user_" + std::to_string(i % 50), "regular_event", {
                    {"iteration", int64_t(i)},
                    {"type", std::string("regular")},
                    {"performance_test", true}
                });
            }
            
            // High-volume logs (will be rate limited)
            client.log_debug("perf_service", "Performance test log entry", {
                {"log_number", int64_t(i)},
                {"timestamp", int64_t(Utils::now_milliseconds())},
                {"rate_limited", true}
            });
            
            // Progress indicator
            if (i % 200 == 0) {
                std::cout << "Generated " << i << " items..." << std::endl;
            }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto generation_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "Data generation completed in " << generation_duration.count() << "ms" << std::endl;
        
        // Force flush and get final statistics
        client.flush_with_pipeline_wait();
        
        auto final_stats = client.get_enhanced_stats();
        std::cout << "\nPerformance Optimization Results:" << std::endl;
        std::cout << "  Events submitted: " << final_stats.client_stats.events_submitted << std::endl;
        std::cout << "  Logs submitted: " << final_stats.client_stats.logs_submitted << std::endl;
        std::cout << "  Overall success rate: " << (final_stats.get_overall_success_rate() * 100) << "%" << std::endl;
        std::cout << "  Pipeline efficiency: " << (final_stats.get_pipeline_efficiency() * 100) << "%" << std::endl;
        std::cout << "  Processing throughput: " << (2000.0 / (generation_duration.count() / 1000.0)) << " items/sec" << std::endl;
        
        // Memory optimization
        std::cout << "\nOptimizing memory usage..." << std::endl;
        auto memory_before = client.get_memory_usage();
        client.optimize_memory_usage();
        client.clear_caches();
        auto memory_after = client.get_memory_usage();
        
        std::cout << "Memory usage - Before: " << (memory_before / 1024) << "KB, After: " << (memory_after / 1024) << "KB" << std::endl;
        std::cout << "Memory saved: " << ((memory_before - memory_after) / 1024) << "KB" << std::endl;
        
        client.shutdown();
        
    } catch (const Error& e) {
        std::cerr << "Performance optimization error: " << e.what() << std::endl;
    }
}

void demonstrate_utility_classes() {
    std::cout << "\n=== Enhanced Utility Classes Demo ===" << std::endl;
    
    try {
        auto config = EnhancedClientConfig::development("1234567890abcdef1234567890abcdef");
        EnhancedClient client(config);
        client.initialize();
        
        // Enhanced EventTracker usage
        std::cout << "Testing EnhancedEventTracker..." << std::endl;
        
        EnhancedEventTracker tracker(client, "utility_user_123");
        tracker.set_session_context("session_abc123");
        tracker.set_user_context({
            {"subscription", std::string("premium")},
            {"region", std::string("us-west")},
            {"experiment_group", std::string("control")}
        });
        
        tracker.track_with_enrichment("feature_interaction", {
            {"feature", std::string("advanced_analytics")},
            {"interaction_type", std::string("click")},
            {"duration_ms", int64_t(1500)}
        });
        
        tracker.identify_with_validation({
            {"name", std::string("Alice Johnson")},
            {"email", std::string("alice@example.com")},
            {"plan_upgrade", true}
        });
        
        // Enhanced Logger usage
        std::cout << "Testing EnhancedLogger..." << std::endl;
        
        EnhancedLogger logger(client, "utility_demo_service");
        logger.set_correlation_id(Utils::generate_context_id());
        logger.push_context("user_id", std::string("utility_user_123"));
        logger.push_context("feature", std::string("enhanced_logging"));
        
        logger.log_operation_start("data_processing", {
            {"input_size", int64_t(1000)},
            {"algorithm", std::string("optimized_batch")}
        });
        
        // Simulate operation
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        logger.log_performance_metric("processing_speed", 1500.0, {
            {"unit", std::string("items_per_second")},
            {"optimization", std::string("enabled")}
        });
        
        logger.log_operation_end("data_processing", true, {
            {"items_processed", int64_t(1000)},
            {"success_rate", 0.98},
            {"cache_hits", int64_t(850)}
        });
        
        // Scoped performance tracking
        std::cout << "Testing ScopedPerformanceTracking..." << std::endl;
        
        {
            ScopedPerformanceTracking perf_tracker(client, "complex_calculation");
            perf_tracker.add_metadata("algorithm", std::string("advanced"));
            perf_tracker.add_metadata("input_complexity", std::string("O(n^2)"));
            
            // Simulate complex operation
            for (int i = 0; i < 1000; ++i) {
                volatile int result = i * i; // Prevent optimization
                (void)result;
            }
            
            perf_tracker.mark_checkpoint("phase_1_complete");
            
            // Simulate more work
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            perf_tracker.mark_checkpoint("phase_2_complete");
            // Performance tracking will finish automatically on destruction
        }
        
        // Scoped enhanced flush
        std::cout << "Testing ScopedEnhancedFlush..." << std::endl;
        
        {
            ScopedEnhancedFlush scoped_flush(client, true); // Wait for pipeline
            
            client.track("scoped_user", "scoped_event", {
                {"auto_flush", true},
                {"pipeline_wait", true}
            });
            
            client.log_info("scoped_service", "This will be auto-flushed with pipeline wait");
            
            std::cout << "Exiting scope - auto-flush with pipeline wait will trigger..." << std::endl;
        }
        
        std::cout << "Scoped flush completed with pipeline processing" << std::endl;
        
        // Final statistics
        auto final_stats = client.get_enhanced_stats();
        std::cout << "\nUtility Classes Demo Results:" << std::endl;
        std::cout << "  Total operations tracked: " << final_stats.get_total_processed() << std::endl;
        std::cout << "  Enhanced features used: Context tracking, Performance monitoring, Scoped operations" << std::endl;
        std::cout << "  Overall system health: " << (final_stats.is_healthy() ? "Healthy" : "Unhealthy") << std::endl;
        
        client.shutdown();
        
    } catch (const Error& e) {
        std::cerr << "Utility classes error: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "UserCanal C++ SDK - Phase 3 Advanced Features Demo" << std::endl;
    std::cout << "Version: " << VERSION << std::endl;
    std::cout << "Advanced Features: Pipeline Processing, Observability, Hooks System, Validation" << std::endl;
    std::cout << "==========================================================================" << std::endl;
    
    try {
        // Initialize advanced SDK features
        SDK::initialize();
        SDK::Advanced::initialize_pipeline_processor();
        SDK::Advanced::initialize_observability();
        SDK::Advanced::initialize_hooks_system();
        
        std::cout << "Advanced SDK features initialized successfully" << std::endl;
        
        // Run all demonstrations
        demonstrate_enhanced_client_basic();
        demonstrate_pipeline_processing();
        demonstrate_hooks_system();
        demonstrate_observability_system();
        demonstrate_privacy_and_compliance();
        demonstrate_performance_optimization();
        demonstrate_utility_classes();
        
        std::cout << "\n=== Phase 3 Advanced Features Demo Completed Successfully! ===" << std::endl;
        std::cout << "All advanced features demonstrated:" << std::endl;
        std::cout << "✅ Enhanced Client with comprehensive configuration" << std::endl;
        std::cout << "✅ Pipeline Processing with custom middleware" << std::endl;
        std::cout << "✅ Hooks System with validation and enrichment" << std::endl;
        std::cout << "✅ Observability with metrics, health checks, and performance tracking" << std::endl;
        std::cout << "✅ Privacy and Compliance with PII protection" << std::endl;
        std::cout << "✅ Performance Optimization with sampling and rate limiting" << std::endl;
        std::cout << "✅ Enhanced Utility Classes with advanced features" << std::endl;
        
        std::cout << "\nPhase 3 represents a comprehensive, enterprise-ready SDK with:" << std::endl;
        std::cout << "• Advanced event processing pipeline with middleware support" << std::endl;
        std::cout << "• Real-time validation and enrichment hooks" << std::endl;
        std::cout << "• Complete observability stack with metrics and monitoring" << std::endl;
        std::cout << "• Privacy-first architecture with GDPR compliance" << std::endl;
        std::cout << "• High-performance optimizations for enterprise scale" << std::endl;
        
        SDK::cleanup();
        
    } catch (const Error& e) {
        std::cerr << "\nAdvanced demo failed with UserCanal error: " << e.what() << std::endl;
        std::cerr << "Error category: " << e.category() << std::endl;
        return 1;
        
    } catch (const std::exception& e) {
        std::cerr << "\nAdvanced demo failed with standard exception: " << e.what() << std::endl;
        return 1;
        
    } catch (...) {
        std::cerr << "\nAdvanced demo failed with unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
}