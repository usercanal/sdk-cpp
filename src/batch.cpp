// src/batch.cpp
// Implementation of batch management system for efficient data grouping

#include "usercanal/batch.hpp"
#include "usercanal/utils.hpp"
#include <algorithm>
#include <sstream>

#ifdef NLOHMANN_JSON_FOUND
#include <nlohmann/json.hpp>
#endif

// Temporary placeholders for FlatBuffers functionality
// These will be replaced with actual FlatBuffers code once headers are available
namespace flatbuffers {
    class FlatBufferBuilder {
    public:
        FlatBufferBuilder(size_t) {}
        void Clear() {}
        void Finish(void*) {}
        const uint8_t* GetBufferPointer() { return nullptr; }
        size_t GetSize() { return 0; }
        void* CreateVector(const std::vector<uint8_t>&) { return nullptr; }
        void* CreateString(const std::string&) { return nullptr; }
    };
}

namespace usercanal {

// BatchItem implementation
BatchItem::BatchItem(BatchItemType type, Timestamp timestamp)
    : type_(type), timestamp_(timestamp) {}

// EventBatchItem implementation
EventBatchItem::EventBatchItem(EventType event_type, const std::string& user_id, const Properties& properties)
    : BatchItem(BatchItemType::EVENT), event_type_(event_type), user_id_(user_id) {
    
    payload_ = BatchUtils::serialize_properties(properties);
    set_estimated_size(calculate_size());
}

EventBatchItem::EventBatchItem(EventType event_type, const std::string& user_id, std::vector<uint8_t> payload)
    : BatchItem(BatchItemType::EVENT), event_type_(event_type), user_id_(user_id), payload_(std::move(payload)) {
    set_estimated_size(calculate_size());
}

std::vector<uint8_t> EventBatchItem::serialize() const {
    // Temporary simple serialization - will be replaced with FlatBuffers
    std::ostringstream oss;
    oss << "EVENT:" << static_cast<int>(event_type_) << ":" << user_id_ << ":" << get_timestamp();
    std::string serialized_str = oss.str();
    return std::vector<uint8_t>(serialized_str.begin(), serialized_str.end());
}

size_t EventBatchItem::calculate_size() const {
    return sizeof(uint64_t) +  // timestamp
           sizeof(uint8_t) +   // event_type
           16 +                // user_id (UUID)
           sizeof(uint32_t) +  // payload length
           payload_.size() +   // payload data
           32;                 // FlatBuffers overhead
}

// LogBatchItem implementation
LogBatchItem::LogBatchItem(LogLevel level, const std::string& service, const std::string& message, 
                           const Properties& data, ContextId context_id)
    : BatchItem(BatchItemType::LOG), level_(level), service_(service), context_id_(context_id) {
    
    // Create payload with message and data
    Properties combined_props = data;
    combined_props["message"] = message;
    payload_ = BatchUtils::serialize_properties(combined_props);
    set_estimated_size(calculate_size());
}

LogBatchItem::LogBatchItem(LogLevel level, const std::string& service, std::vector<uint8_t> payload,
                           ContextId context_id)
    : BatchItem(BatchItemType::LOG), level_(level), service_(service), 
      context_id_(context_id), payload_(std::move(payload)) {
    set_estimated_size(calculate_size());
}

std::vector<uint8_t> LogBatchItem::serialize() const {
    // Temporary simple serialization - will be replaced with FlatBuffers
    std::ostringstream oss;
    oss << "LOG:" << static_cast<int>(level_) << ":" << service_ << ":" << get_timestamp() << ":" << context_id_;
    std::string serialized_str = oss.str();
    return std::vector<uint8_t>(serialized_str.begin(), serialized_str.end());
}

size_t LogBatchItem::calculate_size() const {
    return sizeof(uint8_t) +      // event_type
           sizeof(uint64_t) +     // context_id
           sizeof(uint8_t) +      // level
           sizeof(uint64_t) +     // timestamp
           32 +                   // source hostname (estimated)
           service_.size() +      // service name
           sizeof(uint32_t) +     // payload length
           payload_.size() +      // payload data
           64;                    // FlatBuffers overhead
}

// Batch template implementation
template<typename ItemType>
Batch<ItemType>::Batch(BatchId batch_id) 
    : batch_id_(batch_id), created_time_(std::chrono::system_clock::now()) {}

template<typename ItemType>
bool Batch<ItemType>::add_item(std::unique_ptr<ItemType> item) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ != BatchState::COLLECTING) {
        return false;
    }
    
    total_size_ += item->get_estimated_size();
    items_.push_back(std::move(item));
    return true;
}

