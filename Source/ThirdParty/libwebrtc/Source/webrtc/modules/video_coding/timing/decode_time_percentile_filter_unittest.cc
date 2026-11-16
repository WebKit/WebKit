/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/decode_time_percentile_filter.h"

#include <cstdint>

#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr int64_t kDecodeTimeMs = 44;
constexpr int64_t kNowMs = 1234;
// From decode_time_percentile_filter.cc.
constexpr int kIgnoredSampleCount = 5;
constexpr int64_t kTimeLimitMs = 10000;

void AddSamples(DecodeTimePercentileFilter& filter,
                int count,
                int64_t decode_time_ms,
                int64_t now_ms) {
  for (int i = 0; i < count; ++i) {
    filter.AddSample(decode_time_ms, now_ms);
  }
}

TEST(DecodeTimePercentileFilterTest, InitiallyReturnsZero) {
  DecodeTimePercentileFilter filter;
  EXPECT_EQ(filter.GetPercentileMs(), 0);
}

TEST(DecodeTimePercentileFilterTest, IgnoresFirstSamples) {
  DecodeTimePercentileFilter filter;
  AddSamples(filter, kIgnoredSampleCount, kDecodeTimeMs, kNowMs);
  EXPECT_EQ(filter.GetPercentileMs(), 0);
}

TEST(DecodeTimePercentileFilterTest, IncludesSampleAfterIgnored) {
  DecodeTimePercentileFilter filter;
  AddSamples(filter, kIgnoredSampleCount + 1, kDecodeTimeMs, kNowMs);
  EXPECT_EQ(filter.GetPercentileMs(), kDecodeTimeMs);
}

TEST(DecodeTimePercentileFilterTest, CapsNegativeSamplesToZero) {
  DecodeTimePercentileFilter filter;
  AddSamples(filter, kIgnoredSampleCount + 1, /*decode_time_ms=*/-1, kNowMs);
  EXPECT_EQ(filter.GetPercentileMs(), 0);
}

TEST(DecodeTimePercentileFilterTest, Returns95thPercentile) {
  DecodeTimePercentileFilter filter;
  AddSamples(filter, kIgnoredSampleCount, kDecodeTimeMs, kNowMs);
  for (int i = 1; i <= 20; ++i) {
    filter.AddSample(/*decode_time_ms=*/i, kNowMs);
  }
  EXPECT_EQ(filter.GetPercentileMs(), 19);
}

TEST(DecodeTimePercentileFilterTest, KeepsSamplesWithinTimeWindow) {
  DecodeTimePercentileFilter filter;
  AddSamples(filter, kIgnoredSampleCount + 10, kDecodeTimeMs, kNowMs);
  EXPECT_EQ(filter.GetPercentileMs(), kDecodeTimeMs);

  filter.AddSample(/*decode_time_ms=*/3, kNowMs + kTimeLimitMs);
  EXPECT_EQ(filter.GetPercentileMs(), kDecodeTimeMs);
}

TEST(DecodeTimePercentileFilterTest, DiscardsSamplesOutsideTimeWindow) {
  DecodeTimePercentileFilter filter;
  AddSamples(filter, kIgnoredSampleCount + 10, kDecodeTimeMs, kNowMs);
  EXPECT_EQ(filter.GetPercentileMs(), kDecodeTimeMs);

  filter.AddSample(/*decode_time_ms=*/3, kNowMs + kTimeLimitMs + 1);
  EXPECT_EQ(filter.GetPercentileMs(), 3);
}

}  // namespace
}  // namespace webrtc
