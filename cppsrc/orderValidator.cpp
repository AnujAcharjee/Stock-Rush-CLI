#include "orderValidator.hpp"

bool OrderValidator::validateOrder(shared_ptr<Order> orderPtr) const {
    if (!orderPtr) {
        cerr << "[Error] Null order pointer passed for validation.\n";
        return false;
    }

    if (orderPtr->getPrice() <= 0 || orderPtr->getQty() <= 0) {
        cerr << "[Error] Order price and quantity must be greater than zero.\n";
        return false;
    }

    auto user = orderPtr->getUser();
    if (!user) {
        cerr << "[Error] Order does not have a valid user.\n";
        return false;
    }

    if (orderPtr->getIsBuy()) {
        float totalCost = orderPtr->getPrice() * orderPtr->getQty();
        if (totalCost > user->getFunds()) {
            cerr << "[Error] Insufficient funds to place buy order. Required: "
                 << totalCost << ", Available: " << user->getFunds() << '\n';
            return false;
        }
    } else {
        int availableQty = user->getStockQty_in_demat(orderPtr->getSymbol());
        if (availableQty < orderPtr->getQty()) {
            cerr << "[Error] Insufficient stock quantity to sell. Available: "
                 << availableQty << ", Required: " << orderPtr->getQty() << '\n';
            return false;
        }
    }

    return true;
}
