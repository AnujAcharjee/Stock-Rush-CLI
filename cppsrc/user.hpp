#pragma once
#include <unordered_map>
#include <mutex>

#include "common.hpp"

class User {
    string _username;
    double _funds;
    double _reservedFunds{0.0};
    unordered_map<string, int> _demat;
    unordered_map<string, int> _reservedDemat;
    mutable std::mutex _mtx;

  public:
    User(string username, double funds);

    string getUsername() const;
    double getFunds() const;
    double getAvailableFunds() const;
    double getReservedFunds() const;
    unordered_map<string, int> getDemat() const;
    int getStockQty_in_demat(const string &symbol) const;
    int getAvailableStockQty(const string &symbol) const;

    void addFunds(double amount);
    bool deductFunds(double amount);
    void addToDemat(const string &symbol, int qty);
    void deductDemat(const string &symbol, int qty);

    bool reserveFunds(double amount);
    void releaseReservedFunds(double amount);
    bool reserveDemat(const string &symbol, int qty);
    void releaseReservedDemat(const string &symbol, int qty);

    void printDemat() const;
};
