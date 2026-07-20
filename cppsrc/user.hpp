#pragma once
#include <unordered_map>
#include <mutex>

#include "common.hpp"

class User
{
    string _username;
    double _funds;
    unordered_map<string, int> _demat;
    mutable std::mutex _mtx;

  public:
    User(string username, double funds);

    string getUsername() const;
    double getFunds() const;
    const unordered_map<string, int> &getDemat() const;
    int getStockQty_in_demat(const string &symbol) const;

    void addFunds(double amount);
    void deductFunds(double amount);
    void addToDemat(const string &symbol, int qty);
    void deductDemat(const string &symbol, int qty);

    void printDemat() const;
};
