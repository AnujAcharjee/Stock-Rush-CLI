#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>

#include "common.hpp"
#include "order.hpp"
#include "store.hpp"
#include "threadPool.hpp"
#include "orderValidator.hpp"

class Order;
class OrderExecutioner;

struct BuyOrderComparator {
    bool operator()(const shared_ptr<Order> &a, const shared_ptr<Order> &b) const {
        if (a->getPrice() == b->getPrice()) {
            if (a->getCreationTime() == b->getCreationTime()) {
                return a->getOrderId() > b->getOrderId();
            }
            return a->getCreationTime() > b->getCreationTime();
        }
        return a->getPrice() < b->getPrice();
    }
};

struct SellOrderComparator {
    bool operator()(const shared_ptr<Order> &a, const shared_ptr<Order> &b) const {
        if (a->getPrice() == b->getPrice()) {
            if (a->getCreationTime() == b->getCreationTime()) {
                return a->getOrderId() > b->getOrderId();
            }
            return a->getCreationTime() > b->getCreationTime();
        }
        return a->getPrice() > b->getPrice();
    }
};

class OrderManager {
    static priority_queue<shared_ptr<Order>, vector<shared_ptr<Order>>, BuyOrderComparator> _BuyExecutionQueue;
    static priority_queue<shared_ptr<Order>, vector<shared_ptr<Order>>, SellOrderComparator> _SellExecutionQueue;

    static condition_variable CV_buyProcessing;
    static condition_variable CV_sellProcessing;

    static mutex Mtx_buyQ;
    static mutex Mtx_sellQ;

    static void setOrderInQueue(shared_ptr<Order> order);

    shared_ptr<OrderValidator> _validator = make_shared<OrderValidator>();

    OrderManager();
    ~OrderManager();

    OrderManager(const OrderManager &) = delete;
    OrderManager &operator=(const OrderManager &) = delete;

    friend class OrderExecutioner;

  public:
    static OrderManager &getOrderManagerInstance();
    bool placeOrder(shared_ptr<Order> order);
};
