// src/utils.cpp
// Implementation of utility functions for common operations

#include "usercanal/utils.hpp"
#include <algorithm>
#include <sstream>
#include <regex>
#include <fstream>
#include <cstring>
#include <mutex>
#include <functional>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#else
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#endif

namespace usercanal {
namespace Utils {

// String utilities
std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) return "";
    
    size_t end = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(start, end - start + 1);
}

std::string to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string to_upper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    return tokens;
}

bool starts_with(const std::string& str, const std::string& prefix) {
    return str.length() >= prefix.length() && 
           str.compare(0, prefix.length(), prefix) == 0;
}

bool ends_with(const std::string& str, const std::string& suffix) {
    return str.length() >= suffix.length() && 
           str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

// Network utilities
std::pair<std::string, uint16_t> parse_endpoint(const std::string& endpoint) {
    size_t colon_pos = endpoint.find_last_of(':');
    if (colon_pos == std::string::npos) {
        return {"", 0};
    }
    
    std::string host = endpoint.substr(0, colon_pos);
    std::string port_str = endpoint.substr(colon_pos + 1);
    
    try {
        uint16_t port = static_cast<uint16_t>(std::stoul(port_str));
        return {host, port};
    } catch (const std::exception&) {
        return {"", 0};
    }
}

bool is_valid_hostname(const std::string& hostname) {
    if (hostname.empty() || hostname.length() > 253) {
        return false;
    }
    
    // Basic hostname validation regex
    static const std::regex hostname_regex(R"(^[a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?(\.[a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?)*$)");
    return std::regex_match(hostname, hostname_regex);
}

bool is_valid_port(uint16_t port) {
    return port > 0 && port <= 65535;
}

std::string get_hostname() {
#ifdef _WIN32
    char hostname[256];
    DWORD size = sizeof(hostname);
    if (GetComputerNameA(hostname, &size)) {
        return std::string(hostname);
    }
    return "unknown-host";
#else
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        hostname[255] = '\0'; // Ensure null termination
        return std::string(hostname);
    }
    return "unknown-host";
#endif
}

std::string get_local_ip() {
#ifdef _WIN32
    // Windows implementation would be more complex
    return "127.0.0.1";
#else
    struct ifaddrs *ifaddr, *ifa;
    char host[NI_MAXHOST];
    
    if (getifaddrs(&ifaddr) == -1) {
        return "127.0.0.1";
    }
    
    std::string result = "127.0.0.1";
    
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;
        
        if (ifa->ifa_addr->sa_family == AF_INET) {
            int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                               host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
            
            if (s == 0 && strcmp(host, "127.0.0.1") != 0) {
                result = host;
                break;
            }
        }
    }
    
    freeifaddrs(ifaddr);
    return result;
#endif
}

// Time utilities
Timestamp current_timestamp_ms() {
    return now_milliseconds();
}

std::string timestamp_to_iso8601(Timestamp timestamp_ms) {
    auto time_point = std::chrono::system_clock::from_time_t(timestamp_ms / 1000);
    auto time_t = std::chrono::system_clock::to_time_t(time_point);
    auto ms = timestamp_ms % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms << "Z";
    return ss.str();
}

std::string current_iso8601_timestamp() {
    return timestamp_to_iso8601(current_timestamp_ms());
}

// System utilities
size_t get_available_memory() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return status.ullAvailPhys;
    }
    return 0;
#else
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_size > 0) {
        return static_cast<size_t>(pages) * static_cast<size_t>(page_size);
    }
    return 0;
#endif
}

size_t get_total_memory() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return status.ullTotalPhys;
    }
    return 0;
#else
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_size > 0) {
        return static_cast<size_t>(pages) * static_cast<size_t>(page_size);
    }
    return 0;
#endif
}

double get_cpu_usage() {
    // Simple implementation - would need platform-specific code for accuracy
    return 0.0;
}

size_t get_thread_count() {
    return std::thread::hardware_concurrency();
}

std::string get_os_info() {
#ifdef _WIN32
    return "Windows";
#else
    struct utsname info;
    if (uname(&info) == 0) {
        return std::string(info.sysname) + " " + std::string(info.release);
    }
    return "Unix";
#endif
}

std::string get_arch_info() {
#ifdef _WIN32
    #ifdef _M_X64
    return "x86_64";
    #elif defined(_M_IX86)
    return "x86";
    #else
    return "unknown";
    #endif
#else
    struct utsname info;
    if (uname(&info) == 0) {
        return std::string(info.machine);
    }
    return "unknown";
#endif
}

