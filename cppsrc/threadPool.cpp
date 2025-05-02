#include "threadPool.hpp"

ThreadPool::ThreadPool(size_t numThreads)
{
    for (size_t i = 0; i < numThreads; ++i)
    {
        _workers.emplace_back([this]() {
            while (true)
            {
                move_only_function<void()> task;
                {
                    unique_lock<mutex> lock(_queueMutex);
                    _condition.wait(lock, [this]() { return _stop || !_tasks.empty(); });

                    if (_stop && _tasks.empty())
                        return;

                    task = move(_tasks.front());
                    _tasks.pop();
                }

                task();
            }
        });
    }
}

ThreadPool::~ThreadPool()
{
    {
        scoped_lock lock(_queueMutex);
        _stop = true;
    }
    _condition.notify_all();

    for (jthread &worker : _workers)
    {
        if (worker.joinable())
            worker.join();
    }
}
