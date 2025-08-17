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
using SessionId = std::vector<uint8_t>; // 16-byte session UUID for correlation

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
    ENRICH = 5,
    CONTEXT = 6
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

// EventAdvanced represents an advanced tracking event with optional overrides
struct EventAdvanced {
    std::string user_id;        // Required - user identifier
    std::string event_name;     // Required - event name
    Properties properties;      // Optional - event properties
    
    // Advanced optional overrides
    std::unique_ptr<std::vector<uint8_t>> device_id;   // Optional - override device_id (16-byte UUID)
    std::unique_ptr<std::vector<uint8_t>> session_id;  // Optional - override session_id (16-byte UUID)
    std::unique_ptr<Timestamp> timestamp;              // Optional - custom timestamp
    
    EventAdvanced(const std::string& uid, const std::string& name)
        : user_id(uid), event_name(name) {}
    
    EventAdvanced(const std::string& uid, const std::string& name, const Properties& props)
        : user_id(uid), event_name(name), properties(props) {}
};

// Pre-defined event names for common user actions (matching Go SDK)
namespace EventNames {
    // Authentication & User Management Events
    constexpr const char* USER_SIGNED_UP = "User Signed Up";
    constexpr const char* USER_SIGNED_IN = "User Signed In";
    constexpr const char* USER_SIGNED_OUT = "User Signed Out";
    constexpr const char* USER_INVITED = "User Invited";
    constexpr const char* USER_ONBOARDED = "User Onboarded";
    constexpr const char* AUTHENTICATION_FAILED = "Authentication Failed";
    constexpr const char* PASSWORD_RESET = "Password Reset";
    constexpr const char* TWO_FACTOR_ENABLED = "Two Factor Enabled";
    constexpr const char* TWO_FACTOR_DISABLED = "Two Factor Disabled";

    // Revenue & Billing Events
    constexpr const char* ORDER_COMPLETED = "Order Completed";
    constexpr const char* ORDER_REFUNDED = "Order Refunded";
    constexpr const char* ORDER_CANCELED = "Order Canceled";
    constexpr const char* PAYMENT_FAILED = "Payment Failed";
    constexpr const char* PAYMENT_METHOD_ADDED = "Payment Method Added";
    constexpr const char* PAYMENT_METHOD_UPDATED = "Payment Method Updated";
    constexpr const char* PAYMENT_METHOD_REMOVED = "Payment Method Removed";

    // Subscription Management Events
    constexpr const char* SUBSCRIPTION_STARTED = "Subscription Started";
    constexpr const char* SUBSCRIPTION_RENEWED = "Subscription Renewed";
    constexpr const char* SUBSCRIPTION_PAUSED = "Subscription Paused";
    constexpr const char* SUBSCRIPTION_RESUMED = "Subscription Resumed";
    constexpr const char* SUBSCRIPTION_CHANGED = "Subscription Changed";
    constexpr const char* SUBSCRIPTION_CANCELED = "Subscription Canceled";

    // Trial & Conversion Events
    constexpr const char* TRIAL_STARTED = "Trial Started";
    constexpr const char* TRIAL_ENDING_SOON = "Trial Ending Soon";
    constexpr const char* TRIAL_ENDED = "Trial Ended";
    constexpr const char* TRIAL_CONVERTED = "Trial Converted";

    // Shopping Experience Events
    constexpr const char* CART_VIEWED = "Cart Viewed";
    constexpr const char* CART_UPDATED = "Cart Updated";
    constexpr const char* CART_ABANDONED = "Cart Abandoned";
    constexpr const char* CHECKOUT_STARTED = "Checkout Started";
    constexpr const char* CHECKOUT_COMPLETED = "Checkout Completed";

    // Product Engagement Events
    constexpr const char* PAGE_VIEWED = "Page Viewed";
    constexpr const char* FEATURE_USED = "Feature Used";
    constexpr const char* SEARCH_PERFORMED = "Search Performed";
    constexpr const char* FILE_UPLOADED = "File Uploaded";
    constexpr const char* NOTIFICATION_SENT = "Notification Sent";
    constexpr const char* NOTIFICATION_CLICKED = "Notification Clicked";

    // Communication Events
    constexpr const char* EMAIL_SENT = "Email Sent";
    constexpr const char* EMAIL_OPENED = "Email Opened";
    constexpr const char* EMAIL_CLICKED = "Email Clicked";
    constexpr const char* EMAIL_BOUNCED = "Email Bounced";
    constexpr const char* EMAIL_UNSUBSCRIBED = "Email Unsubscribed";
    constexpr const char* SUPPORT_TICKET_CREATED = "Support Ticket Created";
    constexpr const char* SUPPORT_TICKET_RESOLVED = "Support Ticket Resolved";
}

// Authentication Methods
namespace AuthMethods {
    constexpr const char* PASSWORD = "password";
    constexpr const char* GOOGLE = "google";
    constexpr const char* GITHUB = "github";
    constexpr const char* SSO = "sso";
    constexpr const char* EMAIL = "email";
}

// Revenue Types
namespace RevenueTypes {
    constexpr const char* ONE_TIME = "one_time";
    constexpr const char* SUBSCRIPTION = "subscription";
}

