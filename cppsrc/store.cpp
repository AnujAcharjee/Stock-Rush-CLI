#include "store.hpp"

unordered_map<string, shared_ptr<User>> Store::_usersMap;
unordered_map<string, shared_ptr<Stock>> Store::_stocksMap;

vector<shared_ptr<Order>> Store::_orderVtr;
vector<vector<string>> Store::_executedOrdersVtr = {{"OrderID", "Symbol", "ExecutedQty", "Timestamp"}};

mutex Store::userMtx;
mutex Store::stockMtx;
mutex Store::orderMtx;
mutex Store::executionQueueMtx;
mutex Store::executedOrdersMtx;

void Store::addUser(const string &username) {
    scoped_lock lock(userMtx);
    const float fund = 10000.0f;
    auto [it, inserted] = _usersMap.try_emplace(username, make_shared<User>(username, fund));
    if (!inserted) {
        cerr << "User already exists. Try another name." << '\n';
    }
}

void Store::addStocks(const string &symbol, float price, int qty) {
    scoped_lock lock(stockMtx);
    auto [it, inserted] = _stocksMap.try_emplace(symbol, make_shared<Stock>(symbol, price, qty));
    if (!inserted) {
        cout << "Stock already exists. Cannot overwrite!" << '\n';
    }
}

void Store::addOrder(shared_ptr<Order> orderPtr) {
    scoped_lock lock(orderMtx);
    _orderVtr.push_back(orderPtr);
}

void Store::addExecutedOrder(shared_ptr<Order> orderPtr, int matchedQty) {
    scoped_lock lock(executedOrdersMtx);

    time_t now = time(nullptr);
    string timeStr = ctime(&now);
    timeStr.pop_back();

    _executedOrdersVtr.push_back({to_string(orderPtr->getOrderId()), orderPtr->getSymbol(), to_string(matchedQty), timeStr});
}

shared_ptr<User> Store::getUser(const string &username) {
    scoped_lock lock(userMtx);
    auto it = _usersMap.find(username);
    return (it != _usersMap.end()) ? it->second : nullptr;
}

shared_ptr<Stock> Store::getStock(const string &symbol) {
    scoped_lock lock(stockMtx);
    auto it = _stocksMap.find(symbol);
    return (it != _stocksMap.end()) ? it->second : nullptr;
}

void Store::printUsers() {
    scoped_lock lock(userMtx);
    cout << "------------------ Users ------------------\n";

    if (_usersMap.empty()) {
        cout << "No users found!\n";
        return;
    }

    cout << left << setw(15) << "Username"
         << setw(15) << "Funds"
         << setw(15) << "Demat Holdings\n";
    cout << string(50, '-') << '\n';

    for (const auto &[username, user] : _usersMap) {
        cout << left << setw(15) << username
             << setw(15) << fixed << setprecision(2) << user->getFunds();

        const auto &demat = user->getDemat();
        if (demat.empty()) {
            cout << "None";
        } else {
            bool first = true;
            for (const auto &[symbol, qty] : demat) {
                if (!first)
                    cout << ", ";
                cout << symbol << ":" << qty;
                first = false;
            }
        }

        cout << '\n';
    }

    cout << string(50, '-') << '\n';
}

void Store::printStocks() {
    scoped_lock lock(stockMtx);
    cout << "------------------- Stocks ---------------------\n";

    if (_stocksMap.empty()) {
        cout << "No stocks found!\n";
        return;
    }

    cout << left
         << setw(15) << "Symbol"
         << setw(18) << "Total Quantity"
         << setw(15) << "Current Price"
         << '\n';

    cout << string(48, '-') << '\n';

    for (const auto &[symbol, stock] : _stocksMap) {
        cout << left
             << setw(15) << stock->getSymbol()
             << setw(18) << stock->getQty()
             << setw(15) << fixed << setprecision(2) << stock->getPrice()
             << '\n';
    }

    cout << string(48, '-') << '\n';
}

void Store::printOrderVector() {
    cout << "----- All Orders made ----- \n";

    if (_orderVtr.empty()) {
        cout << "No orders found! \n";
        return; 
    }

    // Header
    cout << left
         << setw(15) << "OrderId"
         << setw(10) << "Symbol"
         << setw(10) << "Qty"
         << setw(10) << "Price"
         << setw(10) << "Trade"
         << setw(10) << "Type"
         << setw(25) << "Expiry"
         << setw(15) << "ExecutedQty"
         << setw(15) << "Username"
         << '\n';

    cout << string(135, '-') << '\n';

    for (const auto &order : _orderVtr) {
        if (!order) {
            throw runtime_error("Error: Null order pointer encountered");
        }

        shared_ptr<User> user = order->getUser();
        string username = (user ? user->getUsername() : "SYSTEM");

        time_t t = chrono::system_clock::to_time_t(order->getDisplayExpiryTime());

        string orderType;
        switch (order->getOrderType()) {
            case ORDER_TYPE::MARKET:
                orderType = "MARKET";
                break;
            case ORDER_TYPE::LIMIT:
                orderType = "LIMIT";
                break;
            default:
                orderType = "UNKNOWN";
                break;
        }

        // Row
        cout << left
             << setw(15) << order->getOrderId()
             << setw(10) << order->getSymbol()
             << setw(10) << order->getQty()
             << setw(10) << order->getPrice()
             << setw(10) << (order->getIsBuy() ? "BUY" : "SELL")
             << setw(10) << orderType;

        if (t) {
            cout << put_time(localtime(&t), "%d-%m-%Y %H:%M:%S"); 
            cout << setw(6) << " ";                               
        } else {
            cout << setw(25) << " ";
        }

        cout << setw(15) << order->getExecutedQty()
             << setw(15) << username
             << '\n';
    }

    cout << string(135, '-') << '\n';
}

void Store::printExecutedOrders() {
    cout << "----------- All Executed Orders -----------\n";

    if (_executedOrdersVtr.size() <= 1) {
        cout << "No data found! \n";
        return;
    }

    const auto &headerVtr = _executedOrdersVtr[0];
    for (const auto &header : headerVtr) {
        cout << left << setw(15) << header;
    }
    cout << '\n';
    cout << string(90, '-') << '\n';

    for (size_t i = 1; i < _executedOrdersVtr.size(); i++) {
        for (const auto &j : _executedOrdersVtr[i]) {
            cout << left << setw(15) << j;
        }
        cout << '\n';
    }
    cout << '\n';

    cout << string(90, '-') << '\n';
}
