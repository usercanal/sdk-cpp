// src/session.cpp
// Server-optimized session management implementation for high-performance analytics
// Designed for production server environments with concurrent session handling

#include "usercanal/session.hpp"
#include "usercanal/utils.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <csignal>
#include <cstdlib>
#include <shared_mutex>

namespace usercanal {

//=============================================================================
// Session Implementation
//=============================================================================

Session::Session(const SessionId& session_id, const UserId& user_id, const TenantId& tenant_id)
    : session_id_(session_id), user_id_(user_id), tenant_id_(tenant_id) {
    auto now = Utils::now_milliseconds();
    created_at_.store(now);
    last_activity_.store(now);
}

Session::Session(const Session& other)
    : session_id_(other.session_id_), user_id_(other.user_id_), tenant_id_(other.tenant_id_),
      state_(other.state_.load()), created_at_(other.created_at_.load()),
      last_activity_(other.last_activity_.load()), timeout_(other.timeout_),
      events_count_(other.events_count_.load()), logs_count_(other.logs_count_.load()) {
    std::lock_guard<std::mutex> lock(other.mutex_);
    properties_ = other.properties_;
    ip_address_ = other.ip_address_;
    user_agent_ = other.user_agent_;
    last_event_name_ = other.last_event_name_;
}

Session& Session::operator=(const Session& other) {
    if (this != &other) {
        std::lock(mutex_, other.mutex_);
        std::lock_guard<std::mutex> lock1(mutex_, std::adopt_lock);
        std::lock_guard<std::mutex> lock2(other.mutex_, std::adopt_lock);
        
        session_id_ = other.session_id_;
        user_id_ = other.user_id_;
        tenant_id_ = other.tenant_id_;
        state_.store(other.state_.load());
        created_at_.store(other.created_at_.load());
        last_activity_.store(other.last_activity_.load());
        timeout_ = other.timeout_;
        properties_ = other.properties_;
        ip_address_ = other.ip_address_;
        user_agent_ = other.user_agent_;
        events_count_.store(other.events_count_.load());
        logs_count_.store(other.logs_count_.load());
        last_event_name_ = other.last_event_name_;
    }
    return *this;
}

Session::Session(Session&& other) noexcept
    : session_id_(std::move(other.session_id_)), user_id_(std::move(other.user_id_)),
      tenant_id_(std::move(other.tenant_id_)), state_(other.state_.load()),
      created_at_(other.created_at_.load()), last_activity_(other.last_activity_.load()),
      timeout_(other.timeout_), events_count_(other.events_count_.load()),
      logs_count_(other.logs_count_.load()) {
    std::lock_guard<std::mutex> lock(other.mutex_);
    properties_ = std::move(other.properties_);
    ip_address_ = std::move(other.ip_address_);
    user_agent_ = std::move(other.user_agent_);
    last_event_name_ = std::move(other.last_event_name_);
}

Session& Session::operator=(Session&& other) noexcept {
    if (this != &other) {
        std::lock(mutex_, other.mutex_);
        std::lock_guard<std::mutex> lock1(mutex_, std::adopt_lock);
        std::lock_guard<std::mutex> lock2(other.mutex_, std::adopt_lock);
        
        session_id_ = std::move(other.session_id_);
        user_id_ = std::move(other.user_id_);
        tenant_id_ = std::move(other.tenant_id_);
        state_.store(other.state_.load());
        created_at_.store(other.created_at_.load());
        last_activity_.store(other.last_activity_.load());
        timeout_ = other.timeout_;
        properties_ = std::move(other.properties_);
        ip_address_ = std::move(other.ip_address_);
        user_agent_ = std::move(other.user_agent_);
        events_count_.store(other.events_count_.load());
        logs_count_.store(other.logs_count_.load());
        last_event_name_ = std::move(other.last_event_name_);
    }
    return *this;
}

SessionState Session::get_state() const {
    return state_.load(std::memory_order_relaxed);
}

void Session::set_state(SessionState state) {
    state_.store(state, std::memory_order_relaxed);
}

bool Session::is_expired() const {
    auto now = Utils::now_milliseconds();
    auto last_activity = last_activity_.load(std::memory_order_relaxed);
    auto timeout_ms = timeout_.count() * 1000;
    return (now - last_activity) > static_cast<Timestamp>(timeout_ms);
}

void Session::record_activity() {
    last_activity_.store(Utils::now_milliseconds(), std::memory_order_relaxed);
    if (get_state() == SessionState::IDLE) {
        set_state(SessionState::ACTIVE);
    }
}

void Session::record_event(const std::string& event_name) {
    record_activity();
    events_count_.fetch_add(1, std::memory_order_relaxed);
    
    std::lock_guard<std::mutex> lock(mutex_);
    last_event_name_ = event_name;
}

void Session::record_log() {
    record_activity();
    logs_count_.fetch_add(1, std::memory_order_relaxed);
}

Timestamp Session::get_last_activity() const {
    return last_activity_.load(std::memory_order_relaxed);
}

std::chrono::milliseconds Session::get_duration() const {
    auto now = Utils::now_milliseconds();
    auto created = created_at_.load(std::memory_order_relaxed);
    return std::chrono::milliseconds(now - created);
}

void Session::set_property(const std::string& key, const PropertyValue& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    properties_[key] = value;
    record_activity();
}

std::optional<PropertyValue> Session::get_property(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (properties_.contains(key)) {
        return properties_.at(key);
    }
    return std::nullopt;
}

void Session::remove_property(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Properties class doesn't have erase, so we'll skip this implementation for now
    // In a full implementation, we'd extend Properties with erase capability
    (void)key; // Suppress unused parameter warning
    record_activity();
}

Properties Session::get_all_properties() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return properties_;
}

