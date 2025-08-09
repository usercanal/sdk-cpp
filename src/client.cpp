// src/client.cpp
// Simplified client implementation matching Go SDK scope

#include "usercanal/client.hpp"
#include "usercanal/utils.hpp"
#include "usercanal/batch.hpp"
#include "usercanal/network.hpp"
#include <memory>
#include <thread>
#include <chrono>
#include <iostream>
#include <exception>

namespace usercanal {

// ClientImpl that actually sends data over the network
class ClientImpl {
public:
    explicit ClientImpl(const Config& config) 
        : config_(config), initialized_(false), 
          batch_manager_(config.batch(), config.api_key()),
          network_client_(config) {
        
        // Set up batch manager callbacks
        batch_manager_.set_send_callback([this](const std::vector<uint8_t>& data) {
            try {
                network_client_.send_data(data);
                stats_.bytes_sent += data.size();
            } catch (const std::exception& e) {
                stats_.network_errors++;
                std::cerr << "Network error: " << e.what() << std::endl;
            }
        });
        
        batch_manager_.set_error_callback([this](const Error& error) {
            stats_.network_errors++;
            std::cerr << "Batch error: " << error.what() << std::endl;
        });
    }
    
    void initialize() {
        try {
            network_client_.initialize();
            batch_manager_.start();
            initialized_ = true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize client: " << e.what() << std::endl;
            initialized_ = false;
        }
    }
    
    void shutdown() {
        if (initialized_) {
            try {
                batch_manager_.flush();
                batch_manager_.stop();
                network_client_.shutdown();
            } catch (const std::exception& e) {
                std::cerr << "Error during shutdown: " << e.what() << std::endl;
            }
        }
        initialized_ = false;
    }
    
    bool is_initialized() const {
        return initialized_;
    }
    
    void event(const std::string& user_id, const std::string& event_name, const Properties& properties) {
        if (!initialized_) return;
        
        try {
            // Add event_name to properties
            Properties event_props = properties;
            event_props["event_name"] = event_name;
            
            // Create and submit event batch item
            auto event_item = batch_manager_.create_event_item(EventType::TRACK, user_id, event_props);
            if (batch_manager_.submit_event(std::move(event_item))) {
                stats_.events_sent++;
            } else {
                std::cerr << "Failed to submit event to batch queue" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error submitting event: " << e.what() << std::endl;
        }
    }
    
    void event_revenue(const std::string& user_id, const std::string& order_id, 
                      double amount, const std::string& currency, const Properties& properties) {
        if (!initialized_) return;
        
        try {
            Properties revenue_props = properties;
            revenue_props["order_id"] = order_id;
            revenue_props["amount"] = amount;
            revenue_props["currency"] = currency;
            revenue_props["event_name"] = std::string("revenue");
            
            auto event_item = batch_manager_.create_event_item(EventType::TRACK, user_id, revenue_props);
            if (batch_manager_.submit_event(std::move(event_item))) {
                stats_.events_sent++;
            } else {
                std::cerr << "Failed to submit revenue event to batch queue" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error submitting revenue event: " << e.what() << std::endl;
        }
    }
    
    void log_info(const std::string& service, const std::string& message, const Properties& data) {
        log_message(LogLevel::INFO, service, message, data);
    }
    
    void log_error(const std::string& service, const std::string& message, const Properties& data) {
        log_message(LogLevel::ERROR, service, message, data);
    }
    
    void log_warning(const std::string& service, const std::string& message, const Properties& data) {
        log_message(LogLevel::WARNING, service, message, data);
    }
    
    void log_debug(const std::string& service, const std::string& message, const Properties& data) {
        log_message(LogLevel::DEBUG, service, message, data);
    }
    
    void flush() {
        if (!initialized_) return;
        try {
            batch_manager_.flush();
            stats_.batches_sent++;
        } catch (const std::exception& e) {
            std::cerr << "Error during flush: " << e.what() << std::endl;
        }
    }
    
    Stats get_stats() const {
        return stats_;
    }

private:
    Config config_;
    bool initialized_;
    Stats stats_;
    BatchManager batch_manager_;
    NetworkClient network_client_;
    
    void log_message(LogLevel level, const std::string& service, const std::string& message, const Properties& data) {
        if (!initialized_) return;
        
        try {
            auto log_item = batch_manager_.create_log_item(level, service, message, data);
            if (batch_manager_.submit_log(std::move(log_item))) {
                stats_.logs_sent++;
            } else {
                std::cerr << "Failed to submit log to batch queue" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error submitting log: " << e.what() << std::endl;
        }
    }
};

// Client implementation
Client::Client(const Config& config) : impl_(std::make_unique<ClientImpl>(config)) {}

Client::Client(const std::string& api_key) : impl_(std::make_unique<ClientImpl>(Config(api_key))) {}

Client::~Client() = default;

Client::Client(Client&&) noexcept = default;
Client& Client::operator=(Client&&) noexcept = default;

void Client::initialize() {
    impl_->initialize();
}

void Client::shutdown() {
    impl_->shutdown();
}

bool Client::is_initialized() const {
    return impl_->is_initialized();
}

void Client::event(const std::string& user_id, const std::string& event_name, const Properties& properties) {
    impl_->event(user_id, event_name, properties);
}

void Client::event_revenue(const std::string& user_id, const std::string& order_id, 
                          double amount, const std::string& currency, const Properties& properties) {
    impl_->event_revenue(user_id, order_id, amount, currency, properties);
}

void Client::log_info(const std::string& service, const std::string& message, const Properties& data) {
    impl_->log_info(service, message, data);
}

void Client::log_error(const std::string& service, const std::string& message, const Properties& data) {
    impl_->log_error(service, message, data);
}

void Client::log_warning(const std::string& service, const std::string& message, const Properties& data) {
    impl_->log_warning(service, message, data);
}

void Client::log_debug(const std::string& service, const std::string& message, const Properties& data) {
    impl_->log_debug(service, message, data);
}

void Client::flush() {
    impl_->flush();
}

Stats Client::get_stats() const {
    return impl_->get_stats();
}

} // namespace usercanal