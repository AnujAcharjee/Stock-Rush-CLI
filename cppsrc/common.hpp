#pragma once
#include <iostream>
#include <string>
#include <memory>

using namespace std;

enum ORDER_TYPE{
    MARKET,
    LIMIT,
    SYSTEM,
};

enum ORDER_STATUS
{
    OPEN,
    EXECUTED,
    CANCELLED,
};

enum ORDER_EXPIRY {
    DAY,
    GTC, // good till cancel
    GTD,  // good till date
};



