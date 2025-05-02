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
            if (it->first == orderPtr->getPrice())
                it->second.push(orderPtr);
            else
                it = op_book.erase(it);
        } else
            ++it;
    }

    if (matchedQty != 0)
        Store::getStock(orderPtr->getSymbol())->updatePrice(lastMatchPrice);

    if (matchedQty < targetQty && orderPtr->getOrderType() == ORDER_TYPE::LIMIT) 
    {
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

OrderBook::~OrderBook() {};