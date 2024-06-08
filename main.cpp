#include <chrono>
#include <cstdio>
#include <functional>
#include <iostream>
#include <thread>
#include <type_traits>

#include "MPSCLockFreeQueue.h"
namespace {
static const std::size_t ITER_NUM = 10000000;
std::atomic<std::size_t> total_write_count = {0};
std::atomic<std::size_t> total_read_count = {0};

template <class TimeT = std::chrono::milliseconds,
          class ClockT = std::chrono::steady_clock>
struct measure {
  template <class F, class... Args>
  static auto duration(F&& func, Args&&... args) {
    auto start = ClockT::now();
    std::invoke(std::forward<F>(func), std::forward<Args>(args)...);
    return std::chrono::duration_cast<TimeT>(ClockT::now() - start);
  }
};


void SetCpuAffinity(int cpu) {
  cpu_set_t cpuset;
  pthread_t thread;

  // Get the current thread
  thread = pthread_self();

  // Initialize the CPU set
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);

  // Set affinity for the current thread
  int result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  if (result != 0) {
    perror("pthread_setaffinity_np");
    exit(EXIT_FAILURE);
  }

  printf("Set affinity for thread %lu to CPU %d\n", thread, cpu);
}

void WriterThread(queue::MPSCLockFreeQueue& lf_queue, const int coreId) {
  SetCpuAffinity(coreId);
  std::size_t total_sum = 0;
  for (size_t i = 0; i < ITER_NUM; ++i) {
    auto res = lf_queue.Reserve(8);
    // printf("reserved %ul\n", res.count_);
    if (res.count_) {
      for (auto it = res.seq_no_; it < res.seq_no_ + res.count_; ++it) {
        lf_queue.Get(it) = i;
      }
      lf_queue.Commit(res);
      // printf("Commited %ul\n", res.count_);
      total_sum += res.count_;
    }
  }
  total_write_count.fetch_add(total_sum);
}

void ReaderThread(queue::MPSCLockFreeQueue& lf_queue, const int coreId)  {
  SetCpuAffinity(coreId);
  size_t total_sum = 0;
  for (size_t i = 0; i < ITER_NUM; ++i) {
    lf_queue.Read(8, [&total_sum](auto& val) noexcept { ++total_sum; });
  }
  total_read_count.fetch_add(total_sum);
}

}  // namespace

int main(int ac, char** av) {
  queue::MPSCLockFreeQueue queue(128);
  auto duration = measure<>::duration([&queue]() {
    int coreId(0);
    std::jthread write_thread1([&queue, coreId]() { WriterThread(queue, coreId); });
    ++coreId;
    std::jthread write_thread2([&queue, coreId]() { WriterThread(queue, coreId); });
    ++coreId;
    std::jthread write_thread3([&queue, coreId]() { WriterThread(queue, coreId); });
    ++coreId;
    std::jthread read_thread([&queue, coreId]() { ReaderThread(queue, coreId); });
  });
  std::printf("Duration %lu ms\n", duration.count());
  std::printf("total writes: %lu, %lu op/s\n", total_write_count.load(),
              total_write_count * 1000 / duration.count());
  std::printf("total reads: %lu, %lu op/s\n", total_read_count.load(),
              total_read_count * 1000 / duration.count());
}