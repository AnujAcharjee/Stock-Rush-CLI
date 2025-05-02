#include "orderManager.hpp"

priority_queue<shared_ptr<Order>, vector<shared_ptr<Order>>, BuyOrderComparator> OrderManager::_BuyExecutionQueue;
priority_queue<shared_ptr<Order>, vector<shared_ptr<Order>>, SellOrderComparator> OrderManager::_SellExecutionQueue;

condition_variable OrderManager::CV_buyProcessing;
condition_variable OrderManager::CV_sellProcessing;

mutex OrderManager::Mtx_buyQ;
mutex OrderManager::Mtx_sellQ;

OrderManager::OrderManager() {
};

OrderManager &OrderManager::getOrderManagerInstance() {
    static OrderManager instance;
    return instance;
}

void OrderManager::setOrderInQueue(shared_ptr<Order> order) {
    bool isBuy = order->getIsBuy();
    if (isBuy) {
        unique_lock<mutex> lock(Mtx_buyQ);

        _BuyExecutionQueue.push(order);
        CV_buyProcessing.notify_one();
    } else {
        unique_lock<mutex> lock(Mtx_sellQ);

        _SellExecutionQueue.push(order);
        CV_sellProcessing.notify_one();
    }
}

bool OrderManager::placeOrder(shared_ptr<Order> orderPtr) {
    const auto symbol = orderPtr->getSymbol();

    bool isValid = _validator->validateOrder(orderPtr);
    if (!isValid) {
        cerr << "Invalid order. The order cannot be processed.\n";
        return false;
    }

    Store::addOrder(orderPtr);

    setOrderInQueue(orderPtr);

    return true;
}

OrderManager::~OrderManager() {
    {
        unique_lock<mutex> buyLock(Mtx_buyQ);
        while (!_BuyExecutionQueue.empty()) {
            _BuyExecutionQueue.pop();
        }
    }

    {
        unique_lock<mutex> sellLock(Mtx_sellQ);
        while (!_SellExecutionQueue.empty()) {
            _SellExecutionQueue.pop();
        }
    }
}
