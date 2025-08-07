// include/usercanal/usercanal.hpp
// Purpose: Main header file for the UserCanal C++ SDK
// This is the primary include for users of the SDK

#pragma once

#include "types.hpp"
#include "config.hpp"
#include "errors.hpp"
#include "client.hpp"
#include "utils.hpp"
#include "pipeline.hpp"
#include "observability.hpp"
#include "hooks.hpp"
#include "session.hpp"

namespace usercanal {

// Version information
constexpr const char* VERSION = "1.0.0";
constexpr int VERSION_MAJOR = 1;
constexpr int VERSION_MINOR = 0;
constexpr int VERSION_PATCH = 0;

// Feature flags for Phase 3
constexpr bool PIPELINE_FEATURE_ENABLED = true;
constexpr bool OBSERVABILITY_FEATURE_ENABLED = true;
constexpr bool HOOKS_FEATURE_ENABLED = true;
constexpr bool ADVANCED_VALIDATION_ENABLED = true;

// SDK initialization and cleanup
namespace SDK {

// Initialize the SDK (optional - clients can be created directly)
bool initialize();

// Cleanup SDK resources (optional - automatic cleanup on exit)
void cleanup();

// Get SDK version
std::string version();

// Check if SDK is initialized
bool is_initialized();

// Advanced SDK features (Phase 3)
namespace Advanced {

// Initialize advanced features
void initialize_pipeline_processor();
void initialize_observability();
void initialize_hooks_system();

// Access advanced components
PipelineProcessor& get_pipeline_processor();
ServerObservabilityManager& get_observability_manager();
HookManager& get_hook_manager();
SessionManager& get_session_manager();
LifecycleManager& get_lifecycle_manager();

// Feature toggles
void enable_pipeline_processing(bool enabled = true);
void enable_observability(bool enabled = true);
void enable_hooks_system(bool enabled = true);

// Quick setup methods
void setup_basic_validation();
void setup_performance_monitoring();
void setup_privacy_compliance();
void setup_comprehensive_pipeline();

} // namespace Advanced

} // namespace SDK

} // namespace usercanal