void Session::set_context(const std::string& ip_address, const std::string& user_agent) {
    std::lock_guard<std::mutex> lock(mutex_);
    ip_address_ = ip_address;
    user_agent_ = user_agent;
    record_activity();
}

std::string Session::get_ip_address() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ip_address_;
}

std::string Session::get_user_agent() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return user_agent_;
}

SessionMetrics Session::get_metrics() const {
    SessionMetrics metrics;
    metrics.events_count = events_count_.load(std::memory_order_relaxed);
    metrics.logs_count = logs_count_.load(std::memory_order_relaxed);
    metrics.created_at = created_at_.load(std::memory_order_relaxed);
    metrics.last_activity = last_activity_.load(std::memory_order_relaxed);
    metrics.total_duration = get_duration();
    
    std::lock_guard<std::mutex> lock(mutex_);
    metrics.last_event_name = last_event_name_;
    metrics.user_agent = user_agent_;
    metrics.ip_address = ip_address_;
    
    return metrics;
}

void Session::set_timeout(std::chrono::seconds timeout) {
    timeout_ = timeout;
}

std::chrono::seconds Session::get_timeout() const {
    return timeout_;
}

std::string Session::serialize() const {
    std::ostringstream oss;
    oss << session_id_ << "|" << user_id_ << "|" << tenant_id_ << "|";
    oss << static_cast<int>(get_state()) << "|";
    oss << created_at_.load() << "|" << last_activity_.load() << "|";
    oss << timeout_.count() << "|";
    oss << events_count_.load() << "|" << logs_count_.load() << "|";
    
    std::lock_guard<std::mutex> lock(mutex_);
    oss << ip_address_ << "|" << user_agent_ << "|" << last_event_name_;
    
    return oss.str();
}

