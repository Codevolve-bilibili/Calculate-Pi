// Copyright (c) 2026 Calculate Pi project
// SPDX-License-Identifier: MIT

#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

namespace cpi {

// A fixed-size thread pool built on C++20 standard threading facilities.
//
// The pool is intended for coarse-grained parallel work such as the recursive
// binary splitting used by the Chudnovsky calculator.  Tasks are submitted as
// callables and their results are retrieved through std::future.
class ThreadPool {
public:
    // Create a pool with the requested number of worker threads.  If
    // num_threads is 0, the pool uses std::thread::hardware_concurrency(),
    // falling back to a single thread if the value cannot be determined.
    explicit ThreadPool(size_t num_threads = 0);

    // Stop accepting new work and wait for all workers to finish.
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Submit a task to the pool.  Returns a future that will hold the task's
    // return value once it completes.  Throws std::runtime_error if the pool
    // has already been shut down.
    template <typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

    // Number of worker threads in the pool.
    [[nodiscard]] size_t thread_count() const noexcept;

    // Returns true if at least one task is currently executing or waiting to
    // be executed.  This is a snapshot and may change immediately after the
    // call returns.
    [[nodiscard]] bool busy() const;

    // Signal the pool to stop accepting new tasks and wait for all queued and
    // running tasks to finish.  Safe to call multiple times.
    void shutdown();

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;

    mutable std::mutex mutex_;
    std::condition_variable task_available_;
    std::condition_variable task_finished_;

    bool stop_ = false;
    size_t active_ = 0;
};

// ---------------------------------------------------------------------------
// Template implementation
// ---------------------------------------------------------------------------

template <typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>> {
    using ReturnType = std::invoke_result_t<F, Args...>;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stop_) {
            throw std::runtime_error("cannot enqueue on a stopped ThreadPool");
        }

        // Bind the callable with its arguments so it can be invoked with no
        // parameters inside the worker thread.
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<ReturnType> result = task->get_future();
        tasks_.emplace([task]() { (*task)(); });
        lock.unlock();

        task_available_.notify_one();
        return result;
    }
}

// Lower the priority of the current process (and/or its threads) to reduce
// impact on the rest of the system.  The implementation is selected at compile
// time via platform macros:
//   - Windows: SetPriorityClass / SetThreadPriority
//   - Linux:   nice(10)
//   - macOS:   nice(10)
//   - others:  no-op
void set_eco_priority();

} // namespace cpi
