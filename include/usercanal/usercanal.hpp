// include/usercanal/usercanal.hpp
// Purpose: Main header file for the UserCanal C++ SDK
// This is the primary include for users of the SDK

#pragma once

#include "types.hpp"
#include "config.hpp"
#include "errors.hpp"
#include "client.hpp"
#include "utils.hpp"
#include "batch.hpp"
#include "network.hpp"

namespace usercanal {

// Version information
constexpr const char* VERSION = "1.0.0";
constexpr int VERSION_MAJOR = 1;
constexpr int VERSION_MINOR = 0;
constexpr int VERSION_PATCH = 0;

// Get SDK version
inline std::string version() {
    return VERSION;
}

} // namespace usercanal