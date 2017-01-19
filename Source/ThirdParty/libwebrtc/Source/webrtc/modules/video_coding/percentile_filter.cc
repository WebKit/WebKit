/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/percentile_filter.h"

#include <iterator>

#include "webrtc/base/checks.h"

namespace webrtc {

PercentileFilter::PercentileFilter(float percentile)
    : percentile_(percentile),
      percentile_it_(set_.begin()),
      percentile_index_(0) {
  RTC_CHECK_GE(percentile, 0.0f);
  RTC_CHECK_LE(percentile, 1.0f);
}

void PercentileFilter::Insert(const int64_t& value) {
  // Insert element at the upper bound.
  set_.insert(value);
  if (set_.size() == 1u) {
    // First element inserted - initialize percentile iterator and index.
    percentile_it_ = set_.begin();
    percentile_index_ = 0;
  } else if (value < *percentile_it_) {
    // If new element is before us, increment |percentile_index_|.
    ++percentile_index_;
  }
  UpdatePercentileIterator();
}

void PercentileFilter::Erase(const int64_t& value) {
  std::multiset<int64_t>::const_iterator it = set_.lower_bound(value);
  // Ignore erase operation if the element is not present in the current set.
  if (it == set_.end() || *it != value)
    return;
  if (it == percentile_it_) {
    // If same iterator, update to the following element. Index is not affected.
    percentile_it_ = set_.erase(it);
  } else {
    set_.erase(it);
    // If erased element was before us, decrement |percentile_index_|.
    if (value <= *percentile_it_)
      --percentile_index_;
  }
  UpdatePercentileIterator();
}

void PercentileFilter::UpdatePercentileIterator() {
  if (set_.empty())
    return;
  const int64_t index = static_cast<int64_t>(percentile_ * (set_.size() - 1));
  std::advance(percentile_it_, index - percentile_index_);
  percentile_index_ = index;
}

int64_t PercentileFilter::GetPercentileValue() const {
  return set_.empty() ? 0 : *percentile_it_;
}

}  // namespace webrtc
