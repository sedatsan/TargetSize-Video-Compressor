#include "TaskQueue.hpp"

void TaskQueue::push(std::shared_ptr<CompressionTask> task) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push(task);
    m_condVar.notify_one();
}

std::shared_ptr<CompressionTask> TaskQueue::pop(std::stop_token stopToken) {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_condVar.wait(lock, stopToken, [this]() { return !m_queue.empty(); });

    if (m_queue.empty() || stopToken.stop_requested()) {
        return nullptr;
    }

    auto task = m_queue.front();
    m_queue.pop();
    return task;
}

void TaskQueue::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_queue.empty()) {
        m_queue.pop();
    }
}

bool TaskQueue::empty() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.empty();
}