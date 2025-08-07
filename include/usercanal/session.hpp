// include/usercanal/session.hpp
// Server-optimized session and lifecycle management for high-performance analytics
// Designed for production server environments with concurrent session handling

#pragma once

#include "usercanal/types.hpp"
#include "usercanal/utils.hpp"
#include <string>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>
#include <chrono>
#include <functional>
#include <thread>
#include <condition_variable>
#include <optional>

namespace usercanal {

//=============================================================================
// SESSION MANAGEMENT TYPES
//=============================================================================

using SessionId = std::string;
using UserId = std::string;
using TenantId = std::string;

enum class SessionState {
    ACTIVE = 0,
    IDLE = 1,
    EXPIRED = 2,
    TERMINATED = 3
};

struct SessionMetrics {
    uint64_t events_count = 0;
    uint64_t logs_count = 0;
    Timestamp created_at = 0;
    Timestamp last_activity = 0;
    std::chrono::milliseconds total_duration{0};
    std::string last_event_name;
    std::string user_agent;
    std::string ip_address;
};

//=============================================================================
// SESSION CLASS - Thread-safe session container
//=============================================================================

class Session {
public:
    explicit Session(const SessionId& session_id, const UserId& user_id, 
                    const TenantId& tenant_id = "default");
    
    // Copy constructor and assignment operator
    Session(const Session& other);
    Session& operator=(const Session& other);
    Session(Session&& other) noexcept;
    Session& operator=(Session&& other) noexcept;
    
    // Session identification
    const SessionId& get_session_id() const { return session_id_; }
    const UserId& get_user_id() const { return user_id_; }
    const TenantId& get_tenant_id() const { return tenant_id_; }
    
    // Session state management
    SessionState get_state() const;
    void set_state(SessionState state);
    bool is_active() const { return get_state() == SessionState::ACTIVE; }
    bool is_expired() const;
    
    // Activity tracking
    void record_activity();
    void record_event(const std::string& event_name);
    void record_log();
    Timestamp get_last_activity() const;
    std::chrono::milliseconds get_duration() const;
    
    // Session properties
    void set_property(const std::string& key, const PropertyValue& value);
    std::optional<PropertyValue> get_property(const std::string& key) const;
    void remove_property(const std::string& key);
    Properties get_all_properties() const;
    
    // Session context (IP, user agent, etc.)
    void set_context(const std::string& ip_address, const std::string& user_agent = "");
    std::string get_ip_address() const;
    std::string get_user_agent() const;
    
    // Session metrics
    SessionMetrics get_metrics() const;
    
    // Session timeout configuration
    void set_timeout(std::chrono::seconds timeout);
    std::chrono::seconds get_timeout() const;
    
    // Serialization for persistence
    std::string serialize() const;
    static std::unique_ptr<Session> deserialize(const std::string& data);

private:
    SessionId session_id_;
    UserId user_id_;
    TenantId tenant_id_;
    
    mutable std::mutex mutex_;
    std::atomic<SessionState> state_{SessionState::ACTIVE};
    std::atomic<Timestamp> created_at_;
    std::atomic<Timestamp> last_activity_;
    std::chrono::seconds timeout_{std::chrono::seconds(1800)}; // 30 minutes default
    
    // Session data
    Properties properties_;
    std::string ip_address_;
    std::string user_agent_;
    
    // Metrics
    std::atomic<uint64_t> events_count_{0};
    std::atomic<uint64_t> logs_count_{0};
    std::string last_event_name_;
};

//=============================================================================
// SESSION STORE - Persistent session storage interface
//=============================================================================

class SessionStore {
public:
    virtual ~SessionStore() = default;
    
    // Session persistence
    virtual bool save_session(const Session& session) = 0;
    virtual std::unique_ptr<Session> load_session(const SessionId& session_id) = 0;
    virtual bool delete_session(const SessionId& session_id) = 0;
    virtual bool session_exists(const SessionId& session_id) const = 0;
    
    // Bulk operations
    virtual std::vector<SessionId> get_all_session_ids() const = 0;
    virtual std::vector<SessionId> get_sessions_for_user(const UserId& user_id) const = 0;
    virtual std::vector<SessionId> get_sessions_for_tenant(const TenantId& tenant_id) const = 0;
    
    // Cleanup
    virtual size_t cleanup_expired_sessions() = 0;
    virtual size_t get_session_count() const = 0;
    
    // Health and metrics
    virtual bool is_healthy() const = 0;
    virtual std::unordered_map<std::string, std::string> get_store_metrics() const = 0;
};

//=============================================================================
// MEMORY SESSION STORE - In-memory implementation with optional persistence
//=============================================================================

class MemorySessionStore : public SessionStore {
public:
    explicit MemorySessionStore(const std::string& persistence_file = "");
    ~MemorySessionStore();
    
