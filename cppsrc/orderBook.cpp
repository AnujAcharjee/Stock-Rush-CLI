#include "orderBook.hpp"

OrderBook::OrderBook(const string &symbol) : _symbol(symbol) {
}

void OrderBook::setInOrderBookForSell(shared_ptr<Order> orderPtr) {
    if (!orderPtr) {
        cout << "Error: null orderPtr in sellAllCreatedStocks \n";
        return;
    }

    if (_sellOrderBook.empty() && orderPtr->getOrderType() == ORDER_TYPE::SYSTEM) {
        auto &orderQueue = _sellOrderBook[orderPtr->getPrice()];
        orderQueue.push(orderPtr);
    }
}

template <typename OppositeOrderBook, typename SameOrderBook, typename CompareFunc>
vector<pair<shared_ptr<Order>, int>> OrderBook::matchOrder_Template(OppositeOrderBook &op_book, SameOrderBook &sm_book,
                                                                    shared_ptr<Order> orderPtr, CompareFunc comp) {
    vector<pair<shared_ptr<Order>, int>> accumulatedOrders;
    int targetQty = orderPtr->getPendingQty();
    float lastMatchPrice = 0;
    int matchedQty = 0;

    auto it = op_book.begin();
    while (it != op_book.end() && comp(it->first, orderPtr->getPrice()) && matchedQty < targetQty) {
        auto &orderQueue = it->second;

        while (!orderQueue.empty() && matchedQty < targetQty) {
            auto q_orderPtr = orderQueue.front();

            if (q_orderPtr->getExpiryTime() < chrono::steady_clock::now() && q_orderPtr->getOrderType() != ORDER_TYPE::SYSTEM) {
                orderQueue.pop();
                continue;
            }

            int availableQty = q_orderPtr->getPendingQty();
            int tradeQty = min(availableQty, targetQty - matchedQty);

            accumulatedOrders.emplace_back(q_orderPtr, tradeQty);

            q_orderPtr->updateExecutedQty(tradeQty);
            matchedQty += tradeQty;
            lastMatchPrice = it->first;

            if (availableQty == tradeQty)
                orderQueue.pop();
        }

        if (orderQueue.empty()) {
            it = op_book.erase(it);
        } else
            ++it;
    }

    if (matchedQty != 0)
        Store::getStock(orderPtr->getSymbol())->updatePrice(lastMatchPrice);

    if (matchedQty < targetQty && orderPtr->getOrderType() == ORDER_TYPE::LIMIT) {
        auto it_sm = sm_book.find(orderPtr->getPrice());
        if (it_sm == sm_book.end()) {
            auto &orderQueue = sm_book[orderPtr->getPrice()];
            orderQueue.push(orderPtr);
        } else {
            it_sm->second.push(orderPtr);
        }
    }

    if (matchedQty == 0)
        return accumulatedOrders;

    accumulatedOrders.emplace_back(orderPtr, matchedQty);
    orderPtr->updateExecutedQty(matchedQty);

    return accumulatedOrders;
}

vector<pair<shared_ptr<Order>, int>> OrderBook::matchOrder_OrderBook(shared_ptr<Order> orderPtr) {
    if (orderPtr->getIsBuy()) {
        return matchOrder_Template(_sellOrderBook, _buyOrderBook, orderPtr,
                                   [](float bookPrice, float orderPrice) {
                                       return bookPrice <= orderPrice;
                                   });
    } else {
        return matchOrder_Template(_buyOrderBook, _sellOrderBook, orderPtr,
                                   [](float bookPrice, float orderPrice) {
                                       return bookPrice >= orderPrice;
                                   });
    }
}

