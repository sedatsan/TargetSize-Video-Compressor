#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <stop_token>
#include "TaskStatus.hpp"

class TaskQueue {
public:
    void push(std::shared_ptr<CompressionTask> task);
    std::shared_ptr<CompressionTask> pop(std::stop_token stopToken);
    void clear();
    bool empty();
private:
    std::queue<std::shared_ptr<CompressionTask>> m_queue;
    std::mutex m_mutex;
    std::condition_variable_any m_condVar;
};