#include <iostream>
#include <vector>
#include <waystone/basic_thread_pool.hpp>

int main() {
  waystone::BasicThreadPool pool(2);
  std::vector<std::future<int>> results;
  for (int value = 1; value <= 4; ++value) {
    results.push_back(pool.Submit([value] { return value * value; }));
  }

  for (auto& result : results) {
    std::cout << result.get() << '\n';
  }
}
