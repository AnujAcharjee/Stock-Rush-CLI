#include <iostream>
#include "user.hpp"

using namespace std;

User::User(string username, double funds) : _username(username), _funds(funds) {};

string User::getUsername() const {
    std::lock_guard<std::mutex> lock(_mtx);
    return _username;
};

double User::getFunds() const {
    std::lock_guard<std::mutex> lock(_mtx);
    return _funds;
};

double User::getAvailableFunds() const {
    std::lock_guard<std::mutex> lock(_mtx);
    return max(0.0, _funds - _reservedFunds);
}

double User::getReservedFunds() const {
    std::lock_guard<std::mutex> lock(_mtx);
    return _reservedFunds;
}

unordered_map<string, int> User::getDemat() const {
    std::lock_guard<std::mutex> lock(_mtx);
    return _demat;
}

int User::getStockQty_in_demat(const string &symbol) const {
    std::lock_guard<std::mutex> lock(_mtx);
    auto it = _demat.find(symbol);
    if (it != _demat.end()) {
        return it->second;
    }
    return 0;
}

int User::getAvailableStockQty(const string &symbol) const {
    std::lock_guard<std::mutex> lock(_mtx);
    auto it = _demat.find(symbol);
    int total = (it != _demat.end()) ? it->second : 0;
    auto rIt = _reservedDemat.find(symbol);
    int reserved = (rIt != _reservedDemat.end()) ? rIt->second : 0;
    return max(0, total - reserved);
}

void User::addFunds(double amount) {
    std::lock_guard<std::mutex> lock(_mtx);
    _funds += amount;
};

bool User::deductFunds(double amount) {
    std::lock_guard<std::mutex> lock(_mtx);
    if (_funds < amount)
        return false;
    _funds -= amount;
    return true;
};

void User::addToDemat(const string &symbol, int qty) {
    std::lock_guard<std::mutex> lock(_mtx);
    _demat[symbol] += qty;
}

void User::deductDemat(const string &symbol, int qty) {
    std::lock_guard<std::mutex> lock(_mtx);
    auto it = _demat.find(symbol);
    if (it == _demat.end()) {
        throw runtime_error("The stock of the requested symbol " + symbol + " can't be found in user demat");
    }

    int newQty = it->second - qty;
    if (newQty < 0) {
        throw runtime_error("Insufficient quantity of " + symbol + " in demat account.");
    }

    if (newQty == 0) {
        _demat.erase(symbol);
    } else {
        it->second = newQty;
    }
}

bool User::reserveFunds(double amount) {
    std::lock_guard<std::mutex> lock(_mtx);
    if (_funds - _reservedFunds < amount)
        return false;
    _reservedFunds += amount;
    return true;
}

void User::releaseReservedFunds(double amount) {
    std::lock_guard<std::mutex> lock(_mtx);
    _reservedFunds = max(0.0, _reservedFunds - amount);
}

bool User::reserveDemat(const string &symbol, int qty) {
    std::lock_guard<std::mutex> lock(_mtx);
    auto it = _demat.find(symbol);
    int total = (it != _demat.end()) ? it->second : 0;
    auto rIt = _reservedDemat.find(symbol);
    int reserved = (rIt != _reservedDemat.end()) ? rIt->second : 0;
    if (total - reserved < qty)
        return false;
    _reservedDemat[symbol] = reserved + qty;
    return true;
}

void User::releaseReservedDemat(const string &symbol, int qty) {
    std::lock_guard<std::mutex> lock(_mtx);
    auto it = _reservedDemat.find(symbol);
    if (it != _reservedDemat.end()) {
        it->second = max(0, it->second - qty);
        if (it->second == 0)
            _reservedDemat.erase(it);
    }
}

void User::printDemat() const {
    std::lock_guard<std::mutex> lock(_mtx);
    for (const auto &item : _demat) {
        cout << "Stock: " << item.first << ", Quantity: " << item.second << "\n";
    }
}