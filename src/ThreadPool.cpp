#include "ThreadPool.hpp"
#include "FFmpegRunner.hpp"

ThreadPool::ThreadPool(size_t numThreads, TaskQueue& queue)
    : m_numThreads(numThreads), m_queue(queue) {}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::start() {
    if (m_running.exchange(true)) return;
    m_workers.reserve(m_numThreads);
    for (size_t i = 0; i < m_numThreads; ++i) {
        m_workers.emplace_back([this](std::stop_token st) { workerLoop(st); });
    }
}

void ThreadPool::stop() {
    if (!m_running.exchange(false)) return;
    m_workers.clear(); // Request stop on stop_tokens and join
}

void ThreadPool::workerLoop(std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        auto task = m_queue.pop(stopToken);
        if (!task || stopToken.stop_requested()) break;
        
        auto result = FFmpegRunner::compressVideo(*task, stopToken);
        if (!result) {
            task->status = TaskStatus::Failed;
            task->errorMessage = result.error();
        } else {
            task->status = TaskStatus::Completed;
            task->progress = 100.0f;
        }
    }
}
