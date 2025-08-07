// event_revenue.cpp
// Revenue Tracking Example
// Shows revenue and subscription tracking with UserCanal C++ SDK

#include <iostream>
#include "usercanal/usercanal.hpp"

using namespace usercanal;

int main() {
    std::cout << "💰 UserCanal C++ SDK - Revenue Tracking\n" << std::endl;
    
    // Replace with your API key
    std::string api_key = "your-api-key-here";
    
    if (api_key == "your-api-key-here") {
        std::cout << "⚠️  Please set your UserCanal API key in the source code" << std::endl;
        std::cout << "   Get your API key from: https://app.usercanal.com/settings/api-keys" << std::endl;
        return 1;
    }
    
    try {
        // Initialize client
        Client client(api_key);
        client.initialize();
        
        std::cout << "✅ UserCanal SDK initialized\n" << std::endl;
        
        // Track one-time purchase
        client.track_revenue("user_123", "order_789", 29.99, Currency::USD, {
            {"product_name", std::string("Premium Plan")},
            {"payment_method", std::string("credit_card")},
            {"discount_applied", false}
        });
        std::cout << "💳 Tracked: One-time purchase ($29.99)" << std::endl;
        
        // Track subscription revenue
        client.track_revenue("user_456", "sub_monthly_123", 9.99, Currency::USD, {
            {"billing_cycle", std::string("monthly")},
            {"plan_name", std::string("Basic Plan")},
            {"is_trial", false},
            {"payment_processor", std::string("stripe")}
        });
        std::cout << "🔄 Tracked: Monthly subscription ($9.99)" << std::endl;
        
        // Track larger enterprise purchase
        client.track_revenue("company_abc", "ent_order_456", 1999.00, Currency::USD, {
            {"product_type", std::string("enterprise_license")},
            {"seats", int64_t(50)},
            {"contract_length_months", int64_t(12)},
            {"discount_percent", 10.0}
        });
        std::cout << "🏢 Tracked: Enterprise purchase ($1,999.00)" << std::endl;
        
        // Track international purchase
        client.track_revenue("user_789", "eu_order_123", 24.99, Currency::EUR, {
            {"product_name", std::string("Pro Plan")},
            {"region", std::string("europe")},
            {"vat_included", true},
            {"currency_conversion_rate", 1.08}
        });
        std::cout << "🌍 Tracked: International purchase (€24.99)" << std::endl;
        
        // Track refund (negative revenue)
        client.track_revenue("user_999", "refund_order_555", -15.00, Currency::USD, {
            {"original_order_id", std::string("order_555")},
            {"refund_reason", std::string("customer_request")},
            {"refund_type", std::string("full")},
            {"days_since_purchase", int64_t(3)}
        });
        std::cout << "↩️  Tracked: Refund (-$15.00)" << std::endl;
        
        // Ensure all revenue events are sent
        client.flush();
        std::cout << "\n✅ All revenue events sent to UserCanal!" << std::endl;
        std::cout << "📈 Check your UserCanal dashboard for revenue analytics" << std::endl;
        
        client.shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}