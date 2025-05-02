#pragma once
#include <chrono>
#include <mutex>
#include <atomic>

#include "common.hpp"
#include "user.hpp"

class Order {
    long long _orderId;
    string _symbol;
    ORDER_TYPE _orderType;
    bool _isBuy;
    int _qty;
    float _price;
    shared_ptr<User> _user;
    atomic<int> _executedQty;
    atomic<ORDER_STATUS> _status;
    chrono::steady_clock::time_point _creationTime;
    chrono::steady_clock::time_point _expiryTime;
    chrono::system_clock::time_point _displayExpiryTime;

    static atomic<long long> _globalOrderCount;
    long long getGlobalOrderCount();

  public:
    Order(const string &symbol, ORDER_TYPE orderType, bool isBuy, int qty, float price, shared_ptr<User> user, int expiryInSeconds);

    void updateStatus(ORDER_STATUS status);
    void updateExecutedQty(int qty);

    bool getIsBuy() const noexcept;
    float getPrice() const noexcept;
    int getPendingQty() const noexcept;
    const string &getSymbol() const noexcept;
    long long getOrderId() const noexcept;
    chrono::steady_clock::time_point getCreationTime() const noexcept;
    chrono::steady_clock::time_point getExpiryTime() const noexcept;
    chrono::system_clock::time_point getDisplayExpiryTime() const noexcept;
    int getQty() const noexcept;
    int getExecutedQty() const noexcept;
    shared_ptr<User> getUser() const noexcept;
    ORDER_TYPE getOrderType() const noexcept;
    ORDER_STATUS getStatus() const noexcept;
};