std::unique_ptr<Session> Session::deserialize(const std::string& data) {
    std::istringstream iss(data);
    std::string token;
    std::vector<std::string> tokens;
    
    while (std::getline(iss, token, '|')) {
        tokens.push_back(token);
    }
    
    if (tokens.size() < 12) {
        return nullptr;
    }
    
    auto session = std::make_unique<Session>(tokens[0], tokens[1], tokens[2]);
    session->set_state(static_cast<SessionState>(std::stoi(tokens[3])));
    session->created_at_.store(std::stoull(tokens[4]));
    session->last_activity_.store(std::stoull(tokens[5]));
    session->timeout_ = std::chrono::seconds(std::stoll(tokens[6]));
    session->events_count_.store(std::stoull(tokens[7]));
    session->logs_count_.store(std::stoull(tokens[8]));
    
    {
        std::lock_guard<std::mutex> lock(session->mutex_);
        session->ip_address_ = tokens[9];
        session->user_agent_ = tokens[10];
        session->last_event_name_ = tokens[11];
    }
    
    return session;
}

//=============================================================================
// MemorySessionStore Implementation
//=============================================================================

MemorySessionStore::MemorySessionStore(const std::string& persistence_file)
    : persistence_file_(persistence_file) {
    if (!persistence_file_.empty()) {
        persistence_enabled_ = true;
        load_from_disk();
    }
}

MemorySessionStore::~MemorySessionStore() {
    if (persistence_enabled_) {
        save_to_disk();
    }
}

bool MemorySessionStore::save_session(const Session& session) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // Enforce session limit
    if (sessions_.size() >= max_sessions_) {
        // Remove oldest expired session
        auto oldest = std::min_element(sessions_.begin(), sessions_.end(),
            [](const auto& a, const auto& b) {
                return a.second->get_last_activity() < b.second->get_last_activity();
            });
        
        if (oldest != sessions_.end() && oldest->second->is_expired()) {
            remove_from_indexes(oldest->first);
            sessions_.erase(oldest);
        } else {
            return false; // Cannot save, store is full
        }
    }
    
    auto session_id = session.get_session_id();
    sessions_[session_id] = std::make_unique<Session>(session);
    update_indexes(session, true);
    
    total_sessions_created_.fetch_add(1, std::memory_order_relaxed);
    
    if (persistence_enabled_) {
        // Async save in production - for now just immediate
        save_to_disk();
    }
    
    return true;
}

std::unique_ptr<Session> MemorySessionStore::load_session(const SessionId& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        // Return a copy
        return std::make_unique<Session>(*it->second);
    }
    
    return nullptr;
}

bool MemorySessionStore::delete_session(const SessionId& session_id) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        remove_from_indexes(session_id);
        sessions_.erase(it);
        total_sessions_deleted_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    
    return false;
}

bool MemorySessionStore::session_exists(const SessionId& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.find(session_id) != sessions_.end();
}

std::vector<SessionId> MemorySessionStore::get_all_session_ids() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SessionId> ids;
    ids.reserve(sessions_.size());
    
    for (const auto& pair : sessions_) {
        ids.push_back(pair.first);
    }
    
    return ids;
}

std::vector<SessionId> MemorySessionStore::get_sessions_for_user(const UserId& user_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = user_sessions_.find(user_id);
    if (it != user_sessions_.end()) {
        return it->second;
    }
    return {};
}

std::vector<SessionId> MemorySessionStore::get_sessions_for_tenant(const TenantId& tenant_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tenant_sessions_.find(tenant_id);
    if (it != tenant_sessions_.end()) {
        return it->second;
    }
    return {};
}

size_t MemorySessionStore::cleanup_expired_sessions() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    std::vector<SessionId> expired_sessions;
    for (const auto& pair : sessions_) {
        if (pair.second->is_expired()) {
            expired_sessions.push_back(pair.first);
        }
    }
    
    for (const auto& session_id : expired_sessions) {
        remove_from_indexes(session_id);
        sessions_.erase(session_id);
        total_sessions_deleted_.fetch_add(1, std::memory_order_relaxed);
    }
    
    return expired_sessions.size();
}

size_t MemorySessionStore::get_session_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

