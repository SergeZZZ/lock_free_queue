#include <chrono>
#include <functional>
#include <iostream>
#include <thread>
#include <type_traits>
#include <cstdio>

#include "MPSCLockFreeQueue.h"

static const size_t ITER_NUM = 1000000;
std::atomic<size_t> total_write_count = {0};
std::atomic<size_t> total_read_count = {0};

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

void WriterThread(queue::MPSCLockFreeQueue& lf_queue) {
  size_t total_sum = 0;
  for (size_t i = 0; i < ITER_NUM; ++i) {
    auto res = lf_queue.Reserve(8);
    //printf("reserved %ul\n", res.count_);
    if (res.count_) {
      for (auto it = res.seq_no_; it < res.seq_no_ + res.count_; ++it) {
        lf_queue.Get(it) = i;
      }
      lf_queue.Commit(res);
      //printf("Commited %ul\n", res.count_);
      total_sum += res.count_;
    }
  }
  total_write_count.fetch_add(total_sum);
}

void ReaderThread(queue::MPSCLockFreeQueue& lf_queue) {
  size_t total_sum = 0;
  for (size_t i = 0; i < ITER_NUM; ++i) {
    lf_queue.Read(8, [&total_sum](auto& val) noexcept { ++total_sum; });
  }
  total_read_count.fetch_add(total_sum);
}

int main(int ac, char** av) {
  queue::MPSCLockFreeQueue queue(128);
  auto duration = measure<>::duration([&queue](){
    std::jthread write_thread1([&queue]() { WriterThread(queue); });
    std::jthread write_thread2([&queue]() { WriterThread(queue); });
    std::jthread read_thread([&queue]() { ReaderThread(queue); });
  });
  std::printf("Duration %lu ms\n", duration.count());
  std::printf("total writes: %lu\n", total_write_count.load());
  std::printf("total reads: %lu\n", total_read_count.load());
}