template<typename ItemType>
bool Batch<ItemType>::is_full(size_t max_items, size_t max_bytes) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return items_.size() >= max_items || total_size_ >= max_bytes;
}

template<typename ItemType>
bool Batch<ItemType>::is_ready(std::chrono::milliseconds max_age) const {
    return get_age() >= max_age || state_ == BatchState::READY;
}

template<typename ItemType>
bool Batch<ItemType>::is_empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return items_.empty();
}

template<typename ItemType>
std::chrono::milliseconds Batch<ItemType>::get_age() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - created_time_);
}

template<typename ItemType>
void Batch<ItemType>::set_state(BatchState state) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = state;
}

template<typename ItemType>
std::vector<uint8_t> Batch<ItemType>::serialize(const ApiKey& api_key, SchemaType schema_type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Temporary simple serialization - will be replaced with FlatBuffers
    std::ostringstream oss;
    oss << "BATCH:" << batch_id_ << ":" << static_cast<int>(schema_type) << ":" << api_key << ":";
    
    // Serialize items
    for (const auto& item : items_) {
        auto item_data = item->serialize();
        std::string item_str(item_data.begin(), item_data.end());
        oss << item_str << "|";
    }
    
    std::string batch_str = oss.str();
    return std::vector<uint8_t>(batch_str.begin(), batch_str.end());
}

template<typename ItemType>
std::vector<std::unique_ptr<ItemType>> Batch<ItemType>::take_items() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::unique_ptr<ItemType>> taken_items;
    taken_items.reserve(items_.size());
    
    for (auto& item : items_) {
        taken_items.push_back(std::move(item));
    }
    
    items_.clear();
    total_size_ = 0;
    state_ = BatchState::COMPLETED;
    
    return taken_items;
}

// Explicit template instantiations
template class Batch<EventBatchItem>;
template class Batch<LogBatchItem>;

// BatchStats implementation
void BatchStats::reset() {
    batches_created = 0;
    batches_sent = 0;
    batches_failed = 0;
    items_batched = 0;
    bytes_batched = 0;
    flush_operations = 0;
    total_batch_time_ms = 0;
    total_serialization_time_ms = 0;
}

double BatchStats::get_success_rate() const {
    uint64_t total = batches_sent.load() + batches_failed.load();
    return total > 0 ? static_cast<double>(batches_sent.load()) / total : 1.0;
}

double BatchStats::get_average_batch_size() const {
    uint64_t batches = batches_sent.load();
    return batches > 0 ? static_cast<double>(items_batched.load()) / batches : 0.0;
}

double BatchStats::get_average_batch_time_ms() const {
    uint64_t batches = batches_sent.load();
    return batches > 0 ? static_cast<double>(total_batch_time_ms.load()) / batches : 0.0;
}

// BatchProcessor implementation
class BatchProcessor::Impl {
public:
    Impl(BatchSendCallback send_callback, BatchErrorCallback error_callback)
        : send_callback_(std::move(send_callback))
        , error_callback_(std::move(error_callback))
        , running_(false) {}

    ~Impl() {
        stop();
    }

