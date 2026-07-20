#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <chrono>

#include "order.hpp"
#include "orderExecutioner.hpp"
#include "orderManager.hpp"
#include "store.hpp"
#include "gui.hpp"

#ifdef ENABLE_METRICS
#include "metricsCollector.hpp"
#endif

using namespace std;

void processOrder(OrderManager &orderMgr, shared_ptr<Order> order) {
    if (!order) {
        cerr << "Error: Null Order received in processOrder \n";
        return;
    }

    try {
        orderMgr.placeOrder(order);
    } catch (const exception &e) {
        cerr << "Exception while placing order: " << e.what() << "\n";
    }
}

void placeOrder(OrderManager &orderMgr) {
    string username, symbol;
    int qty;
    bool isBuy;
    float price;
    string orderTypeStr;
    string expiryTypeStr;
    int expirySeconds;

    cout << "\n----- Place a New Order ----- \n";

    cout << "Enter Username: ";
    cin >> username;

    auto user = Store::getUser(username);
    if (!user) {
        cerr << "Error: User '" << username << "' not found. Please create the user first.\n";
        return;
    }

    cout << "Enter Stock Symbol (TCS, BABA, GOOGL, APPLE): ";
    cin >> symbol;

    cout << "Enter Quantity: ";
    cin >> qty;
    while (cin.fail() || qty <= 0) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid quantity. Enter a positive integer: ";
        cin >> qty;
    }

    cout << "Enter Price per Unit: ";
    cin >> price;
    while (cin.fail() || price <= 0.0f) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid price. Enter a positive number: ";
        cin >> price;
    }

    cout << "Is this a Buy Order? (Enter 1 for Buy or 0 for Sell): ";
    cin >> isBuy;
    while (cin.fail()) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid input. Enter 1 for Buy or 0 for Sell: ";
        cin >> isBuy;
    }

    cout << "Enter the Order Type (MARKET or LIMIT): ";
    cin >> orderTypeStr;
    transform(orderTypeStr.begin(), orderTypeStr.end(), orderTypeStr.begin(), ::toupper);

    ORDER_TYPE orderType;
    if (orderTypeStr == "MARKET")
        orderType = ORDER_TYPE::MARKET;
    else if (orderTypeStr == "LIMIT")
        orderType = ORDER_TYPE::LIMIT;
    else {
        cerr << "Invalid order type. Please enter 'MARKET' or 'LIMIT'.\n";
        return;
    }

    if (orderType == ORDER_TYPE::LIMIT) {

        cout << "Enter the Order Expiry Type (DAY, GTC, GTD): ";
        cin >> expiryTypeStr;
        transform(expiryTypeStr.begin(), expiryTypeStr.end(), expiryTypeStr.begin(), ::toupper);

        if (expiryTypeStr == "DAY")
            expirySeconds = 24 * 60 * 60;      
        else if (expiryTypeStr == "GTC")       
            expirySeconds = 30 * 24 * 60 * 60; 
        else if (expiryTypeStr == "GTD") {
            string gtdDateStr;
            cout << "Enter GTD expiry date (DD-MM-YYYY): ";
            cin >> gtdDateStr;

            tm expiryTm = {};
            istringstream iss(gtdDateStr);
            char dash;
            iss >> expiryTm.tm_mday >> dash >> expiryTm.tm_mon >> dash >> expiryTm.tm_year;

            if (iss.fail() || dash != '-' || expiryTm.tm_mday <= 0 || expiryTm.tm_mon <= 0 || expiryTm.tm_year <= 0) {
                cerr << "Invalid date format. Please use DD-MM-YYYY.\n";
                return;
            }

            expiryTm.tm_mon -= 1;    
            expiryTm.tm_year -= 1900;
            expiryTm.tm_hour = 0;
            expiryTm.tm_min = 0;
            expiryTm.tm_sec = 0;

            time_t expiryTimeT = mktime(&expiryTm);
            time_t nowT = time(nullptr);

            if (expiryTimeT == -1 || expiryTimeT <= nowT) {
                cerr << "Expiry date must be in the future.\n";
                return;
            }

            expirySeconds = static_cast<int>(difftime(expiryTimeT, nowT));
        } else {
            cerr << "Invalid expiry type. Please enter 'DAY', 'GTC', or 'GTD'.\n";
            return;
        }
    }

    auto order = make_shared<Order>(symbol, orderType, isBuy, qty, price, user, expirySeconds);
    if (!order) {
        cerr << "Error: Failed to create order.\n";
        return;
    }

    bool isPlaced = orderMgr.placeOrder(order);
    if (isPlaced) {
        cout << "Order placed successfully for user '" << username << "' [" << (isBuy ? "Buy" : "Sell")
             << " " << qty << " of " << symbol << " @ " << price << " Rs ]\n";
    } else {
        cerr << "Failed to place the order.\n";
    }
}

