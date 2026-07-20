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
void runStressTestTUI(OrderManager &orderMgr) {
    const int NUM_ORDERS = 100;
    addTuiLog("Stress test triggered: Dispatching " + std::to_string(NUM_ORDERS) + " concurrent orders...");

    // 1. Boost user balances and holdings to prevent validation rejections
    std::vector<std::string> usernames = {"User1", "User2", "User3", "User4", "User5"};
    std::vector<std::string> stocks = {"TCS", "APPLE", "BABA", "GOOGL"};
    
    for (const auto &name : usernames) {
        auto user = Store::getUser(name);
        if (user) {
            user->addFunds(10000000.0);
            for (const auto &symbol : stocks) {
                user->addToDemat(symbol, 100000);
            }
        }
    }

    // 2. Generate random orders
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> userDist(0, usernames.size() - 1);
    std::uniform_int_distribution<> stockDist(0, stocks.size() - 1);
    std::uniform_int_distribution<> sideDist(0, 1); // 0 = Sell, 1 = Buy
    std::uniform_int_distribution<> qtyDist(1, 10);
    std::uniform_real_distribution<float> priceVariationDist(-50.0f, 50.0f);

    std::vector<std::shared_ptr<Order>> stressOrders;
    for (int i = 0; i < NUM_ORDERS; ++i) {
        auto user = Store::getUser(usernames[userDist(gen)]);
        std::string symbol = stocks[stockDist(gen)];
        bool isBuy = sideDist(gen) == 1;
        int qty = qtyDist(gen);
        
        auto stock = Store::getStock(symbol);
        float basePrice = stock ? stock->getPrice() : 1000.0f;
        float price = basePrice + priceVariationDist(gen);
        if (price <= 1.0f) price = 10.0f;

        stressOrders.push_back(std::make_shared<Order>(symbol, ORDER_TYPE::LIMIT, isBuy, qty, price, user, 24 * 60 * 60));
    }

    // 3. Spawn threads to place orders concurrently
    auto startTime = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_ORDERS; ++i) {
        // Asynchronously place via threads
        threads.emplace_back([&orderMgr, order = stressOrders[i]]() {
            if (order) {
                orderMgr.placeOrder(order);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    std::ostringstream ss;
    ss << "Stress test finished: " << NUM_ORDERS << " orders in " << duration << " ms (" 
       << std::fixed << std::setprecision(2) << (static_cast<double>(NUM_ORDERS) / (duration / 1000.0)) << " orders/sec)";
    addTuiLog(ss.str());
}

void launchTUI(OrderManager &orderMgr) {
    auto screen = ScreenInteractive::Fullscreen();

    int selected_tab = 0;
    std::vector<std::string> tab_titles = {
        "Dashboard",
        "Order Books",
        "Place Order",
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
    std::vector<std::string> stocks_list = {"TCS", "APPLE", "BABA", "GOOGL"};
    int selected_side = 0; // Buy / Sell
    std::vector<std::string> sides = {"Buy", "Sell"};
    int selected_type = 0; // Limit / Market
    std::vector<std::string> types = {"LIMIT", "MARKET"};

    // Add User Form
    std::string input_new_username = "";

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

    Component btn_stress_test = Button("Trigger 100-Order Stress Test", [&]() {
        std::thread([&]() {
            runStressTestTUI(orderMgr);
        }).detach();
    });

    auto place_order_container = Container::Vertical({
        comp_input_user,
        comp_stock_dropdown,
        comp_input_qty,
        comp_input_price,
        comp_side_radio,
        comp_type_radio,
        btn_place_order,
        comp_input_new_user,
        btn_create_user
    });

    auto book_dropdown_container = Container::Vertical({
        comp_book_stock_dropdown
    });

    auto diagnostic_container = Container::Vertical({
        btn_stress_test
    });

    auto main_container = Container::Vertical({
        comp_tab_toggle,
        Container::Tab({
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
                return hbox({
                    vbox({
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
                    }) | flex | border,
                    vbox({
                        text("REGISTER NEW ACCOUNT") | bold | color(Color::Cyan),
                        separator(),
                        hbox(text("Username:      ") | size(WIDTH, EQUAL, 15), comp_input_new_user->Render() | border),
                        separator(),
                        btn_create_user->Render() | center,
                        text(user_feedback) | color(Color::Yellow) | center
                    }) | flex | border
                });
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
                std::ostringstream ss_ops, ss_mps, ss_lat;
                ss_ops << std::fixed << std::setprecision(2) << collector.getOrdersProcessedPerSec();
                ss_mps << std::fixed << std::setprecision(2) << collector.getMatchesPerSec();
                ss_lat << std::fixed << std::setprecision(4) << collector.getAvgMatchLatencyMs();

                metric_elements.push_back(hbox(text("Orders Processed/sec:  ") | size(WIDTH, EQUAL, 25), text(ss_ops.str() + " /s") | bold));
                metric_elements.push_back(hbox(text("Average Matches/sec:   ") | size(WIDTH, EQUAL, 25), text(ss_mps.str() + " /s") | bold));
                metric_elements.push_back(hbox(text("Active Worker Threads: ") | size(WIDTH, EQUAL, 25), text(std::to_string(collector.getActiveThreads()) + " active (out of 4 threads)") | bold));
                metric_elements.push_back(hbox(text("Avg Matching Latency:  ") | size(WIDTH, EQUAL, 25), text(ss_lat.str() + " ms") | bold));
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
                            btn_stress_test->Render() | center,
                            text("Pushes 100 random Buy/Sell orders into queues concurrently") | color(Color::GrayLight) | center
                        }) | flex | border
                    }),
                    vbox(log_lines) | border | size(HEIGHT, EQUAL, 10)
                });
            })
        }, &selected_tab)
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
            main_container->Render() | flex
        });
    });

    screen.Loop(renderer);

    refresh_active = false;
    if (refresh_thread.joinable()) {
        refresh_thread.join();
    }
}
