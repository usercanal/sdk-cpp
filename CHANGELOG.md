# UserCanal C++ SDK - Changelog

## [1.0.0] - 2025-08-09

### Added - First Production Release

#### Core SDK Features (Matching Go SDK Scope)
- feat: Complete C++ SDK implementation with FlatBuffers serialization
- feat: Basic client with event tracking and structured logging
- feat: Simple configuration management with endpoint and batch settings
- feat: High-performance networking with TCP transport and connection pooling
- feat: Automatic batching system with configurable flush intervals
- feat: API key-based authentication and workspace routing
- feat: Basic statistics tracking (matching Go SDK GetStats)

#### Event Analytics
- feat: `client.event(user_id, event_name, properties)` - basic event tracking
- feat: `client.event_revenue(user_id, order_id, amount, currency, properties)` - revenue tracking
- feat: Support for custom events with flexible properties
- feat: Legacy `track()` methods for backward compatibility

#### Structured Logging
- feat: `client.log_info(service, message, data)` - info level logging
- feat: `client.log_error(service, message, data)` - error level logging
- feat: `client.log_warning(service, message, data)` - warning level logging
- feat: `client.log_debug(service, message, data)` - debug level logging
- feat: Context correlation and service isolation

#### Performance & Reliability
- feat: Zero-copy FlatBuffers binary serialization
- feat: Thread-safe concurrent operations
- feat: Automatic retry with exponential backoff
- feat: Connection health monitoring and reconnection
- feat: Graceful shutdown with proper cleanup

### Fixed - FlatBuffer Serialization Issues

#### Critical Bug Fixes (Resolved SDK Interoperability Issues)
- fix: Corrected API key hex-to-byte conversion for proper authentication
- fix: Fixed FlatBuffer field ordering to match schema specification
- fix: Resolved LogEventType enum synchronization with collector (COLLECT→LOG)
- fix: Enhanced binary message validation and error handling
- fix: Synchronized serialization format with Go and Swift SDKs for compatibility

#### Network & Transport Fixes
- fix: Improved TCP connection handling with proper error recovery
- fix: Enhanced batching reliability with atomic operations
- fix: Fixed message framing for consistent collector parsing
- fix: Resolved thread safety issues in concurrent client operations