    void process_event_batch(std::unique_ptr<EventBatch> batch, const ApiKey& api_key) {
        if (!running_) {
            error_callback_(Errors::client_shutdown());
            return;
        }

        stats_.batches_created++;
        
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            batch->mark_as_processing();
            
            auto serialized = batch->serialize(api_key, SchemaType::EVENT);
            
            auto serialization_time = std::chrono::steady_clock::now();
            auto serialization_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                serialization_time - start_time);
            stats_.total_serialization_time_ms += serialization_duration.count();
            
            send_callback_(serialized);
            
            batch->mark_as_completed();
            stats_.batches_sent++;
            stats_.items_batched += batch->get_item_count();
            stats_.bytes_batched += serialized.size();
            
            auto end_time = std::chrono::steady_clock::now();
            auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time);
            stats_.total_batch_time_ms += total_duration.count();
            
        } catch (const Error& e) {
            batch->mark_as_failed();
            stats_.batches_failed++;
            error_callback_(e);
        }
    }
    
    void process_log_batch(std::unique_ptr<LogBatch> batch, const ApiKey& api_key) {
        if (!running_) {
            error_callback_(Errors::client_shutdown());
            return;
        }

        stats_.batches_created++;
        
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            batch->mark_as_processing();
            
            auto serialized = batch->serialize(api_key, SchemaType::LOG);
            
            auto serialization_time = std::chrono::steady_clock::now();
            auto serialization_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                serialization_time - start_time);
            stats_.total_serialization_time_ms += serialization_duration.count();
            
            send_callback_(serialized);
            
            batch->mark_as_completed();
            stats_.batches_sent++;
            stats_.items_batched += batch->get_item_count();
            stats_.bytes_batched += serialized.size();
            
            auto end_time = std::chrono::steady_clock::now();
            auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time);
            stats_.total_batch_time_ms += total_duration.count();
            
        } catch (const Error& e) {
            batch->mark_as_failed();
            stats_.batches_failed++;
            error_callback_(e);
        }
    }

    void start() {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = true;
    }

    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }

    bool is_running() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }

    BatchStats::Snapshot get_stats() const { return stats_.snapshot(); }
    void reset_stats() { stats_.reset(); }

private:
    BatchSendCallback send_callback_;
    BatchErrorCallback error_callback_;
    BatchStats stats_;
    bool running_;
    mutable std::mutex mutex_;
};

BatchProcessor::BatchProcessor(BatchSendCallback send_callback, BatchErrorCallback error_callback)
    : pimpl_(std::make_unique<Impl>(std::move(send_callback), std::move(error_callback))) {}

BatchProcessor::~BatchProcessor() = default;

void BatchProcessor::process_event_batch(std::unique_ptr<EventBatch> batch, const ApiKey& api_key) {
    pimpl_->process_event_batch(std::move(batch), api_key);
}

void BatchProcessor::process_log_batch(std::unique_ptr<LogBatch> batch, const ApiKey& api_key) {
    pimpl_->process_log_batch(std::move(batch), api_key);
}

void BatchProcessor::process_event_batch_sync(std::unique_ptr<EventBatch> batch, const ApiKey& api_key) {
    pimpl_->process_event_batch(std::move(batch), api_key);
}

void BatchProcessor::process_log_batch_sync(std::unique_ptr<LogBatch> batch, const ApiKey& api_key) {
    pimpl_->process_log_batch(std::move(batch), api_key);
}

void BatchProcessor::start() { pimpl_->start(); }
void BatchProcessor::stop() { pimpl_->stop(); }
void BatchProcessor::flush() { /* No-op for now */ }
bool BatchProcessor::is_running() const { return pimpl_->is_running(); }
BatchStats::Snapshot BatchProcessor::get_stats() const { return pimpl_->get_stats(); }
void BatchProcessor::reset_stats() { pimpl_->reset_stats(); }

// BatchManager implementation
class BatchManager::Impl {
public:
    Impl(const BatchConfig& config, const ApiKey& api_key)
        : config_(config), api_key_(api_key), running_(false),
          event_batch_(std::make_unique<EventBatch>()),
          log_batch_(std::make_unique<LogBatch>()) {}