    // SessionStore interface
    bool save_session(const Session& session) override;
    std::unique_ptr<Session> load_session(const SessionId& session_id) override;
    bool delete_session(const SessionId& session_id) override;
    bool session_exists(const SessionId& session_id) const override;
    
    std::vector<SessionId> get_all_session_ids() const override;
    std::vector<SessionId> get_sessions_for_user(const UserId& user_id) const override;
    std::vector<SessionId> get_sessions_for_tenant(const TenantId& tenant_id) const override;
    
    size_t cleanup_expired_sessions() override;
    size_t get_session_count() const override;
    
    bool is_healthy() const override { return true; }
    std::unordered_map<std::string, std::string> get_store_metrics() const override;
    
    // Configuration
    void set_max_sessions(size_t max_sessions) { max_sessions_ = max_sessions; }
    void enable_persistence(bool enabled) { persistence_enabled_ = enabled; }
    
    // Persistence operations
    bool save_to_disk() const;
    bool load_from_disk();

private:
    mutable std::mutex mutex_;
    std::unordered_map<SessionId, std::unique_ptr<Session>> sessions_;
    
    // User and tenant indexes for fast lookup
    std::unordered_map<UserId, std::vector<SessionId>> user_sessions_;
    std::unordered_map<TenantId, std::vector<SessionId>> tenant_sessions_;
    
    // Configuration
    std::string persistence_file_;
    bool persistence_enabled_ = false;
    size_t max_sessions_ = 10000;
    
    // Metrics
    mutable std::atomic<uint64_t> total_sessions_created_{0};
    mutable std::atomic<uint64_t> total_sessions_deleted_{0};
    
    void update_indexes(const Session& session, bool add);
    void remove_from_indexes(const SessionId& session_id);
};

//=============================================================================
// SESSION MANAGER - Core session management with lifecycle
//=============================================================================

class SessionManager {
public:
    explicit SessionManager(std::unique_ptr<SessionStore> store = nullptr);
    ~SessionManager();
    
    // Session lifecycle
    SessionId create_session(const UserId& user_id, const TenantId& tenant_id = "default");
    std::shared_ptr<Session> get_session(const SessionId& session_id);
    bool terminate_session(const SessionId& session_id);
    bool session_exists(const SessionId& session_id) const;
    
    // Session operations
    void record_session_event(const SessionId& session_id, const std::string& event_name);
    void record_session_log(const SessionId& session_id);
    void update_session_context(const SessionId& session_id, 
                               const std::string& ip_address, 
                               const std::string& user_agent = "");
    
    // Batch operations
    std::vector<SessionId> get_user_sessions(const UserId& user_id) const;
    std::vector<SessionId> get_tenant_sessions(const TenantId& tenant_id) const;
    void terminate_user_sessions(const UserId& user_id);
    void terminate_tenant_sessions(const TenantId& tenant_id);
    
    // Session analytics
    struct SessionAnalytics {
        uint64_t total_sessions = 0;
        uint64_t active_sessions = 0;
        uint64_t idle_sessions = 0;
        uint64_t expired_sessions = 0;
        std::chrono::seconds average_session_duration{0};
        double events_per_session = 0.0;
        double logs_per_session = 0.0;
        std::unordered_map<TenantId, uint64_t> sessions_by_tenant;
    };
    
    SessionAnalytics get_analytics() const;
    
    // Configuration
    void set_default_timeout(std::chrono::seconds timeout);
    void set_cleanup_interval(std::chrono::seconds interval);
    void set_max_sessions_per_user(size_t max_sessions);
    void set_max_sessions_per_tenant(size_t max_sessions);
    
    // Lifecycle management
    void start();
    void stop();
    bool is_running() const { return running_; }
    
    // Maintenance
    size_t cleanup_expired_sessions();
    void save_all_sessions();
    void load_all_sessions();
    
    // Callbacks
    using SessionCreatedCallback = std::function<void(const SessionId&, const UserId&)>;
    using SessionExpiredCallback = std::function<void(const SessionId&, const UserId&)>;
    using SessionTerminatedCallback = std::function<void(const SessionId&, const UserId&)>;
    
    void set_session_created_callback(SessionCreatedCallback callback);
    void set_session_expired_callback(SessionExpiredCallback callback);
    void set_session_terminated_callback(SessionTerminatedCallback callback);

private:
    std::unique_ptr<SessionStore> store_;
    mutable std::mutex sessions_mutex_;
    std::unordered_map<SessionId, std::shared_ptr<Session>> active_sessions_;
    
    // Configuration
    std::chrono::seconds default_timeout_{std::chrono::seconds(1800)}; // 30 minutes
    std::chrono::seconds cleanup_interval_{std::chrono::seconds(300)};  // 5 minutes
    size_t max_sessions_per_user_ = 10;
    size_t max_sessions_per_tenant_ = 1000;
    