// Encoding utilities
static const char base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string base64_encode(const std::vector<uint8_t>& data) {
    std::string result;
    int val = 0, valb = -6;
    for (uint8_t c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            result.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        result.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (result.size() % 4) {
        result.push_back('=');
    }
    return result;
}

std::vector<uint8_t> base64_decode(const std::string& encoded) {
    std::vector<uint8_t> result;
    int val = 0, valb = -8;
    for (char c : encoded) {
        if (c == '=') break;
        auto pos = strchr(base64_chars, c);
        if (!pos) continue;
        val = (val << 6) + (pos - base64_chars);
        valb += 6;
        if (valb >= 0) {
            result.push_back((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    return result;
}

std::string hex_encode(const std::vector<uint8_t>& data) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t byte : data) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

std::vector<uint8_t> hex_decode(const std::string& hex) {
    std::vector<uint8_t> result;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte_str = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
        result.push_back(byte);
    }
    return result;
}

// Hash utilities (simplified implementations)
std::string sha256_hash(const std::string& data) {
    // This would need a proper SHA256 implementation
    // For now, return a simple hash as placeholder
    std::hash<std::string> hasher;
    size_t hash = hasher(data);
    std::stringstream ss;
    ss << std::hex << hash;
    return ss.str();
}

std::string md5_hash(const std::string& data) {
    // This would need a proper MD5 implementation
    // For now, return a simple hash as placeholder
    std::hash<std::string> hasher;
    size_t hash = hasher(data);
    std::stringstream ss;
    ss << std::hex << hash;
    return ss.str();
}

uint32_t crc32_hash(const std::string& data) {
    // Simple CRC32 implementation
    uint32_t crc = 0xFFFFFFFF;
    for (char c : data) {
        crc ^= static_cast<uint32_t>(c);
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

// File utilities
bool file_exists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

bool directory_exists(const std::string& path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES) && 
           (attrs & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat info;
    return (stat(path.c_str(), &info) == 0) && S_ISDIR(info.st_mode);
#endif
}

bool create_directory(const std::string& path) {
#ifdef _WIN32
    return CreateDirectoryA(path.c_str(), nullptr) != 0;
#else
    return mkdir(path.c_str(), 0755) == 0;
#endif
}

size_t get_file_size(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return 0;
    return static_cast<size_t>(file.tellg());
}

std::string read_file_contents(const std::string& path) {
    std::ifstream file(path);
    if (!file) return "";
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool write_file_contents(const std::string& path, const std::string& contents) {
    std::ofstream file(path);
    if (!file) return false;
    
    file << contents;
    return file.good();
}

// ThreadSafeCounter implementation
ThreadSafeCounter::ThreadSafeCounter(uint64_t initial_value) : value_(initial_value) {}

uint64_t ThreadSafeCounter::increment() {
    std::lock_guard<std::mutex> lock(mutex_);
    return ++value_;
}

uint64_t ThreadSafeCounter::decrement() {
    std::lock_guard<std::mutex> lock(mutex_);
    return --value_;
}

uint64_t ThreadSafeCounter::get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return value_;
}

void ThreadSafeCounter::set(uint64_t value) {
    std::lock_guard<std::mutex> lock(mutex_);
    value_ = value;
}

void ThreadSafeCounter::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    value_ = 0;
}

// ScopedTimer implementation
ScopedTimer::ScopedTimer(Duration& output) 
    : start_time_(Clock::now()), output_(&output), callback_(nullptr) {}

ScopedTimer::ScopedTimer(std::function<void(Duration)> callback)
    : start_time_(Clock::now()), output_(nullptr), callback_(std::move(callback)) {}

ScopedTimer::~ScopedTimer() {
    Duration elapsed_time = std::chrono::duration_cast<Duration>(Clock::now() - start_time_);
    if (output_) {
        *output_ = elapsed_time;
    }
    if (callback_) {
        callback_(elapsed_time);
    }
}

ScopedTimer::Duration ScopedTimer::elapsed() const {
    return std::chrono::duration_cast<Duration>(Clock::now() - start_time_);
}

// RateLimiter implementation
RateLimiter::RateLimiter(double requests_per_second, size_t burst_size)
    : rate_(requests_per_second), burst_size_(burst_size), tokens_(burst_size),
      last_refill_(std::chrono::steady_clock::now()) {}

bool RateLimiter::try_acquire(size_t tokens) {
    std::lock_guard<std::mutex> lock(mutex_);
    refill_tokens();
    
    if (tokens_ >= tokens) {
        tokens_ -= tokens;
        return true;
    }
    return false;
}

bool RateLimiter::wait_for_tokens(size_t tokens, std::chrono::milliseconds timeout) {
    auto start = std::chrono::steady_clock::now();
    
    while (std::chrono::steady_clock::now() - start < timeout) {
        if (try_acquire(tokens)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

double RateLimiter::get_rate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return rate_;
}

void RateLimiter::set_rate(double requests_per_second) {
    std::lock_guard<std::mutex> lock(mutex_);
    rate_ = requests_per_second;
}

void RateLimiter::refill_tokens() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_refill_);
    
    double tokens_to_add = rate_ * elapsed.count() / 1000.0;
    tokens_ = std::min(tokens_ + tokens_to_add, static_cast<double>(burst_size_));
    last_refill_ = now;
}

// ExponentialBackoff implementation
ExponentialBackoff::ExponentialBackoff(std::chrono::milliseconds base_delay,
                                       double multiplier,
                                       std::chrono::milliseconds max_delay)
    : base_delay_(base_delay), multiplier_(multiplier), max_delay_(max_delay),
      attempts_(0), current_delay_(base_delay) {}

std::chrono::milliseconds ExponentialBackoff::next_delay() {
    if (attempts_ == 0) {
        attempts_++;
        return current_delay_;
    }
    
    attempts_++;
    current_delay_ = std::chrono::milliseconds(
        static_cast<long long>(current_delay_.count() * multiplier_)
    );
    
    if (current_delay_ > max_delay_) {
        current_delay_ = max_delay_;
    }
    
    return current_delay_;
}

void ExponentialBackoff::reset() {
    attempts_ = 0;
    current_delay_ = base_delay_;
}

int ExponentialBackoff::attempt_count() const {
    return attempts_;
}

// CircuitBreaker implementation
CircuitBreaker::CircuitBreaker(int failure_threshold,
                               std::chrono::milliseconds timeout,
                               int success_threshold)
    : failure_threshold_(failure_threshold), success_threshold_(success_threshold),
      timeout_(timeout), state_(State::CLOSED), failure_count_(0), success_count_(0) {}

bool CircuitBreaker::is_call_allowed() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ == State::CLOSED) {
        return true;
    }
    
    if (state_ == State::OPEN) {
        auto now = std::chrono::steady_clock::now();
        if (now - last_failure_time_ > timeout_) {
            state_ = State::HALF_OPEN;
            success_count_ = 0;
            return true;
        }
        return false;
    }
    
    // HALF_OPEN state
    return true;
}

void CircuitBreaker::record_success() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ == State::HALF_OPEN) {
        success_count_++;
        if (success_count_ >= success_threshold_) {
            state_ = State::CLOSED;
            failure_count_ = 0;
        }
    } else if (state_ == State::CLOSED) {
        failure_count_ = 0;
    }
}