// Currency Types
namespace Currencies {
    // Major Global Currencies
    constexpr const char* USD = "USD"; // US Dollar
    constexpr const char* EUR = "EUR"; // Euro
    constexpr const char* GBP = "GBP"; // British Pound
    constexpr const char* JPY = "JPY"; // Japanese Yen
    constexpr const char* CAD = "CAD"; // Canadian Dollar
    constexpr const char* AUD = "AUD"; // Australian Dollar
    constexpr const char* NZD = "NZD"; // New Zealand Dollar
    constexpr const char* KRW = "KRW"; // South Korean Won
    constexpr const char* CNY = "CNY"; // Chinese Yuan
    constexpr const char* HKD = "HKD"; // Hong Kong Dollar
    constexpr const char* SGD = "SGD"; // Singapore Dollar
    constexpr const char* MXN = "MXN"; // Mexican Peso
    constexpr const char* INR = "INR"; // Indian Rupee
    constexpr const char* PLN = "PLN"; // Polish Zloty
    constexpr const char* BRL = "BRL"; // Brazilian Real
    constexpr const char* RUB = "RUB"; // Russian Ruble
    constexpr const char* DKK = "DKK"; // Danish Krone
    constexpr const char* NOK = "NOK"; // Norwegian Krone
    constexpr const char* SEK = "SEK"; // Swedish Krona
    constexpr const char* CHF = "CHF"; // Swiss Franc
    constexpr const char* TRY = "TRY"; // Turkish Lira
    constexpr const char* ILS = "ILS"; // Israeli Shekel
    constexpr const char* THB = "THB"; // Thai Baht
    constexpr const char* MYR = "MYR"; // Malaysian Ringgit
    constexpr const char* IDR = "IDR"; // Indonesian Rupiah
    constexpr const char* VND = "VND"; // Vietnamese Dong
    constexpr const char* PHP = "PHP"; // Philippine Peso
    constexpr const char* CZK = "CZK"; // Czech Koruna
    constexpr const char* HUF = "HUF"; // Hungarian Forint
    constexpr const char* ZAR = "ZAR"; // South African Rand
    constexpr const char* BTC = "BTC"; // Bitcoin
    constexpr const char* ETH = "ETH"; // Ethereum
    constexpr const char* USDC = "USDC"; // USD Coin
    constexpr const char* USDT = "USDT"; // Tether
}

// Payment Methods
namespace PaymentMethods {
    constexpr const char* CARD = "card";
    constexpr const char* PAYPAL = "paypal";
    constexpr const char* WIRE = "wire";
    constexpr const char* APPLE_PAY = "apple_pay";
    constexpr const char* GOOGLE_PAY = "google_pay";
    constexpr const char* STRIPE = "stripe";
    constexpr const char* SQUARE = "square";
    constexpr const char* VENMO = "venmo";
    constexpr const char* ZELLE = "zelle";
    constexpr const char* ACH = "ach";
    constexpr const char* CHECK = "check";
    constexpr const char* CASH = "cash";
    constexpr const char* CRYPTO = "crypto";
    constexpr const char* BANK_TRANSFER = "bank_transfer";
    constexpr const char* GIFT_CARD = "gift_card";
    constexpr const char* STORE_CREDIT = "store_credit";
}

// Channel Types
namespace Channels {
    constexpr const char* DIRECT = "direct";
    constexpr const char* ORGANIC = "organic";
    constexpr const char* PAID = "paid";
    constexpr const char* SOCIAL = "social";
    constexpr const char* EMAIL = "email";
    constexpr const char* SMS = "sms";
    constexpr const char* PUSH = "push";
    constexpr const char* REFERRAL = "referral";
    constexpr const char* AFFILIATE = "affiliate";
    constexpr const char* DISPLAY = "display";
    constexpr const char* VIDEO = "video";
    constexpr const char* AUDIO = "audio";
    constexpr const char* PRINT = "print";
    constexpr const char* EVENT = "event";
    constexpr const char* WEBINAR = "webinar";
    constexpr const char* PODCAST = "podcast";
}

// Traffic Sources
namespace Sources {
    constexpr const char* GOOGLE = "google";
    constexpr const char* FACEBOOK = "facebook";
    constexpr const char* TWITTER = "twitter";
    constexpr const char* LINKEDIN = "linkedin";
    constexpr const char* INSTAGRAM = "instagram";
    constexpr const char* YOUTUBE = "youtube";
    constexpr const char* TIKTOK = "tiktok";
    constexpr const char* SNAPCHAT = "snapchat";
    constexpr const char* PINTEREST = "pinterest";
    constexpr const char* REDDIT = "reddit";
    constexpr const char* BING = "bing";
    constexpr const char* YAHOO = "yahoo";
    constexpr const char* DUCKDUCKGO = "duckduckgo";
    constexpr const char* NEWSLETTER = "newsletter";
    constexpr const char* EMAIL = "email";
    constexpr const char* BLOG = "blog";
    constexpr const char* PODCAST = "podcast";
    constexpr const char* WEBINAR = "webinar";
    constexpr const char* PARTNER = "partner";
    constexpr const char* AFFILIATE = "affiliate";
    constexpr const char* DIRECT = "direct";
    constexpr const char* ORGANIC = "organic";
    constexpr const char* UNKNOWN = "unknown";
}

