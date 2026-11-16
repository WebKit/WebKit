/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/timestamp_aligner.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/random.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
// Computes the difference x_k - mean(x), when x_k is the linear sequence x_k =
// k, and the "mean" is plain mean for the first `window_size` samples, followed
// by exponential averaging with weight 1 / `window_size` for each new sample.
// This is needed to predict the effect of camera clock drift on the timestamp
// translation. See the comment on TimestampAligner::UpdateOffset for more
// context.
double MeanTimeDifference(int nsamples, int window_size) {
  if (nsamples <= window_size) {
    // Plain averaging.
    return nsamples / 2.0;
  } else {
    // Exponential convergence towards
    // interval_error * (window_size - 1)
    double alpha = 1.0 - 1.0 / window_size;

    return ((window_size - 1) -
            (window_size / 2.0 - 1) * pow(alpha, nsamples - window_size));
  }
}

class TimestampAlignerForTest : public TimestampAligner {
  // Make internal methods accessible to testing.
 public:
  using TimestampAligner::ClipTimestamp;
  using TimestampAligner::UpdateOffset;
};

void TestTimestampFilter(double rel_freq_error) {
  TimestampAlignerForTest timestamp_aligner_for_test;
  TimestampAligner timestamp_aligner;

  constexpr Timestamp kSystemStart = Timestamp::Micros(123456);
  SimulatedClock clock(kSystemStart);

  const Timestamp kEpoch = Timestamp::Micros(10000);
  const TimeDelta kJitter = TimeDelta::Micros(5000);
  const TimeDelta kInterval = TimeDelta::Micros(33333);  // 30 FPS
  const int kWindowSize = 100;
  const int kNumFrames = 3 * kWindowSize;

  TimeDelta interval_error = kInterval * rel_freq_error;
  Random random(17);

  Timestamp prev_translated_time = kSystemStart;

  for (int i = 0; i < kNumFrames; i++) {
    // Camera time subject to drift.
    Timestamp camera_time = kEpoch + i * (kInterval + interval_error);
    Timestamp system_time = kSystemStart + i * kInterval;
    // And system time readings are subject to jitter.
    Timestamp system_measured =
        system_time + TimeDelta::Micros(random.Rand(kJitter.us()));

    int64_t offset_us = timestamp_aligner_for_test.UpdateOffset(
        camera_time.us(), system_measured.us());

    Timestamp filtered_time = camera_time + TimeDelta::Micros(offset_us);
    Timestamp translated_time =
        Timestamp::Micros(timestamp_aligner_for_test.ClipTimestamp(
            filtered_time.us(), system_measured.us()));

    // Check that we get identical result from the all-in-one helper method.
    ASSERT_EQ(translated_time.us(),
              timestamp_aligner.TranslateTimestamp(camera_time.us(),
                                                   system_measured.us()));

    EXPECT_LE(translated_time, system_measured);
    EXPECT_GE(translated_time, prev_translated_time + TimeDelta::Millis(1));

    // The relative frequency error contributes to the expected error
    // by a factor which is the difference between the current time
    // and the average of earlier sample times.
    TimeDelta expected_error =
        kJitter / 2 +
        rel_freq_error * kInterval * MeanTimeDifference(i, kWindowSize);

    TimeDelta bias = filtered_time - translated_time;
    EXPECT_GE(bias, TimeDelta::Zero());

    if (i == 0) {
      EXPECT_EQ(translated_time, system_measured);
    } else {
      EXPECT_NEAR(filtered_time.us(), (system_time + expected_error).us(),
                  2.0 * kJitter.us() / sqrt(std::max(i, kWindowSize)));
    }
    // If the camera clock runs too fast (rel_freq_error > 0.0), The
    // bias is expected to roughly cancel the expected error from the
    // clock drift, as this grows. Otherwise, it reflects the
    // measurement noise. The tolerances here were selected after some
    // trial and error.
    if (i < 10 || rel_freq_error <= 0.0) {
      EXPECT_LE(bias, TimeDelta::Micros(3000));
    } else {
      EXPECT_NEAR(bias.us(), expected_error.us(), 1500);
    }
    prev_translated_time = translated_time;
  }
}

}  // Anonymous namespace

TEST(TimestampAlignerTest, AttenuateTimestampJitterNoDrift) {
  TestTimestampFilter(0.0);
}

// 100 ppm is a worst case for a reasonable crystal.
TEST(TimestampAlignerTest, AttenuateTimestampJitterSmallPosDrift) {
  TestTimestampFilter(0.0001);
}

