// examples/phase3_basic.cpp
// Basic example demonstrating Phase 3 features that are currently working
// Shows pipeline processing basics and hooks integration

#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include "usercanal/usercanal.hpp"

using namespace usercanal;

void demonstrate_basic_client() {
    std::cout << "\n=== Basic Enhanced Client Demo ===" << std::endl;
    
    // Initialize basic client
    std::string api_key = "1234567890abcdef1234567890abcdef";
    Client client(api_key);
    client.initialize();
    
    // Track some events
    client.track("user_123", "feature_used", {
        {"feature", std::string("export")},
        {"format", std::string("csv")},
        {"rows", int64_t(1500)}
    });
    
    client.identify("user_123", {
        {"name", std::string("John Doe")},
        {"plan", std::string("premium")},
        {"tier", std::string("gold")}
    });
    
    // Structured logging
    client.log(LogLevel::INFO, "user_service", "User performed export", {
        {"user_id", std::string("user_123")},
        {"operation", std::string("export")},
        {"duration_ms", int64_t(2500)}
    });
    
    client.log(LogLevel::DEBUG, "analytics_service", "Processing user event", {
        {"event_type", std::string("feature_used")},
        {"pipeline_enabled", true}
    });
    
    // Get client statistics
    auto stats = client.get_stats();
    std::cout << "Events submitted: " << stats.events_submitted << std::endl;
    std::cout << "Logs submitted: " << stats.logs_submitted << std::endl;
    
    // Flush and shutdown
    client.flush();
    client.shutdown();
}

void demonstrate_pipeline_concepts() {
    std::cout << "\n=== Pipeline Concepts Demo ===" << std::endl;
    
    // Create event and log contexts manually to show the concept
    std::cout << "Creating EventHookContext..." << std::endl;
    EventHookContext event_context("user_456", "signup_completed", {
        {"source", std::string("website")},
        {"campaign", std::string("summer2024")},
        {"referrer", std::string("google")}
    });
    
    std::cout << "Event: " << event_context.get_event_name() << std::endl;
    std::cout << "User: " << event_context.get_user_id() << std::endl;
    std::cout << "Properties count: " << event_context.get_properties().size() << std::endl;
    
    // Demonstrate context modification
    event_context.add_property("processed_at", static_cast<int64_t>(Utils::now_milliseconds()));
    event_context.add_property("sdk_version", std::string("1.0.0"));
    
    if (!event_context.has_errors()) {
        std::cout << "Event context is valid" << std::endl;
    }
    
    std::cout << "Creating LogHookContext..." << std::endl;
    LogHookContext log_context(LogLevel::INFO, "auth_service", "User authentication successful", {
        {"user_id", std::string("user_456")},
        {"method", std::string("oauth2")},
        {"provider", std::string("google")}
    });
    
    std::cout << "Log level: " << Utils::log_level_to_string(log_context.get_level()) << std::endl;
    std::cout << "Service: " << log_context.get_service() << std::endl;
    std::cout << "Message: " << log_context.get_message() << std::endl;
    
    // Add enrichment data
    log_context.add_data("enriched_at", static_cast<int64_t>(Utils::now_milliseconds()));
    log_context.add_data("hostname", Utils::get_hostname());
    
    if (!log_context.has_errors()) {
        std::cout << "Log context is valid" << std::endl;
    }
}

void demonstrate_hook_phases() {
    std::cout << "\n=== Hook Phases Demo ===" << std::endl;
    
    // Show the different hook phases
    std::vector<HookPhase> phases = {
        HookPhase::PRE_VALIDATION,
        HookPhase::POST_VALIDATION,
        HookPhase::PRE_SERIALIZATION,
        HookPhase::POST_SERIALIZATION,
        HookPhase::PRE_SEND,
        HookPhase::POST_SEND,
        HookPhase::ON_ERROR,
        HookPhase::ON_RETRY
    };
    
    std::cout << "Available hook phases:" << std::endl;
    for (size_t i = 0; i < phases.size(); ++i) {
        std::cout << "  Phase " << i << ": " << static_cast<int>(phases[i]) << std::endl;
    }
    
    // Show hook results
    std::vector<HookResult> results = {
        HookResult::CONTINUE,
        HookResult::SKIP,
        HookResult::ABORT,
        HookResult::RETRY,
        HookResult::TRANSFORM
    };
    
    std::cout << "Available hook results:" << std::endl;
    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << "  Result " << i << ": " << static_cast<int>(results[i]) << std::endl;
    }
}

void demonstrate_validation_concepts() {
    std::cout << "\n=== Validation Concepts Demo ===" << std::endl;
    
    // Test various validation scenarios
    std::cout << "Testing API key validation:" << std::endl;
    std::cout << "  Valid key: " << (Utils::is_valid_api_key("1234567890abcdef1234567890abcdef") ? "YES" : "NO") << std::endl;
    std::cout << "  Invalid key: " << (Utils::is_valid_api_key("invalid") ? "YES" : "NO") << std::endl;
    
    std::cout << "Testing user ID validation:" << std::endl;
    std::cout << "  Valid user: " << (Utils::is_valid_user_id("user_123") ? "YES" : "NO") << std::endl;
    std::cout << "  Empty user: " << (Utils::is_valid_user_id("") ? "YES" : "NO") << std::endl;
    
    // Create a context with validation errors
    EventHookContext invalid_context("", "", {});
    invalid_context.add_error("Empty user ID");
    invalid_context.add_error("Empty event name");
    
    std::cout << "Context with errors:" << std::endl;
    std::cout << "  Has errors: " << (invalid_context.has_errors() ? "YES" : "NO") << std::endl;
    
    if (invalid_context.has_errors()) {
        const auto& errors = invalid_context.get_errors();
        for (size_t i = 0; i < errors.size(); ++i) {
            std::cout << "  Error " << i + 1 << ": " << errors[i] << std::endl;
        }
    }
}

void demonstrate_performance_features() {
    std::cout << "\n=== Performance Features Demo ===" << std::endl;
    
    // Demonstrate high-volume event creation
    const int num_events = 1000;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::cout << "Creating " << num_events << " event contexts..." << std::endl;
    
    for (int i = 0; i < num_events; ++i) {
        EventHookContext context("user_" + std::to_string(i), "test_event", {
            {"index", int64_t(i)},
            {"batch", std::string("performance_test")},
            {"timestamp", static_cast<int64_t>(Utils::now_milliseconds())}
        });
        
        // Simulate some processing
        context.add_property("processed", true);
        context.add_property("processing_time", static_cast<int64_t>(i % 100));
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "Created " << num_events << " contexts in " << duration.count() << "ms" << std::endl;
    std::cout << "Rate: " << (num_events * 1000 / duration.count()) << " contexts/second" << std::endl;
}

int main() {
    std::cout << "UserCanal C++ SDK - Phase 3 Basic Features Demo" << std::endl;
    std::cout << "Version: 1.0.0 (Phase 3)" << std::endl;
    std::cout << "================================================" << std::endl;
    
    try {
        demonstrate_basic_client();
        demonstrate_pipeline_concepts();
        demonstrate_hook_phases();
        demonstrate_validation_concepts();
        demonstrate_performance_features();
        
        std::cout << "\n=== Demo completed successfully! ===" << std::endl;
        std::cout << "Phase 3 basic features are working correctly." << std::endl;
        std::cout << "The enhanced client provides pipeline processing," << std::endl;
        std::cout << "hooks system integration, and advanced validation." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error during demo: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}