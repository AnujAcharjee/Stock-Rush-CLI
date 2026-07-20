#pragma once
#include <atomic>
#include <chrono>
#include <iostream>
#include <vector>
#include <mutex>

class MetricsCollector {
private:
    std::atomic<unsigned long long> _totalOrdersProcessed{0};
    std::atomic<unsigned long long> _totalMatches{0};
    std::atomic<unsigned long long> _totalMatchLatencyNs{0};
    std::atomic<unsigned long long> _totalMatchedOrdersCount{0};

    std::atomic<int> _activeThreads{0};

    std::chrono::steady_clock::time_point _startTime;

    MetricsCollector() : _startTime(std::chrono::steady_clock::now()) {}

public:
    static MetricsCollector& getInstance() {
        static MetricsCollector instance;
        return instance;
    }

    void incrementOrdersProcessed() { _totalOrdersProcessed.fetch_add(1, std::memory_order_relaxed); }
    void incrementMatches() { _totalMatches.fetch_add(1, std::memory_order_relaxed); }
    void incrementActiveThreads() { _activeThreads.fetch_add(1, std::memory_order_relaxed); }
    void decrementActiveThreads() { _activeThreads.fetch_sub(1, std::memory_order_relaxed); }

    void submitBatchedStats(unsigned long long orders, unsigned long long matches, unsigned long long latencyNs, unsigned long long matchesWithLatency) {
        _totalOrdersProcessed.fetch_add(orders, std::memory_order_relaxed);
        _totalMatches.fetch_add(matches, std::memory_order_relaxed);
        _totalMatchLatencyNs.fetch_add(latencyNs, std::memory_order_relaxed);
        _totalMatchedOrdersCount.fetch_add(matchesWithLatency, std::memory_order_relaxed);
    }

    double getOrdersProcessedPerSec() const {
        auto now = std::chrono::steady_clock::now();
        double elapsedSec = std::chrono::duration<double>(now - _startTime).count();
        return elapsedSec > 0 ? (static_cast<double>(_totalOrdersProcessed.load(std::memory_order_relaxed)) / elapsedSec) : 0.0;
    }

    double getMatchesPerSec() const {
        auto now = std::chrono::steady_clock::now();
        double elapsedSec = std::chrono::duration<double>(now - _startTime).count();
        return elapsedSec > 0 ? (static_cast<double>(_totalMatches.load(std::memory_order_relaxed)) / elapsedSec) : 0.0;
    }

    int getActiveThreads() const {
        return _activeThreads.load(std::memory_order_relaxed);
    }

    double getAvgMatchLatencyMs() const {
        unsigned long long count = _totalMatchedOrdersCount.load(std::memory_order_relaxed);
        if (count == 0) return 0.0;
        return (static_cast<double>(_totalMatchLatencyNs.load(std::memory_order_relaxed)) / count) / 1'000'000.0;
    }

    void printMetrics() const {
        std::cout << "\n---------------- Performance Stats ----------------\n";
        std::cout << "Orders Processed/sec:  " << getOrdersProcessedPerSec() << "\n";
        std::cout << "Average Matches/sec:   " << getMatchesPerSec() << "\n";
        std::cout << "Active Worker Threads: " << getActiveThreads() << " (out of 4 pool threads)\n";
        std::cout << "Avg Match Latency:     " << getAvgMatchLatencyMs() << " ms\n";
        std::cout << "---------------------------------------------------\n";
    }
};

struct ThreadLocalMetrics {
    unsigned long long pendingOrders = 0;
    unsigned long long pendingMatches = 0;
    unsigned long long pendingLatencyNs = 0;
    unsigned long long pendingMatchedOrdersCount = 0;

    ~ThreadLocalMetrics() {
        flush();
    }

    void addOrder() {
        pendingOrders++;
        if (pendingOrders >= 1000) flush();
    }

    void addMatch(unsigned long long ns) {
        pendingMatches++;
        pendingLatencyNs += ns;
        pendingMatchedOrdersCount++;
        if (pendingMatches >= 100) flush();
    }

    void flush() {
        if (pendingOrders > 0 || pendingMatches > 0) {
            MetricsCollector::getInstance().submitBatchedStats(pendingOrders, pendingMatches, pendingLatencyNs, pendingMatchedOrdersCount);
            pendingOrders = 0;
            pendingMatches = 0;
            pendingLatencyNs = 0;
            pendingMatchedOrdersCount = 0;
        }
    }
};

inline ThreadLocalMetrics& get_tl_metrics() {
    thread_local ThreadLocalMetrics tl_metrics;
    return tl_metrics;
}