// Device Types
namespace DeviceTypes {
    constexpr const char* DESKTOP = "desktop";
    constexpr const char* MOBILE = "mobile";
    constexpr const char* TABLET = "tablet";
    constexpr const char* TV = "tv";
    constexpr const char* WATCH = "watch";
    constexpr const char* VR = "vr";
    constexpr const char* IOT = "iot";
    constexpr const char* BOT = "bot";
    constexpr const char* UNKNOWN = "unknown";
}

// Operating Systems
namespace OperatingSystems {
    constexpr const char* WINDOWS = "windows";
    constexpr const char* MACOS = "macos";
    constexpr const char* LINUX = "linux";
    constexpr const char* IOS = "ios";
    constexpr const char* ANDROID = "android";
    constexpr const char* CHROMEOS = "chromeos";
    constexpr const char* FIREOS = "fireos";
    constexpr const char* WEBOS = "webos";
    constexpr const char* TIZEN = "tizen";
    constexpr const char* WATCHOS = "watchos";
    constexpr const char* TVOS = "tvos";
    constexpr const char* PLAYSTATION = "playstation";
    constexpr const char* XBOX = "xbox";
    constexpr const char* UNKNOWN = "unknown";
}

// Browsers
namespace Browsers {
    constexpr const char* CHROME = "chrome";
    constexpr const char* SAFARI = "safari";
    constexpr const char* FIREFOX = "firefox";
    constexpr const char* EDGE = "edge";
    constexpr const char* OPERA = "opera";
    constexpr const char* IE = "ie";
    constexpr const char* SAMSUNG = "samsung";
    constexpr const char* UC = "uc";
    constexpr const char* OTHER = "other";
    constexpr const char* UNKNOWN = "unknown";
}

// Subscription Intervals
namespace SubscriptionIntervals {
    constexpr const char* DAILY = "daily";
    constexpr const char* WEEKLY = "weekly";
    constexpr const char* MONTHLY = "monthly";
    constexpr const char* QUARTERLY = "quarterly";
    constexpr const char* YEARLY = "yearly";
    constexpr const char* ANNUAL = "annual";
    constexpr const char* LIFETIME = "lifetime";
    constexpr const char* CUSTOM = "custom";
}

// Plan Types
namespace PlanTypes {
    constexpr const char* FREE = "free";
    constexpr const char* FREEMIUM = "freemium";
    constexpr const char* BASIC = "basic";
    constexpr const char* STANDARD = "standard";
    constexpr const char* PROFESSIONAL = "professional";
    constexpr const char* PREMIUM = "premium";
    constexpr const char* ENTERPRISE = "enterprise";
    constexpr const char* CUSTOM = "custom";
    constexpr const char* TRIAL = "trial";
    constexpr const char* BETA = "beta";
}

// User Roles
namespace UserRoles {
    constexpr const char* OWNER = "owner";
    constexpr const char* ADMIN = "admin";
    constexpr const char* MANAGER = "manager";
    constexpr const char* USER = "user";
    constexpr const char* GUEST = "guest";
    constexpr const char* VIEWER = "viewer";
    constexpr const char* EDITOR = "editor";
    constexpr const char* MODERATOR = "moderator";
    constexpr const char* SUPPORT = "support";
    constexpr const char* DEVELOPER = "developer";
    constexpr const char* ANALYST = "analyst";
    constexpr const char* BILLING = "billing";
}

// Company Sizes
namespace CompanySizes {
    constexpr const char* SOLOPRENEUR = "solopreneur";
    constexpr const char* SMALL = "small";        // 1-10
    constexpr const char* MEDIUM = "medium";       // 11-50
    constexpr const char* LARGE = "large";         // 51-200
    constexpr const char* ENTERPRISE = "enterprise"; // 201-1000
    constexpr const char* MEGA_CORP = "mega_corp"; // 1000+
    constexpr const char* UNKNOWN = "unknown";
}

// Industries
namespace Industries {
    constexpr const char* TECHNOLOGY = "technology";
    constexpr const char* FINANCE = "finance";
    constexpr const char* HEALTHCARE = "healthcare";
    constexpr const char* EDUCATION = "education";
    constexpr const char* ECOMMERCE = "ecommerce";
    constexpr const char* RETAIL = "retail";
    constexpr const char* MANUFACTURING = "manufacturing";
    constexpr const char* REAL_ESTATE = "real_estate";
    constexpr const char* MEDIA = "media";
    constexpr const char* NON_PROFIT = "non_profit";
    constexpr const char* GOVERNMENT = "government";
    constexpr const char* CONSULTING = "consulting";
    constexpr const char* LEGAL = "legal";
    constexpr const char* MARKETING = "marketing";
    constexpr const char* OTHER = "other";
    constexpr const char* UNKNOWN = "unknown";
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

// Generate a random session ID
SessionId generate_session_id();

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