    ~Impl() {
        stop();
    }

    void start() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) return;
        
        running_ = true;
        
        // Start flush timer thread
        flush_thread_ = std::thread([this]() {
            flush_timer_thread();
        });
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) return;
            running_ = false;
        }
        
        condition_.notify_all();
        
        if (flush_thread_.joinable()) {
            flush_thread_.join();
        }
        
        // Final flush
        flush_internal();
    }

    void flush() {
        flush_internal();
    }

    bool submit_event(std::unique_ptr<EventBatchItem> event) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!running_) return false;
        
        if (event_queue_.size() >= config_.max_queue_size) {
            return false; // Queue full
        }
        
        // Check if current batch is full
        if (event_batch_->is_full(config_.event_batch_size, config_.max_batch_size_bytes)) {
            flush_event_batch();
        }
        
        if (!event_batch_->add_item(std::move(event))) {
            // Create new batch if current one is not accepting items
            event_batch_ = std::make_unique<EventBatch>();
            return event_batch_->add_item(std::move(event));
        }
        
        return true;
    }

    bool submit_log(std::unique_ptr<LogBatchItem> log) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!running_) return false;
        
        if (log_queue_.size() >= config_.max_queue_size) {
            return false; // Queue full
        }
        
        // Check if current batch is full
        if (log_batch_->is_full(config_.log_batch_size, config_.max_batch_size_bytes)) {
            flush_log_batch();
        }
        
        if (!log_batch_->add_item(std::move(log))) {
            // Create new batch if current one is not accepting items
            log_batch_ = std::make_unique<LogBatch>();
            return log_batch_->add_item(std::move(log));
        }
        
        return true;
    }

    std::unique_ptr<EventBatchItem> create_event_item(EventType type, const std::string& user_id, const Properties& properties) {
        return std::make_unique<EventBatchItem>(type, user_id, properties);
    }

    std::unique_ptr<LogBatchItem> create_log_item(LogLevel level, const std::string& service, const std::string& message, const Properties& data) {
        return std::make_unique<LogBatchItem>(level, service, message, data);
    }

    void set_send_callback(std::function<void(const std::vector<uint8_t>&)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        send_callback_ = std::move(callback);
    }

    void set_error_callback(std::function<void(const Error&)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        error_callback_ = std::move(callback);
    }

    size_t get_event_queue_size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return event_queue_.size();
    }

    size_t get_log_queue_size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return log_queue_.size();
    }

    bool is_running() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }

    const BatchConfig& get_config() const { return config_; }
    
    BatchStats::Snapshot get_event_stats() const { return event_stats_.snapshot(); }
    BatchStats::Snapshot get_log_stats() const { return log_stats_.snapshot(); }
    
    void reset_stats() {
        event_stats_.reset();
        log_stats_.reset();
    }

