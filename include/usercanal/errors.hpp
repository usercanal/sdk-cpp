// include/usercanal/errors.hpp
// Purpose: Comprehensive error handling system for the UserCanal C++ SDK
// Provides hierarchical error types for different failure scenarios

#pragma once

#include <stdexcept>
#include <string>
#include <chrono>
#include <system_error>

namespace usercanal {

// Base error category for UserCanal SDK errors
class UserCanalErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override {
        return "usercanal";
    }

    std::string message(int ev) const override;
};

// Global error category instance
const UserCanalErrorCategory& usercanal_error_category();

// Error codes enum
enum class ErrorCode {
    // Success
    SUCCESS = 0,
    
    // Configuration errors (1-99)
    INVALID_API_KEY = 1,
    INVALID_CONFIG = 2,
    INVALID_ENDPOINT = 3,
    INVALID_BATCH_SIZE = 4,
    INVALID_FLUSH_INTERVAL = 5,
    
    // Validation errors (100-199)
    INVALID_USER_ID = 100,
    INVALID_EVENT_NAME = 101,
    INVALID_PROPERTIES = 102,
    INVALID_TIMESTAMP = 103,
    INVALID_LOG_LEVEL = 104,
    INVALID_SERVICE_NAME = 105,
    INVALID_MESSAGE = 106,
    EMPTY_PAYLOAD = 107,
    PAYLOAD_TOO_LARGE = 108,
    
    // Network errors (200-299)
    CONNECTION_FAILED = 200,
    CONNECTION_TIMEOUT = 201,
    CONNECTION_REFUSED = 202,
    DNS_RESOLUTION_FAILED = 203,
    NETWORK_UNREACHABLE = 204,
    SEND_FAILED = 205,
    RECEIVE_FAILED = 206,
    SOCKET_ERROR = 207,
    TLS_ERROR = 208,
    
    // Protocol errors (300-399)
    INVALID_RESPONSE = 300,
    PROTOCOL_MISMATCH = 301,
    SERIALIZATION_FAILED = 302,
    DESERIALIZATION_FAILED = 303,
    BATCH_TOO_LARGE = 304,
    MALFORMED_DATA = 305,
    
    // Authentication errors (400-499)
    INVALID_CREDENTIALS = 400,
    AUTHENTICATION_FAILED = 401,
    AUTHORIZATION_DENIED = 402,
    API_KEY_EXPIRED = 403,
    RATE_LIMITED = 404,
    
    // Server errors (500-599)
    SERVER_ERROR = 500,
    SERVICE_UNAVAILABLE = 501,
    INTERNAL_SERVER_ERROR = 502,
    BAD_GATEWAY = 503,
    GATEWAY_TIMEOUT = 504,
    
    // Client state errors (600-699)
    CLIENT_NOT_INITIALIZED = 600,
    CLIENT_ALREADY_INITIALIZED = 601,
    CLIENT_SHUTTING_DOWN = 602,
    CLIENT_SHUTDOWN = 603,
    QUEUE_FULL = 604,
    FLUSH_IN_PROGRESS = 605,
    
    // System errors (700-799)
    OUT_OF_MEMORY = 700,
    DISK_FULL = 701,
    PERMISSION_DENIED = 702,
    RESOURCE_EXHAUSTED = 703,
    SYSTEM_ERROR = 704,
    
    // Unknown/Generic errors (800+)
    UNKNOWN_ERROR = 800,
    OPERATION_CANCELLED = 801,
    TIMEOUT = 802,
    RETRY_EXHAUSTED = 803
};

// Create error codes
std::error_code make_error_code(ErrorCode ec);

// Base exception class for all UserCanal errors
class Error : public std::exception {
public:
    explicit Error(const std::string& message)
        : message_(message)
        , error_code_(ErrorCode::UNKNOWN_ERROR)
        , timestamp_(std::chrono::system_clock::now()) {}

    Error(ErrorCode code, const std::string& message)
        : message_(message)
        , error_code_(code)
        , timestamp_(std::chrono::system_clock::now()) {}

    const char* what() const noexcept override {
        return message_.c_str();
    }

    ErrorCode code() const noexcept {
        return error_code_;
    }

    std::chrono::system_clock::time_point timestamp() const noexcept {
        return timestamp_;
    }

    virtual std::string category() const {
        return "usercanal::Error";
    }

protected:
    std::string message_;
    ErrorCode error_code_;
    std::chrono::system_clock::time_point timestamp_;
};

// Configuration-related errors
class ConfigError : public Error {
public:
    ConfigError(ErrorCode code, const std::string& field, const std::string& message)
        : Error(code, "Configuration error in '" + field + "': " + message)
        , field_(field) {}

    const std::string& field() const noexcept {
        return field_;
    }

    std::string category() const override {
        return "usercanal::ConfigError";
    }

private:
    std::string field_;
};

// Validation-related errors
class ValidationError : public Error {
public:
    ValidationError(ErrorCode code, const std::string& field, const std::string& message)
        : Error(code, "Validation error in '" + field + "': " + message)
        , field_(field) {}

    ValidationError(ErrorCode code, const std::string& field, const std::string& message, 
                   const std::string& value)
        : Error(code, "Validation error in '" + field + "': " + message + " (value: '" + value + "')")
        , field_(field)
        , value_(value) {}

    const std::string& field() const noexcept {
        return field_;
    }

    const std::string& value() const noexcept {
        return value_;
    }

    std::string category() const override {
        return "usercanal::ValidationError";
    }

private:
    std::string field_;
    std::string value_;
};