void OrderBook::rollbackMatch(const vector<pair<shared_ptr<Order>, int>> &accumulatedOrders) {
    if (accumulatedOrders.empty())
        return;

    auto [primaryOrder, primaryMatchedQty] = accumulatedOrders.back();

    // 1. Remove primary order from same-side book if it was added as a resting limit remainder
    if (primaryOrder && primaryOrder->getOrderType() == ORDER_TYPE::LIMIT) {
        auto removeRestingOrder = [&](auto &book) {
            auto it = book.find(primaryOrder->getPrice());
            if (it != book.end()) {
                queue<shared_ptr<Order>> filteredQ;
                while (!it->second.empty()) {
                    auto o = it->second.front();
                    it->second.pop();
                    if (o != primaryOrder) {
                        filteredQ.push(o);
                    }
                }
                if (filteredQ.empty()) {
                    book.erase(it);
                } else {
                    it->second = move(filteredQ);
                }
            }
        };

        if (primaryOrder->getIsBuy()) {
            removeRestingOrder(_buyOrderBook);
        } else {
            removeRestingOrder(_sellOrderBook);
        }
    }

    if (primaryOrder) {
        primaryOrder->updateExecutedQty(-primaryMatchedQty);
        if (primaryOrder->getStatus() == ORDER_STATUS::EXECUTED) {
            primaryOrder->updateStatus(ORDER_STATUS::OPEN);
        }
    }

    // 2. Restore all counterparty orders (elements 0 to size - 2)
    for (size_t i = 0; i + 1 < accumulatedOrders.size(); ++i) {
        const auto &[matchedOrder, qty] = accumulatedOrders[i];
        if (!matchedOrder)
            continue;

        matchedOrder->updateExecutedQty(-qty);
        if (matchedOrder->getStatus() == ORDER_STATUS::EXECUTED) {
            matchedOrder->updateStatus(ORDER_STATUS::OPEN);
        }

        if (matchedOrder->getIsBuy()) {
            _buyOrderBook[matchedOrder->getPrice()].push(matchedOrder);
        } else {
            _sellOrderBook[matchedOrder->getPrice()].push(matchedOrder);
        }
    }
}

void OrderBook::printOrderBook() const {
    auto createDisplayRows = [](const auto &orderBook) -> vector<pair<float, int>> {
        vector<pair<float, int>> displayRows;
        for (const auto &[price, q] : orderBook) {
            int totalStocks = 0;
            auto orderQueue = q;
            while (!orderQueue.empty()) {
                auto order = orderQueue.front();
                orderQueue.pop();
                if (!order) {
                    throw runtime_error("Warning: null order encountered in OrderBook while printing");
                }
                totalStocks += order->getPendingQty();
            }
            displayRows.emplace_back(price, totalStocks);
        }
        return displayRows;
    };

    cout << "\n======================= Order Book for Stock: " << _symbol << " ====================\n";
    cout << left
         << setw(12) << "Qty"
         << setw(15) << "Bid "
         << " | " << right
         << setw(15) << "Ask"
         << setw(12) << "Qty" << '\n';

    cout << string(70, '-') << '\n';

    auto buyRows = createDisplayRows(_buyOrderBook);
    auto sellRows = createDisplayRows(_sellOrderBook);

    size_t maxRows = max(buyRows.size(), sellRows.size());
    for (size_t i = 0; i < maxRows; ++i) {
        if (i < buyRows.size())
            cout << left << setw(12) << buyRows[i].second
                 << setw(15) << fixed << setprecision(2) << buyRows[i].first;
        else
            cout << left << setw(12) << " " << setw(15) << " ";

        cout << " | ";

        if (i < sellRows.size())
            cout << right << setw(15) << fixed << setprecision(2) << sellRows[i].first
                 << setw(12) << sellRows[i].second << '\n';
        else
            cout << right << setw(15) << " " << setw(12) << " " << '\n';
    }

    cout << string(70, '=') << '\n';
}

vector<pair<float, int>> OrderBook::getAggregatedBuyLevels() const {
    vector<pair<float, int>> levels;
    for (const auto &[price, q] : _buyOrderBook) {
        int totalQty = 0;
        auto qCopy = q;
        while (!qCopy.empty()) {
            if (qCopy.front())
                totalQty += qCopy.front()->getPendingQty();
            qCopy.pop();
        }
        if (totalQty > 0)
            levels.emplace_back(price, totalQty);
    }
    return levels;
}

vector<pair<float, int>> OrderBook::getAggregatedSellLevels() const {
    vector<pair<float, int>> levels;
    for (const auto &[price, q] : _sellOrderBook) {
        int totalQty = 0;
        auto qCopy = q;
        while (!qCopy.empty()) {
            if (qCopy.front())
                totalQty += qCopy.front()->getPendingQty();
            qCopy.pop();
        }
        if (totalQty > 0)
            levels.emplace_back(price, totalQty);
    }
    return levels;
}

OrderBook::~OrderBook() {};