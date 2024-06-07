#pragma once

#include <cassert>
#include <cstdint>
#include <atomic>
#include <vector>

namespace queue
{
  struct Reserved
  {
    std::size_t seq_no_;
    std::size_t count_;
  };

  constexpr std::uint8_t CalcMask(const std::size_t num) noexcept
  {
    assert(num > 1);
    const auto mask = (num - 1);
    assert((num & mask) == 0);
    return mask;
  }

  struct MPSCLockFreeQueue //: private boost::noncopyable
  {
    explicit MPSCLockFreeQueue(const std::size_t capacity)
        : capacity_(capacity), mask_(CalcMask(capacity))
    {
      storage_.resize(capacity_);
    }

    std::size_t capacity_; // must be power of 2
    std::size_t mask_;

    constexpr auto CalcRealPos(const std::size_t pos) const
    {
      return pos & mask_;
    }

    Reserved Reserve(const std::size_t num)
    {
      Reserved res{0, 0};
      std::size_t current_reserved_head, new_reserved_head;      
      do
      {
        current_reserved_head = reserved_head_.load(std::memory_order_acquire);
        const auto current_reader_pos = tail_.load(std::memory_order_acquire);
        if (current_reader_pos + capacity_ >= current_reserved_head)
        {
          return res; // we are full
        }
        const auto max_available_elems = current_reserved_head - capacity_ - current_reader_pos;
        const auto num_items_to_reserve = std::min(max_available_elems, num);
        new_reserved_head = current_reserved_head + num_items_to_reserve;        
      } while (!reserved_head_.compare_exchange_weak(current_reserved_head, new_reserved_head, std::memory_order_release, std::memory_order_relaxed));
      res.seq_no_ = current_reserved_head + 1;
      res.count_ = new_reserved_head - current_reserved_head;
      return res;
    }

    std::atomic<std::size_t> tail_ = {0};
    std::atomic<std::size_t> head_ = {0};
    std::atomic<std::size_t> reserved_head_ = {0}; // >= tail_

    std::vector<int> storage_;
  };
} // namespace queue
