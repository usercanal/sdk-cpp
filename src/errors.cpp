// src/errors.cpp
// Implementation of error handling system with messages and factory functions

#include "usercanal/errors.hpp"
#include <sstream>

namespace usercanal {

// Error category implementation
std::string UserCanalErrorCategory::message(int ev) const {
    switch (static_cast<ErrorCode>(ev)) {
        case ErrorCode::SUCCESS:
            return "Success";
        
        // Configuration errors (1-99)
        case ErrorCode::INVALID_API_KEY:
            return "Invalid API key format";
        case ErrorCode::INVALID_CONFIG:
            return "Invalid configuration";
        case ErrorCode::INVALID_ENDPOINT:
            return "Invalid endpoint format";
        case ErrorCode::INVALID_BATCH_SIZE:
            return "Invalid batch size";
        case ErrorCode::INVALID_FLUSH_INTERVAL:
            return "Invalid flush interval";
        
        // Validation errors (100-199)
        case ErrorCode::INVALID_USER_ID:
            return "Invalid user ID";
        case ErrorCode::INVALID_EVENT_NAME:
            return "Invalid event name";
        case ErrorCode::INVALID_PROPERTIES:
            return "Invalid properties";
        case ErrorCode::INVALID_TIMESTAMP:
            return "Invalid timestamp";
        case ErrorCode::INVALID_LOG_LEVEL:
            return "Invalid log level";
        case ErrorCode::INVALID_SERVICE_NAME:
            return "Invalid service name";
        case ErrorCode::INVALID_MESSAGE:
            return "Invalid message";
        case ErrorCode::EMPTY_PAYLOAD:
            return "Empty payload";
        case ErrorCode::PAYLOAD_TOO_LARGE:
            return "Payload too large";
        
        // Network errors (200-299)
        case ErrorCode::CONNECTION_FAILED:
            return "Connection failed";
        case ErrorCode::CONNECTION_TIMEOUT:
            return "Connection timeout";
        case ErrorCode::CONNECTION_REFUSED:
            return "Connection refused";
        case ErrorCode::DNS_RESOLUTION_FAILED:
            return "DNS resolution failed";
        case ErrorCode::NETWORK_UNREACHABLE:
            return "Network unreachable";
        case ErrorCode::SEND_FAILED:
            return "Send operation failed";
        case ErrorCode::RECEIVE_FAILED:
            return "Receive operation failed";
        case ErrorCode::SOCKET_ERROR:
            return "Socket error";
        case ErrorCode::TLS_ERROR:
            return "TLS/SSL error";
        
        // Protocol errors (300-399)
        case ErrorCode::INVALID_RESPONSE:
            return "Invalid response";
        case ErrorCode::PROTOCOL_MISMATCH:
            return "Protocol version mismatch";
        case ErrorCode::SERIALIZATION_FAILED:
            return "Serialization failed";
        case ErrorCode::DESERIALIZATION_FAILED:
            return "Deserialization failed";
        case ErrorCode::BATCH_TOO_LARGE:
            return "Batch too large";
        case ErrorCode::MALFORMED_DATA:
            return "Malformed data";
        
        // Authentication errors (400-499)
        case ErrorCode::INVALID_CREDENTIALS:
            return "Invalid credentials";
        case ErrorCode::AUTHENTICATION_FAILED:
            return "Authentication failed";
        case ErrorCode::AUTHORIZATION_DENIED:
            return "Authorization denied";
        case ErrorCode::API_KEY_EXPIRED:
            return "API key expired";
        case ErrorCode::RATE_LIMITED:
            return "Rate limited";
        
        // Server errors (500-599)
        case ErrorCode::SERVER_ERROR:
            return "Server error";
        case ErrorCode::SERVICE_UNAVAILABLE:
            return "Service unavailable";
        case ErrorCode::INTERNAL_SERVER_ERROR:
            return "Internal server error";
        case ErrorCode::BAD_GATEWAY:
            return "Bad gateway";
        case ErrorCode::GATEWAY_TIMEOUT:
            return "Gateway timeout";
        
        // Client state errors (600-699)
        case ErrorCode::CLIENT_NOT_INITIALIZED:
            return "Client not initialized";
        case ErrorCode::CLIENT_ALREADY_INITIALIZED:
            return "Client already initialized";
        case ErrorCode::CLIENT_SHUTTING_DOWN:
            return "Client shutting down";
        case ErrorCode::CLIENT_SHUTDOWN:
            return "Client shutdown";
        case ErrorCode::QUEUE_FULL:
            return "Queue full";
        case ErrorCode::FLUSH_IN_PROGRESS:
            return "Flush operation in progress";
        
        // System errors (700-799)
        case ErrorCode::OUT_OF_MEMORY:
            return "Out of memory";
        case ErrorCode::DISK_FULL:
            return "Disk full";
        case ErrorCode::PERMISSION_DENIED:
            return "Permission denied";
        case ErrorCode::RESOURCE_EXHAUSTED:
            return "Resource exhausted";
        case ErrorCode::SYSTEM_ERROR:
            return "System error";
        
        // Unknown/Generic errors (800+)
        case ErrorCode::UNKNOWN_ERROR:
            return "Unknown error";
        case ErrorCode::OPERATION_CANCELLED:
            return "Operation cancelled";
        case ErrorCode::TIMEOUT:
            return "Operation timeout";
        case ErrorCode::RETRY_EXHAUSTED:
            return "Retry attempts exhausted";
        
        default:
            return "Unknown error code";
    }
}

const UserCanalErrorCategory& usercanal_error_category() {
    static const UserCanalErrorCategory instance;
    return instance;
}

std::error_code make_error_code(ErrorCode ec) {
    return std::error_code{static_cast<int>(ec), usercanal_error_category()};
}

// Error factory functions implementation
namespace Errors {

// Configuration errors
ConfigError invalid_api_key(const std::string& api_key) {
    std::ostringstream oss;
    oss << "API key must be 32 hexadecimal characters, got: '" 
        << api_key.substr(0, 8) << "...' (length: " << api_key.length() << ")";
    return ConfigError(ErrorCode::INVALID_API_KEY, "api_key", oss.str());
}

ConfigError invalid_endpoint(const std::string& endpoint) {
    return ConfigError(ErrorCode::INVALID_ENDPOINT, "endpoint", 
        "Endpoint must be in format 'host:port', got: '" + endpoint + "'");
}

ConfigError invalid_batch_size(int batch_size) {
    return ConfigError(ErrorCode::INVALID_BATCH_SIZE, "batch_size", 
        "Batch size must be between 1 and 10000, got: " + std::to_string(batch_size));
}

ConfigError invalid_flush_interval(std::chrono::milliseconds interval) {
    return ConfigError(ErrorCode::INVALID_FLUSH_INTERVAL, "flush_interval", 
        "Flush interval must be between 100ms and 300s, got: " + std::to_string(interval.count()) + "ms");
}

// Validation errors
ValidationError invalid_user_id(const std::string& user_id) {
    if (user_id.empty()) {
        return ValidationError(ErrorCode::INVALID_USER_ID, "user_id", "User ID cannot be empty");
    } else if (user_id.length() > 255) {
        return ValidationError(ErrorCode::INVALID_USER_ID, "user_id", 
            "User ID too long (max 255 chars)", user_id.substr(0, 50) + "...");
    } else {
        return ValidationError(ErrorCode::INVALID_USER_ID, "user_id", 
            "User ID contains only whitespace", user_id);
    }
}

ValidationError invalid_event_name(const std::string& event_name) {
    if (event_name.empty()) {
        return ValidationError(ErrorCode::INVALID_EVENT_NAME, "event_name", "Event name cannot be empty");
    } else if (event_name.length() > 255) {
        return ValidationError(ErrorCode::INVALID_EVENT_NAME, "event_name", 
            "Event name too long (max 255 chars)", event_name.substr(0, 50) + "...");
    } else {
        return ValidationError(ErrorCode::INVALID_EVENT_NAME, "event_name", 
            "Invalid event name format", event_name);
    }
}

ValidationError invalid_service_name(const std::string& service_name) {
    if (service_name.empty()) {
        return ValidationError(ErrorCode::INVALID_SERVICE_NAME, "service_name", 
            "Service name cannot be empty");
    } else if (service_name.length() > 255) {
        return ValidationError(ErrorCode::INVALID_SERVICE_NAME, "service_name", 
            "Service name too long (max 255 chars)", service_name.substr(0, 50) + "...");
    } else {
        return ValidationError(ErrorCode::INVALID_SERVICE_NAME, "service_name", 
            "Invalid service name format", service_name);
    }
}

ValidationError empty_message() {
    return ValidationError(ErrorCode::INVALID_MESSAGE, "message", "Log message cannot be empty");
}

ValidationError payload_too_large(size_t size, size_t max_size) {
    std::ostringstream oss;
    oss << "Payload size " << size << " bytes exceeds maximum " << max_size << " bytes";
    return ValidationError(ErrorCode::PAYLOAD_TOO_LARGE, "payload", oss.str());
}

// Network errors
NetworkError connection_failed(const std::string& endpoint) {
    return NetworkError(ErrorCode::CONNECTION_FAILED, "connect", 
        "Failed to connect to " + endpoint);
}

NetworkError connection_timeout(const std::string& endpoint, std::chrono::milliseconds timeout) {
    std::ostringstream oss;
    oss << "Connection to " << endpoint << " timed out after " << timeout.count() << "ms";
    return NetworkError(ErrorCode::CONNECTION_TIMEOUT, "connect", oss.str());
}

NetworkError dns_resolution_failed(const std::string& hostname) {
    return NetworkError(ErrorCode::DNS_RESOLUTION_FAILED, "dns_resolve", 
        "Failed to resolve hostname: " + hostname);
}

NetworkError send_failed(const std::string& reason) {
    return NetworkError(ErrorCode::SEND_FAILED, "send", reason);
}

// Protocol errors
ProtocolError serialization_failed(const std::string& type) {
    return ProtocolError(ErrorCode::SERIALIZATION_FAILED, "serialize", 
        "Failed to serialize " + type + " data");
}

ProtocolError batch_too_large(size_t size, size_t max_size) {
    std::ostringstream oss;
    oss << "Batch size " << size << " bytes exceeds maximum " << max_size << " bytes";
    return ProtocolError(ErrorCode::BATCH_TOO_LARGE, oss.str());
}

ProtocolError malformed_data(const std::string& details) {
    return ProtocolError(ErrorCode::MALFORMED_DATA, "parse", 
        "Malformed data: " + details);
}

// Authentication errors
AuthError authentication_failed() {
    return AuthError(ErrorCode::AUTHENTICATION_FAILED, 
        "Invalid API key or authentication failed");
}

AuthError api_key_expired() {
    return AuthError(ErrorCode::API_KEY_EXPIRED, 
        "API key has expired, please generate a new one");
}

AuthError rate_limited(std::chrono::seconds retry_after) {
    std::ostringstream oss;
    oss << "Rate limited. ";
    if (retry_after.count() > 0) {
        oss << "Retry after " << retry_after.count() << " seconds";
    } else {
        oss << "Please reduce request rate";
    }
    return AuthError(ErrorCode::RATE_LIMITED, oss.str());
}

// Server errors
ServerError server_error(int status_code, const std::string& message) {
    return ServerError(ErrorCode::SERVER_ERROR, message, status_code);
}

ServerError service_unavailable() {
    return ServerError(ErrorCode::SERVICE_UNAVAILABLE, 
        "Service temporarily unavailable, please retry later", 503);
}

// Client errors
ClientError client_not_initialized() {
    return ClientError(ErrorCode::CLIENT_NOT_INITIALIZED, 
        "Client must be initialized before use");
}

ClientError client_shutdown() {
    return ClientError(ErrorCode::CLIENT_SHUTDOWN, 
        "Client has been shut down and cannot be used");
}

ClientError queue_full(size_t current_size, size_t max_size) {
    std::ostringstream oss;
    oss << "Queue is full (" << current_size << "/" << max_size 
        << "). Data will be dropped or operation will block";
    return ClientError(ErrorCode::QUEUE_FULL, oss.str());
}

// System errors
SystemError out_of_memory() {
    return SystemError(ErrorCode::OUT_OF_MEMORY, 
        "Insufficient memory to complete operation");
}

SystemError disk_full() {
    return SystemError(ErrorCode::DISK_FULL, 
        "Insufficient disk space");
}

SystemError permission_denied(const std::string& resource) {
    return SystemError(ErrorCode::PERMISSION_DENIED, 
        "Permission denied accessing: " + resource);
}

// Additional client operation errors
ClientError client_already_initialized() {
    return ClientError(ErrorCode::CLIENT_ALREADY_INITIALIZED, 
        "Client is already initialized");
}

NetworkError receive_failed(const std::string& reason) {
    return NetworkError(ErrorCode::RECEIVE_FAILED, "receive", reason);
}

} // namespace Errors
} // namespace usercanal