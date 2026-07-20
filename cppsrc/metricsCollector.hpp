#pragma once
#include <atomic>
#include <chrono>
#include <iostream>
#include <vector>
#include <mutex>
#include <algorithm>
#include <iomanip>

class MetricsCollector {
private:
    std::atomic<unsigned long long> _totalOrdersSubmitted{0};
    std::atomic<unsigned long long> _totalOrdersProcessed{0};
    std::atomic<unsigned long long> _totalMatches{0};
    std::atomic<unsigned long long> _totalMatchLatencyNs{0};
    std::atomic<unsigned long long> _totalMatchedOrdersCount{0};

    std::atomic<int> _activeThreads{0};
    std::chrono::steady_clock::time_point _startTime;
    std::chrono::steady_clock::time_point _endTime;
    std::atomic<bool> _timerStopped{false};

    std::atomic<unsigned long long> _totalCpuTimeNs{0};
    std::atomic<size_t> _peakQueueSize{0};

    std::vector<double> _latencies;
    mutable std::mutex _latenciesMtx;

    MetricsCollector() : _startTime(std::chrono::steady_clock::now()) {}

public:
    static MetricsCollector& getInstance() {
        static MetricsCollector instance;
        return instance;
    }

    void resetTimer() {
        _startTime = std::chrono::steady_clock::now();
        _timerStopped.store(false, std::memory_order_relaxed);
        _totalOrdersSubmitted.store(0, std::memory_order_relaxed);
        _totalOrdersProcessed.store(0, std::memory_order_relaxed);
        _totalMatches.store(0, std::memory_order_relaxed);
        _totalMatchLatencyNs.store(0, std::memory_order_relaxed);
        _totalMatchedOrdersCount.store(0, std::memory_order_relaxed);
        _totalCpuTimeNs.store(0, std::memory_order_relaxed);
        _peakQueueSize.store(0, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(_latenciesMtx);
            _latencies.clear();
        }
    }

    void stopTimer() {
        _endTime = std::chrono::steady_clock::now();
        _timerStopped.store(true, std::memory_order_relaxed);
    }

    void incrementOrdersSubmitted() { _totalOrdersSubmitted.fetch_add(1, std::memory_order_relaxed); }
    void incrementOrdersProcessed() { _totalOrdersProcessed.fetch_add(1, std::memory_order_relaxed); }
    void incrementMatches() { _totalMatches.fetch_add(1, std::memory_order_relaxed); }
    void incrementActiveThreads() { _activeThreads.fetch_add(1, std::memory_order_relaxed); }
    void decrementActiveThreads() { _activeThreads.fetch_sub(1, std::memory_order_relaxed); }

    void addActiveCpuTime(unsigned long long ns) {
        _totalCpuTimeNs.fetch_add(ns, std::memory_order_relaxed);
    }

    void updatePeakQueueSize(size_t currentSize) {
        size_t prev = _peakQueueSize.load(std::memory_order_relaxed);
        while (currentSize > prev && !_peakQueueSize.compare_exchange_weak(prev, currentSize, std::memory_order_relaxed));
    }

    size_t getPeakQueueSize() const {
        return _peakQueueSize.load(std::memory_order_relaxed);
    }

    unsigned long long getTotalOrdersProcessed() const {
        return _totalOrdersProcessed.load(std::memory_order_relaxed);
    }

    unsigned long long getTotalMatches() const {
        return _totalMatches.load(std::memory_order_relaxed);
    }

    void submitBatchedStats(unsigned long long orders, unsigned long long matches, unsigned long long latencyNs, unsigned long long matchesWithLatency, const std::vector<double>& latencies) {
        _totalOrdersProcessed.fetch_add(orders, std::memory_order_relaxed);
        _totalMatches.fetch_add(matches, std::memory_order_relaxed);
        _totalMatchLatencyNs.fetch_add(latencyNs, std::memory_order_relaxed);
        _totalMatchedOrdersCount.fetch_add(matchesWithLatency, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(_latenciesMtx);
            _latencies.insert(_latencies.end(), latencies.begin(), latencies.end());
        }
    }

    double getOrdersProcessedPerSec() const {
        auto now = _timerStopped.load(std::memory_order_relaxed) ? _endTime : std::chrono::steady_clock::now();
        double elapsedSec = std::chrono::duration<double>(now - _startTime).count();
        return elapsedSec > 0 ? (static_cast<double>(_totalOrdersProcessed.load(std::memory_order_relaxed)) / elapsedSec) : 0.0;
    }

