// src/types.cpp
// Implementation of utility functions for types and constants

#include "usercanal/types.hpp"
#include <random>
#include <regex>
#include <chrono>
#include <algorithm>
#include <cctype>

namespace usercanal {
namespace Utils {

std::string currency_to_string(Currency currency) {
    switch (currency) {
        case Currency::USD: return "USD";
        case Currency::EUR: return "EUR";
        case Currency::GBP: return "GBP";
        case Currency::JPY: return "JPY";
        case Currency::CAD: return "CAD";
        case Currency::AUD: return "AUD";
        case Currency::CHF: return "CHF";
        case Currency::CNY: return "CNY";
        case Currency::SEK: return "SEK";
        case Currency::NZD: return "NZD";
        case Currency::MXN: return "MXN";
        case Currency::SGD: return "SGD";
        case Currency::HKD: return "HKD";
        case Currency::NOK: return "NOK";
        case Currency::TRY: return "TRY";
        case Currency::ZAR: return "ZAR";
        case Currency::BRL: return "BRL";
        case Currency::INR: return "INR";
        case Currency::KRW: return "KRW";
        case Currency::PLN: return "PLN";
        case Currency::CZK: return "CZK";
        case Currency::DKK: return "DKK";
        case Currency::HUF: return "HUF";
        case Currency::ILS: return "ILS";
        case Currency::CLP: return "CLP";
        case Currency::PHP: return "PHP";
        case Currency::AED: return "AED";
        case Currency::THB: return "THB";
        case Currency::MYR: return "MYR";
        case Currency::IDR: return "IDR";
        default: return "USD";
    }
}

std::string log_level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::EMERGENCY: return "EMERGENCY";
        case LogLevel::ALERT: return "ALERT";
        case LogLevel::CRITICAL: return "CRITICAL";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::NOTICE: return "NOTICE";
        case LogLevel::INFO: return "INFO";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::TRACE: return "TRACE";
        default: return "INFO";
    }
}

std::string event_type_to_string(EventType type) {
    switch (type) {
        case EventType::UNKNOWN: return "UNKNOWN";
        case EventType::TRACK: return "TRACK";
        case EventType::IDENTIFY: return "IDENTIFY";
        case EventType::GROUP: return "GROUP";
        case EventType::ALIAS: return "ALIAS";
        case EventType::ENRICH: return "ENRICH";
        default: return "UNKNOWN";
    }
}

Currency string_to_currency(const std::string& currency_str) {
    std::string upper_currency = currency_str;
    std::transform(upper_currency.begin(), upper_currency.end(), upper_currency.begin(), ::toupper);
    
    if (upper_currency == "USD") return Currency::USD;
    if (upper_currency == "EUR") return Currency::EUR;
    if (upper_currency == "GBP") return Currency::GBP;
    if (upper_currency == "JPY") return Currency::JPY;
    if (upper_currency == "CAD") return Currency::CAD;
    if (upper_currency == "AUD") return Currency::AUD;
    if (upper_currency == "CHF") return Currency::CHF;
    if (upper_currency == "CNY") return Currency::CNY;
    if (upper_currency == "SEK") return Currency::SEK;
    if (upper_currency == "NZD") return Currency::NZD;
    if (upper_currency == "MXN") return Currency::MXN;
    if (upper_currency == "SGD") return Currency::SGD;
    if (upper_currency == "HKD") return Currency::HKD;
    if (upper_currency == "NOK") return Currency::NOK;
    if (upper_currency == "TRY") return Currency::TRY;
    if (upper_currency == "ZAR") return Currency::ZAR;
    if (upper_currency == "BRL") return Currency::BRL;
    if (upper_currency == "INR") return Currency::INR;
    if (upper_currency == "KRW") return Currency::KRW;
    if (upper_currency == "PLN") return Currency::PLN;
    if (upper_currency == "CZK") return Currency::CZK;
    if (upper_currency == "DKK") return Currency::DKK;
    if (upper_currency == "HUF") return Currency::HUF;
    if (upper_currency == "ILS") return Currency::ILS;
    if (upper_currency == "CLP") return Currency::CLP;
    if (upper_currency == "PHP") return Currency::PHP;
    if (upper_currency == "AED") return Currency::AED;
    if (upper_currency == "THB") return Currency::THB;
    if (upper_currency == "MYR") return Currency::MYR;
    if (upper_currency == "IDR") return Currency::IDR;
    
    return Currency::USD; // Default fallback
}

bool is_valid_api_key(const std::string& api_key) {
    // API key should be 32 hexadecimal characters
    if (api_key.length() != 32) {
        return false;
    }
    
    // Check if all characters are valid hex
    static const std::regex hex_regex("^[a-fA-F0-9]{32}$");
    return std::regex_match(api_key, hex_regex);
}

bool is_valid_user_id(const std::string& user_id) {
    // User ID should be non-empty and not exceed reasonable length
    if (user_id.empty() || user_id.length() > 255) {
        return false;
    }
    
    // Should not contain only whitespace
    bool has_non_whitespace = false;
    for (char c : user_id) {
        if (!std::isspace(c)) {
            has_non_whitespace = true;
            break;
        }
    }
    
    return has_non_whitespace;
}

BatchId generate_batch_id() {
    static thread_local std::random_device rd;
    static thread_local std::mt19937_64 gen(rd());
    static thread_local std::uniform_int_distribution<BatchId> dis;
    
    return dis(gen);
}

ContextId generate_context_id() {
    static thread_local std::random_device rd;
    static thread_local std::mt19937_64 gen(rd());
    static thread_local std::uniform_int_distribution<ContextId> dis;
    
    return dis(gen);
}

} // namespace Utils
} // namespace usercanal