/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_TIMING_DECODE_TIME_PERCENTILE_FILTER_H_
#define MODULES_VIDEO_CODING_TIMING_DECODE_TIME_PERCENTILE_FILTER_H_

#include <cstdint>
#include <queue>

#include "rtc_base/numerics/percentile_filter.h"

namespace webrtc {

// This class estimates the 95th percentile of per-frame decode times. This
// estimate can be used to determine how large the "decode delay term" should be
// when determining the render timestamp for a frame.
class DecodeTimePercentileFilter {
 public:
  DecodeTimePercentileFilter();
  ~DecodeTimePercentileFilter();

  // Adds a new decode time to the filter.
  void AddSample(int64_t decode_time_ms, int64_t now_ms);

  // Returns the 95th percentile of observed decode times within a time window,
  // in milliseconds.
  int64_t GetPercentileMs() const;

 private:
  struct Sample {
    Sample(int64_t decode_time_ms, int64_t sample_time_ms);
    int64_t decode_time_ms;
    int64_t sample_time_ms;
  };

  // The number of samples ignored so far.
  int ignored_sample_count_ = 0;
  // Queue with history of latest decode time values.
  std::queue<Sample> history_;
  // `filter_` contains the same values as `history_`, but in a data structure
  // that allows efficient retrieval of the percentile value.
  PercentileFilter<int64_t> filter_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_TIMING_DECODE_TIME_PERCENTILE_FILTER_H_
