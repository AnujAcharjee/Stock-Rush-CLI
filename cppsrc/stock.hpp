#pragma once
#include <mutex>
#include <atomic>

#include "common.hpp"
#include "orderBook.hpp"
#include "order.hpp"
#include "stockLock.hpp"

class OrderBook;

class Stock {
  private:
    const string _symbol;
    atomic<float> _price; // last match price
    int _qty;

    shared_ptr<OrderBook> _orderBookInstance;

  public:
    Stock(const string &symbol, float price, int qty);
    ~Stock();

    string getSymbol() const noexcept;
    float getPrice() const noexcept;
    int getQty() const noexcept;

    void updatePrice(const int newPrice);

    shared_ptr<OrderBook> getOrderBookInstance();
};