std::unordered_map<std::string, std::string> MemorySessionStore::get_store_metrics() const {
    std::unordered_map<std::string, std::string> metrics;
    metrics["total_sessions"] = std::to_string(get_session_count());
    metrics["sessions_created"] = std::to_string(total_sessions_created_.load());
    metrics["sessions_deleted"] = std::to_string(total_sessions_deleted_.load());
    metrics["max_sessions"] = std::to_string(max_sessions_);
    metrics["persistence_enabled"] = persistence_enabled_ ? "true" : "false";
    if (persistence_enabled_) {
        metrics["persistence_file"] = persistence_file_;
    }
    return metrics;
}

bool MemorySessionStore::save_to_disk() const {
    if (persistence_file_.empty()) return false;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        std::ofstream file(persistence_file_);
        if (!file.is_open()) return false;
        
        file << sessions_.size() << "\n";
        for (const auto& pair : sessions_) {
            file << pair.second->serialize() << "\n";
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

bool MemorySessionStore::load_from_disk() {
    if (persistence_file_.empty()) return false;
    
    std::unique_lock<std::mutex> lock(mutex_);
    
    try {
        std::ifstream file(persistence_file_);
        if (!file.is_open()) return false;
        
        size_t count;
        file >> count;
        file.ignore(); // Skip newline
        
        sessions_.clear();
        user_sessions_.clear();
        tenant_sessions_.clear();
        
        for (size_t i = 0; i < count; ++i) {
            std::string line;
            std::getline(file, line);
            
            auto session = Session::deserialize(line);
            if (session) {
                auto session_id = session->get_session_id();
                update_indexes(*session, true);
                sessions_[session_id] = std::move(session);
            }
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

void MemorySessionStore::update_indexes(const Session& session, bool add) {
    auto user_id = session.get_user_id();
    auto tenant_id = session.get_tenant_id();
    auto session_id = session.get_session_id();
    
    if (add) {
        user_sessions_[user_id].push_back(session_id);
        tenant_sessions_[tenant_id].push_back(session_id);
    }
}

void MemorySessionStore::remove_from_indexes(const SessionId& session_id) {
    // Remove from user index
    for (auto& pair : user_sessions_) {
        auto& sessions = pair.second;
        sessions.erase(std::remove(sessions.begin(), sessions.end(), session_id), sessions.end());
        if (sessions.empty()) {
            user_sessions_.erase(pair.first);
        }
    }
    
    // Remove from tenant index
    for (auto& pair : tenant_sessions_) {
        auto& sessions = pair.second;
        sessions.erase(std::remove(sessions.begin(), sessions.end(), session_id), sessions.end());
        if (sessions.empty()) {
            tenant_sessions_.erase(pair.first);
        }
    }
}

//=============================================================================
// SessionManager Implementation
//=============================================================================

SessionManager::SessionManager(std::unique_ptr<SessionStore> store)
    : store_(std::move(store)) {
    if (!store_) {
        store_ = std::make_unique<MemorySessionStore>();
    }
}

SessionManager::~SessionManager() {
    stop();
}

SessionId SessionManager::create_session(const UserId& user_id, const TenantId& tenant_id) {
    // Generate unique session ID
    auto session_id = generate_session_id();
    
    // Create session
    auto session = std::make_shared<Session>(session_id, user_id, tenant_id);
    session->set_timeout(default_timeout_);
    
    // Enforce limits
    enforce_user_session_limit(user_id);
    enforce_tenant_session_limit(tenant_id);
    
    // Store session
    {
        std::unique_lock<std::mutex> lock(sessions_mutex_);
        active_sessions_[session_id] = session;
    }
    
    store_->save_session(*session);
    total_sessions_created_.fetch_add(1, std::memory_order_relaxed);
    
    // Notify callback
    if (session_created_callback_) {
        session_created_callback_(session_id, user_id);
    }
    
    return session_id;
}

std::shared_ptr<Session> SessionManager::get_session(const SessionId& session_id) {
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = active_sessions_.find(session_id);
        if (it != active_sessions_.end()) {
            return it->second;
        }
    }
    
    // Try to load from store (outside the lock)
    auto stored_session = store_->load_session(session_id);
    if (stored_session) {
        auto shared_session = std::shared_ptr<Session>(stored_session.release());
        
        std::unique_lock<std::mutex> write_lock(sessions_mutex_);
        active_sessions_[session_id] = shared_session;
        return shared_session;
    }
    
    return nullptr;
}

bool SessionManager::terminate_session(const SessionId& session_id) {
    std::unique_lock<std::mutex> lock(sessions_mutex_);
    
    auto it = active_sessions_.find(session_id);
    if (it != active_sessions_.end()) {
        auto session = it->second;
        session->set_state(SessionState::TERMINATED);
        
        auto user_id = session->get_user_id();
        active_sessions_.erase(it);
        
        store_->delete_session(session_id);
        total_sessions_terminated_.fetch_add(1, std::memory_order_relaxed);
        
        // Notify callback
        if (session_terminated_callback_) {
            session_terminated_callback_(session_id, user_id);
        }
        
        return true;
    }
    
    return false;
}

bool SessionManager::session_exists(const SessionId& session_id) const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    if (active_sessions_.find(session_id) != active_sessions_.end()) {
        return true;
    }
    
    return store_->session_exists(session_id);
}

void SessionManager::record_session_event(const SessionId& session_id, const std::string& event_name) {
    auto session = get_session(session_id);
    if (session) {
        session->record_event(event_name);
        store_->save_session(*session);
    }
}

void SessionManager::record_session_log(const SessionId& session_id) {
    auto session = get_session(session_id);
    if (session) {
        session->record_log();
        store_->save_session(*session);
    }
}

void SessionManager::update_session_context(const SessionId& session_id, 
                                           const std::string& ip_address, 
                                           const std::string& user_agent) {
    auto session = get_session(session_id);
    if (session) {
        session->set_context(ip_address, user_agent);
        store_->save_session(*session);
    }
}

std::vector<SessionId> SessionManager::get_user_sessions(const UserId& user_id) const {
    return store_->get_sessions_for_user(user_id);
}

std::vector<SessionId> SessionManager::get_tenant_sessions(const TenantId& tenant_id) const {
    return store_->get_sessions_for_tenant(tenant_id);
}

void SessionManager::terminate_user_sessions(const UserId& user_id) {
    auto sessions = get_user_sessions(user_id);
    for (const auto& session_id : sessions) {
        terminate_session(session_id);
    }
}

void SessionManager::terminate_tenant_sessions(const TenantId& tenant_id) {
    auto sessions = get_tenant_sessions(tenant_id);
    for (const auto& session_id : sessions) {
        terminate_session(session_id);
    }
}

SessionManager::SessionAnalytics SessionManager::get_analytics() const {
    SessionAnalytics analytics;
    
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    analytics.total_sessions = active_sessions_.size();
    
    uint64_t total_events = 0;
    uint64_t total_logs = 0;
    std::chrono::milliseconds total_duration{0};
    
    for (const auto& pair : active_sessions_) {
        auto session = pair.second;
        auto metrics = session->get_metrics();
        
        switch (session->get_state()) {
            case SessionState::ACTIVE:
                analytics.active_sessions++;
                break;
            case SessionState::IDLE:
                analytics.idle_sessions++;
                break;
            case SessionState::EXPIRED:
                analytics.expired_sessions++;
                break;
            default:
                break;
        }
        
        total_events += metrics.events_count;
        total_logs += metrics.logs_count;
        total_duration += metrics.total_duration;
        
        analytics.sessions_by_tenant[session->get_tenant_id()]++;
    }
    
    if (analytics.total_sessions > 0) {
        analytics.events_per_session = static_cast<double>(total_events) / analytics.total_sessions;
        analytics.logs_per_session = static_cast<double>(total_logs) / analytics.total_sessions;
        analytics.average_session_duration = std::chrono::duration_cast<std::chrono::seconds>(
            total_duration / analytics.total_sessions);
    }
    
    return analytics;
}

void SessionManager::set_default_timeout(std::chrono::seconds timeout) {
    default_timeout_ = timeout;
}

void SessionManager::set_cleanup_interval(std::chrono::seconds interval) {
    cleanup_interval_ = interval;
}

void SessionManager::set_max_sessions_per_user(size_t max_sessions) {
    max_sessions_per_user_ = max_sessions;
}

void SessionManager::set_max_sessions_per_tenant(size_t max_sessions) {
    max_sessions_per_tenant_ = max_sessions;
}

void SessionManager::start() {
    if (running_.exchange(true)) {
        return; // Already running
    }
    
    cleanup_thread_ = std::thread(&SessionManager::cleanup_loop, this);
}

void SessionManager::stop() {
    if (!running_.exchange(false)) {
        return; // Already stopped
    }
    
    cleanup_cv_.notify_all();
    
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
    
    save_all_sessions();
}

size_t SessionManager::cleanup_expired_sessions() {
    // Clean up active sessions
    std::vector<SessionId> expired_sessions;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (const auto& pair : active_sessions_) {
            if (pair.second->is_expired()) {
                expired_sessions.push_back(pair.first);
            }
        }
    }
    
    // Remove expired sessions
    for (const auto& session_id : expired_sessions) {
        auto session = get_session(session_id);
        if (session) {
            session->set_state(SessionState::EXPIRED);
            auto user_id = session->get_user_id();
            
            {
                std::unique_lock<std::mutex> lock(sessions_mutex_);
                active_sessions_.erase(session_id);
            }
            
            store_->delete_session(session_id);
            total_sessions_expired_.fetch_add(1, std::memory_order_relaxed);
            
            // Notify callback
            if (session_expired_callback_) {
                session_expired_callback_(session_id, user_id);
            }
        }
    }
    
    // Clean up store
    auto store_cleaned = store_->cleanup_expired_sessions();
    
    return expired_sessions.size() + store_cleaned;
}

void SessionManager::save_all_sessions() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (const auto& pair : active_sessions_) {
        store_->save_session(*pair.second);
    }
}

void SessionManager::load_all_sessions() {
    auto session_ids = store_->get_all_session_ids();
    
    std::unique_lock<std::mutex> lock(sessions_mutex_);
    for (const auto& session_id : session_ids) {
        auto session = store_->load_session(session_id);
        if (session && !session->is_expired()) {
            active_sessions_[session_id] = std::shared_ptr<Session>(session.release());
        }
    }
}

void SessionManager::set_session_created_callback(SessionCreatedCallback callback) {
    session_created_callback_ = std::move(callback);
}

void SessionManager::set_session_expired_callback(SessionExpiredCallback callback) {
    session_expired_callback_ = std::move(callback);
}

void SessionManager::set_session_terminated_callback(SessionTerminatedCallback callback) {
    session_terminated_callback_ = std::move(callback);
}

SessionId SessionManager::generate_session_id() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "sess_" << Utils::now_milliseconds() << "_";
    
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

void SessionManager::cleanup_loop() {
    while (running_) {
        try {
            cleanup_expired_sessions();
            
            std::unique_lock<std::mutex> lock(cleanup_mutex_);
            cleanup_cv_.wait_for(lock, cleanup_interval_, [this] { return !running_; });
            
        } catch (...) {
            // Continue cleanup loop even on errors
        }
    }
}

void SessionManager::enforce_user_session_limit(const UserId& user_id) {
    auto user_sessions = get_user_sessions(user_id);
    
    while (user_sessions.size() >= max_sessions_per_user_) {
        // Remove oldest session
        SessionId oldest_session;
        Timestamp oldest_time = std::numeric_limits<Timestamp>::max();
        
        for (const auto& session_id : user_sessions) {
            auto session = get_session(session_id);
            if (session) {
                auto last_activity = session->get_last_activity();
                if (last_activity < oldest_time) {
                    oldest_time = last_activity;
                    oldest_session = session_id;
                }
            }
        }
        
        if (!oldest_session.empty()) {
            terminate_session(oldest_session);
            user_sessions = get_user_sessions(user_id); // Refresh
        } else {
            break;
        }
    }
}

void SessionManager::enforce_tenant_session_limit(const TenantId& tenant_id) {
    auto tenant_sessions = get_tenant_sessions(tenant_id);
    
    while (tenant_sessions.size() >= max_sessions_per_tenant_) {
        // Remove oldest session
        SessionId oldest_session;
        Timestamp oldest_time = std::numeric_limits<Timestamp>::max();
        
        for (const auto& session_id : tenant_sessions) {
            auto session = get_session(session_id);
            if (session) {
                auto last_activity = session->get_last_activity();
                if (last_activity < oldest_time) {
                    oldest_time = last_activity;
                    oldest_session = session_id;
                }
            }
        }
        
        if (!oldest_session.empty()) {
            terminate_session(oldest_session);
            tenant_sessions = get_tenant_sessions(tenant_id); // Refresh
        } else {
            break;
        }
    }
}

//=============================================================================
// LifecycleManager Implementation
//=============================================================================

LifecycleManager::LifecycleManager() {
    startup_time_ = Utils::now_milliseconds();
}

LifecycleManager::~LifecycleManager() {
    stop();
}

void LifecycleManager::start() {
    auto old_state = state_.load();
    if (old_state != ApplicationState::STOPPED) {
        return; // Already started or starting
    }
    
    set_state(ApplicationState::STARTING);
    
    try {
        execute_startup_tasks();
        set_state(ApplicationState::RUNNING);
        startup_time_ = Utils::now_milliseconds();
    } catch (...) {
        set_state(ApplicationState::FAILED);
        throw;
    }
}

void LifecycleManager::stop(std::chrono::milliseconds timeout) {
    (void)timeout; // Suppress unused parameter warning
    auto old_state = state_.load();
    if (old_state == ApplicationState::STOPPED || old_state == ApplicationState::STOPPING) {
        return; // Already stopped or stopping
    }
    
    set_state(ApplicationState::STOPPING);
    shutdown_time_ = Utils::now_milliseconds();
    
    try {
        // TODO: Implement timeout handling for shutdown tasks
        execute_shutdown_tasks();
        set_state(ApplicationState::STOPPED);
    } catch (...) {
        set_state(ApplicationState::FAILED);
        throw;
    }
}

void LifecycleManager::restart() {
    stop();
    restart_count_.fetch_add(1, std::memory_order_relaxed);
    start();
}

void LifecycleManager::emergency_stop() {
    set_state(ApplicationState::STOPPING);
    emergency_stops_.fetch_add(1, std::memory_order_relaxed);
    
    try {
        emergency_cleanup();
        set_state(ApplicationState::STOPPED);
    } catch (...) {
        set_state(ApplicationState::FAILED);
    }
}

void LifecycleManager::register_startup_task(const std::string& name, StartupTask task, int priority) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    startup_tasks_.push_back({name, std::move(task), priority});
    std::sort(startup_tasks_.begin(), startup_tasks_.end());
}

