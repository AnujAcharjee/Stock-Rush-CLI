#include "stock.hpp"

Stock::Stock(const string &symbol, float price, int qty) : _symbol(symbol), _price(price), _qty(qty) {
    _orderBookInstance = make_shared<OrderBook>(symbol);

    // Add stock lock
    StockLock::getInstance().createLock(symbol);

    // Add all the stocks in the order book for sell
    shared_ptr<Order> order = make_shared<Order>(symbol,ORDER_TYPE::SYSTEM, false, qty, price, nullptr, 0);
    _orderBookInstance->setInOrderBookForSell(order);
}

string Stock::getSymbol() const noexcept {
    return _symbol;
}

float Stock::getPrice() const noexcept {
    return _price;
}

int Stock::getQty() const noexcept {
    return _qty;
}

shared_ptr<OrderBook> Stock::getOrderBookInstance() {
    return _orderBookInstance;
}

void Stock::updatePrice(const int newPrice) {
    _price.store(newPrice, std::memory_order_relaxed);
}

Stock::~Stock() {
}