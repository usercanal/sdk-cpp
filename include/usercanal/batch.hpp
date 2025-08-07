// include/usercanal/batch.hpp
// Purpose: Batch management system for efficient data grouping and transmission
// Handles separate batching for events and logs with performance optimization

#pragma once

#include "types.hpp"
#include "config.hpp"
#include "errors.hpp"
#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <thread>
#include <functional>
#include <future>

namespace usercanal {

// Forward declarations
class BatchManager;
class BatchProcessor;

// Batch item types
enum class BatchItemType : uint8_t {
    EVENT = 1,
    LOG = 2
};

// Batch state enumeration
enum class BatchState : uint8_t {
    COLLECTING = 0,  // Accepting new items
    READY = 1,       // Ready for transmission
    PROCESSING = 2,  // Being serialized/transmitted
    COMPLETED = 3,   // Successfully transmitted
    FAILED = 4,      // Transmission failed
    EXPIRED = 5      // Timed out waiting for transmission
};

// Base batch item interface
class BatchItem {
public:
    BatchItem(BatchItemType type, Timestamp timestamp = Utils::now_milliseconds());
    virtual ~BatchItem() = default;
    
    BatchItemType get_type() const { return type_; }
    Timestamp get_timestamp() const { return timestamp_; }
    size_t get_estimated_size() const { return estimated_size_; }
    
    virtual std::vector<uint8_t> serialize() const = 0;
    virtual size_t calculate_size() const = 0;

protected:
    void set_estimated_size(size_t size) { estimated_size_ = size; }

private:
    BatchItemType type_;
    Timestamp timestamp_;
    size_t estimated_size_ = 0;
};

// Event batch item
class EventBatchItem : public BatchItem {
public:
    EventBatchItem(EventType event_type, const std::string& user_id, const Properties& properties);
    EventBatchItem(EventType event_type, const std::string& user_id, std::vector<uint8_t> payload);
    
    EventType get_event_type() const { return event_type_; }
    const std::string& get_user_id() const { return user_id_; }
    const std::vector<uint8_t>& get_payload() const { return payload_; }
    
    std::vector<uint8_t> serialize() const override;
    size_t calculate_size() const override;

private:
    EventType event_type_;
    std::string user_id_;
    std::vector<uint8_t> payload_;
};

// Log batch item
class LogBatchItem : public BatchItem {
public:
    LogBatchItem(LogLevel level, const std::string& service, const std::string& message, 
                 const Properties& data = {}, ContextId context_id = 0);
    LogBatchItem(LogLevel level, const std::string& service, std::vector<uint8_t> payload,
                 ContextId context_id = 0);
    
    LogLevel get_level() const { return level_; }
    const std::string& get_service() const { return service_; }
    ContextId get_context_id() const { return context_id_; }
    const std::vector<uint8_t>& get_payload() const { return payload_; }
    
    std::vector<uint8_t> serialize() const override;
    size_t calculate_size() const override;

private:
    LogLevel level_;
    std::string service_;
    ContextId context_id_;
    std::vector<uint8_t> payload_;
};

// Generic batch container
template<typename ItemType>
class Batch {
public:
    explicit Batch(BatchId batch_id = Utils::generate_batch_id());
    ~Batch() = default;
    
    // Non-copyable, movable
    Batch(const Batch&) = delete;
    Batch& operator=(const Batch&) = delete;
    Batch(Batch&&) noexcept = default;
    Batch& operator=(Batch&&) noexcept = default;
    
    // Batch management
    bool add_item(std::unique_ptr<ItemType> item);
    bool is_full(size_t max_items, size_t max_bytes) const;
    bool is_ready(std::chrono::milliseconds max_age) const;
    bool is_empty() const;
    
    // Batch info
    BatchId get_id() const { return batch_id_; }
    size_t get_item_count() const { return items_.size(); }
    size_t get_total_size() const { return total_size_; }
    BatchState get_state() const { return state_; }
    std::chrono::system_clock::time_point get_created_time() const { return created_time_; }
    std::chrono::milliseconds get_age() const;
    