private:
    void flush_timer_thread() {
        auto last_event_flush = std::chrono::steady_clock::now();
        auto last_log_flush = std::chrono::steady_clock::now();
        
        while (running_) {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait_for(lock, std::chrono::milliseconds(100));
            
            if (!running_) break;
            
            auto now = std::chrono::steady_clock::now();
            
            // Check event batch flush interval
            if (now - last_event_flush >= config_.event_flush_interval) {
                if (!event_batch_->is_empty()) {
                    flush_event_batch();
                    last_event_flush = now;
                }
            }
            
            // Check log batch flush interval
            if (now - last_log_flush >= config_.log_flush_interval) {
                if (!log_batch_->is_empty()) {
                    flush_log_batch();
                    last_log_flush = now;
                }
            }
        }
    }

    void flush_internal() {
        std::lock_guard<std::mutex> lock(mutex_);
        flush_event_batch();
        flush_log_batch();
    }

    void flush_event_batch() {
        if (event_batch_->is_empty()) return;
        
        event_batch_->set_state(BatchState::READY);
        event_queue_.push(std::move(event_batch_));
        event_batch_ = std::make_unique<EventBatch>();
        
        // Process queue
        process_event_queue();
    }

    void flush_log_batch() {
        if (log_batch_->is_empty()) return;
        
        log_batch_->set_state(BatchState::READY);
        log_queue_.push(std::move(log_batch_));
        log_batch_ = std::make_unique<LogBatch>();
        
        // Process queue
        process_log_queue();
    }

    void process_event_queue() {
        while (!event_queue_.empty()) {
            auto batch = std::move(event_queue_.front());
            event_queue_.pop();
            
            try {
                auto serialized = batch->serialize(api_key_, SchemaType::EVENT);
                
                if (send_callback_) {
                    send_callback_(serialized);
                }
                
                event_stats_.batches_sent++;
                event_stats_.items_batched += batch->get_item_count();
                event_stats_.bytes_batched += serialized.size();
                
            } catch (const Error& e) {
                event_stats_.batches_failed++;
                if (error_callback_) {
                    error_callback_(e);
                }
            }
        }
    }

    void process_log_queue() {
        while (!log_queue_.empty()) {
            auto batch = std::move(log_queue_.front());
            log_queue_.pop();
            
            try {
                auto serialized = batch->serialize(api_key_, SchemaType::LOG);
                
                if (send_callback_) {
                    send_callback_(serialized);
                }
                
                log_stats_.batches_sent++;
                log_stats_.items_batched += batch->get_item_count();
                log_stats_.bytes_batched += serialized.size();
                
            } catch (const Error& e) {
                log_stats_.batches_failed++;
                if (error_callback_) {
                    error_callback_(e);
                }
            }
        }
    }

private:
    BatchConfig config_;
    ApiKey api_key_;
    bool running_;
    
    std::unique_ptr<EventBatch> event_batch_;
    std::unique_ptr<LogBatch> log_batch_;
    std::queue<std::unique_ptr<EventBatch>> event_queue_;
    std::queue<std::unique_ptr<LogBatch>> log_queue_;
    
    BatchStats event_stats_;
    BatchStats log_stats_;
    
    std::function<void(const std::vector<uint8_t>&)> send_callback_;
    std::function<void(const Error&)> error_callback_;
    
    std::thread flush_thread_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
};

BatchManager::BatchManager(const BatchConfig& config, const ApiKey& api_key)
    : pimpl_(std::make_unique<Impl>(config, api_key)) {}

BatchManager::~BatchManager() = default;
BatchManager::BatchManager(BatchManager&&) noexcept = default;
BatchManager& BatchManager::operator=(BatchManager&&) noexcept = default;

void BatchManager::start() { pimpl_->start(); }
void BatchManager::stop() { pimpl_->stop(); }
void BatchManager::flush() { pimpl_->flush(); }
bool BatchManager::is_running() const { return pimpl_->is_running(); }

bool BatchManager::submit_event(std::unique_ptr<EventBatchItem> event) {
    return pimpl_->submit_event(std::move(event));
}

bool BatchManager::submit_log(std::unique_ptr<LogBatchItem> log) {
    return pimpl_->submit_log(std::move(log));
}

std::unique_ptr<EventBatchItem> BatchManager::create_event_item(EventType type, const std::string& user_id, const Properties& properties) {
    return pimpl_->create_event_item(type, user_id, properties);
}

std::unique_ptr<LogBatchItem> BatchManager::create_log_item(LogLevel level, const std::string& service, const std::string& message, const Properties& data) {
    return pimpl_->create_log_item(level, service, message, data);
}

size_t BatchManager::get_event_queue_size() const { return pimpl_->get_event_queue_size(); }
size_t BatchManager::get_log_queue_size() const { return pimpl_->get_log_queue_size(); }

