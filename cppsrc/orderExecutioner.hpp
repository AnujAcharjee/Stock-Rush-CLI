#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "common.hpp"
#include "order.hpp"
#include "orderBook.hpp"
#include "orderManager.hpp"
#include "stock.hpp"
#include "stockLock.hpp"
#include "store.hpp"
#include "threadPool.hpp"

// Singleton
class OrderExecutioner
{
  private:
    static thread _buyProcessingThread;
    static thread _sellProcessingThread;
    static atomic<bool> _isBuyProcessing;
    static atomic<bool> _isSellProcessing;

    static unique_ptr<ThreadPool> threadPool;
    static condition_variable CV_threadPool;

    static mutex Mtx_buyThread;
    static mutex Mtx_sellThread;

    static StockLock &_lockManager;
    static OrderManager &_manager;

    void executeOrder(shared_ptr<Order> order);

    OrderExecutioner();
    ~OrderExecutioner();

    OrderExecutioner(const OrderExecutioner &) = delete;
    OrderExecutioner &operator=(const OrderExecutioner &) = delete;

  public:
    static OrderExecutioner &getExecutionerInstance();

    template <thread &processingThread, atomic<bool> &isProcessing, typename QueueType, typename CVType>
    void checkExecutionQueue(QueueType &executionQueue, CVType &cv, const string &queueType);
};
