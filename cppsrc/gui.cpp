#include "gui.hpp"
#include "store.hpp"
#include "order.hpp"
#include "common.hpp"

#ifdef ENABLE_METRICS
#include "metricsCollector.hpp"
#endif

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <vector>
#include <string>
#include <thread>
#include <random>
#include <iomanip>
#include <sstream>
#include <algorithm>

using namespace ftxui;

// Global log list for TUI actions
std::vector<std::string> tui_logs = {"TUI System Initialized."};
std::mutex tui_logs_mutex;

void addTuiLog(const std::string &msg) {
    std::lock_guard<std::mutex> lock(tui_logs_mutex);
    tui_logs.push_back(msg);
    if (tui_logs.size() > 30) {
        tui_logs.erase(tui_logs.begin());
    }
}

// stress test runner
void runStressTestTUI(OrderManager &orderMgr, int numOrders) {
    const int NUM_ORDERS = numOrders;
    addTuiLog("Stress test: Initializing users and stocks...");

    // 1. Boost user balances and holdings to prevent validation rejections
    std::vector<std::string> usernames = {"User1", "User2", "User3", "User4", "User5"};
    std::vector<std::string> stocks = {"TCS", "AAPL", "BABA", "GOOGL"};
    
    for (const auto &name : usernames) {
        auto user = Store::getUser(name);
        if (user) {
            user->addFunds(100000000.0); // 100 million
            for (const auto &symbol : stocks) {
                user->addToDemat(symbol, 1000000); // 1 million shares
            }
        }
    }

    if (!Store::getStock("AAPL")) Store::addStocks("AAPL", 225.0f, 100000);
    if (!Store::getStock("TCS")) Store::addStocks("TCS", 999.0f, 100000);
    if (!Store::getStock("GOOGL")) Store::addStocks("GOOGL", 165.0f, 100000);
    if (!Store::getStock("BABA")) Store::addStocks("BABA", 75.0f, 100000);

    // 2. Generate random orders
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> userDist(0, usernames.size() - 1);
    std::uniform_int_distribution<> stockDist(0, stocks.size() - 1);
    std::uniform_int_distribution<> sideDist(0, 1); // 0 = Sell, 1 = Buy
    std::uniform_int_distribution<> qtyDist(1, 10);
    std::uniform_real_distribution<float> priceVariationDist(-20.0f, 20.0f);

    std::vector<std::shared_ptr<Order>> stressOrders;
    for (int i = 0; i < NUM_ORDERS; ++i) {
        auto user = Store::getUser(usernames[userDist(gen)]);
        std::string symbol = stocks[stockDist(gen)];
        bool isBuy = sideDist(gen) == 1;
        int qty = qtyDist(gen);
        
        auto stock = Store::getStock(symbol);
        float basePrice = stock ? stock->getPrice() : 500.0f;
        float price = basePrice + priceVariationDist(gen);
        if (price <= 1.0f) price = 10.0f;

        stressOrders.push_back(std::make_shared<Order>(symbol, ORDER_TYPE::LIMIT, isBuy, qty, price, user, 24 * 60 * 60));
    }

    addTuiLog("Stress test: Dispatching 1000 orders using 4 concurrent threads...");

    // 3. Reset and start benchmark timer
    MetricsCollector::getInstance().resetTimer();
    auto startTime = std::chrono::high_resolution_clock::now();

    int num_dispatch_threads = 4;
    std::vector<std::thread> dispatchThreads;
    int orders_per_thread = NUM_ORDERS / num_dispatch_threads;

    for (int t = 0; t < num_dispatch_threads; ++t) {
        int startIdx = t * orders_per_thread;
        int endIdx = (t == num_dispatch_threads - 1) ? NUM_ORDERS : (t + 1) * orders_per_thread;

        dispatchThreads.emplace_back([&orderMgr, &stressOrders, startIdx, endIdx]() {
            for (int i = startIdx; i < endIdx; ++i) {
                orderMgr.placeOrder(stressOrders[i]);
            }
        });
    }

    for (auto &t : dispatchThreads) {
        t.join();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    addTuiLog("Stress test: Dispatched 1000 orders in " + std::to_string(duration) + " ms. Matching trades...");

    // 4. Poll until all dispatched orders are processed and thread pool is idle
    while (true) {
        if (MetricsCollector::getInstance().getTotalOrdersProcessed() >= (unsigned long long)NUM_ORDERS &&
            MetricsCollector::getInstance().getActiveThreads() == 0) {
            MetricsCollector::getInstance().stopTimer();
            break;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }

    double totalMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startTime).count();
    auto &collector = MetricsCollector::getInstance();
    std::ostringstream ss;
    ss << "Stress test finished: " << NUM_ORDERS << " orders in " << std::fixed << std::setprecision(1) << totalMs << " ms (" 
       << static_cast<unsigned long long>(collector.getOrdersProcessedPerSec()) << " orders/sec) | Avg Latency: " 
       << std::fixed << std::setprecision(1) << (collector.getAvgMatchLatencyMs() * 1000.0) << " us";
    addTuiLog(ss.str());
}

void launchTUI(OrderManager &orderMgr) {
    auto screen = ScreenInteractive::Fullscreen();

    int selected_tab = 0;
    std::vector<std::string> tab_titles = {
        "Dashboard",
        "Order Books",
        "Place Order",
        "Register User",
        "Ledger & Logs",
        "Diagnostics"
    };

    // User Selection (Dashboard Tab)
    int selected_user_idx = 0;
    std::vector<std::string> usernames_list = {"User1", "User2", "User3", "User4", "User5"};

    // Form inputs (Place Order Tab)
    std::string input_username = "User1";
    std::string input_qty = "5";
    std::string input_price = "990";
    int selected_stock = 0;
    std::vector<std::string> stocks_list;
    for (const auto& [symbol, stockPtr] : Store::getStocksMap()) {
        stocks_list.push_back(symbol);
    }
    std::sort(stocks_list.begin(), stocks_list.end());
    int selected_side = 0; // Buy / Sell
    std::vector<std::string> sides = {"Buy", "Sell"};
    int selected_type = 0; // Limit / Market
    std::vector<std::string> types = {"LIMIT", "MARKET"};

    // Add User Form
    std::string input_new_username = "";
    std::string input_stress_orders = "1000";

    // Books Tab selection
    int selected_stock_book = 0;

    // Components
    Component comp_tab_toggle = Toggle(&tab_titles, &selected_tab);
    Component comp_user_toggle = Radiobox(&usernames_list, &selected_user_idx);

    Component comp_input_user = Input(&input_username, "Username...");
    Component comp_input_qty = Input(&input_qty, "Quantity...");
    Component comp_input_price = Input(&input_price, "Price...");
    Component comp_stock_dropdown = Dropdown(&stocks_list, &selected_stock);
    Component comp_side_radio = Radiobox(&sides, &selected_side);
    Component comp_type_radio = Radiobox(&types, &selected_type);

    Component comp_input_new_user = Input(&input_new_username, "New username...");
    Component comp_input_stress = Input(&input_stress_orders, "Order count...");

    Component comp_book_stock_dropdown = Dropdown(&stocks_list, &selected_stock_book);

    std::string order_feedback = "";
    std::string user_feedback = "";

    Component btn_place_order = Button("Submit Order", [&]() {
        auto user = Store::getUser(input_username);
        if (!user) {
            order_feedback = "Error: User '" + input_username + "' not found.";
            return;
        }
        try {
            int qty = std::stoi(input_qty);
            float price = std::stof(input_price);
            bool isBuy = (selected_side == 0);
            ORDER_TYPE o_type = (selected_type == 0) ? ORDER_TYPE::LIMIT : ORDER_TYPE::MARKET;

            auto order = std::make_shared<Order>(stocks_list[selected_stock], o_type, isBuy, qty, price, user, 24 * 60 * 60);
            if (orderMgr.placeOrder(order)) {
                order_feedback = "Order placed successfully! (" + std::string(isBuy ? "BUY" : "SELL") + " " + std::to_string(qty) + " " + stocks_list[selected_stock] + ")";
                addTuiLog("User " + input_username + " placed " + (isBuy ? "BUY" : "SELL") + " order for " + std::to_string(qty) + " " + stocks_list[selected_stock]);
            } else {
                order_feedback = "Error: Order validation failed (check funds/demat).";
            }
        } catch (...) {
            order_feedback = "Error: Invalid quantity or price format.";
        }
    });

    Component btn_create_user = Button("Create User", [&]() {
        if (input_new_username.empty()) {
            user_feedback = "Error: Username cannot be empty.";
            return;
        }
        auto existing = Store::getUser(input_new_username);
        if (existing) {
            user_feedback = "Error: User already exists.";
        } else {
            Store::addUser(input_new_username);
            usernames_list.push_back(input_new_username);
            user_feedback = "User '" + input_new_username + "' created with 10,000 Rs!";
            addTuiLog("Created new user: " + input_new_username);
            input_new_username = "";
        }
    });

    Component btn_stress_test = Button("Trigger Stress Test", [&]() {
        int numOrders = 1000;
        try {
            numOrders = std::stoi(input_stress_orders);
        } catch (...) {
            addTuiLog("Invalid order count input. Defaulting to 1000.");
            numOrders = 1000;
        }
        if (numOrders <= 0) {
            addTuiLog("Order count must be greater than zero.");
            return;
        }
        std::thread([&orderMgr, numOrders]() {
            runStressTestTUI(orderMgr, numOrders);
        }).detach();
    });

    auto place_order_container = Container::Vertical({
        comp_input_user,
        comp_stock_dropdown,
        comp_input_qty,
        comp_input_price,
        comp_side_radio,
        comp_type_radio,
        btn_place_order
    });

    auto register_user_container = Container::Vertical({
        comp_input_new_user,
        btn_create_user
    });

    auto book_dropdown_container = Container::Vertical({
        comp_book_stock_dropdown
    });

    auto diagnostic_container = Container::Vertical({
        comp_input_stress,
        btn_stress_test
    });

    auto active_tab_container = Container::Tab({
            // Tab 0: Dashboard
            Renderer(comp_user_toggle, [&]() {
                // Compile Stocks Table
                std::vector<Element> stock_rows;
                stock_rows.push_back(hbox({
                    text("STOCK") | bold | flex,
                    text("LAST PRICE (Rs)") | bold | flex,
                    text("LIQUIDITY (shares)") | bold | flex
                }));
                stock_rows.push_back(separator());
                for (const auto& [symbol, stockPtr] : Store::getStocksMap()) {
                    stock_rows.push_back(hbox({
                        text(symbol) | bold | flex,
                        text(std::to_string(static_cast<int>(stockPtr->getPrice())) + ".00") | flex,
                        text(std::to_string(stockPtr->getQty())) | flex
                    }));
                }

                // Compile Selected User Details
                std::vector<Element> user_details;
                if (selected_user_idx < static_cast<int>(usernames_list.size())) {
                    auto user = Store::getUser(usernames_list[selected_user_idx]);
                    if (user) {
                        user_details.push_back(text("User Account: " + user->getUsername()) | bold | color(Color::Cyan));
                        std::ostringstream ss;
                        ss << std::fixed << std::setprecision(2) << user->getFunds();
                        user_details.push_back(text("Available Cash Balance: " + ss.str() + " Rs"));
                        user_details.push_back(separator());
                        user_details.push_back(text("Demat Holdings:") | bold);
                        bool hasHoldings = false;
                        for (const auto& [symbol, qty] : user->getDemat()) {
                            user_details.push_back(text(" - " + symbol + ": " + std::to_string(qty) + " shares"));
                            hasHoldings = true;
                        }
                        if (!hasHoldings) {
                            user_details.push_back(text(" - None (no active holdings)"));
                        }
                    }
                }

                return hbox({
                    vbox(stock_rows) | flex | borderDouble | size(WIDTH, GREATER_THAN, 40),
                    vbox({
                        text("Select Portfolio:") | bold,
                        separator(),
                        comp_user_toggle->Render()
                    }) | border | size(WIDTH, EQUAL, 25),
                    vbox(user_details) | flex | border
                });
            }),

            // Tab 1: Live Order Books
            Renderer(book_dropdown_container, [&]() {
                std::vector<Element> buy_rows;
                std::vector<Element> sell_rows;

                buy_rows.push_back(hbox({
                    text("QUANTITY") | bold | flex,
                    text("BID PRICE") | bold | flex
                }));
                buy_rows.push_back(separator());

                sell_rows.push_back(hbox({
                    text("ASK PRICE") | bold | flex,
                    text("QUANTITY") | bold | flex
                }));
                sell_rows.push_back(separator());

                std::string selected_sym = stocks_list[selected_stock_book];
                auto stock = Store::getStock(selected_sym);
                if (stock) {
                    auto book = stock->getOrderBookInstance();
                    if (book) {
                        for (const auto& [price, q] : book->getBuyOrderBook()) {
                            int total_qty = 0;
                            auto q_copy = q;
                            while (!q_copy.empty()) {
                                if (q_copy.front()) {
                                    total_qty += q_copy.front()->getPendingQty();
                                }
                                q_copy.pop();
                            }
                            if (total_qty > 0) {
                                std::ostringstream ss;
                                ss << std::fixed << std::setprecision(2) << price;
                                buy_rows.push_back(hbox({
                                    text(std::to_string(total_qty)) | flex,
                                    text(ss.str()) | color(Color::Green) | flex
                                }));
                            }
                        }

                        for (const auto& [price, q] : book->getSellOrderBook()) {
                            int total_qty = 0;
                            auto q_copy = q;
                            while (!q_copy.empty()) {
                                if (q_copy.front()) {
                                    total_qty += q_copy.front()->getPendingQty();
                                }
                                q_copy.pop();
                            }
                            if (total_qty > 0) {
                                std::ostringstream ss;
                                ss << std::fixed << std::setprecision(2) << price;
                                sell_rows.push_back(hbox({
                                    text(ss.str()) | color(Color::Red) | flex,
                                    text(std::to_string(total_qty)) | flex
                                }));
                            }
                        }
                    }
                }

                return vbox({
                    hbox({
                        text("Filter Stock Ticker: ") | center | bold,
                        comp_book_stock_dropdown->Render() | size(WIDTH, EQUAL, 15)
                    }),
                    separator(),
                    hbox({
                        vbox(buy_rows) | flex | border | size(HEIGHT, GREATER_THAN, 12),
                        vbox(sell_rows) | flex | border | size(HEIGHT, GREATER_THAN, 12)
                    })
                });
            }),

            // Tab 2: Place Order Form
            Renderer(place_order_container, [&]() {
                return vbox({
                    text("PLACE NEW TRADING ORDER") | bold | color(Color::Green),
                    separator(),
                    hbox(text("Username:      ") | size(WIDTH, EQUAL, 15), comp_input_user->Render() | border),
                    hbox(text("Stock Ticker:  ") | size(WIDTH, EQUAL, 15), comp_stock_dropdown->Render()),
                    hbox(text("Quantity:      ") | size(WIDTH, EQUAL, 15), comp_input_qty->Render() | border),
                    hbox(text("Price (Rs):    ") | size(WIDTH, EQUAL, 15), comp_input_price->Render() | border),
                    hbox(text("Side:          ") | size(WIDTH, EQUAL, 15), comp_side_radio->Render()),
                    hbox(text("Type:          ") | size(WIDTH, EQUAL, 15), comp_type_radio->Render()),
                    separator(),
                    btn_place_order->Render() | center,
                    text(order_feedback) | color(Color::Yellow) | center
                }) | border;
            }),

            // Tab 3: Register User Form
            Renderer(register_user_container, [&]() {
                return vbox({
                    text("REGISTER NEW ACCOUNT") | bold | color(Color::Cyan),
                    separator(),
                    hbox(text("Username:      ") | size(WIDTH, EQUAL, 15), comp_input_new_user->Render() | border),
                    separator(),
                    btn_create_user->Render() | center,
                    text(user_feedback) | color(Color::Yellow) | center
                }) | border;
            }),

            // Tab 3: Ledger & Logs
            Renderer([&]() {
                std::vector<Element> order_rows;
                order_rows.push_back(hbox({
                    text("ID") | bold | size(WIDTH, EQUAL, 5),
                    text("USER") | bold | size(WIDTH, EQUAL, 10),
                    text("STOCK") | bold | size(WIDTH, EQUAL, 8),
                    text("SIDE") | bold | size(WIDTH, EQUAL, 8),
                    text("TYPE") | bold | size(WIDTH, EQUAL, 8),
                    text("PRICE") | bold | size(WIDTH, EQUAL, 10),
                    text("QTY") | bold | size(WIDTH, EQUAL, 8),
                    text("PENDING") | bold | size(WIDTH, EQUAL, 8)
                }));
                order_rows.push_back(separator());

                const auto &orders = Store::getOrderVector();
                int o_start = std::max(0, static_cast<int>(orders.size()) - 15);
                for (int i = o_start; i < static_cast<int>(orders.size()); ++i) {
                    const auto &order = orders[i];
                    if (order) {
                        bool isBuy = order->getIsBuy();
                        std::ostringstream ss;
                        ss << std::fixed << std::setprecision(2) << order->getPrice();
                        order_rows.push_back(hbox({
                            text(std::to_string(order->getOrderId())) | size(WIDTH, EQUAL, 5),
                            text(order->getUser() ? order->getUser()->getUsername() : "SYSTEM") | size(WIDTH, EQUAL, 10),
                            text(order->getSymbol()) | size(WIDTH, EQUAL, 8),
                            text(isBuy ? "BUY" : "SELL") | color(isBuy ? Color::Green : Color::Red) | size(WIDTH, EQUAL, 8),
                            text(order->getOrderType() == ORDER_TYPE::LIMIT ? "LIMIT" : "MARKET") | size(WIDTH, EQUAL, 8),
                            text(ss.str()) | size(WIDTH, EQUAL, 10),
                            text(std::to_string(order->getQty())) | size(WIDTH, EQUAL, 8),
                            text(std::to_string(order->getPendingQty())) | size(WIDTH, EQUAL, 8)
                        }));
                    }
                }

                std::vector<Element> exec_rows;
                exec_rows.push_back(hbox({
                    text("ORDER") | bold | size(WIDTH, EQUAL, 8),
                    text("TICKER") | bold | size(WIDTH, EQUAL, 10),
                    text("QTY") | bold | size(WIDTH, EQUAL, 10),
                    text("TIMESTAMP") | bold | flex
                }));
                exec_rows.push_back(separator());

                const auto &executions = Store::getExecutedOrdersVector();
                // Row index 0 is header row {"OrderID", "Symbol", "ExecutedQty", "Timestamp"}
                int e_start = std::max(1, static_cast<int>(executions.size()) - 15);
                for (int i = e_start; i < static_cast<int>(executions.size()); ++i) {
                    const auto &row = executions[i];
                    if (row.size() >= 4) {
                        exec_rows.push_back(hbox({
                            text(row[0]) | size(WIDTH, EQUAL, 8),
                            text(row[1]) | size(WIDTH, EQUAL, 10),
                            text(row[2]) | size(WIDTH, EQUAL, 10),
                            text(row[3]) | flex
                        }));
                    }
                }

                return hbox({
                    vbox(order_rows) | flex | border | size(WIDTH, GREATER_THAN, 50),
                    vbox(exec_rows) | flex | border
                });
            }),

            // Tab 4: Diagnostics
            Renderer(diagnostic_container, [&]() {
                std::vector<Element> metric_elements;
                metric_elements.push_back(text("Matching Engine Diagnostics") | bold | color(Color::Cyan));
                metric_elements.push_back(separator());

#ifdef ENABLE_METRICS
                auto& collector = MetricsCollector::getInstance();
                
                auto formatWithCommas = [](unsigned long long value) -> std::string {
                    std::string numStr = std::to_string(value);
                    int insertPos = static_cast<int>(numStr.length()) - 3;
                    while (insertPos > 0) {
                        numStr.insert(insertPos, ",");
                        insertPos -= 3;
                    }
                    return numStr;
                };

                // Read percentiles
                double median = 0.0, p95 = 0.0, p99 = 0.0;
                auto sortedLatencies = collector.getSortedLatencies();
                double avgLatencyUs = (sortedLatencies.empty()) ? 0.0 : (collector.getAvgMatchLatencyMs() * 1000.0);
                if (!sortedLatencies.empty()) {
                    size_t n = sortedLatencies.size();
                    median = sortedLatencies[n * 50 / 100] / 1000.0;
                    p95 = sortedLatencies[n * 95 / 100] / 1000.0;
                    p99 = sortedLatencies[n * 99 / 100] / 1000.0;
                }

                metric_elements.push_back(hbox(text("Orders Submitted:      ") | size(WIDTH, EQUAL, 25), text(std::to_string(collector.getTotalOrdersSubmitted())) | bold));
                metric_elements.push_back(hbox(text("Orders Executed:       ") | size(WIDTH, EQUAL, 25), text(std::to_string(collector.getTotalOrdersProcessed())) | bold));
                metric_elements.push_back(hbox(text("Trades Generated:      ") | size(WIDTH, EQUAL, 25), text(std::to_string(collector.getTotalMatches())) | bold));
                metric_elements.push_back(separator());

                metric_elements.push_back(hbox(text("Throughput:            ") | size(WIDTH, EQUAL, 25), text(formatWithCommas(static_cast<unsigned long long>(collector.getOrdersProcessedPerSec())) + " orders/sec") | bold));
                
                std::ostringstream ss_avg, ss_med, ss_p95, ss_p99;
                ss_avg << std::fixed << std::setprecision(1) << avgLatencyUs;
                ss_med << std::fixed << std::setprecision(1) << median;
                ss_p95 << std::fixed << std::setprecision(1) << p95;
                ss_p99 << std::fixed << std::setprecision(1) << p99;

                metric_elements.push_back(hbox(text("Average Match Latency: ") | size(WIDTH, EQUAL, 25), text(ss_avg.str() + " us") | bold));
                metric_elements.push_back(hbox(text("Median Latency:        ") | size(WIDTH, EQUAL, 25), text(ss_med.str() + " us") | bold));
                metric_elements.push_back(hbox(text("P95 Latency:           ") | size(WIDTH, EQUAL, 25), text(ss_p95.str() + " us") | bold));
                metric_elements.push_back(hbox(text("P99 Latency:           ") | size(WIDTH, EQUAL, 25), text(ss_p99.str() + " us") | bold));
                metric_elements.push_back(separator());

                metric_elements.push_back(hbox(text("Worker Threads:        ") | size(WIDTH, EQUAL, 25), text("4 (out of 4 threads)") | bold));
                
                std::ostringstream ss_util;
                ss_util << std::fixed << std::setprecision(0) << collector.getCpuUtilization();
                metric_elements.push_back(hbox(text("CPU Utilization:       ") | size(WIDTH, EQUAL, 25), text(ss_util.str() + "%") | bold));
                metric_elements.push_back(hbox(text("Peak Queue Size:       ") | size(WIDTH, EQUAL, 25), text(std::to_string(collector.getPeakQueueSize())) | bold));
#else
                metric_elements.push_back(text("Metrics Disabled. Please compile with ENABLE_METRICS toggled ON."));
#endif
                metric_elements.push_back(separator());

                // Fetch logs
                std::vector<Element> log_lines;
                log_lines.push_back(text("System Logs:") | bold);
                {
                    std::lock_guard<std::mutex> lock(tui_logs_mutex);
                    for (auto it = tui_logs.rbegin(); it != tui_logs.rend(); ++it) {
                        log_lines.push_back(text(" - " + *it));
                    }
                }

                return vbox({
                    hbox({
                        vbox(metric_elements) | flex | border | size(WIDTH, GREATER_THAN, 40),
                        vbox({
                            text("Stress Testing Controls") | bold | color(Color::Red),
                            separator(),
                            hbox({
                                text("Order Count: ") | center,
                                comp_input_stress->Render() | border | size(WIDTH, EQUAL, 12)
                            }) | center,
                            separator(),
                            btn_stress_test->Render() | center,
                            text("Pushes random Buy/Sell orders into queues concurrently using 4 threads") | color(Color::GrayLight) | center
                        }) | flex | border
                    }),
                    vbox(log_lines) | border | size(HEIGHT, EQUAL, 10)
                });
            })
        }, &selected_tab);

    auto main_container = Container::Vertical({
        comp_tab_toggle,
        active_tab_container
    });

    // Auto-refresh loop thread to redraw the screen periodically (e.g. 5 times per sec to update book charts/metrics)
    std::atomic<bool> refresh_active{true};
    std::thread refresh_thread([&]() {
        while (refresh_active) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            screen.PostEvent(Event::Custom);
        }
    });

    auto renderer = Renderer(main_container, [&]() {
        return vbox({
            text("STOCK RUSH CLI ENGINE - TRANSACTION DASHBOARD") | center | bold | color(Color::Cyan),
            comp_tab_toggle->Render() | center,
            separator(),
            active_tab_container->Render() | flex
        });
    });

    screen.Loop(renderer);

    refresh_active = false;
    if (refresh_thread.joinable()) {
        refresh_thread.join();
    }
}