    double getMatchesPerSec() const {
        auto now = _timerStopped.load(std::memory_order_relaxed) ? _endTime : std::chrono::steady_clock::now();
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

    double getCpuUtilization() const {
        auto now = _timerStopped.load(std::memory_order_relaxed) ? _endTime : std::chrono::steady_clock::now();
        double elapsedSec = std::chrono::duration<double>(now - _startTime).count();
        if (elapsedSec <= 0) return 0.0;
        double totalAvailableNs = elapsedSec * 4.0 * 1'000'000'000.0;
        double utilization = (static_cast<double>(_totalCpuTimeNs.load(std::memory_order_relaxed)) / totalAvailableNs) * 100.0;
        if (utilization > 100.0) utilization = 100.0;
        if (utilization < 0.0) utilization = 0.0;
        return utilization;
    }

    void printMetrics() const {
        auto now = _timerStopped.load(std::memory_order_relaxed) ? _endTime : std::chrono::steady_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(now - _startTime).count();

        double median = 0.0, p95 = 0.0, p99 = 0.0;
        std::vector<double> sortedLatencies;
        {
            std::lock_guard<std::mutex> lock(_latenciesMtx);
            sortedLatencies = _latencies;
        }

        if (!sortedLatencies.empty()) {
            std::sort(sortedLatencies.begin(), sortedLatencies.end());
            size_t n = sortedLatencies.size();
            median = sortedLatencies[n * 50 / 100] / 1000.0; // ns to us
            p95 = sortedLatencies[n * 95 / 100] / 1000.0;
            p99 = sortedLatencies[n * 99 / 100] / 1000.0;
        }

        double avgLatencyUs = (sortedLatencies.empty()) ? 0.0 : (getAvgMatchLatencyMs() * 1000.0);

        auto formatWithCommas = [](unsigned long long value) -> std::string {
            std::string numStr = std::to_string(value);
            int insertPos = static_cast<int>(numStr.length()) - 3;
            while (insertPos > 0) {
                numStr.insert(insertPos, ",");
                insertPos -= 3;
            }
            return numStr;
        };

        std::cout << "\n";
        std::cout << "Orders Submitted:       " << _totalOrdersSubmitted.load(std::memory_order_relaxed) << "\n";
        std::cout << "Orders Executed:        " << _totalOrdersProcessed.load(std::memory_order_relaxed) << "\n";
        std::cout << "Trades Generated:       " << _totalMatches.load(std::memory_order_relaxed) << "\n\n";

        std::cout << std::fixed << std::setprecision(1);
        std::cout << "Total Runtime:          " << elapsedMs << " ms\n\n";

        std::cout << std::defaultfloat;
        std::cout << "Throughput:             " << formatWithCommas(static_cast<unsigned long long>(getOrdersProcessedPerSec())) << " orders/sec\n";
        
        std::cout << std::fixed << std::setprecision(1);
        std::cout << "Average Match Latency:  " << avgLatencyUs << " \xCE\xBCs\n";
        std::cout << "Median Latency:         " << median << " \xCE\xBCs\n";
        std::cout << "P95 Latency:            " << p95 << " \xCE\xBCs\n";
        std::cout << "P99 Latency:            " << p99 << " \xCE\xBCs\n\n";

        std::cout << "Worker Threads:         4\n";
        std::cout << "CPU Utilization:        " << static_cast<int>(getCpuUtilization()) << "%\n";
        std::cout << "Peak Queue Size:        " << getPeakQueueSize() << "\n";
        std::cout << "---------------------------------------------------\n";
    }
};

struct ThreadLocalMetrics {
    unsigned long long pendingOrders = 0;
    unsigned long long pendingMatches = 0;
    unsigned long long pendingLatencyNs = 0;
    unsigned long long pendingMatchedOrdersCount = 0;
    std::vector<double> pendingLatencies;

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
        pendingLatencies.push_back(static_cast<double>(ns));
        if (pendingMatches >= 100) flush();
    }

    void flush() {
        if (pendingOrders > 0 || pendingMatches > 0) {
            MetricsCollector::getInstance().submitBatchedStats(pendingOrders, pendingMatches, pendingLatencyNs, pendingMatchedOrdersCount, pendingLatencies);
            pendingOrders = 0;
            pendingMatches = 0;
            pendingLatencyNs = 0;
            pendingMatchedOrdersCount = 0;
            pendingLatencies.clear();
        }
    }
};

inline ThreadLocalMetrics& get_tl_metrics() {
    thread_local ThreadLocalMetrics tl_metrics;
    return tl_metrics;
}