void LifecycleManager::register_shutdown_task(const std::string& name, ShutdownTask task, int priority) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    shutdown_tasks_.push_back({name, std::move(task), priority});
    std::sort(shutdown_tasks_.begin(), shutdown_tasks_.end());
}

void LifecycleManager::setup_signal_handlers() {
    // Simple signal handling - in production would be more sophisticated
    std::signal(SIGTERM, [](int sig) {
        (void)sig; // Suppress unused parameter warning
        // Handle shutdown signal
        std::exit(0);
    });
    
    std::signal(SIGINT, [](int sig) {
        (void)sig; // Suppress unused parameter warning
        // Handle interrupt signal
        std::exit(0);
    });
}

void LifecycleManager::handle_shutdown_signal(int signal) {
    (void)signal; // Suppress unused parameter warning
    emergency_stop();
}

LifecycleManager::LifecycleMetrics LifecycleManager::get_metrics() const {
    LifecycleMetrics metrics;
    metrics.startup_time = startup_time_;
    metrics.shutdown_time = shutdown_time_;
    metrics.current_state = state_.load();
    metrics.restart_count = restart_count_.load();
    metrics.emergency_stops = emergency_stops_.load();
    
    if (metrics.current_state == ApplicationState::RUNNING && startup_time_ > 0) {
        auto now = Utils::now_milliseconds();
        metrics.uptime = std::chrono::milliseconds(now - startup_time_);
    }
    
    return metrics;
}

