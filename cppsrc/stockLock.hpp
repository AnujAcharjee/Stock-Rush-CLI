#pragma once
#include <mutex>
#include <unordered_map>

#include "common.hpp"

// Singleton
class StockLock
{
    static unordered_map<string, shared_ptr<mutex>> _lockMap;
    static mutex Mtx_lockMap;

    StockLock();
    ~StockLock();

    StockLock(const StockLock &) = delete;
    StockLock &operator=(StockLock &) = delete;

  public:
    static StockLock &getInstance();

    static void createLock(const string &symbol);
    static shared_ptr<mutex> acquireLock(const string &symbol);
    static void releaseLock(const string &symbol);
};