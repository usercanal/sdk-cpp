// event_revenue.cpp
// Revenue Tracking Example
// Shows revenue and subscription tracking with UserCanal C++ SDK

#include <iostream>
#include "usercanal/usercanal.hpp"

using namespace usercanal;

int main() {
    std::cout << "💰 UserCanal C++ SDK - Revenue Tracking\n" << std::endl;
    
    std::string api_key = "000102030405060708090a0b0c0d0e0f";
    
    try {
        // Initialize client
        Client client(api_key);
        client.initialize();
        
        std::cout << "✅ UserCanal SDK initialized" << std::endl;
        
        // Track one-time purchase
        client.event_revenue("user_123", "order_789", 29.99, "USD", {
            {"product_name", std::string("Premium Plan")},
            {"payment_method", std::string("credit_card")},
            {"discount_applied", false}
        });

        
        // Track subscription revenue
        client.event_revenue("user_456", "sub_monthly_123", 9.99, "USD", {
            {"billing_cycle", std::string("monthly")},
            {"plan_name", std::string("Basic Plan")},
            {"is_trial", false},
            {"payment_processor", std::string("stripe")}
        });

        
        // Track larger enterprise purchase
        client.event_revenue("company_abc", "ent_order_456", 1999.00, "USD", {
            {"product_type", std::string("enterprise_license")},
            {"seats", int64_t(50)},
            {"contract_length_months", int64_t(12)},
            {"discount_percent", 10.0}
        });

        
        // Track international purchase
        client.event_revenue("user_789", "eu_order_123", 24.99, "EUR", {
            {"product_name", std::string("Pro Plan")},
            {"region", std::string("europe")},
            {"vat_included", true},
            {"currency_conversion_rate", 1.08}
        });

        
        // Track refund (negative revenue)
        client.event_revenue("user_999", "refund_order_555", -15.00, "USD", {
            {"original_order_id", std::string("order_555")},
            {"refund_reason", std::string("customer_request")},
            {"refund_type", std::string("full")},
            {"days_since_purchase", int64_t(3)}
        });

        
        // Ensure all revenue events are sent
        client.flush();
        std::cout << "✅ Revenue events sent to UserCanal!" << std::endl;
        std::cout << "📈 Check your UserCanal dashboard for revenue analytics" << std::endl;
        
        client.shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}