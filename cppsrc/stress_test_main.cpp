#include <iostream>
#include <vector>
#include <thread>
#include <random>
#include <chrono>
#include <iomanip>

#include "order.hpp"
#include "orderExecutioner.hpp"
#include "orderManager.hpp"
#include "store.hpp"

#ifdef ENABLE_METRICS
#include "metricsCollector.hpp"
#endif

using namespace std;

// Wrapper function to process orders
void processOrder(OrderManager &orderMgr, shared_ptr<Order> order) {
    if (order) {
        orderMgr.placeOrder(order);
    }
}

int main(int argc, char* argv[]) {
    try {
        int num_orders = 1000; // Default to 1000 orders
        if (argc > 1) {
            try {
                num_orders = stoi(argv[1]);
            } catch (...) {
                // keep default
            }
        }

        cout << "=== Running Headless Stress Test with " << num_orders << " orders ===" << endl;

        // 1. Setup users
        Store::addUser("User1");
        Store::addUser("User2");
        Store::addUser("User3");
        Store::addUser("User4");
        Store::addUser("User5");

        // Boost funds and holdings
        vector<string> usernames = {"User1", "User2", "User3", "User4", "User5"};
        vector<string> stocks = {"TCS", "AAPL", "BABA", "GOOGL"};

        for (const auto &name : usernames) {
            auto user = Store::getUser(name);
            if (user) {
                user->addFunds(100000000.0); // 100 million
                for (const auto &symbol : stocks) {
                    user->addToDemat(symbol, 1000000); // 1 million shares
                }
            }
        }

        // 2. Setup stocks
        Store::addStocks("AAPL", 225.0f, 100000);
        Store::addStocks("TCS", 999.0f, 100000);
        Store::addStocks("GOOGL", 165.0f, 100000);
        Store::addStocks("BABA", 75.0f, 100000);

        // Initialize executioner and manager
        OrderExecutioner::getExecutionerInstance();
        OrderManager &orderMgr = OrderManager::getOrderManagerInstance();

        // 3. Generate randomized orders
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> userDist(0, usernames.size() - 1);
        uniform_int_distribution<> stockDist(0, stocks.size() - 1);
        uniform_int_distribution<> sideDist(0, 1);
        uniform_int_distribution<> qtyDist(1, 10);
        uniform_real_distribution<float> priceVariationDist(-20.0f, 20.0f);

        vector<shared_ptr<Order>> stressOrders;
        for (int i = 0; i < num_orders; ++i) {
            auto user = Store::getUser(usernames[userDist(gen)]);
            string symbol = stocks[stockDist(gen)];
            bool isBuy = sideDist(gen) == 1;
            int qty = qtyDist(gen);
            auto stock = Store::getStock(symbol);
            float basePrice = stock ? stock->getPrice() : 500.0f;
            float price = basePrice + priceVariationDist(gen);
            if (price <= 1.0f) price = 10.0f;

            stressOrders.push_back(make_shared<Order>(symbol, ORDER_TYPE::LIMIT, isBuy, qty, price, user, 24 * 60 * 60));
        }

        // 4. Dispatch concurrently using a small, fixed number of generator threads
        auto startTime = chrono::high_resolution_clock::now();
        
#ifdef ENABLE_METRICS
        MetricsCollector::getInstance().resetTimer();
#endif

        int num_dispatch_threads = 4;
        vector<thread> dispatchThreads;
        int orders_per_thread = num_orders / num_dispatch_threads;

        for (int t = 0; t < num_dispatch_threads; ++t) {
            int startIdx = t * orders_per_thread;
            int endIdx = (t == num_dispatch_threads - 1) ? num_orders : (t + 1) * orders_per_thread;

            dispatchThreads.emplace_back([&orderMgr, &stressOrders, startIdx, endIdx]() {
                try {
                    for (int i = startIdx; i < endIdx; ++i) {
                        orderMgr.placeOrder(stressOrders[i]);
                    }
                } catch (const exception &e) {
                    cerr << "DISPATCH THREAD ERROR: " << e.what() << endl;
                } catch (...) {
                    cerr << "DISPATCH THREAD ERROR: Unknown exception" << endl;
                }
            });
        }

        for (auto &t : dispatchThreads) {
            t.join();
        }
        auto endTime = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count();

        cout << "Dispatched " << num_orders << " orders in " << duration << " ms." << endl;

        // 5. Wait for matches to complete in the ThreadPool
        cout << "Processing matches..." << endl;
        
        // Poll the processed order count until all dispatched orders have been matched/processed and thread pool is idle
        while (true) {
#ifdef ENABLE_METRICS
            if (MetricsCollector::getInstance().getTotalOrdersProcessed() >= (unsigned long long)num_orders &&
                MetricsCollector::getInstance().getActiveThreads() == 0) {
                MetricsCollector::getInstance().stopTimer();
                break;
            }
#else
            // If metrics are disabled, wait a static period
            this_thread::sleep_for(chrono::milliseconds(100));
            break;
#endif
            this_thread::sleep_for(chrono::microseconds(10));
        }

#ifdef ENABLE_METRICS
        MetricsCollector::getInstance().printMetrics();
#else
        cout << "Metrics are disabled. Please compile with -DENABLE_METRICS enabled to view stats." << endl;
#endif

        cout << "=== Stress Test Complete ===" << endl;
        return 0;
    } catch (const exception &e) {
        cerr << "CRITICAL ERROR: " << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "CRITICAL ERROR: Unknown exception occurred!" << endl;
        return 1;
    }
}
