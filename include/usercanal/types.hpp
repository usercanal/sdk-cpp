// include/usercanal/types.hpp
// Purpose: Core types, constants, and enums for the UserCanal C++ SDK
// Optimized for high-performance logging and analytics with minimal overhead

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <cstdint>
#include <memory>
#include <variant>

namespace usercanal {

// Forward declarations
class Properties;
class Error;

// Type aliases for clarity and performance
using ApiKey = std::string;
using UserId = std::string;
using GroupId = std::string;
using OrderId = std::string;
using ServiceName = std::string;
using HostName = std::string;
using Timestamp = uint64_t;
using BatchId = uint64_t;
using ContextId = uint64_t;

// Properties type - optimized for performance
using PropertyValue = std::variant<
    std::string,
    int64_t,
    double,
    bool,
    std::nullptr_t
>;

class Properties {
public:
    using Container = std::unordered_map<std::string, PropertyValue>;
    using iterator = Container::iterator;
    using const_iterator = Container::const_iterator;

    Properties() = default;
    Properties(std::initializer_list<std::pair<const std::string, PropertyValue>> init)
        : data_(init) {}

    // Element access
    PropertyValue& operator[](const std::string& key) { return data_[key]; }
    const PropertyValue& at(const std::string& key) const { return data_.at(key); }

    // Iterators
    iterator begin() { return data_.begin(); }
    const_iterator begin() const { return data_.begin(); }
    iterator end() { return data_.end(); }
    const_iterator end() const { return data_.end(); }

    // Capacity
    bool empty() const { return data_.empty(); }
    size_t size() const { return data_.size(); }

    // Modifiers
    void clear() { data_.clear(); }
    std::pair<iterator, bool> insert(const std::pair<std::string, PropertyValue>& value) {
        return data_.insert(value);
    }

