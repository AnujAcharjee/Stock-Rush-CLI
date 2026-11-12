#include "orderExecutioner.hpp"

#ifdef ENABLE_METRICS
#include "metricsCollector.hpp"
#endif

thread OrderExecutioner::_buyProcessingThread;
thread OrderExecutioner::_sellProcessingThread;
atomic<bool> OrderExecutioner::_isBuyProcessing{true};
atomic<bool> OrderExecutioner::_isSellProcessing{true};

const size_t OrderExecutioner::THREAD_POOL_SIZE = std::thread::hardware_concurrency() > 4 ? std::thread::hardware_concurrency() : 4;
unique_ptr<ThreadPool> OrderExecutioner::threadPool = make_unique<ThreadPool>(OrderExecutioner::THREAD_POOL_SIZE);

condition_variable OrderExecutioner::CV_threadPool;

mutex OrderExecutioner::Mtx_buyThread;
mutex OrderExecutioner::Mtx_sellThread;

StockLock &OrderExecutioner::_lockManager = StockLock::getInstance();
OrderManager &OrderExecutioner::_manager = OrderManager::getOrderManagerInstance();

OrderExecutioner::OrderExecutioner() {
#ifdef ENABLE_METRICS
    MetricsCollector::getInstance().setWorkerThreadCount(getThreadPoolCount());
#endif
    checkExecutionQueue<_sellProcessingThread, _isSellProcessing>(_manager._SellExecutionQueue, _manager.CV_sellProcessing, "sell");
    checkExecutionQueue<_buyProcessingThread, _isBuyProcessing>(_manager._BuyExecutionQueue, _manager.CV_buyProcessing, "buy");
}

OrderExecutioner &OrderExecutioner::getExecutionerInstance() {
    static OrderExecutioner instance;
    return instance;
}

size_t OrderExecutioner::getThreadPoolCount() {
    return threadPool ? threadPool->getThreadCount() : 0;
}

template <thread &processingThread, atomic<bool> &isProcessing, typename QueueType, typename CVType>
void OrderExecutioner::checkExecutionQueue(QueueType &executionQueue, CVType &cv, const string &queueType) {
    if (processingThread.joinable()) {
        return;
    }

    processingThread = thread([this, &executionQueue, &cv, queueType]() {
        try {
            while (isProcessing) {
                unique_lock<mutex> processingThreadLock(queueType == "buy" ? OrderManager::Mtx_buyQ : OrderManager::Mtx_sellQ);

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
#ifdef ENABLE_METRICS
                        get_tl_metrics().addOrder();
                        if (executionQueue.empty()) {
                            get_tl_metrics().flush();
                        }
#endif
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

        auto mutex_ptr = _lockManager.acquireLock(symbol);
        if (!mutex_ptr) {
            throw runtime_error("Failed to acquire stock lock for " + symbol);
        }

        unique_lock<mutex> stockLock(*mutex_ptr);

        vector<pair<shared_ptr<Order>, int>> accumulatedOrders;

#ifdef ENABLE_METRICS
        auto start = std::chrono::steady_clock::now();
#endif
        accumulatedOrders = orderBook->matchOrder_OrderBook(orderPtr);
#ifdef ENABLE_METRICS
        if (!accumulatedOrders.empty()) {
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            get_tl_metrics().addMatch(duration);
        }
#endif
        if (accumulatedOrders.empty()) {
#ifdef ENABLE_METRICS
            get_tl_metrics().flush();
#endif
            return;
        }

        float tradeAmount = 0.0f;

        // Track completed settlements for rollback
        struct Settlement {
            shared_ptr<User> user;
            string symbol;
            int qty;
            float price;
            bool isBuy;
            float reservedAmount;
        };
        vector<Settlement> completedSettlements;

        auto rollbackCompletedSettlements = [&]() {
            for (auto it = completedSettlements.rbegin(); it != completedSettlements.rend(); ++it) {
                try {
                    if (it->isBuy) {
                        it->user->addFunds(it->price);
                        it->user->reserveFunds(it->reservedAmount);
                        it->user->deductDemat(it->symbol, it->qty);
                    } else {
                        it->user->deductFunds(it->price);
                        it->user->addToDemat(it->symbol, it->qty);
                        it->user->reserveDemat(it->symbol, it->qty);
                    }
                } catch (...) {
                    cerr << "Critical: Rollback failed for user " << it->user->getUsername() << "\n";
                }
            }
#ifdef ENABLE_METRICS
            get_tl_metrics().flush();
#endif
        };

        for (size_t i = 0; i + 1 < accumulatedOrders.size(); ++i) {
            const auto &[matchedOrder, matchedQty] = accumulatedOrders[i];

            if (matchedOrder->getPendingQty() == 0 && matchedOrder->getStatus() == ORDER_STATUS::OPEN)
                matchedOrder->updateStatus(ORDER_STATUS::EXECUTED);

            float price = matchedOrder->getPrice() * matchedQty;

            auto user = matchedOrder->getUser();
            string m_order_symbol = matchedOrder->getSymbol();

            if (!user && matchedOrder->getOrderType() != ORDER_TYPE::SYSTEM) {
                cerr << "Error: User is null in matched Order " << matchedOrder->getOrderId() << "\n";
                rollbackCompletedSettlements();
                return;
            }

            if (user) {
                try {
                    float reservedAmt = 0.0f;
                    if (matchedOrder->getIsBuy()) {
                        if (!user->deductFunds(price)) {
                            throw runtime_error("Insufficient funds for user " + user->getUsername() +
                                                 " on order " + to_string(matchedOrder->getOrderId()));
                        }
                        reservedAmt = matchedOrder->getPrice() * matchedQty;
                        user->releaseReservedFunds(reservedAmt);
                        user->addToDemat(m_order_symbol, matchedQty);
                    } else {
                        user->addFunds(price);
                        user->deductDemat(m_order_symbol, matchedQty);
                        user->releaseReservedDemat(m_order_symbol, matchedQty);
                        reservedAmt = static_cast<float>(matchedQty);
                    }
                    completedSettlements.push_back({user, m_order_symbol, matchedQty, price, matchedOrder->getIsBuy(), reservedAmt});
                } catch (const exception &e) {
                    cerr << "Settlement error for order " << matchedOrder->getOrderId() << ": " << e.what() << "\n";
                    rollbackCompletedSettlements();
                    return;
                }
            }

            tradeAmount += price;
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
            if (!p_order_user->deductFunds(tradeAmount)) {
                cerr << "Error: Insufficient funds for primary order user " << p_order_user->getUsername() << "\n";
                rollbackCompletedSettlements();
                return;
            }
            p_order_user->releaseReservedFunds(orderPtr->getPrice() * matchedQty);
            p_order_user->addToDemat(p_order_symbol, matchedQty);
        } else {
            p_order_user->addFunds(tradeAmount);
            p_order_user->deductDemat(p_order_symbol, matchedQty);
            p_order_user->releaseReservedDemat(p_order_symbol, matchedQty);
        }

        Store::addExecutedOrder(orderPtr, matchedQty);
    } catch (const exception &e) {
        cerr << "Error executing order: " << e.what() << "\n";
    }
#ifdef ENABLE_METRICS
    get_tl_metrics().flush();
#endif
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
