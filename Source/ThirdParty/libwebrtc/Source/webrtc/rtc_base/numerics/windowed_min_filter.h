/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_WINDOWED_MIN_FILTER_H_
#define RTC_BASE_NUMERICS_WINDOWED_MIN_FILTER_H_

#include <deque>

#include "rtc_base/checks.h"

namespace webrtc {

template <typename V>
class WindowedMinFilter {
 public:
  explicit WindowedMinFilter(int window_length) : max_size_(window_length) {
    RTC_DCHECK_GT(window_length, 1);
  }

  void Insert(V value) {
    if (!min_values_.empty()) {
      if (min_values_.front().index == index_) {
        // Min value is too old.
        min_values_.pop_front();
      }

      // If value <= min_values_.front().value, value is the minimum value and
      // we can forget all other. The alternative had been to always
      // check the back value, but then we would also have to check for
      // empty.
      if (min_values_.front().value >= value) {
        min_values_.clear();
      } else {
        while (min_values_.back().value >= value) {
          min_values_.pop_back();
        }
      }
    }
    RTC_DCHECK_LT(min_values_.size(), max_size_);
    min_values_.push_back({.value = value, .index = index_});
    index_ = (index_ + 1) % max_size_;
  }

  // Returns the min value within the window. If no value has been inserted,
  // returns the default value of V.
  V GetMin() const {
    if (min_values_.empty()) {
      return V();
    }
    return min_values_.front().value;
  }

  void Reset() {
    min_values_.clear();
    index_ = 0;
  }

 private:
  const int max_size_;
  struct ValueAndIndex {
    V value;
    int index;
  };

  int index_ = 0;
  std::deque<ValueAndIndex> min_values_;
};

}  // namespace webrtc

#endif  // RTC_BASE_NUMERICS_WINDOWED_MIN_FILTER_H_
