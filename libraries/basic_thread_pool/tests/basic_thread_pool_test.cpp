#include <array>
#include <atomic>
#include <cassert>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>
#include <waystone/basic_thread_pool.hpp>

namespace {

struct MoveOnlyCallable {
  explicit MoveOnlyCallable(int value) : value(std::make_unique<int>(value)) {}

  MoveOnlyCallable(const MoveOnlyCallable&) = delete;
  MoveOnlyCallable& operator=(const MoveOnlyCallable&) = delete;
  MoveOnlyCallable(MoveOnlyCallable&&) = default;
  MoveOnlyCallable& operator=(MoveOnlyCallable&&) = default;

  int operator()() && { return *value; }

  std::unique_ptr<int> value;
};

void TestConstructionAndResults() {
  try {
    waystone::BasicThreadPool invalid_pool(0);
    assert(false);
  } catch (const std::invalid_argument&) {
  }

  waystone::BasicThreadPool pool(2);
  assert(pool.Submit([](int left, int right) { return left + right; }, 20, 22).get() == 42);

  std::atomic<bool> void_task_ran = false;
  pool.Submit([&void_task_ran] { void_task_ran = true; }).get();
  assert(void_task_ran);

  assert(pool.Submit(MoveOnlyCallable(7)).get() == 7);
  auto pointer = std::make_unique<int>(9);
  auto moved_argument =
      pool.Submit([](std::unique_ptr<int> value) { return *value; }, std::move(pointer));
  assert(pointer == nullptr);
  assert(moved_argument.get() == 9);
}

void TestTaskExceptionDoesNotStopWorker() {
  waystone::BasicThreadPool pool(1);
  auto failure = pool.Submit([]() -> int { throw std::runtime_error("task failed"); });
  auto success = pool.Submit([] { return 11; });

  try {
    static_cast<void>(failure.get());
    assert(false);
  } catch (const std::runtime_error&) {
  }
  assert(success.get() == 11);
}

void TestConcurrentSubmitExecutesEachTaskOnce() {
  constexpr std::size_t kProducerCount = 4;
  constexpr std::size_t kTasksPerProducer = 32;
  constexpr std::size_t kTaskCount = kProducerCount * kTasksPerProducer;

  waystone::BasicThreadPool pool(4);
  std::array<std::atomic<int>, kTaskCount> executions{};
  std::vector<std::thread> producers;
  producers.reserve(kProducerCount);

  for (std::size_t producer = 0; producer < kProducerCount; ++producer) {
    producers.emplace_back([producer, &executions, &pool] {
      std::vector<std::future<void>> results;
      results.reserve(kTasksPerProducer);
      for (std::size_t index = 0; index < kTasksPerProducer; ++index) {
        const std::size_t task_id = producer * kTasksPerProducer + index;
        results.push_back(pool.Submit([task_id, &executions] { ++executions[task_id]; }));
      }
      for (auto& result : results) result.get();
    });
  }

  for (auto& producer : producers) producer.join();
  for (const auto& execution_count : executions) assert(execution_count == 1);
}

void TestShutdownDrainsAndRejects() {
  waystone::BasicThreadPool pool(2);
  std::atomic<int> completed = 0;
  std::vector<std::future<void>> results;
  for (int index = 0; index < 32; ++index) {
    results.push_back(pool.Submit([&completed] { ++completed; }));
  }

  std::thread first_shutdown([&pool] { pool.Shutdown(); });
  std::thread second_shutdown([&pool] { pool.Shutdown(); });
  first_shutdown.join();
  second_shutdown.join();

  for (auto& result : results) result.get();
  assert(completed == 32);
  pool.Shutdown();

  try {
    static_cast<void>(pool.Submit([] {}));
    assert(false);
  } catch (const std::runtime_error&) {
  }
}

void TestDestructorDrains() {
  std::promise<void> task_started;
  auto started = task_started.get_future();
  std::promise<void> release_task;
  auto release = release_task.get_future().share();
  std::atomic<bool> task_finished = false;

  std::thread releaser([&started, &release_task] {
    started.wait();
    release_task.set_value();
  });
  {
    waystone::BasicThreadPool pool(1);
    static_cast<void>(pool.Submit([&task_started, release, &task_finished] {
      task_started.set_value();
      release.wait();
      task_finished = true;
    }));
  }
  releaser.join();
  assert(task_finished);
}

}  // namespace

int main() {
  TestConstructionAndResults();
  TestTaskExceptionDoesNotStopWorker();
  TestConcurrentSubmitExecutesEachTaskOnce();
  TestShutdownDrainsAndRejects();
  TestDestructorDrains();
}
