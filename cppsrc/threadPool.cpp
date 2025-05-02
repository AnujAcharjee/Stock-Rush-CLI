#include "threadPool.hpp"

ThreadPool::ThreadPool(size_t numThreads)
{
    for (size_t i = 0; i < numThreads; ++i)
    {
        _workers.emplace_back([this]() {
            // function<void()> task;
            while (true)
            {
                move_only_function<void()> task;
                {
                    unique_lock<mutex> lock(_queueMutex);
                    _condition.wait(lock, [this]() { return _stop || !_tasks.empty(); });

                    if (_stop && _tasks.empty())
                        return; // terminate thread

                    task = move(_tasks.front()); // move the thread obj from queue to task var
                    _tasks.pop();
                }

                task(); // execute task func. extracted
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
            worker.join(); // Wait for threads to finish
    }
}