    // Background cleanup
    std::atomic<bool> running_{false};
    std::thread cleanup_thread_;
    std::mutex cleanup_mutex_;
    std::condition_variable cleanup_cv_;
    
    // Callbacks
    SessionCreatedCallback session_created_callback_;
    SessionExpiredCallback session_expired_callback_;
    SessionTerminatedCallback session_terminated_callback_;
    
    // Metrics
    std::atomic<uint64_t> total_sessions_created_{0};
    std::atomic<uint64_t> total_sessions_expired_{0};
    std::atomic<uint64_t> total_sessions_terminated_{0};
    
    // Internal methods
    SessionId generate_session_id() const;
    void cleanup_loop();
    void enforce_user_session_limit(const UserId& user_id);
    void enforce_tenant_session_limit(const TenantId& tenant_id);
};

//=============================================================================
// APPLICATION LIFECYCLE MANAGER
//=============================================================================

class LifecycleManager {
public:
    enum class ApplicationState {
        STARTING = 0,
        RUNNING = 1,
        STOPPING = 2,
        STOPPED = 3,
        FAILED = 4
    };
    
    LifecycleManager();
    ~LifecycleManager();
    
    // Lifecycle control
    void start();
    void stop(std::chrono::milliseconds timeout = std::chrono::milliseconds(10000));
    void restart();
    void emergency_stop();
    
    // State management
    ApplicationState get_state() const { return state_.load(); }
    bool is_running() const { return get_state() == ApplicationState::RUNNING; }
    bool is_stopping() const { return get_state() == ApplicationState::STOPPING; }
    bool is_stopped() const { return get_state() == ApplicationState::STOPPED; }
    
    // Component registration
    using StartupTask = std::function<void()>;
    using ShutdownTask = std::function<void()>;
    
    void register_startup_task(const std::string& name, StartupTask task, int priority = 0);
    void register_shutdown_task(const std::string& name, ShutdownTask task, int priority = 0);
    
    // Signal handling
    void setup_signal_handlers();
    void handle_shutdown_signal(int signal);
    
    // Status and metrics
    struct LifecycleMetrics {
        Timestamp startup_time = 0;
        Timestamp shutdown_time = 0;
        std::chrono::milliseconds uptime{0};
        uint32_t restart_count = 0;
        uint32_t emergency_stops = 0;
        ApplicationState current_state = ApplicationState::STOPPED;
    };
    
    LifecycleMetrics get_metrics() const;
    
    // Callbacks
    using StateChangeCallback = std::function<void(ApplicationState old_state, ApplicationState new_state)>;
    void set_state_change_callback(StateChangeCallback callback);

private:
    std::atomic<ApplicationState> state_{ApplicationState::STOPPED};
    
    // Tasks
    struct Task {
        std::string name;
        std::function<void()> function;
        int priority;
        
        bool operator<(const Task& other) const {
            return priority > other.priority; // Higher priority first
        }
    };
    
    std::vector<Task> startup_tasks_;
    std::vector<Task> shutdown_tasks_;
    mutable std::mutex tasks_mutex_;
    
    // Metrics
    Timestamp startup_time_ = 0;
    Timestamp shutdown_time_ = 0;
    std::atomic<uint32_t> restart_count_{0};
    std::atomic<uint32_t> emergency_stops_{0};
    
    // Callbacks
    StateChangeCallback state_change_callback_;
    
    // Internal methods
    void set_state(ApplicationState new_state);
    void execute_startup_tasks();
    void execute_shutdown_tasks();
    void emergency_cleanup();
};

//=============================================================================
// GLOBAL SESSION MANAGEMENT
//=============================================================================

namespace GlobalSession {

// Initialize global session management
void initialize(std::unique_ptr<SessionStore> store = nullptr);
void shutdown();
bool is_initialized();

// Access global session manager
SessionManager& get_manager();
LifecycleManager& get_lifecycle();

// Convenience functions
SessionId create_session(const UserId& user_id, const TenantId& tenant_id = "default");
std::shared_ptr<Session> get_session(const SessionId& session_id);
void record_event(const SessionId& session_id, const std::string& event_name);
void record_log(const SessionId& session_id);

} // namespace GlobalSession

//=============================================================================
// CONVENIENCE MACROS FOR SESSION MANAGEMENT
//=============================================================================

#define UC_SESSION_CREATE(user_id) GlobalSession::create_session(user_id)
#define UC_SESSION_GET(session_id) GlobalSession::get_session(session_id)
#define UC_SESSION_EVENT(session_id, event_name) GlobalSession::record_event(session_id, event_name)
#define UC_SESSION_LOG(session_id) GlobalSession::record_log(session_id)

#define UC_LIFECYCLE_START() GlobalSession::get_lifecycle().start()
#define UC_LIFECYCLE_STOP() GlobalSession::get_lifecycle().stop()
#define UC_LIFECYCLE_STATE() GlobalSession::get_lifecycle().get_state()

} // namespace usercanal