bool BatchManager::is_event_queue_full() const {
    return get_event_queue_size() >= pimpl_->get_config().max_queue_size;
}

bool BatchManager::is_log_queue_full() const {
    return get_log_queue_size() >= pimpl_->get_config().max_queue_size;
}

void BatchManager::set_send_callback(std::function<void(const std::vector<uint8_t>&)> callback) {
    pimpl_->set_send_callback(std::move(callback));
}

void BatchManager::set_error_callback(std::function<void(const Error&)> callback) {
    pimpl_->set_error_callback(std::move(callback));
}

const BatchConfig& BatchManager::get_config() const { return pimpl_->get_config(); }

BatchStats::Snapshot BatchManager::get_event_stats() const { return pimpl_->get_event_stats(); }
BatchStats::Snapshot BatchManager::get_log_stats() const { return pimpl_->get_log_stats(); }

BatchStats::Snapshot BatchManager::get_combined_stats() const {
    auto event_stats = get_event_stats();
    auto log_stats = get_log_stats();
    
    BatchStats::Snapshot combined;
    combined.batches_created = event_stats.batches_created + log_stats.batches_created;
    combined.batches_sent = event_stats.batches_sent + log_stats.batches_sent;
    combined.batches_failed = event_stats.batches_failed + log_stats.batches_failed;
    combined.items_batched = event_stats.items_batched + log_stats.items_batched;
    combined.bytes_batched = event_stats.bytes_batched + log_stats.bytes_batched;
    combined.flush_operations = event_stats.flush_operations + log_stats.flush_operations;
    combined.total_batch_time_ms = event_stats.total_batch_time_ms + log_stats.total_batch_time_ms;
    combined.total_serialization_time_ms = event_stats.total_serialization_time_ms + log_stats.total_serialization_time_ms;
    
    return combined;
}

void BatchManager::reset_stats() {
    pimpl_->reset_stats();
}

// BatchUtils implementation
namespace BatchUtils {

std::vector<uint8_t> serialize_properties(const Properties& properties) {
#ifdef NLOHMANN_JSON_FOUND
    nlohmann::json json_obj;
    
    for (const auto& [key, value] : properties) {
        std::visit([&json_obj, &key](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>) {
                json_obj[key] = v;
            } else if constexpr (std::is_same_v<T, int64_t>) {
                json_obj[key] = v;
            } else if constexpr (std::is_same_v<T, double>) {
                json_obj[key] = v;
            } else if constexpr (std::is_same_v<T, bool>) {
                json_obj[key] = v;
            } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                json_obj[key] = nullptr;
            }
        }, value);
    }
    
    std::string json_str = json_obj.dump();
    return std::vector<uint8_t>(json_str.begin(), json_str.end());
#else
    // Fallback to simple key=value format if JSON not available
    std::ostringstream oss;
    bool first = true;
    
    for (const auto& [key, value] : properties) {
        if (!first) oss << "&";
        first = false;
        
        oss << key << "=";
        std::visit([&oss](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>) {
                oss << v;
            } else if constexpr (std::is_same_v<T, int64_t>) {
                oss << v;
            } else if constexpr (std::is_same_v<T, double>) {
                oss << v;
            } else if constexpr (std::is_same_v<T, bool>) {
                oss << (v ? "true" : "false");
            } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
                oss << "null";
            }
        }, value);
    }
    
    std::string str = oss.str();
    return std::vector<uint8_t>(str.begin(), str.end());
#endif
}

