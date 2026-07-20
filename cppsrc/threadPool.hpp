#pragma once
#include <iostream>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <future>
#include <type_traits>

#ifdef ENABLE_METRICS
#include "metricsCollector.hpp"
#endif

using namespace std;

class ThreadPool
{
    vector<jthread> _workers;
    queue<move_only_function<void()>> _tasks;
    mutex _queueMutex;
    condition_variable _condition;
    bool _stop = false;

public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();

    template <class F, class... Args>
    auto enqueue(F &&f, Args &&...args) -> future<invoke_result_t<F, Args...>>
    {
        using ReturnType = invoke_result_t<F, Args...>;
        auto task = packaged_task<ReturnType()>(
            [func = forward<F>(f), ... args = forward<Args>(args)]() mutable
            {
#ifdef ENABLE_METRICS
                MetricsCollector::getInstance().incrementActiveThreads();
                if constexpr (is_void_v<ReturnType>) {
                    func(move(args)...);
                    MetricsCollector::getInstance().decrementActiveThreads();
                } else {
                    auto res = func(move(args)...);
                    MetricsCollector::getInstance().decrementActiveThreads();
                    return res;
                }
#else
                return func(move(args)...);
#endif
            });

        future<ReturnType> result = task.get_future();
        {
            scoped_lock lock(_queueMutex);
            if (_stop)
                throw runtime_error("Enqueue on stopped ThreadPool");
            _tasks.emplace(move(task));
        }
        _condition.notify_one();
        return result;
    }
};