    // State management
    void set_state(BatchState state);
    void mark_as_processing() { set_state(BatchState::PROCESSING); }
    void mark_as_completed() { set_state(BatchState::COMPLETED); }
    void mark_as_failed() { set_state(BatchState::FAILED); }
    
    // Serialization
    std::vector<uint8_t> serialize(const ApiKey& api_key, SchemaType schema_type) const;
    
    // Access to items (for processing)
    const std::vector<std::unique_ptr<ItemType>>& get_items() const { return items_; }
    std::vector<std::unique_ptr<ItemType>> take_items();

private:
    BatchId batch_id_;
    std::vector<std::unique_ptr<ItemType>> items_;
    size_t total_size_ = 0;
    BatchState state_ = BatchState::COLLECTING;
    std::chrono::system_clock::time_point created_time_;
    mutable std::mutex mutex_;
};

// Specialized batch types
using EventBatch = Batch<EventBatchItem>;
using LogBatch = Batch<LogBatchItem>;

// Batch statistics
struct BatchStats {
    std::atomic<uint64_t> batches_created{0};
    std::atomic<uint64_t> batches_sent{0};
    std::atomic<uint64_t> batches_failed{0};
    std::atomic<uint64_t> items_batched{0};
    std::atomic<uint64_t> bytes_batched{0};
    std::atomic<uint64_t> flush_operations{0};
    
    // Timing statistics
    std::atomic<uint64_t> total_batch_time_ms{0};
    std::atomic<uint64_t> total_serialization_time_ms{0};
    
    void reset();
    double get_success_rate() const;
    double get_average_batch_size() const;
    double get_average_batch_time_ms() const;
    
    // Create a copyable snapshot of the stats
    struct Snapshot {
        uint64_t batches_created = 0;
        uint64_t batches_sent = 0;
        uint64_t batches_failed = 0;
        uint64_t items_batched = 0;
        uint64_t bytes_batched = 0;
        uint64_t flush_operations = 0;
        uint64_t total_batch_time_ms = 0;
        uint64_t total_serialization_time_ms = 0;
        
        double get_success_rate() const {
            uint64_t total = batches_sent + batches_failed;
            return total > 0 ? static_cast<double>(batches_sent) / total : 1.0;
        }
        
        double get_average_batch_size() const {
            return batches_sent > 0 ? static_cast<double>(items_batched) / batches_sent : 0.0;
        }
        
        double get_average_batch_time_ms() const {
            return batches_sent > 0 ? static_cast<double>(total_batch_time_ms) / batches_sent : 0.0;
        }
    };
    
    Snapshot snapshot() const {
        Snapshot s;
        s.batches_created = batches_created.load();
        s.batches_sent = batches_sent.load();
        s.batches_failed = batches_failed.load();
        s.items_batched = items_batched.load();
        s.bytes_batched = bytes_batched.load();
        s.flush_operations = flush_operations.load();
        s.total_batch_time_ms = total_batch_time_ms.load();
        s.total_serialization_time_ms = total_serialization_time_ms.load();
        return s;
    }
};

// Batch processor for handling transmission
class BatchProcessor {
public:
    using BatchSendCallback = std::function<void(const std::vector<uint8_t>& data)>;
    using BatchErrorCallback = std::function<void(const Error& error)>;
    
    BatchProcessor(BatchSendCallback send_callback, BatchErrorCallback error_callback);
    ~BatchProcessor();
    
    // Process batches asynchronously
    void process_event_batch(std::unique_ptr<EventBatch> batch, const ApiKey& api_key);
    void process_log_batch(std::unique_ptr<LogBatch> batch, const ApiKey& api_key);
    
    // Synchronous processing
    void process_event_batch_sync(std::unique_ptr<EventBatch> batch, const ApiKey& api_key);
    void process_log_batch_sync(std::unique_ptr<LogBatch> batch, const ApiKey& api_key);
    
    // Processor control
    void start();
    void stop();
    void flush();
    bool is_running() const;
    