// Network-related errors
class NetworkError : public Error {
public:
    NetworkError(ErrorCode code, const std::string& operation, const std::string& message)
        : Error(code, "Network error during '" + operation + "': " + message)
        , operation_(operation)
        , retries_(0) {}

    NetworkError(ErrorCode code, const std::string& operation, const std::string& message, 
                int retries)
        : Error(code, "Network error during '" + operation + "' after " + 
               std::to_string(retries) + " retries: " + message)
        , operation_(operation)
        , retries_(retries) {}

    const std::string& operation() const noexcept {
        return operation_;
    }

    int retries() const noexcept {
        return retries_;
    }

    void increment_retries() {
        retries_++;
        message_ = "Network error during '" + operation_ + "' after " + 
                  std::to_string(retries_) + " retries: " + 
                  message_.substr(message_.find(": ") + 2);
    }

    std::string category() const override {
        return "usercanal::NetworkError";
    }

private:
    std::string operation_;
    int retries_;
};

// Protocol-related errors
class ProtocolError : public Error {
public:
    ProtocolError(ErrorCode code, const std::string& message)
        : Error(code, "Protocol error: " + message) {}

    ProtocolError(ErrorCode code, const std::string& operation, const std::string& message)
        : Error(code, "Protocol error during '" + operation + "': " + message)
        , operation_(operation) {}

    const std::string& operation() const noexcept {
        return operation_;
    }

    std::string category() const override {
        return "usercanal::ProtocolError";
    }

private:
    std::string operation_;
};

// Authentication-related errors
class AuthError : public Error {
public:
    AuthError(ErrorCode code, const std::string& message)
        : Error(code, "Authentication error: " + message) {}

    std::string category() const override {
        return "usercanal::AuthError";
    }
};

// Server-related errors
class ServerError : public Error {
public:
    ServerError(ErrorCode code, const std::string& message)
        : Error(code, "Server error: " + message)
        , status_code_(0) {}

    ServerError(ErrorCode code, const std::string& message, int status_code)
        : Error(code, "Server error (" + std::to_string(status_code) + "): " + message)
        , status_code_(status_code) {}

    int status_code() const noexcept {
        return status_code_;
    }

    std::string category() const override {
        return "usercanal::ServerError";
    }

private:
    int status_code_;
};

// Client state-related errors
class ClientError : public Error {
public:
    ClientError(ErrorCode code, const std::string& message)
        : Error(code, "Client error: " + message) {}

    std::string category() const override {
        return "usercanal::ClientError";
    }
};

// System-related errors
class SystemError : public Error {
public:
    SystemError(ErrorCode code, const std::string& message)
        : Error(code, "System error: " + message) {}

    SystemError(ErrorCode code, const std::string& message, const std::error_code& system_error)
        : Error(code, "System error: " + message + " (" + system_error.message() + ")")
        , system_error_(system_error) {}

    const std::error_code& system_error() const noexcept {
        return system_error_;
    }

    std::string category() const override {
        return "usercanal::SystemError";
    }

private:
    std::error_code system_error_;
};

// Timeout-related errors
class TimeoutError : public Error {
public:
    TimeoutError(const std::string& operation, std::chrono::milliseconds timeout)
        : Error(ErrorCode::TIMEOUT, "Operation '" + operation + "' timed out after " + 
               std::to_string(timeout.count()) + "ms")
        , operation_(operation)
        , timeout_(timeout) {}

    const std::string& operation() const noexcept {
        return operation_;
    }

    std::chrono::milliseconds timeout() const noexcept {
        return timeout_;
    }

    std::string category() const override {
        return "usercanal::TimeoutError";
    }

private:
    std::string operation_;
    std::chrono::milliseconds timeout_;
};

// Error factory functions for common error scenarios
namespace Errors {

// Configuration errors
ConfigError invalid_api_key(const std::string& api_key);
ConfigError invalid_endpoint(const std::string& endpoint);
ConfigError invalid_batch_size(int batch_size);
ConfigError invalid_flush_interval(std::chrono::milliseconds interval);

// Validation errors
ValidationError invalid_user_id(const std::string& user_id);
ValidationError invalid_event_name(const std::string& event_name);
ValidationError invalid_service_name(const std::string& service_name);
ValidationError empty_message();
ValidationError payload_too_large(size_t size, size_t max_size);

// Network errors
NetworkError connection_failed(const std::string& endpoint);
NetworkError connection_timeout(const std::string& endpoint, std::chrono::milliseconds timeout);
NetworkError dns_resolution_failed(const std::string& hostname);
NetworkError send_failed(const std::string& reason);

// Protocol errors
ProtocolError serialization_failed(const std::string& type);
ProtocolError batch_too_large(size_t size, size_t max_size);
ProtocolError malformed_data(const std::string& details);

// Authentication errors
AuthError authentication_failed();
AuthError api_key_expired();
AuthError rate_limited(std::chrono::seconds retry_after = std::chrono::seconds(0));

// Server errors
ServerError server_error(int status_code, const std::string& message);
ServerError service_unavailable();

// Client errors
ClientError client_not_initialized();
ClientError client_shutdown();
ClientError queue_full(size_t current_size, size_t max_size);

// System errors
SystemError out_of_memory();
SystemError disk_full();
SystemError permission_denied(const std::string& resource);

// Network operation errors
NetworkError receive_failed(const std::string& reason);

// Additional client operation errors
ClientError client_already_initialized();

} // namespace Errors

} // namespace usercanal

// Enable std::error_code support
namespace std {
template <>
struct is_error_code_enum<usercanal::ErrorCode> : true_type {};
}