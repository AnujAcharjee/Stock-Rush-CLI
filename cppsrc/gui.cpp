#include "gui.hpp"
#include "store.hpp"
#include "order.hpp"
#include "common.hpp"
#include "orderExecutioner.hpp"

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

void launchTUI(OrderManager &orderMgr) {
    auto screen = ScreenInteractive::Fullscreen();

    int selected_tab = 0;
    std::vector<std::string> tab_titles = {
        "Dashboard",
        "Order Books",
        "Place Order",
        "Register User",
        "Ledger & Logs"};

    // User Selection (Dashboard Tab)
    int selected_user_idx = 0;
    std::vector<std::string> usernames_list = {"User1", "User2", "User3", "User4", "User5"};

    // Form inputs (Place Order Tab)
    std::string input_order_username = "";
    std::string input_qty = "";
    std::string input_price = "";
    std::string input_order_stock = "";
    std::vector<std::string> stocks_list;
    for (const auto &[symbol, stockPtr] : Store::getStocksMapSnapshot()) {
        stocks_list.push_back(symbol);
    }
    std::sort(stocks_list.begin(), stocks_list.end());
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

    Component comp_input_user = Input(&input_order_username, "Username...");
    Component comp_input_qty = Input(&input_qty, "Quantity...");
    Component comp_input_price = Input(&input_price, "Price...");
    Component comp_stock_dropdown = Input(&input_order_stock, "Stock symbol...");
    Component comp_side_radio = Radiobox(&sides, &selected_side);
    Component comp_type_radio = Radiobox(&types, &selected_type);

    Component comp_input_new_user = Input(&input_new_username, "New username...");

    Component comp_book_stock_dropdown = Dropdown(&stocks_list, &selected_stock_book);

    std::string order_feedback = "";
    std::string user_feedback = "";

    Component btn_place_order = Button("Submit Order", [&]() {
        if (input_order_username.empty()) {
            order_feedback = "Error: Username cannot be empty.";
            return;
        }
        std::string order_username = input_order_username;
        auto user = Store::getUser(order_username);
        if (!user) {
            order_feedback = "Error: User '" + order_username + "' not found.";
            return;
        }
        if (input_order_stock.empty()) {
            order_feedback = "Error: Stock symbol cannot be empty.";
            return;
        }
        auto stock = Store::getStock(input_order_stock);
        if (!stock) {
            order_feedback = "Error: Stock '" + input_order_stock + "' not found.";
            return;
        }
        try {
            int qty = std::stoi(input_qty);
            float price = std::stof(input_price);
            bool isBuy = (selected_side == 0);
            ORDER_TYPE o_type = (selected_type == 0) ? ORDER_TYPE::LIMIT : ORDER_TYPE::MARKET;

            auto order = std::make_shared<Order>(input_order_stock, o_type, isBuy, qty, price, user, 24 * 60 * 60);
            if (orderMgr.placeOrder(order)) {
                order_feedback = "Order placed successfully! (" + std::string(isBuy ? "BUY" : "SELL") + " " + std::to_string(qty) + " " + input_order_stock + ")";
                addTuiLog("User " + order_username + " placed " + (isBuy ? "BUY" : "SELL") + " order for " + std::to_string(qty) + " " + input_order_stock);
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

    auto place_order_container = Container::Vertical({comp_input_user,
                                                      comp_stock_dropdown,
                                                      comp_input_qty,
                                                      comp_input_price,
                                                      comp_side_radio,
                                                      comp_type_radio,
                                                      btn_place_order});

    auto register_user_container = Container::Vertical({comp_input_new_user,
                                                        btn_create_user});

    auto book_dropdown_container = Container::Vertical({comp_book_stock_dropdown});

    auto active_tab_container = Container::Tab({// Tab 0: Dashboard
                                                Renderer(comp_user_toggle, [&]() {
                                                    // Compile Stocks Table (thread-safe snapshot)
                                                    auto stocksSnapshot = Store::getStocksMapSnapshot();
                                                    std::vector<Element> stock_rows;
                                                    stock_rows.push_back(hbox({text("STOCK") | bold | flex,
                                                                               text("LAST PRICE (Rs)") | bold | flex,
                                                                               text("LIQUIDITY (shares)") | bold | flex}));
                                                    stock_rows.push_back(separator());
                                                    for (const auto &[symbol, stockPtr] : stocksSnapshot) {
                                                        stock_rows.push_back(hbox({text(symbol) | bold | flex,
                                                                                   text(std::to_string(static_cast<int>(stockPtr->getPrice())) + ".00") | flex,
                                                                                   text(std::to_string(stockPtr->getQty())) | flex}));
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
                                                            auto dematSnapshot = user->getDemat();
                                                            for (const auto &[symbol, qty] : dematSnapshot) {
                                                                user_details.push_back(text(" - " + symbol + ": " + std::to_string(qty) + " shares"));
                                                                hasHoldings = true;
                                                            }
                                                            if (!hasHoldings) {
                                                                user_details.push_back(text(" - None (no active holdings)"));
                                                            }
                                                        }
                                                    }

                                                    return hbox({vbox(stock_rows) | flex | borderDouble | size(WIDTH, GREATER_THAN, 40),
                                                                 vbox({text("Select Portfolio:") | bold,
                                                                       separator(),
                                                                       comp_user_toggle->Render()}) |
                                                                     border | size(WIDTH, EQUAL, 25),
                                                                 vbox(user_details) | flex | border});
                                                }),

                                                // Tab 1: Live Order Books
                                                Renderer(book_dropdown_container, [&]() {
                                                    std::vector<Element> buy_rows;
                                                    std::vector<Element> sell_rows;

                                                    buy_rows.push_back(hbox({text("QUANTITY") | bold | flex,
                                                                             text("BID PRICE") | bold | flex}));
                                                    buy_rows.push_back(separator());

                                                    sell_rows.push_back(hbox({text("ASK PRICE") | bold | flex,
                                                                              text("QUANTITY") | bold | flex}));
                                                    sell_rows.push_back(separator());

                                                    std::string selected_sym = stocks_list[selected_stock_book];
                                                    auto stock = Store::getStock(selected_sym);
                                                    if (stock) {
                                                        auto book = stock->getOrderBookInstance();
                                                        if (book) {
                                                            auto buyLevels = book->getAggregatedBuyLevels();
                                                            for (const auto &[price, total_qty] : buyLevels) {
                                                                std::ostringstream ss;
                                                                ss << std::fixed << std::setprecision(2) << price;
                                                                buy_rows.push_back(hbox({text(std::to_string(total_qty)) | flex,
                                                                                         text(ss.str()) | color(Color::Green) | flex}));
                                                            }

                                                            auto sellLevels = book->getAggregatedSellLevels();
                                                            for (const auto &[price, total_qty] : sellLevels) {
                                                                std::ostringstream ss;
                                                                ss << std::fixed << std::setprecision(2) << price;
                                                                sell_rows.push_back(hbox({text(ss.str()) | color(Color::Red) | flex,
                                                                                          text(std::to_string(total_qty)) | flex}));
                                                            }
                                                        }
                                                    }

                                                    return vbox({hbox({text("Filter Stock Ticker: ") | center | bold,
                                                                       comp_book_stock_dropdown->Render() | size(WIDTH, EQUAL, 15)}),
                                                                 separator(),
                                                                 hbox({vbox(buy_rows) | flex | border | size(HEIGHT, GREATER_THAN, 12),
                                                                       vbox(sell_rows) | flex | border | size(HEIGHT, GREATER_THAN, 12)})});
                                                }),

                                                // Tab 2: Place Order Form
                                                Renderer(place_order_container, [&]() {
                                                    return vbox({text("PLACE NEW TRADING ORDER") | bold | color(Color::Green),
                                                                 separator(),
                                                                 hbox(text("Username:      ") | size(WIDTH, EQUAL, 15), comp_input_user->Render()),
                                                                 hbox(text("Stock Ticker:  ") | size(WIDTH, EQUAL, 15), comp_stock_dropdown->Render()),
                                                                 hbox(text("Quantity:      ") | size(WIDTH, EQUAL, 15), comp_input_qty->Render() | border),
                                                                 hbox(text("Price (Rs):    ") | size(WIDTH, EQUAL, 15), comp_input_price->Render() | border),
                                                                 hbox(text("Side:          ") | size(WIDTH, EQUAL, 15), comp_side_radio->Render()),
                                                                 hbox(text("Type:          ") | size(WIDTH, EQUAL, 15), comp_type_radio->Render()),
                                                                 separator(),
                                                                 btn_place_order->Render() | center,
                                                                 text(order_feedback) | color(Color::Yellow) | center}) |
                                                           border;
                                                }),

                                                // Tab 3: Register User Form
                                                Renderer(register_user_container, [&]() {
                                                    return vbox({text("REGISTER NEW ACCOUNT") | bold | color(Color::Cyan),
                                                                 separator(),
                                                                 hbox(text("Username:      ") | size(WIDTH, EQUAL, 15), comp_input_new_user->Render() | border),
                                                                 separator(),
                                                                 btn_create_user->Render() | center,
                                                                 text(user_feedback) | color(Color::Yellow) | center}) |
                                                           border;
                                                }),

                                                // Tab 3: Ledger & Logs
                                                Renderer([&]() {
                                                    std::vector<Element> order_rows;
                                                    order_rows.push_back(hbox({text("ID") | bold | size(WIDTH, EQUAL, 5),
                                                                               text("USER") | bold | size(WIDTH, EQUAL, 10),
                                                                               text("STOCK") | bold | size(WIDTH, EQUAL, 8),
                                                                               text("SIDE") | bold | size(WIDTH, EQUAL, 8),
                                                                               text("TYPE") | bold | size(WIDTH, EQUAL, 8),
                                                                               text("PRICE") | bold | size(WIDTH, EQUAL, 10),
                                                                               text("QTY") | bold | size(WIDTH, EQUAL, 8),
                                                                               text("PENDING") | bold | size(WIDTH, EQUAL, 8)}));
                                                    order_rows.push_back(separator());

                                                    auto orders = Store::getOrderVectorSnapshot();
                                                    int o_start = std::max(0, static_cast<int>(orders.size()) - 15);
                                                    for (int i = o_start; i < static_cast<int>(orders.size()); ++i) {
                                                        const auto &order = orders[i];
                                                        if (order) {
                                                            bool isBuy = order->getIsBuy();
                                                            std::ostringstream ss;
                                                            ss << std::fixed << std::setprecision(2) << order->getPrice();
                                                            order_rows.push_back(hbox({text(std::to_string(order->getOrderId())) | size(WIDTH, EQUAL, 5),
                                                                                       text(order->getUser() ? order->getUser()->getUsername() : "SYSTEM") | size(WIDTH, EQUAL, 10),
                                                                                       text(order->getSymbol()) | size(WIDTH, EQUAL, 8),
                                                                                       text(isBuy ? "BUY" : "SELL") | color(isBuy ? Color::Green : Color::Red) | size(WIDTH, EQUAL, 8),
                                                                                       text(order->getOrderType() == ORDER_TYPE::LIMIT ? "LIMIT" : "MARKET") | size(WIDTH, EQUAL, 8),
                                                                                       text(ss.str()) | size(WIDTH, EQUAL, 10),
                                                                                       text(std::to_string(order->getQty())) | size(WIDTH, EQUAL, 8),
                                                                                       text(std::to_string(order->getPendingQty())) | size(WIDTH, EQUAL, 8)}));
                                                        }
                                                    }

                                                    std::vector<Element> exec_rows;
                                                    exec_rows.push_back(hbox({text("ORDER") | bold | size(WIDTH, EQUAL, 8),
                                                                              text("TICKER") | bold | size(WIDTH, EQUAL, 10),
                                                                              text("QTY") | bold | size(WIDTH, EQUAL, 10),
                                                                              text("TIMESTAMP") | bold | flex}));
                                                    exec_rows.push_back(separator());

                                                    auto executions = Store::getExecutedOrdersSnapshot();
                                                    // Row index 0 is header row {"OrderID", "Symbol", "ExecutedQty", "Timestamp"}
                                                    int e_start = std::max(1, static_cast<int>(executions.size()) - 15);
                                                    for (int i = e_start; i < static_cast<int>(executions.size()); ++i) {
                                                        const auto &row = executions[i];
                                                        if (row.size() >= 4) {
                                                            exec_rows.push_back(hbox({text(row[0]) | size(WIDTH, EQUAL, 8),
                                                                                      text(row[1]) | size(WIDTH, EQUAL, 10),
                                                                                      text(row[2]) | size(WIDTH, EQUAL, 10),
                                                                                      text(row[3]) | flex}));
                                                        }
                                                    }

                                                    return hbox({vbox(order_rows) | flex | border | size(WIDTH, GREATER_THAN, 50),
                                                                 vbox(exec_rows) | flex | border});
                                                })},
                                               &selected_tab);

    auto main_container = Container::Vertical({comp_tab_toggle,
                                               active_tab_container});

    // Auto-refresh loop thread to redraw the screen periodically (e.g. 5 times per sec to update book charts/metrics)
    std::atomic<bool> refresh_active{true};
    std::thread refresh_thread([&]() {
        while (refresh_active) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            screen.PostEvent(Event::Custom);
        }
    });

    auto renderer = Renderer(main_container, [&]() {
        return vbox({text("STOCK RUSH CLI ENGINE - TRANSACTION DASHBOARD") | center | bold | color(Color::Cyan),
                     comp_tab_toggle->Render() | center,
                     separator(),
                     active_tab_container->Render() | flex});
    });

    screen.Loop(renderer);

    refresh_active = false;
    if (refresh_thread.joinable()) {
        refresh_thread.join();
    }
}
