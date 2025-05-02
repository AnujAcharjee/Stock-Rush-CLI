#include "stockLock.hpp"

unordered_map<string, shared_ptr<mutex>> StockLock::_lockMap;
mutex StockLock::Mtx_lockMap;

StockLock::StockLock()
{
    // cout << "StockExecutionerLock instantiated \n";
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

// T1-> acquire the share_ptr of the lock, then mapGuard() locks immediately
// T2-> acquire the same shared_ptr, but mapGuard() is already locked, so it waits util T1 releases mapGuard()
// when mapGuard() unlocks - T2 locking succeed
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
    // cout << "StockExecutionerLock destructed \n"l;
}