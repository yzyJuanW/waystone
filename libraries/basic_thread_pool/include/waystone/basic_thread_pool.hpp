#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace waystone {

class BasicThreadPool {
 public:
  explicit BasicThreadPool(std::size_t worker_count) {
    if (worker_count == 0) {
      throw std::invalid_argument("BasicThreadPool requires at least one worker");
    }

    workers_.reserve(worker_count);
    try {
      for (std::size_t index = 0; index < worker_count; ++index) {
        workers_.emplace_back([this] { WorkerLoop(); });
      }
    } catch (...) {
      // Some workers may already be observing this object. Publish the stop state before
      // joining them so a failed constructor never leaves background threads behind.
      {
        std::lock_guard lock(mutex_);
        accepting_ = false;
      }
      task_available_.notify_all();
      for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
      }
      throw;
    }
  }

  ~BasicThreadPool() { Shutdown(); }

  BasicThreadPool(const BasicThreadPool&) = delete;
  BasicThreadPool& operator=(const BasicThreadPool&) = delete;
  BasicThreadPool(BasicThreadPool&&) = delete;
  BasicThreadPool& operator=(BasicThreadPool&&) = delete;

  template <class F, class... Args>
  [[nodiscard]] auto Submit(F&& function, Args&&... args)
      -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> {
    using Result = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

    auto bound_task = [function = std::decay_t<F>(std::forward<F>(function)),
                       arguments =
                           std::make_tuple(std::forward<Args>(args)...)]() mutable -> Result {
      return std::apply(std::move(function), std::move(arguments));
    };
    std::packaged_task<Result()> result_task(std::move(bound_task));
    auto result = result_task.get_future();
    std::packaged_task<void()> queued_task([task = std::move(result_task)]() mutable { task(); });

    {
      std::lock_guard lock(mutex_);
      // This mutex is the acceptance boundary: either enqueue completes before Shutdown
      // closes the pool, or the caller gets an immediate rejection.
      if (!accepting_) {
        throw std::runtime_error("cannot submit to a stopped BasicThreadPool");
      }
      tasks_.push(std::move(queued_task));
    }
    task_available_.notify_one();
    return result;
  }

  // Precondition: callers must not be workers owned by this pool, because the join leader
  // would otherwise wait for itself. Concurrent calls from external threads are safe.
  void Shutdown() noexcept {
    bool join_workers = false;
    {
      std::unique_lock lock(mutex_);
      if (joined_) return;

      accepting_ = false;
      if (!joining_) {
        // Exactly one caller becomes the join leader. Followers wait instead of racing on
        // std::thread::join, which is not safe to call concurrently on the same thread.
        joining_ = true;
        join_workers = true;
      } else {
        shutdown_complete_.wait(lock, [this] { return joined_; });
        return;
      }
    }

    task_available_.notify_all();
    if (join_workers) {
      for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
      }

      {
        std::lock_guard lock(mutex_);
        joined_ = true;
      }
      shutdown_complete_.notify_all();
    }
  }

 private:
  void WorkerLoop() {
    while (true) {
      std::packaged_task<void()> task;
      {
        std::unique_lock lock(mutex_);
        task_available_.wait(lock, [this] { return !accepting_ || !tasks_.empty(); });

        // Closing rejects new work but does not discard accepted work. A worker exits only
        // after the shared queue is empty, which gives Shutdown its drain guarantee.
        if (tasks_.empty()) return;

        task = std::move(tasks_.front());
        tasks_.pop();
      }
      task();
    }
  }

  std::mutex mutex_;
  std::condition_variable task_available_;
  std::condition_variable shutdown_complete_;
  std::queue<std::packaged_task<void()>> tasks_;
  std::vector<std::thread> workers_;
  bool accepting_ = true;
  bool joining_ = false;
  bool joined_ = false;
};

}  // namespace waystone