void addUser() {
    string username;

    cout << "Please enter a username to create a new user \n";
    cout << "> ";
    cin >> username;

    while (cin.fail() || username.empty()) {
        cin.clear();                                         
        cin.ignore(numeric_limits<streamsize>::max(), '\n'); 
        cout << "Invalid input. Please enter a valid username: \n";
        cin >> username;
    }

    cout << "Creating user...\n";
    Store::addUser(username);
    cout << "User '" << username << "' has been successfully created \n";
}

void printOrderBook() {
    string symbol;

    cout << "Enter the stock symbol (TCS, APPLE, GOOGL, BABA): ";
    cin >> symbol;

    auto stockPtr = Store::getStock(symbol);
    if (!stockPtr) {
        cerr << "Error: Stock symbol \"" << symbol << "\" not found. Please try again.\n";
        return;
    }

    auto orderBook = stockPtr->getOrderBookInstance();
    if (!orderBook) {
        cerr << "Error: Order book for \"" << symbol << "\" is unavailable.\n";
        return;
    }

    orderBook->printOrderBook();
}

int main() {
    cout << "Starting the application... \n";

    try {
        // Adding Default Users
        Store::addUser("User1");
        Store::addUser("User2");
        Store::addUser("User3");
        Store::addUser("User4");
        Store::addUser("User5");
    } catch (const exception &e) {
        cerr << "Error: in addUser in main" << e.what() << "\n";
    }

    try {
        struct StockInit {
            string symbol;
            float price;
            int qty;
        };
        vector<StockInit> init_stocks = {
            {"AAPL", 225.0f, 10000},
            {"MSFT", 415.0f, 10000},
            {"NVDA", 125.0f, 10000},
            {"GOOGL", 165.0f, 10000},
            {"AMZN", 185.0f, 10000},
            {"META", 495.0f, 10000},
            {"TSLA", 210.0f, 10000},
            {"BRK.B", 445.0f, 10000},
            {"LLY", 810.0f, 10000},
            {"AVGO", 155.0f, 10000},
            {"JPM", 205.0f, 10000},
            {"V", 275.0f, 10000},
            {"UNH", 520.0f, 10000},
            {"XOM", 115.0f, 10000},
            {"MA", 460.0f, 10000},
            {"JNJ", 160.0f, 10000},
            {"PG", 165.0f, 10000},
            {"WMT", 68.0f, 10000},
            {"HD", 360.0f, 10000},
            {"CVX", 155.0f, 10000},
            {"NFLX", 640.0f, 10000},
            {"MRK", 120.0f, 10000},
            {"AMD", 150.0f, 10000},
            {"KO", 65.0f, 10000},
            {"PEP", 165.0f, 10000},
            {"ADBE", 530.0f, 10000},
            {"COST", 810.0f, 10000},
            {"ORCL", 140.0f, 10000},
            {"CRM", 240.0f, 10000},
            {"ACN", 310.0f, 10000},
            {"CSCO", 46.0f, 10000},
            {"MCD", 265.0f, 10000},
            {"INTC", 30.0f, 10000},
            {"CMCSA", 38.0f, 10000},
            {"PFE", 28.0f, 10000},
            {"DIS", 95.0f, 10000},
            {"NKE", 75.0f, 10000},
            {"VZ", 41.0f, 10000},
            {"T", 18.0f, 10000},
            {"IBM", 190.0f, 10000},
            {"QCOM", 175.0f, 10000},
            {"HON", 200.0f, 10000},
            {"BA", 170.0f, 10000},
            {"CAT", 340.0f, 10000},
            {"GS", 480.0f, 10000},
            {"MS", 98.0f, 10000},
            {"AMGN", 315.0f, 10000},
            {"UNP", 230.0f, 10000},
            {"SBUX", 78.0f, 10000},
            {"LOW", 235.0f, 10000},
            {"ISRG", 430.0f, 10000},
            {"GE", 165.0f, 10000},
            {"UBER", 70.0f, 10000},
            {"ABNB", 145.0f, 10000},
            {"BKNG", 3900.0f, 10000},
            {"BX", 135.0f, 10000},
            {"TGT", 145.0f, 10000},
            {"F", 11.0f, 10000},
            {"GM", 45.0f, 10000},
            {"FDX", 290.0f, 10000},
            {"UPS", 135.0f, 10000},
            {"PYPL", 62.0f, 10000},
            {"SQ", 65.0f, 10000},
            {"SHOP", 68.0f, 10000},
            {"SPOT", 310.0f, 10000},
            {"ZM", 60.0f, 10000},
            {"SNOW", 125.0f, 10000},
            {"PLTR", 28.0f, 10000},
            {"CRWD", 260.0f, 10000},
            {"PANW", 330.0f, 10000},
            {"DDOG", 115.0f, 10000},
            {"COIN", 220.0f, 10000},
            {"HOOD", 22.0f, 10000},
            {"SNAP", 10.0f, 10000},
            {"PINS", 38.0f, 10000},
            {"TWLO", 58.0f, 10000},
            {"U", 18.0f, 10000},
            {"RBLX", 36.0f, 10000},
            {"ETSY", 60.0f, 10000},
            {"EBAY", 52.0f, 10000},
            {"HPQ", 35.0f, 10000},
            {"DELL", 130.0f, 10000},
            {"NTAP", 120.0f, 10000},
            {"STX", 98.0f, 10000},
            {"WDC", 72.0f, 10000},
            {"MU", 115.0f, 10000},
            {"AMAT", 220.0f, 10000},
            {"LRCX", 950.0f, 10000},
            {"ASML", 920.0f, 10000},
            {"TSM", 165.0f, 10000},
            {"SONY", 82.0f, 10000},
            {"NTDOY", 13.0f, 10000},
            {"TM", 205.0f, 10000},
            {"HMC", 32.0f, 10000},
            {"SHEL", 72.0f, 10000},
            {"BP", 35.0f, 10000},
            {"TTE", 68.0f, 10000},
            {"TCS", 999.0f, 10000},
            {"RELIANCE", 1250.0f, 10000},
            {"BABA", 777.0f, 10000}
        };

        for (const auto& s : init_stocks) {
            Store::addStocks(s.symbol, s.price, s.qty);
        }
    } catch (const exception &e) {
        cerr << "Error: in addStocks in main: " << e.what() << "\n";
    }

    OrderExecutioner::getExecutionerInstance();
    OrderManager &orderMgr = OrderManager::getOrderManagerInstance();

    // Get user from store - default User added
    auto user1 = Store::getUser("User1");
    auto user2 = Store::getUser("User2");
    auto user3 = Store::getUser("User3");
    auto user4 = Store::getUser("User4");
    auto user5 = Store::getUser("User5");
    if (!user1 || !user2 || !user3 || !user4 || !user5) {
        cerr << "Error: User not found! \n";
        return -1;
    }

    // Create Default Orders here - (stock, orderType, isBuy, order_price_pre_stock, user_obj, expiry_time)
   shared_ptr<Order> order1 = make_shared<Order>("TCS", ORDER_TYPE::LIMIT, true, 1, 993.0f, user1, 24 * 60 * 60);
    shared_ptr<Order> order2 = make_shared<Order>("AAPL", ORDER_TYPE::LIMIT, true, 2, 999.0f, user2, 24 * 60 * 60);
    shared_ptr<Order> order3 = make_shared<Order>("GOOGL", ORDER_TYPE::LIMIT, true, 3, 999.0f, user4, 24 * 60 * 60);
    shared_ptr<Order> order4 = make_shared<Order>("BABA", ORDER_TYPE::LIMIT, true, 4, 999.0f, user5, 24 * 60 * 60);
    shared_ptr<Order> order5 = make_shared<Order>("TCS", ORDER_TYPE::LIMIT, true, 2, 998.0f, user1, 24 * 60 * 60);
    shared_ptr<Order> order6 = make_shared<Order>("TCS", ORDER_TYPE::LIMIT, true, 2, 900.0f, user3, 24 * 60 * 60);

    // Create threads - place Default orders from
    vector<thread> userThreads;
    userThreads.emplace_back(processOrder, ref(orderMgr), order1);
    userThreads.emplace_back(processOrder, ref(orderMgr), order2);
    userThreads.emplace_back(processOrder, ref(orderMgr), order3);
    userThreads.emplace_back(processOrder, ref(orderMgr), order4);
    userThreads.emplace_back(processOrder, ref(orderMgr), order5);
    userThreads.emplace_back(processOrder, ref(orderMgr), order6);

    cout << "-->> Placing concurrent system orders \n";
    for (auto &thread : userThreads) {
        thread.join();
    }
    cout << "-->> All system orders placed \n";

    // Launch interactive fullscreen FTXUI TUI dashboard
    launchTUI(orderMgr);

    cout << "Closing application... \n";
    return 0;
}