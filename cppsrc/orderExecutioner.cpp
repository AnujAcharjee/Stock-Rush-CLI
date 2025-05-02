#include "orderExecutioner.hpp"

thread OrderExecutioner::_buyProcessingThread;
thread OrderExecutioner::_sellProcessingThread;
atomic<bool> OrderExecutioner::_isBuyProcessing{true};
atomic<bool> OrderExecutioner::_isSellProcessing{true};

unique_ptr<ThreadPool> OrderExecutioner::threadPool = make_unique<ThreadPool>(4);

condition_variable OrderExecutioner::CV_threadPool;

mutex OrderExecutioner::Mtx_buyThread;
mutex OrderExecutioner::Mtx_sellThread;

StockLock &OrderExecutioner::_lockManager = StockLock::getInstance();
OrderManager &OrderExecutioner::_manager = OrderManager::getOrderManagerInstance();

OrderExecutioner::OrderExecutioner() {
    checkExecutionQueue<_sellProcessingThread, _isSellProcessing>(_manager._SellExecutionQueue, _manager.CV_sellProcessing, "sell");
    checkExecutionQueue<_buyProcessingThread, _isBuyProcessing>(_manager._BuyExecutionQueue, _manager.CV_buyProcessing, "buy");
}

OrderExecutioner &OrderExecutioner::getExecutionerInstance() {
    static OrderExecutioner instance;
    return instance;
}

template <thread &processingThread, atomic<bool> &isProcessing, typename QueueType, typename CVType>
void OrderExecutioner::checkExecutionQueue(QueueType &executionQueue, CVType &cv, const string &queueType) {
    if (processingThread.joinable()) {
        return;
    }

    processingThread = thread([this, &executionQueue, &cv, queueType]() {
        try {
            while (isProcessing) {
                unique_lock<mutex> processingThreadLock(queueType == "buy" ? Mtx_buyThread : Mtx_sellThread);

                cv.wait(processingThreadLock, [&]() {
                    return !executionQueue.empty() || !isProcessing;
                });

                if (!isProcessing)
                    break;

                else if (!executionQueue.empty()) {
                    shared_ptr<Order> pop_order;

                    try {
                        pop_order = executionQueue.top();
                        executionQueue.pop();
                    } catch (const exception &e) {
                        cerr << "Error popping from " << queueType << " execution queue: " << e.what() << "\n";
                        continue;
                    }

                    if (pop_order) {
                        try {
                            auto symbol = pop_order->getSymbol();
                            threadPool->enqueue([this, order = pop_order, symbol]() {
                                try {
                                    executeOrder(order);
                                } catch (const exception &e) {
                                    cerr << "Error executing order " << order->getOrderId() << ": " << e.what() << "\n";
                                } catch (...) {
                                    cerr << "Unknown error while executing order " << order->getOrderId() << "\n";
                                }
                            });
                        } catch (const exception &e) {
                            cerr << "Error enqueuing " << queueType << " order: " << e.what() << "\n";
                        }
                    }
                }
            }
        } catch (const exception &e) {
            cerr << "Fatal error in " << queueType << " execution thread: " << e.what() << "\n";
        } catch (...) {
            cerr << "Unknown fatal error in " << queueType << " execution thread!" << "\n";
        }
    });
}

void OrderExecutioner::executeOrder(shared_ptr<Order> orderPtr) {
    try {
        if (!orderPtr)
            throw runtime_error("Cannot execute null order");

        auto symbol = orderPtr->getSymbol();

        auto stock = Store::getStock(symbol);
        if (!stock) {
            throw runtime_error("executeOrder failed: Stock not found for symbol " + symbol);
        }

        auto orderBook = stock->getOrderBookInstance();
        if (!orderBook) {
            throw runtime_error("executeOrder failed: OrderBook not found for symbol " + symbol);
        }

        vector<pair<shared_ptr<Order>, int>> accumulatedOrders;

        try {
            auto mutex_ptr = _lockManager.acquireLock(symbol);
            if (!mutex_ptr) {
                throw runtime_error("Failed to acquire stock lock for " + symbol);
            }

            unique_lock<mutex> stockLock(*mutex_ptr);

            accumulatedOrders = orderBook->matchOrder_OrderBook(orderPtr);
            if (accumulatedOrders.empty())
                return;

        } catch (const exception &e) {
            cerr << "Error executing order " << orderPtr->getOrderId() << ": " << e.what() << "\n";
            return;
        }

        float tradeAmount = 0.0f;

        for (size_t i = 0; i + 1 < accumulatedOrders.size(); ++i) {
            const auto &[matchedOrder, matchedQty] = accumulatedOrders[i];

            if (matchedOrder->getPendingQty() == 0 && matchedOrder->getStatus() == ORDER_STATUS::OPEN)
                matchedOrder->updateStatus(ORDER_STATUS::EXECUTED);

            float price = matchedOrder->getPrice() * matchedQty;
            tradeAmount += price;

            auto user = matchedOrder->getUser();
            string m_order_symbol = matchedOrder->getSymbol();

            if (!user && matchedOrder->getOrderType() != ORDER_TYPE::SYSTEM) {
                throw runtime_error("Error: User is null in matched Order");
                return;
            }

            if (user) {
                if (matchedOrder->getIsBuy()) {
                    user->deductFunds(price);
                    cout << "Fund deducted \n";
                    user->addToDemat(m_order_symbol, matchedQty);
                    cout << "Stock added to demat \n";
                } else {
                    user->addFunds(price);
                    user->deductDemat(m_order_symbol, matchedQty);
                }
            }

            Store::addExecutedOrder(matchedOrder, matchedQty);
        }

        int matchedQty = accumulatedOrders.back().second;

        if (orderPtr->getPendingQty() == 0 && orderPtr->getStatus() == ORDER_STATUS::OPEN)
            orderPtr->updateStatus(ORDER_STATUS::EXECUTED);

        auto p_order_user = orderPtr->getUser();
        if (!p_order_user)
            throw runtime_error("Processing Order user is undefined");
        string p_order_symbol = orderPtr->getSymbol();

        if (orderPtr->getIsBuy()) {
            p_order_user->deductFunds(tradeAmount);
            p_order_user->addToDemat(p_order_symbol, matchedQty);
        } else {
            p_order_user->addFunds(tradeAmount);
            p_order_user->deductDemat(p_order_symbol, matchedQty);
        }

        Store::addExecutedOrder(orderPtr, matchedQty);
    } catch (const exception &e) {
        cerr << "Error executing order: " << e.what() << "\n";
    }
}

OrderExecutioner::~OrderExecutioner() {

    _isBuyProcessing = false;
    _isSellProcessing = false;

    _manager.CV_buyProcessing.notify_all();
    _manager.CV_sellProcessing.notify_all();

    if (_buyProcessingThread.joinable()) {
        _buyProcessingThread.join();
    }

    if (_sellProcessingThread.joinable()) {
        _sellProcessingThread.join();
    }

    if (threadPool) {
        threadPool.reset();
    }
}
