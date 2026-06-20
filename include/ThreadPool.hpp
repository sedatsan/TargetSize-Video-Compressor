#pragma once
#include <vector>
#include <thread>
#include <stop_token>
#include <memory>
#include <atomic>
#include "TaskQueue.hpp"

class ThreadPool {
public:
    ThreadPool(size_t numThreads, TaskQueue& queue);
    ~ThreadPool();
    void start();
    void stop();
private:
    void workerLoop(std::stop_token stopToken);
    size_t m_numThreads;
    TaskQueue& m_queue;
    std::vector<std::jthread> m_workers;
    std::atomic<bool> m_running{ false };
};
