//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// count_min_sketch.cpp
//
// Identification: src/primer/count_min_sketch.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "primer/count_min_sketch.h"
#include <algorithm>
#include <atomic>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace bustub {
static_assert(std::atomic<uint32_t>::is_always_lock_free, "count-min sketch counters must be lock-free");
/**
 * Constructor for the count-min sketch.
 *
 * @param width The width of the sketch matrix.
 * @param depth The depth of the sketch matrix.
 * @throws std::invalid_argument if width or depth are zero.
 */
template <typename KeyType>
CountMinSketch<KeyType>::CountMinSketch(uint32_t width, uint32_t depth)
    : width_(width), depth_(depth), counters_(static_cast<size_t>(width) * depth) {
  /** @TODO(student) Implement this function! */
  if (width_ == 0 || depth_ == 0) {
    throw std::invalid_argument("CountMinSketch width and depth must both be greater than zero.");
  }

  /** @spring2026 PLEASE DO NOT MODIFY THE FOLLOWING */
  // Initialize seeded hash functions
  // deth就是有几行
  // 把哈希函数存矩阵里
  hash_functions_.reserve(depth_);
  for (size_t i = 0; i < depth_; i++) {
    hash_functions_.push_back(this->HashFunction(i));
  }
}
// 它负责把一个临时/即将销毁的 CountMinSketch 对象的资源"偷"过来，同时把对方置为安全空状态。
template <typename KeyType>
CountMinSketch<KeyType>::CountMinSketch(CountMinSketch &&other) noexcept
    : width_(std::exchange(other.width_, 0U)),
      depth_(std::exchange(other.depth_, 0U)),
      hash_functions_(std::exchange(other.hash_functions_, {})),
      counters_(std::exchange(other.counters_, {})) {
  /** @TODO(student) Implement this function! */
  RebindHashFunctions();
}

template <typename KeyType>
auto CountMinSketch<KeyType>::operator=(CountMinSketch &&other) noexcept -> CountMinSketch & {
  /** @TODO(student) Implement this function! */
  if (this != &other) {
    width_ = std::exchange(other.width_, 0U);
    depth_ = std::exchange(other.depth_, 0U);
    hash_functions_ = std::exchange(other.hash_functions_, {});
    counters_ = std::exchange(other.counters_, {});
    RebindHashFunctions();
  }
  return *this;
}

template <typename KeyType>
void CountMinSketch<KeyType>::Insert(const KeyType &item) {
  /** @TODO(student) Implement this function! */
  for (uint32_t row = 0; row < depth_; ++row) {
    counters_[CellIndex(row, item)].fetch_add(1, std::memory_order_relaxed);
  }
}

template <typename KeyType>
void CountMinSketch<KeyType>::Merge(const CountMinSketch<KeyType> &other) {
  if (width_ != other.width_ || depth_ != other.depth_) {
    throw std::invalid_argument("Incompatible CountMinSketch dimensions for merge.");
  }
  /** @TODO(student) Implement this function! */
  for (size_t i = 0; i < counters_.size(); ++i) {
    counters_[i].fetch_add(other.counters_[i].load(std::memory_order_relaxed), std::memory_order_relaxed);
  }
}

template <typename KeyType>
auto CountMinSketch<KeyType>::Count(const KeyType &item) const -> uint32_t {
  if (depth_ == 0) {
    return 0;
  }
  uint32_t estimate = std::numeric_limits<uint32_t>::max();
  for (uint32_t row = 0; row < depth_; ++row) {
    estimate = std::min(estimate, counters_[CellIndex(row, item)].load(std::memory_order_relaxed));
  }
  // 内存序参数——告诉 CPU："读就行，不用保证和其他线程的读写顺序同步"
  return estimate;
}

template <typename KeyType>
void CountMinSketch<KeyType>::Clear() {
  /** @TODO(student) Implement this function! */
  for (auto &counter : counters_) {
    counter.store(0, std::memory_order_relaxed);
  }
}
// 每个都要去查一遍
template <typename KeyType>
auto CountMinSketch<KeyType>::TopK(uint16_t k, const std::vector<KeyType> &candidates)
    -> std::vector<std::pair<KeyType, uint32_t>> {
  /** @TODO(student) Implement this function! */
  std::vector<std::pair<uint32_t, size_t>> ranked;
  ranked.reserve(candidates.size());
  for (size_t i = 0; i < candidates.size(); ++i) {
    // 没有在外面先造一个临时对象，再复制进去。​ 而是直接把原材料（参数）交给 vector
    // 内部，就地构造。
    ranked.emplace_back(Count(candidates[i]), i);
  }

  auto ranks_higher = [](const std::pair<uint32_t, size_t> &lhs, const std::pair<uint32_t, size_t> &rhs) {
    if (lhs.first != rhs.first) {
      return lhs.first > rhs.first;
    }
    return lhs.second < rhs.second;
  };

  const size_t result_size = std::min(static_cast<size_t>(k), ranked.size());
  const auto top_end = ranked.begin() + static_cast<std::ptrdiff_t>(result_size);
  std::partial_sort(ranked.begin(), top_end, ranked.end(), ranks_higher);

  std::vector<std::pair<KeyType, uint32_t>> top_k;
  top_k.reserve(result_size);
  for (size_t i = 0; i < result_size; ++i) {
    top_k.emplace_back(candidates[ranked[i].second], ranked[i].first);
  }
  return top_k;
}
template <typename KeyType>
auto CountMinSketch<KeyType>::CellIndex(uint32_t row, const KeyType &item) const -> size_t {
  // 看第row行它在哪个位置
  return (static_cast<size_t>(row) * width_) + hash_functions_[row](item);
}

template <typename KeyType>
void CountMinSketch<KeyType>::RebindHashFunctions() {
  for (size_t row = 0; row < hash_functions_.size(); ++row) {
    hash_functions_[row] = this->HashFunction(row);
    // lambda 里需要 this 是因为它用了成员变量 width_。成员变量属于对象，必须通过 this 指针才能访问。捕获 this 就是给
    // lambda 一个"回去找对象"的地址。
  }
}
// Explicit instantiations for all types used in tests
template class CountMinSketch<std::string>;
template class CountMinSketch<int64_t>;  // For int64_t tests
template class CountMinSketch<int>;      // This covers both int and int32_t
}  // namespace bustub
