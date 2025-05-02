#include "stockLock.hpp"

unordered_map<string, shared_ptr<mutex>> StockLock::_lockMap;
mutex StockLock::Mtx_lockMap;

StockLock::StockLock()
{
}

StockLock &StockLock::getInstance()
{
    static StockLock instance;
    return instance;
}

void StockLock::createLock(const string &symbol)
{
    if (_lockMap.find(symbol) == _lockMap.end())
    {
        _lockMap[symbol] = make_shared<mutex>();
    }
    else
    {
        cerr << "Error: re-creation of existing stock lock" << symbol << "\n";
    }
}

shared_ptr<mutex> StockLock::acquireLock(const string &symbol)
{
    scoped_lock mapGuard(Mtx_lockMap);

    if (_lockMap.find(symbol) == _lockMap.end())
    {
        cerr << "Error: Lock for symbol" << symbol << "doesn't exist \n";
    }

    return _lockMap[symbol];
}

void StockLock::releaseLock(const string &symbol)
{
    scoped_lock lock(Mtx_lockMap);
    _lockMap.erase(symbol);
}

StockLock::~StockLock()
{
}