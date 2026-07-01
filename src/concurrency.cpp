// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#include "cpi/concurrency.hpp"

#include <algorithm>
#include <memory>
#include <utility>

namespace cpi {

ThreadPool::ThreadPool(size_t num_threads) {
    if (num_threads == 0) {
        num_threads = std::max(
            size_t{1}, static_cast<size_t>(std::thread::hardware_concurrency()));
    }

    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this]() {
            for (;;) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    task_available_.wait(
                        lock, [this]() { return stop_ || !tasks_.empty(); });

                    if (stop_ && tasks_.empty()) {
                        return;
                    }

                    task = std::move(tasks_.front());
                    tasks_.pop();
                    ++active_;
                }

                // Execute outside the lock so other threads can enqueue while
                // this task runs.
                task();

                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    --active_;
                }
                task_finished_.notify_one();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

size_t ThreadPool::thread_count() const noexcept {
    return workers_.size();
}

bool ThreadPool::busy() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_ > 0 || !tasks_.empty();
}

void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_) {
            return;
        }
        stop_ = true;
    }

    task_available_.notify_all();

    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        workers_.clear();
    }
}

} // namespace cpi