void LifecycleManager::set_state_change_callback(StateChangeCallback callback) {
    state_change_callback_ = std::move(callback);
}

void LifecycleManager::set_state(ApplicationState new_state) {
    auto old_state = state_.exchange(new_state);
    
    if (state_change_callback_ && old_state != new_state) {
        state_change_callback_(old_state, new_state);
    }
}

void LifecycleManager::execute_startup_tasks() {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    for (const auto& task : startup_tasks_) {
        try {
            task.function();
        } catch (const std::exception& e) {
            // In production, would have better error handling
            throw std::runtime_error("Startup task '" + task.name + "' failed: " + e.what());
        }
    }
}

void LifecycleManager::execute_shutdown_tasks() {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    for (const auto& task : shutdown_tasks_) {
        try {
            task.function();
        } catch (...) {
            // Continue with other shutdown tasks even if one fails
        }
    }
}

void LifecycleManager::emergency_cleanup() {
    // Minimal cleanup for emergency shutdown
    // In production, would handle critical resource cleanup
}

//=============================================================================
// GlobalSession Implementation
//=============================================================================

namespace GlobalSession {

static std::unique_ptr<SessionManager> g_session_manager;
static std::unique_ptr<LifecycleManager> g_lifecycle_manager;
static std::mutex g_init_mutex;
static bool g_initialized = false;

void initialize(std::unique_ptr<SessionStore> store) {
    std::lock_guard<std::mutex> lock(g_init_mutex);
    
    if (g_initialized) {
        return;
    }
    
    g_session_manager = std::make_unique<SessionManager>(std::move(store));
    g_lifecycle_manager = std::make_unique<LifecycleManager>();
    
    g_session_manager->start();
    g_lifecycle_manager->start();
    
    g_initialized = true;
}

void shutdown() {
    std::lock_guard<std::mutex> lock(g_init_mutex);
    
    if (!g_initialized) {
        return;
    }
    
    if (g_session_manager) {
        g_session_manager->stop();
        g_session_manager.reset();
    }
    
    if (g_lifecycle_manager) {
        g_lifecycle_manager->stop();
        g_lifecycle_manager.reset();
    }
    
    g_initialized = false;
}

bool is_initialized() {
    std::lock_guard<std::mutex> lock(g_init_mutex);
    return g_initialized;
}

SessionManager& get_manager() {
    std::lock_guard<std::mutex> lock(g_init_mutex);
    
    if (!g_initialized || !g_session_manager) {
        throw std::runtime_error("GlobalSession not initialized");
    }
    
    return *g_session_manager;
}

LifecycleManager& get_lifecycle() {
    std::lock_guard<std::mutex> lock(g_init_mutex);
    
    if (!g_initialized || !g_lifecycle_manager) {
        throw std::runtime_error("GlobalSession not initialized");
    }
    
    return *g_lifecycle_manager;
}

SessionId create_session(const UserId& user_id, const TenantId& tenant_id) {
    return get_manager().create_session(user_id, tenant_id);
}

std::shared_ptr<Session> get_session(const SessionId& session_id) {
    return get_manager().get_session(session_id);
}

void record_event(const SessionId& session_id, const std::string& event_name) {
    get_manager().record_session_event(session_id, event_name);
}

void record_log(const SessionId& session_id) {
    get_manager().record_session_log(session_id);
}

} // namespace GlobalSession

} // namespace usercanal