TEST(TimestampAlignerTest, AttenuateTimestampJitterSmallNegDrift) {
  TestTimestampFilter(-0.0001);
}

// 3000 ppm, 3 ms / s, is the worst observed drift, see
// https://bugs.chromium.org/p/webrtc/issues/detail?id=5456
TEST(TimestampAlignerTest, AttenuateTimestampJitterLargePosDrift) {
  TestTimestampFilter(0.003);
}

TEST(TimestampAlignerTest, AttenuateTimestampJitterLargeNegDrift) {
  TestTimestampFilter(-0.003);
}

// Exhibits a mostly hypothetical problem, where certain inputs to the
// TimestampAligner.UpdateOffset filter result in non-monotonous
// translated timestamps. This test verifies that the ClipTimestamp
// logic handles this case correctly.
TEST(TimestampAlignerTest, ClipToMonotonous) {
  TimestampAlignerForTest timestamp_aligner;

  // For system time stamps { 0, s1, s1 + s2 }, and camera timestamps
  // {0, c1, c1 + c2}, we exhibit non-monotonous behaviour if and only
  // if c1 > s1 + 2 s2 + 4 c2.
  const int kNumSamples = 3;
  const Timestamp kCaptureTime[kNumSamples] = {
      Timestamp::Micros(0), Timestamp::Micros(80000), Timestamp::Micros(90001)};
  const Timestamp kSystemTime[kNumSamples] = {
      Timestamp::Micros(0), Timestamp::Micros(10000), Timestamp::Micros(20000)};
  const TimeDelta expected_offset[kNumSamples] = {TimeDelta::Micros(0),
                                                  TimeDelta::Micros(-35000),
                                                  TimeDelta::Micros(-46667)};

  // Non-monotonic translated timestamps can happen when only for
  // translated timestamps in the future. Which is tolerated if
  // `timestamp_aligner.clip_bias_us` is large enough. Instead of
  // changing that private member for this test, just add the bias to
  // `kSystemTimeUs` when calling ClipTimestamp.
  const TimeDelta kClipBias = TimeDelta::Micros(100000);

  bool did_clip = false;
  std::optional<Timestamp> prev_timestamp;
  for (int i = 0; i < kNumSamples; i++) {
    TimeDelta offset = TimeDelta::Micros(timestamp_aligner.UpdateOffset(
        kCaptureTime[i].us(), kSystemTime[i].us()));
    EXPECT_EQ(offset, expected_offset[i]);

    Timestamp translated_timestamp = kCaptureTime[i] + offset;
    Timestamp clip_timestamp =
        Timestamp::Micros(timestamp_aligner.ClipTimestamp(
            translated_timestamp.us(), (kSystemTime[i] + kClipBias).us()));
    if (prev_timestamp && translated_timestamp <= *prev_timestamp) {
      did_clip = true;
      EXPECT_EQ(clip_timestamp, *prev_timestamp + TimeDelta::Millis(1));
    } else {
      // No change from clipping.
      EXPECT_EQ(clip_timestamp, translated_timestamp);
    }
    prev_timestamp = clip_timestamp;
  }
  EXPECT_TRUE(did_clip);
}

TEST(TimestampAlignerTest, TranslateTimestampWithoutStateUpdate) {
  TimestampAligner timestamp_aligner;

  constexpr int kNumSamples = 4;
  constexpr Timestamp kCaptureTime[kNumSamples] = {
      Timestamp::Micros(0), Timestamp::Micros(80000), Timestamp::Micros(90001),
      Timestamp::Micros(100000)};
  constexpr Timestamp kSystemTime[kNumSamples] = {
      Timestamp::Micros(0), Timestamp::Micros(10000), Timestamp::Micros(20000),
      Timestamp::Micros(30000)};
  constexpr TimeDelta kQueryCaptureTimeOffset[kNumSamples] = {
      TimeDelta::Micros(0), TimeDelta::Micros(123), TimeDelta::Micros(-321),
      TimeDelta::Micros(345)};

  for (int i = 0; i < kNumSamples; i++) {
    Timestamp reference_timestamp =
        Timestamp::Micros(timestamp_aligner.TranslateTimestamp(
            kCaptureTime[i].us(), kSystemTime[i].us()));
    EXPECT_EQ((reference_timestamp - kQueryCaptureTimeOffset[i]).us(),
              timestamp_aligner.TranslateTimestamp(
                  (kCaptureTime[i] - kQueryCaptureTimeOffset[i]).us()));
  }
}

}  // namespace webrtc
