/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/rate_statistics.h"

#include <algorithm>
#include <limits>
#include <memory>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

RateStatistics::RateStatistics(int64_t window_size_ms, float scale)
    : buckets_(new Bucket[window_size_ms]()),
      accumulated_count_(0),
      num_samples_(0),
      oldest_time_(-window_size_ms),
      oldest_index_(0),
      scale_(scale),
      max_window_size_ms_(window_size_ms),
      current_window_size_ms_(max_window_size_ms_) {}

RateStatistics::RateStatistics(const RateStatistics& other)
    : accumulated_count_(other.accumulated_count_),
      overflow_(other.overflow_),
      num_samples_(other.num_samples_),
      oldest_time_(other.oldest_time_),
      oldest_index_(other.oldest_index_),
      scale_(other.scale_),
      max_window_size_ms_(other.max_window_size_ms_),
      current_window_size_ms_(other.current_window_size_ms_) {
  buckets_ = std::make_unique<Bucket[]>(other.max_window_size_ms_);
  std::copy(other.buckets_.get(),
            other.buckets_.get() + other.max_window_size_ms_, buckets_.get());
}

RateStatistics::RateStatistics(RateStatistics&& other) = default;

RateStatistics::~RateStatistics() {}

void RateStatistics::Reset() {
  accumulated_count_ = 0;
  overflow_ = false;
  num_samples_ = 0;
  oldest_time_ = -max_window_size_ms_;
  oldest_index_ = 0;
  current_window_size_ms_ = max_window_size_ms_;
  for (int64_t i = 0; i < max_window_size_ms_; i++)
    buckets_[i] = Bucket();
}

void RateStatistics::Update(int64_t count, int64_t now_ms) {
  RTC_DCHECK_LE(0, count);
  if (now_ms < oldest_time_) {
    // Too old data is ignored.
    return;
  }

  EraseOld(now_ms);

  // First ever sample, reset window to start now.
  if (!IsInitialized())
    oldest_time_ = now_ms;

  uint32_t now_offset = rtc::dchecked_cast<uint32_t>(now_ms - oldest_time_);
  RTC_DCHECK_LT(now_offset, max_window_size_ms_);
  uint32_t index = oldest_index_ + now_offset;
  if (index >= max_window_size_ms_)
    index -= max_window_size_ms_;
  buckets_[index].sum += count;
  ++buckets_[index].samples;
  if (std::numeric_limits<int64_t>::max() - accumulated_count_ > count) {
    accumulated_count_ += count;
  } else {
    overflow_ = true;
  }
  ++num_samples_;
}

absl::optional<int64_t> RateStatistics::Rate(int64_t now_ms) const {
  // Yeah, this const_cast ain't pretty, but the alternative is to declare most
  // of the members as mutable...
  const_cast<RateStatistics*>(this)->EraseOld(now_ms);

  // If window is a single bucket or there is only one sample in a data set that
  // has not grown to the full window size, or if the accumulator has
  // overflowed, treat this as rate unavailable.
  int active_window_size = now_ms - oldest_time_ + 1;
  if (num_samples_ == 0 || active_window_size <= 1 ||
      (num_samples_ <= 1 &&
       rtc::SafeLt(active_window_size, current_window_size_ms_)) ||
      overflow_) {
    return absl::nullopt;
  }

  float scale = static_cast<float>(scale_) / active_window_size;
  float result = accumulated_count_ * scale + 0.5f;

  // Better return unavailable rate than garbage value (undefined behavior).
  if (result > static_cast<float>(std::numeric_limits<int64_t>::max())) {
    return absl::nullopt;
  }
  return rtc::dchecked_cast<int64_t>(result);
}

void RateStatistics::EraseOld(int64_t now_ms) {
  if (!IsInitialized())
    return;

  // New oldest time that is included in data set.
  int64_t new_oldest_time = now_ms - current_window_size_ms_ + 1;

  // New oldest time is older than the current one, no need to cull data.
  if (new_oldest_time <= oldest_time_)
    return;

  // Loop over buckets and remove too old data points.
  while (num_samples_ > 0 && oldest_time_ < new_oldest_time) {
    const Bucket& oldest_bucket = buckets_[oldest_index_];
    RTC_DCHECK_GE(accumulated_count_, oldest_bucket.sum);
    RTC_DCHECK_GE(num_samples_, oldest_bucket.samples);
    accumulated_count_ -= oldest_bucket.sum;
    num_samples_ -= oldest_bucket.samples;
    buckets_[oldest_index_] = Bucket();
    if (++oldest_index_ >= max_window_size_ms_)
      oldest_index_ = 0;
    ++oldest_time_;
    // This does not clear overflow_ even when counter is empty.
    // TODO(https://bugs.webrtc.org/11247): Consider if overflow_ can be reset.
  }
  oldest_time_ = new_oldest_time;
}

bool RateStatistics::SetWindowSize(int64_t window_size_ms, int64_t now_ms) {
  if (window_size_ms <= 0 || window_size_ms > max_window_size_ms_)
    return false;
  current_window_size_ms_ = window_size_ms;
  EraseOld(now_ms);
  return true;
}

bool RateStatistics::IsInitialized() const {
  return oldest_time_ != -max_window_size_ms_;
}

}  // namespace webrtc
