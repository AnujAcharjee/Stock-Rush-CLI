#pragma once
#include <chrono>
#include <iomanip>
#include <memory>
#include <mutex>
#include <numeric>
#include <unordered_map>
#include <vector>

#include "common.hpp"
#include "stock.hpp"
#include "user.hpp"

class Stock;
class User;

class Store {
    static unordered_map<string, shared_ptr<User>> _usersMap;
    static unordered_map<string, shared_ptr<Stock>> _stocksMap;

    static vector<shared_ptr<Order>> _orderVtr;
    static vector<vector<string>> _executedOrdersVtr;

    static mutex userMtx;
    static mutex stockMtx;
    static mutex orderMtx;
    static mutex executionQueueMtx;
    static mutex executedOrdersMtx;

  public:
    static void addUser(const string &username);
    static void addStocks(const string &symbol, float price, int qty);
    static void addOrder(shared_ptr<Order> orderPtr);
    static void addExecutedOrder(shared_ptr<Order> orderPtr, int matchedQty);

    static shared_ptr<User> getUser(const string &username);
    static shared_ptr<Stock> getStock(const string &symbol);

    static void printUsers();
    static void printStocks();
    static void printOrderVector();
    static void printExecutedOrders();
};