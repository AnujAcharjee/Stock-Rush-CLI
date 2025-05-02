#include "order.hpp"

atomic<long long> Order::_globalOrderCount{0};

Order::Order(const string &symbol, ORDER_TYPE orderType, bool isBuy, int qty, float price, shared_ptr<User> user, int expiryInSeconds)
    : _symbol(symbol), _orderType(orderType), _isBuy(isBuy), _qty(qty), _price(price), _user(user), _executedQty(0),
      _status(ORDER_STATUS::OPEN) {

    auto timeNow = chrono::steady_clock::now();
    chrono::seconds duration(expiryInSeconds);
    _creationTime = timeNow;
    _expiryTime = timeNow + duration;
    _displayExpiryTime = chrono::system_clock::now() + duration;

    _orderId = getGlobalOrderCount();
};

long long Order::getGlobalOrderCount() {
    return ++_globalOrderCount;
}

void Order::updateStatus(ORDER_STATUS status) {
    this->_status.store(status);
}

void Order::updateExecutedQty(int qty) {
    _executedQty += qty;
};

bool Order::getIsBuy() const noexcept {
    return _isBuy;
};
float Order::getPrice() const noexcept {
    return _price;
};

const string &Order::getSymbol() const noexcept {
    return _symbol;
};

long long Order::getOrderId() const noexcept {
    return _orderId;
};

chrono::steady_clock::time_point Order::getCreationTime() const noexcept {
    return _creationTime;
}

chrono::steady_clock::time_point Order::getExpiryTime() const noexcept {
    return _expiryTime;
}

chrono::system_clock::time_point Order::getDisplayExpiryTime() const noexcept {
    return _displayExpiryTime;
}

int Order::getQty() const noexcept {
    return _qty;
}

int Order::getExecutedQty() const noexcept {
    return _executedQty.load();
}

int Order::getPendingQty() const noexcept {
    return (_qty - _executedQty.load());
};

shared_ptr<User> Order::getUser() const noexcept {
    return _user;
}

ORDER_TYPE Order::getOrderType() const noexcept {
    return _orderType;
}

ORDER_STATUS Order::getStatus() const noexcept {
    return _status.load();
}
