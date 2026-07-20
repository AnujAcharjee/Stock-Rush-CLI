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

const unordered_map<string, int> &User::getDemat() const {
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

void User::addFunds(double amount) {
    std::lock_guard<std::mutex> lock(_mtx);
    _funds += amount;
};

void User::deductFunds(double amount) {
    std::lock_guard<std::mutex> lock(_mtx);
    if (_funds < amount)
        return;
    _funds -= amount;
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

void User::printDemat() const {
    std::lock_guard<std::mutex> lock(_mtx);
    for (const auto &item : _demat) {
        cout << "Stock: " << item.first << ", Quantity: " << item.second << "\n";
    }
}