#pragma once
#include <iomanip>
#include <map>
#include <mutex>
#include <queue>
#include <utility>
#include <vector>

#include "common.hpp"
#include "order.hpp"
#include "store.hpp"

class OrderBook {
    string _symbol;
    map<float, queue<shared_ptr<Order>>, greater<float>> _buyOrderBook; // descending order
    map<float, queue<shared_ptr<Order>>> _sellOrderBook;                // ascending order

    template <typename OppositeOrderBook, typename SameOrderBook, typename CompareFunc>
    vector<pair<shared_ptr<Order>, int>> matchOrder_Template(OppositeOrderBook &op_book, SameOrderBook &sm_book,
                                                              shared_ptr<Order> orderPtr, CompareFunc comp);

  public:
    OrderBook(const string &symbol);
    ~OrderBook();

    vector<pair<shared_ptr<Order>, int>> matchOrder_OrderBook(shared_ptr<Order> orderPtr);

    void setInOrderBookForSell(shared_ptr<Order> orderPtr);
    void printOrderBook() const;
};
