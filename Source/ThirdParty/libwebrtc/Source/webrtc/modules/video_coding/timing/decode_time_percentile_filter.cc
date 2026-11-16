/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/decode_time_percentile_filter.h"

#include <algorithm>
#include <cstdint>

namespace webrtc {
namespace {

// The number of initial samples to ignore.
constexpr int kIgnoredSampleCount = 5;
// The percentile value used by the filter.
constexpr float kPercentile = 0.95f;
// The window size in ms.
constexpr int64_t kTimeLimitMs = 10000;

}  // namespace

DecodeTimePercentileFilter::DecodeTimePercentileFilter()
    : filter_(kPercentile) {}
DecodeTimePercentileFilter::~DecodeTimePercentileFilter() = default;

void DecodeTimePercentileFilter::AddSample(int64_t decode_time_ms,
                                           int64_t now_ms) {
  // Ignore the first samples.
  if (ignored_sample_count_ < kIgnoredSampleCount) {
    ++ignored_sample_count_;
    return;
  }

  // Insert new decode time value.
  const int64_t capped_decode_ms = std::max(int64_t{0}, decode_time_ms);
  filter_.Insert(capped_decode_ms);
  history_.emplace(capped_decode_ms, now_ms);

  // Pop old decode time values.
  while (!history_.empty() &&
         now_ms - history_.front().sample_time_ms > kTimeLimitMs) {
    filter_.Erase(history_.front().decode_time_ms);
    history_.pop();
  }
}

int64_t DecodeTimePercentileFilter::GetPercentileMs() const {
  return filter_.GetPercentileValue();
}

DecodeTimePercentileFilter::Sample::Sample(int64_t decode_time_ms,
                                           int64_t sample_time_ms)
    : decode_time_ms(decode_time_ms), sample_time_ms(sample_time_ms) {}

}  // namespace webrtc
