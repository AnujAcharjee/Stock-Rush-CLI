#pragma once
#include "common.hpp"
#include "order.hpp"
#include "store.hpp"
#include "user.hpp"

class OrderValidator
{
public:
    bool validateOrder(shared_ptr<Order> order) const;
};