    // Utility methods
    bool contains(const std::string& key) const {
        return data_.find(key) != data_.end();
    }

private:
    Container data_;
};

// Schema type enumeration (matches FlatBuffers schema)
enum class SchemaType : uint8_t {
    UNKNOWN = 0,
    EVENT = 1,
    LOG = 2,
    METRIC = 3,
    INVENTORY = 4
};

// Event type enumeration (matches FlatBuffers schema)
enum class EventType : uint8_t {
    UNKNOWN = 0,
    TRACK = 1,
    IDENTIFY = 2,
    GROUP = 3,
    ALIAS = 4,
    ENRICH = 5
};

// Log level enumeration (RFC 5424 + TRACE)
enum class LogLevel : uint8_t {
    EMERGENCY = 0,  // System is unusable
    ALERT = 1,      // Action must be taken immediately
    CRITICAL = 2,   // Critical conditions
    ERROR = 3,      // Error conditions
    WARNING = 4,    // Warning conditions
    NOTICE = 5,     // Normal but significant condition
    INFO = 6,       // Informational messages
    DEBUG = 7,      // Debug-level messages
    TRACE = 8       // Detailed tracing information
};

// Log event type enumeration
enum class LogEventType : uint8_t {
    UNKNOWN = 0,
    LOG = 1,
    ENRICH = 2
};

// Currency codes for revenue tracking
enum class Currency {
    USD, EUR, GBP, JPY, CAD, AUD, CHF, CNY, SEK, NZD, MXN, SGD, HKD, NOK, TRY,
    ZAR, BRL, INR, KRW, PLN, CZK, DKK, HUF, ILS, CLP, PHP, AED, THB, MYR, IDR
};

// Revenue type enumeration
enum class RevenueType {
    UNKNOWN = 0,
    ONE_TIME = 1,
    SUBSCRIPTION = 2,
    USAGE_BASED = 3,
    FREEMIUM_UPGRADE = 4
};

// Pre-defined event names for common user actions
namespace EventNames {
    constexpr const char* USER_SIGNED_UP = "user_signed_up";
    constexpr const char* USER_LOGGED_IN = "user_logged_in";
    constexpr const char* USER_LOGGED_OUT = "user_logged_out";
    constexpr const char* FEATURE_USED = "feature_used";
    constexpr const char* PAGE_VIEWED = "page_viewed";
    constexpr const char* BUTTON_CLICKED = "button_clicked";
    constexpr const char* FORM_SUBMITTED = "form_submitted";
    constexpr const char* PURCHASE_COMPLETED = "purchase_completed";
    constexpr const char* SUBSCRIPTION_STARTED = "subscription_started";
    constexpr const char* SUBSCRIPTION_CANCELLED = "subscription_cancelled";
    constexpr const char* ERROR_OCCURRED = "error_occurred";
    constexpr const char* EXPERIMENT_VIEWED = "experiment_viewed";
    constexpr const char* SEARCH_PERFORMED = "search_performed";
    constexpr const char* CONTENT_SHARED = "content_shared";
    constexpr const char* NOTIFICATION_RECEIVED = "notification_received";
}

// Property name constants for consistency
namespace PropertyNames {
    constexpr const char* USER_ID = "user_id";
    constexpr const char* GROUP_ID = "group_id";
    constexpr const char* SESSION_ID = "session_id";
    constexpr const char* DEVICE_ID = "device_id";
    constexpr const char* TIMESTAMP = "timestamp";
    constexpr const char* EVENT_TYPE = "event_type";
    constexpr const char* SOURCE = "source";
    constexpr const char* SERVICE = "service";
    constexpr const char* LEVEL = "level";
    constexpr const char* MESSAGE = "message";
    constexpr const char* ERROR_CODE = "error_code";
    constexpr const char* ERROR_MESSAGE = "error_message";
    constexpr const char* STACK_TRACE = "stack_trace";
    constexpr const char* REQUEST_ID = "request_id";
    constexpr const char* CORRELATION_ID = "correlation_id";
    constexpr const char* DURATION_MS = "duration_ms";
    constexpr const char* STATUS_CODE = "status_code";
    constexpr const char* URL = "url";
    constexpr const char* METHOD = "method";
    constexpr const char* USER_AGENT = "user_agent";
    constexpr const char* IP_ADDRESS = "ip_address";
    constexpr const char* COUNTRY = "country";
    constexpr const char* REGION = "region";
    constexpr const char* CITY = "city";
    constexpr const char* TIMEZONE = "timezone";
    constexpr const char* LANGUAGE = "language";
    constexpr const char* CURRENCY = "currency";
    constexpr const char* AMOUNT = "amount";
    constexpr const char* ORDER_ID = "order_id";
    constexpr const char* PRODUCT_ID = "product_id";
    constexpr const char* CATEGORY = "category";
    constexpr const char* QUANTITY = "quantity";
}

// Utility functions for common operations
namespace Utils {

// Get current timestamp in milliseconds
inline Timestamp now_milliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// Convert currency enum to string
std::string currency_to_string(Currency currency);

// Convert log level enum to string
std::string log_level_to_string(LogLevel level);

// Convert event type enum to string
std::string event_type_to_string(EventType type);

// Parse currency from string
Currency string_to_currency(const std::string& currency_str);

// Validate API key format (should be 32 hex characters)
bool is_valid_api_key(const std::string& api_key);

// Validate user ID (non-empty, reasonable length)
bool is_valid_user_id(const std::string& user_id);

// Generate a random batch ID
BatchId generate_batch_id();

// Generate a random context ID
ContextId generate_context_id();

} // namespace Utils

// Performance statistics
struct Stats {
    uint64_t events_sent = 0;
    uint64_t logs_sent = 0;
    uint64_t batches_sent = 0;
    uint64_t network_errors = 0;
    uint64_t validation_errors = 0;
    uint64_t bytes_sent = 0;
    std::chrono::milliseconds total_network_time{0};
    std::chrono::milliseconds last_flush_time{0};

    // Calculate average batch size
    double average_batch_size() const {
        return batches_sent > 0 ? static_cast<double>(bytes_sent) / batches_sent : 0.0;
    }

    // Calculate success rate
    double success_rate() const {
        uint64_t total_operations = events_sent + logs_sent;
        return total_operations > 0 ? 
            static_cast<double>(total_operations - validation_errors - network_errors) / total_operations : 1.0;
    }
};

} // namespace usercanal