void CircuitBreaker::record_failure() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    failure_count_++;
    last_failure_time_ = std::chrono::steady_clock::now();
    
    if (state_ == State::HALF_OPEN || failure_count_ >= failure_threshold_) {
        state_ = State::OPEN;
    }
}

CircuitBreaker::State CircuitBreaker::get_state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

void CircuitBreaker::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = State::CLOSED;
    failure_count_ = 0;
    success_count_ = 0;
}

//=============================================================================
// SystemMonitor Implementation
//=============================================================================

size_t SystemMonitor::get_current_memory_usage() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
#else
    // Read from /proc/self/status on Linux
    std::ifstream status_file("/proc/self/status");
    if (!status_file.is_open()) {
        return 1024 * 1024; // Default to 1MB if can't read
    }
    
    std::string line;
    while (std::getline(status_file, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            std::istringstream iss(line);
            std::string label;
            size_t size;
            std::string unit;
            if (iss >> label >> size >> unit) {
                if (unit == "kB") {
                    return size * 1024; // Convert to bytes
                }
            }
            break;
        }
    }
    return 1024 * 1024; // Default to 1MB
#endif
}

double SystemMonitor::get_current_cpu_usage() {
    // Simplified CPU usage - would need more sophisticated implementation
    // For server environments, this could read from /proc/stat
    return 0.0;
}

uint64_t SystemMonitor::get_thread_count() {
#ifdef _WIN32
    return std::thread::hardware_concurrency();
#else
    // Count threads in /proc/self/task/
    std::ifstream stat_file("/proc/self/stat");
    if (stat_file.is_open()) {
        std::string line;
        std::getline(stat_file, line);
        std::istringstream iss(line);
        std::string token;
        
        // Skip to the 20th field (num_threads)
        for (int i = 0; i < 19; ++i) {
            iss >> token;
        }
        
        uint64_t thread_count;
        if (iss >> thread_count) {
            return thread_count;
        }
    }
    return std::thread::hardware_concurrency();
#endif
}

size_t SystemMonitor::get_available_memory() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return status.ullAvailPhys;
    }
    return 0;
#else
    // Read from /proc/meminfo
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) {
        return 1024 * 1024 * 1024; // Default to 1GB
    }
    
    std::string line;
    size_t available = 0;
    
    while (std::getline(meminfo, line)) {
        if (line.substr(0, 10) == "MemAvailable:") {
            std::istringstream iss(line);
            std::string label;
            size_t size;
            std::string unit;
            if (iss >> label >> size >> unit && unit == "kB") {
                available = size * 1024; // Convert to bytes
                break;
            }
        }
    }
    
    return available > 0 ? available : 1024 * 1024 * 1024; // Default to 1GB
#endif
}

size_t SystemMonitor::get_total_memory() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return status.ullTotalPhys;
    }
    return 0;
#else
    // Read from /proc/meminfo
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) {
        return 4ULL * 1024 * 1024 * 1024; // Default to 4GB
    }
    
    std::string line;
    size_t total = 0;
    
    while (std::getline(meminfo, line)) {
        if (line.substr(0, 9) == "MemTotal:") {
            std::istringstream iss(line);
            std::string label;
            size_t size;
            std::string unit;
            if (iss >> label >> size >> unit && unit == "kB") {
                total = size * 1024; // Convert to bytes
                break;
            }
        }
    }
    
    return total > 0 ? total : 4ULL * 1024 * 1024 * 1024; // Default to 4GB
#endif
}



} // namespace Utils
} // namespace usercanal