    // Statistics
    BatchStats::Snapshot get_stats() const;
    void reset_stats();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

// Main batch manager coordinating event and log batching
class BatchManager {
public:
    explicit BatchManager(const BatchConfig& config, const ApiKey& api_key);
    ~BatchManager();
    
    // Non-copyable, movable
    BatchManager(const BatchManager&) = delete;
    BatchManager& operator=(const BatchManager&) = delete;
    BatchManager(BatchManager&&) noexcept;
    BatchManager& operator=(BatchManager&&) noexcept;
    
    // Lifecycle
    void start();
    void stop();
    void flush();
    bool is_running() const;
    
    // Item submission
    bool submit_event(std::unique_ptr<EventBatchItem> event);
    bool submit_log(std::unique_ptr<LogBatchItem> log);
    
    // Batch creation helpers
    std::unique_ptr<EventBatchItem> create_event_item(EventType type, const std::string& user_id, const Properties& properties);
    std::unique_ptr<LogBatchItem> create_log_item(LogLevel level, const std::string& service, const std::string& message, const Properties& data = {});
    
    // Queue management
    size_t get_event_queue_size() const;
    size_t get_log_queue_size() const;
    bool is_event_queue_full() const;
    bool is_log_queue_full() const;
    
    // Callbacks for network transmission
    void set_send_callback(std::function<void(const std::vector<uint8_t>&)> callback);
    void set_error_callback(std::function<void(const Error&)> callback);
    
    // Configuration
    const BatchConfig& get_config() const;
    void update_config(const BatchConfig& config);
    
    // Statistics
    BatchStats::Snapshot get_event_stats() const;
    BatchStats::Snapshot get_log_stats() const;
    BatchStats::Snapshot get_combined_stats() const;
    void reset_stats();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

// Batch utilities
namespace BatchUtils {

// Serialization helpers
std::vector<uint8_t> serialize_properties(const Properties& properties);
Properties deserialize_properties(const std::vector<uint8_t>& data);

// Size estimation
size_t estimate_event_size(const std::string& user_id, const Properties& properties);
size_t estimate_log_size(const std::string& service, const std::string& message, const Properties& data);

// Batch optimization
bool should_flush_batch(size_t current_items, size_t max_items, 
                       size_t current_bytes, size_t max_bytes,
                       std::chrono::milliseconds age, std::chrono::milliseconds max_age);

// Compression utilities (if enabled)
std::vector<uint8_t> compress_batch(const std::vector<uint8_t>& data);
std::vector<uint8_t> decompress_batch(const std::vector<uint8_t>& compressed_data);

// Validation
bool validate_event_item(const EventBatchItem& item);
bool validate_log_item(const LogBatchItem& item);
bool validate_batch_size(size_t items, size_t bytes, const BatchConfig& config);

} // namespace BatchUtils

// Batch event listeners for monitoring
class BatchEventListener {
public:
    virtual ~BatchEventListener() = default;
    
    // Batch lifecycle events
    virtual void on_batch_created(BatchId /*batch_id*/, BatchItemType /*type*/) {}
    virtual void on_batch_ready(BatchId /*batch_id*/, size_t /*items*/, size_t /*bytes*/) {}
    virtual void on_batch_sent(BatchId /*batch_id*/, std::chrono::milliseconds /*duration*/) {}
    virtual void on_batch_failed(BatchId /*batch_id*/, const Error& /*error*/) {}
    
    // Item events
    virtual void on_item_added(BatchId /*batch_id*/, BatchItemType /*type*/) {}
    virtual void on_item_dropped(BatchItemType /*type*/, const std::string& /*reason*/) {}
    
    // Queue events
    virtual void on_queue_full(BatchItemType /*type*/, size_t /*queue_size*/) {}
    virtual void on_flush_triggered(const std::string& /*reason*/) {}
};

// RAII batch flusher for automatic cleanup
class ScopedBatchFlusher {
public:
    explicit ScopedBatchFlusher(BatchManager& manager);
    ~ScopedBatchFlusher();
    
    // Manual flush
    void flush_now();
    
private:
    BatchManager& manager_;
    bool flushed_ = false;
};

} // namespace usercanal