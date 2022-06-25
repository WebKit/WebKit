/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_MULTI_HEAD_QUEUE_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_MULTI_HEAD_QUEUE_H_

#include <deque>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// A queue that allows more than one reader. Readers are independent, and all
// readers will see all elements; an inserted element stays in the queue until
// all readers have extracted it. Elements are copied and copying is assumed to
// be cheap.
template <typename T>
class MultiHeadQueue {
 public:
  // Creates queue with exactly |readers_count| readers.
  explicit MultiHeadQueue(size_t readers_count) {
    for (size_t i = 0; i < readers_count; ++i) {
      queues_.push_back(std::deque<T>());
    }
  }

  // Creates a copy of an existing head. Complexity O(MultiHeadQueue::size()).
  void AddHead(size_t copy_index) { queues_.push_back(queues_[copy_index]); }

  // Add value to the end of the queue. Complexity O(readers_count).
  void PushBack(T value) {
    for (auto& queue : queues_) {
      queue.push_back(value);
    }
  }

  // Extract element from specified head. Complexity O(1).
  absl::optional<T> PopFront(size_t index) {
    RTC_CHECK_LT(index, queues_.size());
    if (queues_[index].empty()) {
      return absl::nullopt;
    }
    T out = queues_[index].front();
    queues_[index].pop_front();
    return out;
  }

  // Returns element at specified head. Complexity O(1).
  absl::optional<T> Front(size_t index) const {
    RTC_CHECK_LT(index, queues_.size());
    if (queues_[index].empty()) {
      return absl::nullopt;
    }
    return queues_[index].front();
  }

  // Returns true if for specified head there are no more elements in the queue
  // or false otherwise. Complexity O(1).
  bool IsEmpty(size_t index) const {
    RTC_CHECK_LT(index, queues_.size());
    return queues_[index].empty();
  }

  // Returns size of the longest queue between all readers.
  // Complexity O(readers_count).
  size_t size() const {
    size_t size = 0;
    for (auto& queue : queues_) {
      if (queue.size() > size) {
        size = queue.size();
      }
    }
    return size;
  }

  // Returns size of the specified queue. Complexity O(1).
  size_t size(size_t index) const {
    RTC_CHECK_LT(index, queues_.size());
    return queues_[index].size();
  }

  size_t readers_count() const { return queues_.size(); }

 private:
  std::vector<std::deque<T>> queues_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_MULTI_HEAD_QUEUE_H_
