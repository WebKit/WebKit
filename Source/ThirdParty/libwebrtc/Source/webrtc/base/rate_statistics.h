/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_RATE_STATISTICS_H_
#define WEBRTC_BASE_RATE_STATISTICS_H_

#include <memory>

#include "webrtc/base/optional.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class RateStatistics {
 public:
  static constexpr float kBpsScale = 8000.0f;

  // max_window_size_ms = Maximum window size in ms for the rate estimation.
  //                      Initial window size is set to this, but may be changed
  //                      to something lower by calling SetWindowSize().
  // scale = coefficient to convert counts/ms to desired unit
  //         ex: kBpsScale (8000) for bits/s if count represents bytes.
  RateStatistics(int64_t max_window_size_ms, float scale);
  ~RateStatistics();

  // Reset instance to original state.
  void Reset();

  // Update rate with a new data point, moving averaging window as needed.
  void Update(size_t count, int64_t now_ms);

  // Note that despite this being a const method, it still updates the internal
  // state (moves averaging window), but it doesn't make any alterations that
  // are observable from the other methods, as long as supplied timestamps are
  // from a monotonic clock. Ie, it doesn't matter if this call moves the
  // window, since any subsequent call to Update or Rate would still have moved
  // the window as much or more.
  rtc::Optional<uint32_t> Rate(int64_t now_ms) const;

  // Update the size of the averaging window. The maximum allowed value for
  // window_size_ms is max_window_size_ms as supplied in the constructor.
  bool SetWindowSize(int64_t window_size_ms, int64_t now_ms);

 private:
  void EraseOld(int64_t now_ms);
  bool IsInitialized() const;

  // Counters are kept in buckets (circular buffer), with one bucket
  // per millisecond.
  struct Bucket {
    size_t sum;      // Sum of all samples in this bucket.
    size_t samples;  // Number of samples in this bucket.
  };
  std::unique_ptr<Bucket[]> buckets_;

  // Total count recorded in buckets.
  size_t accumulated_count_;

  // The total number of samples in the buckets.
  size_t num_samples_;

  // Oldest time recorded in buckets.
  int64_t oldest_time_;

  // Bucket index of oldest counter recorded in buckets.
  uint32_t oldest_index_;

  // To convert counts/ms to desired units
  const float scale_;

  // The window sizes, in ms, over which the rate is calculated.
  const int64_t max_window_size_ms_;
  int64_t current_window_size_ms_;
};
}  // namespace webrtc

#endif  // WEBRTC_BASE_RATE_STATISTICS_H_