Properties deserialize_properties(const std::vector<uint8_t>& data) {
    Properties properties;
    
#ifdef NLOHMANN_JSON_FOUND
    try {
        std::string json_str(data.begin(), data.end());
        auto json_obj = nlohmann::json::parse(json_str);
        
        for (auto& [key, value] : json_obj.items()) {
            if (value.is_string()) {
                properties[key] = value.get<std::string>();
            } else if (value.is_number_integer()) {
                properties[key] = value.get<int64_t>();
            } else if (value.is_number_float()) {
                properties[key] = value.get<double>();
            } else if (value.is_boolean()) {
                properties[key] = value.get<bool>();
            } else if (value.is_null()) {
                properties[key] = nullptr;
            }
        }
    } catch (const std::exception&) {
        // Fallback to simple parsing
    }
#else
    // Simple key=value parsing
    std::string str(data.begin(), data.end());
    auto pairs = Utils::split(str, '&');
    
    for (const auto& pair : pairs) {
        auto kv = Utils::split(pair, '=');
        if (kv.size() == 2) {
            const std::string& key = kv[0];
            const std::string& value = kv[1];
            
            // Try to infer type
            if (value == "true") {
                properties[key] = true;
            } else if (value == "false") {
                properties[key] = false;
            } else if (value == "null") {
                properties[key] = nullptr;
            } else {
                // Try to parse as number
                try {
                    if (value.find('.') != std::string::npos) {
                        properties[key] = std::stod(value);
                    } else {
                        properties[key] = std::stoll(value);
                    }
                } catch (const std::exception&) {
                    properties[key] = value; // Default to string
                }
            }
        }
    }
#endif
    
    return properties;
}

size_t estimate_event_size(const std::string& /*user_id*/, const Properties& properties) {
    size_t base_size = sizeof(uint64_t) + sizeof(uint8_t) + 16 + 32; // timestamp + type + user_id + overhead
    size_t payload_size = serialize_properties(properties).size();
    return base_size + payload_size;
}

size_t estimate_log_size(const std::string& service, const std::string& message, const Properties& data) {
    size_t base_size = sizeof(uint8_t) + sizeof(uint64_t) + sizeof(uint8_t) + sizeof(uint64_t) + 64; // Basic fields + overhead
    size_t service_size = service.size();
    
    Properties combined_props = data;
    combined_props["message"] = message;
    size_t payload_size = serialize_properties(combined_props).size();
    
    return base_size + service_size + payload_size;
}

bool should_flush_batch(size_t current_items, size_t max_items, 
                       size_t current_bytes, size_t max_bytes,
                       std::chrono::milliseconds age, std::chrono::milliseconds max_age) {
    return current_items >= max_items || current_bytes >= max_bytes || age >= max_age;
}

std::vector<uint8_t> compress_batch(const std::vector<uint8_t>& data) {
    // Simple compression placeholder - would need actual compression library
    // For now, just return the original data
    return data;
}

std::vector<uint8_t> decompress_batch(const std::vector<uint8_t>& compressed_data) {
    // Simple decompression placeholder - would need actual compression library
    // For now, just return the original data
    return compressed_data;
}

bool validate_event_item(const EventBatchItem& item) {
    if (item.get_user_id().empty()) {
        return false;
    }
    
    if (item.get_payload().empty()) {
        return false;
    }
    
    if (item.get_estimated_size() > 1024 * 1024) { // 1MB limit
        return false;
    }
    
    return true;
}

bool validate_log_item(const LogBatchItem& item) {
    if (item.get_service().empty()) {
        return false;
    }
    
    if (item.get_payload().empty()) {
        return false;
    }
    
    if (item.get_estimated_size() > 1024 * 1024) { // 1MB limit
        return false;
    }
    
    return true;
}

bool validate_batch_size(size_t items, size_t bytes, const BatchConfig& config) {
    return items <= config.event_batch_size && bytes <= config.max_batch_size_bytes;
}

} // namespace BatchUtils

// ScopedBatchFlusher implementation
ScopedBatchFlusher::ScopedBatchFlusher(BatchManager& manager) : manager_(manager) {}

ScopedBatchFlusher::~ScopedBatchFlusher() {
    if (!flushed_) {
        flush_now();
    }
}

void ScopedBatchFlusher::flush_now() {
    if (!flushed_) {
        manager_.flush();
        flushed_ = true;
    }
}

} // namespace usercanal