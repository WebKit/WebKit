/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/timestamp_extrapolator.h"

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>

#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/metrics.h"
#include "test/create_test_field_trials.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::Eq;
using ::testing::Optional;

namespace {

constexpr Frequency kRtpHz = Frequency::KiloHertz(90);
constexpr Frequency k25Fps = Frequency::Hertz(25);
constexpr TimeDelta k25FpsDelay = 1 / k25Fps;

}  // namespace

TEST(TimestampExtrapolatorTest, ExtrapolationOccursAfter2Packets) {
  SimulatedClock clock(Timestamp::Millis(1337));
  TimestampExtrapolator ts_extrapolator(clock.CurrentTime(),
                                        CreateTestFieldTrials());

  // No packets so no timestamp.
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(90000), Eq(std::nullopt));

  uint32_t rtp = 90000;
  clock.AdvanceTime(k25FpsDelay);
  // First result is a bit confusing since it is based off the "start" time,
  // which is arbitrary.
  ts_extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(rtp),
              Optional(clock.CurrentTime()));

  rtp += kRtpHz / k25Fps;
  clock.AdvanceTime(k25FpsDelay);
  ts_extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(rtp),
              Optional(clock.CurrentTime()));
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(rtp + 90000),
              Optional(clock.CurrentTime() + TimeDelta::Seconds(1)));
}

TEST(TimestampExtrapolatorTest, ResetsAfter10SecondPause) {
  SimulatedClock clock(Timestamp::Millis(1337));
  TimestampExtrapolator ts_extrapolator(clock.CurrentTime(),
                                        CreateTestFieldTrials());

  uint32_t rtp = 90000;
  ts_extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(rtp),
              Optional(clock.CurrentTime()));

  rtp += kRtpHz / k25Fps;
  clock.AdvanceTime(k25FpsDelay);
  ts_extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(rtp),
              Optional(clock.CurrentTime()));

  rtp += 10 * kRtpHz.hertz();
  clock.AdvanceTime(TimeDelta::Seconds(10) + TimeDelta::Micros(1));
  ts_extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(rtp),
              Optional(clock.CurrentTime()));
}

TEST(TimestampExtrapolatorTest, TimestampExtrapolatesMultipleRtpWrapArounds) {
  SimulatedClock clock(Timestamp::Millis(1337));
  TimestampExtrapolator ts_extrapolator(clock.CurrentTime(),
                                        CreateTestFieldTrials());

  uint32_t rtp = std::numeric_limits<uint32_t>::max();
  ts_extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(rtp),
              Optional(clock.CurrentTime()));

  // One overflow. Static cast to avoid undefined behaviour with +=.
  rtp += static_cast<uint32_t>(kRtpHz / k25Fps);
  clock.AdvanceTime(k25FpsDelay);
  ts_extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(rtp),
              Optional(clock.CurrentTime()));

  // Assert that extrapolation works across the boundary as expected.
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(rtp + 90000),
              Optional(clock.CurrentTime() + TimeDelta::Seconds(1)));
  // This is not quite 1s since the math always rounds up.
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(rtp - 90000),
              Optional(clock.CurrentTime() - TimeDelta::Millis(999)));

  // In order to avoid a wrap arounds reset, add a packet every 10s until we
  // overflow twice.
  constexpr TimeDelta kRtpOverflowDelay =
      std::numeric_limits<uint32_t>::max() / kRtpHz;
  const Timestamp overflow_time = clock.CurrentTime() + kRtpOverflowDelay * 2;

  while (clock.CurrentTime() < overflow_time) {
    clock.AdvanceTime(TimeDelta::Seconds(10));
    // Static-cast before += to avoid undefined behaviour of overflow.
    rtp += static_cast<uint32_t>(kRtpHz * TimeDelta::Seconds(10));
    ts_extrapolator.Update(clock.CurrentTime(), rtp);
    EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(rtp),
                Optional(clock.CurrentTime()));
  }
}

TEST(TimestampExtrapolatorTest, NegativeRtpTimestampWrapAround) {
  SimulatedClock clock(Timestamp::Millis(1337));
  TimestampExtrapolator ts_extrapolator(clock.CurrentTime(),
                                        CreateTestFieldTrials());
  uint32_t rtp = 0;
  ts_extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(rtp),
              Optional(clock.CurrentTime()));
  // Go backwards! Static cast to avoid undefined behaviour with -=.
  rtp -= static_cast<uint32_t>(kRtpHz.hertz());
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(rtp),
              Optional(clock.CurrentTime() - TimeDelta::Seconds(1)));
}

TEST(TimestampExtrapolatorTest, NegativeRtpTimestampWrapAroundSecondScenario) {
  SimulatedClock clock(Timestamp::Millis(1337));
  TimestampExtrapolator ts_extrapolator(clock.CurrentTime(),
                                        CreateTestFieldTrials());
  uint32_t rtp = 0;
  ts_extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(rtp),
              Optional(clock.CurrentTime()));
  // Go backwards! Static cast to avoid undefined behaviour with -=.
  rtp -= static_cast<uint32_t>(kRtpHz * TimeDelta::Seconds(10));
  ts_extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(rtp), std::nullopt);
}

TEST(TimestampExtrapolatorTest, Slow90KHzClock) {
  // This simulates a slow camera, which produces frames at 24Hz instead of
  // 25Hz. The extrapolator should be able to resolve this with enough data.
  SimulatedClock clock(Timestamp::Millis(1337));
  TimestampExtrapolator ts_extrapolator(clock.CurrentTime(),
                                        CreateTestFieldTrials());

  constexpr TimeDelta k24FpsDelay = 1 / Frequency::Hertz(24);
  uint32_t rtp = 90000;

  // Slow camera will increment RTP at 25 FPS rate even though its producing at
  // 24 FPS. After 25 frames the extrapolator should settle at this rate.
  for (int i = 0; i < 25; ++i) {
    ts_extrapolator.Update(clock.CurrentTime(), rtp);
    rtp += kRtpHz / k25Fps;
    clock.AdvanceTime(k24FpsDelay);
  }

  // The camera would normally produce 25 frames in 90K ticks, but is slow
  // so takes 1s + k24FpsDelay for 90K ticks.
  constexpr Frequency kSlowRtpHz = 90000 / (25 * k24FpsDelay);
  // The extrapolator will be predicting that time at millisecond precision.
  auto ts = ts_extrapolator.ExtrapolateLocalTime(rtp + kSlowRtpHz.hertz());
  ASSERT_TRUE(ts.has_value());
  EXPECT_EQ(ts->ms(), clock.TimeInMilliseconds() + 1000);
}

TEST(TimestampExtrapolatorTest, Fast90KHzClock) {
  // This simulates a fast camera, which produces frames at 26Hz instead of
  // 25Hz. The extrapolator should be able to resolve this with enough data.
  SimulatedClock clock(Timestamp::Millis(1337));
  TimestampExtrapolator ts_extrapolator(clock.CurrentTime(),
                                        CreateTestFieldTrials());

  constexpr TimeDelta k26FpsDelay = 1 / Frequency::Hertz(26);
  uint32_t rtp = 90000;

  // Fast camera will increment RTP at 25 FPS rate even though its producing at
  // 26 FPS. After 25 frames the extrapolator should settle at this rate.
  for (int i = 0; i < 25; ++i) {
    ts_extrapolator.Update(clock.CurrentTime(), rtp);
    rtp += kRtpHz / k25Fps;
    clock.AdvanceTime(k26FpsDelay);
  }

  // The camera would normally produce 25 frames in 90K ticks, but is slow
  // so takes 1s + k24FpsDelay for 90K ticks.
  constexpr Frequency kSlowRtpHz = 90000 / (25 * k26FpsDelay);
  // The extrapolator will be predicting that time at millisecond precision.
  auto ts = ts_extrapolator.ExtrapolateLocalTime(rtp + kSlowRtpHz.hertz());
  ASSERT_TRUE(ts.has_value());
  EXPECT_EQ(ts->ms(), clock.TimeInMilliseconds() + 1000);
}

TEST(TimestampExtrapolatorTest, TimestampJump) {
  // This simulates a jump in RTP timestamp, which could occur if a camera was
  // swapped for example.
  SimulatedClock clock(Timestamp::Millis(1337));
  TimestampExtrapolator ts_extrapolator(clock.CurrentTime(),
                                        CreateTestFieldTrials());

  uint32_t rtp = 90000;
  clock.AdvanceTime(k25FpsDelay);
  ts_extrapolator.Update(clock.CurrentTime(), rtp);
  rtp += kRtpHz / k25Fps;
  clock.AdvanceTime(k25FpsDelay);
  ts_extrapolator.Update(clock.CurrentTime(), rtp);
  rtp += kRtpHz / k25Fps;
  clock.AdvanceTime(k25FpsDelay);
  ts_extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(rtp),
              Optional(clock.CurrentTime()));
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(rtp + 90000),
              Optional(clock.CurrentTime() + TimeDelta::Seconds(1)));

  // Jump RTP.
  uint32_t new_rtp = 1337 * 90000;
  clock.AdvanceTime(k25FpsDelay);
  ts_extrapolator.Update(clock.CurrentTime(), new_rtp);
  new_rtp += kRtpHz / k25Fps;
  clock.AdvanceTime(k25FpsDelay);
  ts_extrapolator.Update(clock.CurrentTime(), new_rtp);
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(new_rtp),
              Optional(clock.CurrentTime()));
}

TEST(TimestampExtrapolatorTest, GapInReceivedFrames) {
  SimulatedClock clock(
      Timestamp::Seconds(std::numeric_limits<uint32_t>::max() / 90000 - 31));
  TimestampExtrapolator ts_extrapolator(clock.CurrentTime(),
                                        CreateTestFieldTrials());

  uint32_t rtp = std::numeric_limits<uint32_t>::max();
  clock.AdvanceTime(k25FpsDelay);
  ts_extrapolator.Update(clock.CurrentTime(), rtp);

  rtp += 30 * 90000;
  clock.AdvanceTime(TimeDelta::Seconds(30));
  ts_extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_THAT(ts_extrapolator.ExtrapolateLocalTime(rtp),
              Optional(clock.CurrentTime()));
}

TEST(TimestampExtrapolatorTest, EstimatedClockDriftHistogram) {
  const std::string kHistogramName = "WebRTC.Video.EstimatedClockDrift_ppm";
  constexpr int kPpmTolerance = 50;
  constexpr int kToPpmFactor = 1e6;
  constexpr int kMinimumSamples = 3000;
  constexpr Frequency k24Fps = Frequency::Hertz(24);
  constexpr TimeDelta k24FpsDelay = 1 / k24Fps;

  // This simulates a remote clock without drift with frames produced at 25 fps.
  // Local scope to trigger the destructor of TimestampExtrapolator.
  {
    // Clear all histogram data.
    metrics::Reset();
    SimulatedClock clock(Timestamp::Millis(1337));
    TimestampExtrapolator ts_extrapolator(clock.CurrentTime(),
                                          CreateTestFieldTrials());

    uint32_t rtp = 90000;
    for (int i = 0; i < kMinimumSamples; ++i) {
      ts_extrapolator.Update(clock.CurrentTime(), rtp);
      rtp += kRtpHz / k25Fps;
      clock.AdvanceTime(k25FpsDelay);
    }
  }
  EXPECT_EQ(metrics::NumSamples(kHistogramName), 1);
  const int kExpectedIdealClockDriftPpm = 0;
  EXPECT_NEAR(kExpectedIdealClockDriftPpm, metrics::MinSample(kHistogramName),
              kPpmTolerance);

  // This simulates a slow remote clock, where the RTP timestamps are
  // incremented as if the camera was 25 fps even though frames arrive at 24
  // fps. Local scope to trigger the destructor of TimestampExtrapolator.
  {
    // Clear all histogram data.
    metrics::Reset();
    SimulatedClock clock(Timestamp::Millis(1337));
    TimestampExtrapolator ts_extrapolator(clock.CurrentTime(),
                                          CreateTestFieldTrials());

    uint32_t rtp = 90000;
    for (int i = 0; i < kMinimumSamples; ++i) {
      ts_extrapolator.Update(clock.CurrentTime(), rtp);
      rtp += kRtpHz / k25Fps;
      clock.AdvanceTime(k24FpsDelay);
    }
  }
  EXPECT_EQ(metrics::NumSamples(kHistogramName), 1);
  const int kExpectedSlowClockDriftPpm =
      std::abs(k24Fps / k25Fps - 1.0) * kToPpmFactor;
  EXPECT_NEAR(kExpectedSlowClockDriftPpm, metrics::MinSample(kHistogramName),
              kPpmTolerance);

  // This simulates a fast remote clock, where the RTP timestamps are
  // incremented as if the camera was 24 fps even though frames arrive at 25
  // fps. Local scope to trigger the destructor of TimestampExtrapolator.
  {
    // Clear all histogram data.
    metrics::Reset();
    SimulatedClock clock(Timestamp::Millis(1337));
    TimestampExtrapolator ts_extrapolator(clock.CurrentTime(),
                                          CreateTestFieldTrials());

    uint32_t rtp = 90000;
    for (int i = 0; i < kMinimumSamples; ++i) {
      ts_extrapolator.Update(clock.CurrentTime(), rtp);
      rtp += kRtpHz / k24Fps;
      clock.AdvanceTime(k25FpsDelay);
    }
  }
  EXPECT_EQ(metrics::NumSamples(kHistogramName), 1);
  const int kExpectedFastClockDriftPpm = (k25Fps / k24Fps - 1.0) * kToPpmFactor;
  EXPECT_NEAR(kExpectedFastClockDriftPpm, metrics::MinSample(kHistogramName),
              kPpmTolerance);
}

TEST(TimestampExtrapolatorTest, SetsValidConfig) {
  SimulatedClock clock(Timestamp::Millis(1337));
  // clang-format off
  TimestampExtrapolator ts_extrapolator(
      clock.CurrentTime(), CreateTestFieldTrials(
          "WebRTC-TimestampExtrapolatorConfig/"
            "hard_reset_timeout:1s,"
            "hard_reset_rtp_timestamp_jump_threshold:45000,"
            "outlier_rejection_startup_delay:123,"
            "outlier_rejection_max_consecutive:456,"
            "outlier_rejection_forgetting_factor:0.987,"
            "outlier_rejection_stddev:3.5,"
            "alarm_threshold:123,"
            "acc_drift:456,"
            "acc_max_error:789,"
            "reset_full_cov_on_alarm:true/"));
  // clang-format on

  TimestampExtrapolator::Config config = ts_extrapolator.GetConfigForTest();
  EXPECT_TRUE(config.OutlierRejectionEnabled());
  EXPECT_EQ(config.hard_reset_timeout, TimeDelta::Seconds(1));
  EXPECT_EQ(config.hard_reset_rtp_timestamp_jump_threshold, 45000);
  EXPECT_EQ(config.outlier_rejection_startup_delay, 123);
  EXPECT_EQ(config.outlier_rejection_max_consecutive, 456);
  EXPECT_EQ(config.outlier_rejection_forgetting_factor, 0.987);
  EXPECT_EQ(config.outlier_rejection_stddev, 3.5);
  EXPECT_EQ(config.alarm_threshold, 123);
  EXPECT_EQ(config.acc_drift, 456);
  EXPECT_EQ(config.acc_max_error, 789);
  EXPECT_TRUE(config.reset_full_cov_on_alarm);
}

TEST(TimestampExtrapolatorTest, DoesNotSetInvalidConfig) {
  SimulatedClock clock(Timestamp::Millis(1337));
  // clang-format off
  TimestampExtrapolator ts_extrapolator(
      clock.CurrentTime(), CreateTestFieldTrials(
          "WebRTC-TimestampExtrapolatorConfig/"
            "hard_reset_timeout:-1s,"
            "hard_reset_rtp_timestamp_jump_threshold:-1,"
            "outlier_rejection_startup_delay:-1,"
            "outlier_rejection_max_consecutive:0,"
            "outlier_rejection_forgetting_factor:1.1,"
            "outlier_rejection_stddev:-1,"
            "alarm_threshold:-123,"
            "acc_drift:-456,"
            "acc_max_error:-789/"));
  // clang-format on

  TimestampExtrapolator::Config config = ts_extrapolator.GetConfigForTest();
  EXPECT_TRUE(config.OutlierRejectionEnabled());
  EXPECT_EQ(config.hard_reset_timeout, TimeDelta::Seconds(10));
  EXPECT_EQ(config.hard_reset_rtp_timestamp_jump_threshold, 900000);
  EXPECT_EQ(config.outlier_rejection_startup_delay, 300);
  EXPECT_EQ(config.outlier_rejection_max_consecutive, 150);
  EXPECT_EQ(config.outlier_rejection_forgetting_factor, 0.999);
  EXPECT_EQ(config.outlier_rejection_stddev, 2.0);
  EXPECT_EQ(config.alarm_threshold, 60000);
  EXPECT_EQ(config.acc_drift, 6600);
  EXPECT_EQ(config.acc_max_error, 7000);
}

TEST(TimestampExtrapolatorTest, ExtrapolationNotAffectedByRtpTimestampJump) {
  SimulatedClock clock(Timestamp::Millis(1337));
  TimestampExtrapolator extrapolator(
      clock.CurrentTime(),
      CreateTestFieldTrials("WebRTC-TimestampExtrapolatorConfig/"
                            "outlier_rejection_stddev:3,hard_reset_rtp_"
                            "timestamp_jump_threshold:900000/"));

  // Stabilize filter.
  uint32_t rtp = 0;
  for (int i = 0; i < 2000; ++i) {
    rtp += kRtpHz / k25Fps;
    clock.AdvanceTime(k25FpsDelay);
    extrapolator.Update(clock.CurrentTime(), rtp);
  }

  // Last frame before jump is expected on time.
  rtp += kRtpHz / k25Fps;
  clock.AdvanceTime(k25FpsDelay);
  extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_EQ(extrapolator.ExtrapolateLocalTime(rtp), clock.CurrentTime());

  // Next frame arrives on time, but with a 20 second RTP timestamp jump.
  rtp += 2 * 900000;  // 20 seconds.
  clock.AdvanceTime(k25FpsDelay);
  extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_EQ(extrapolator.ExtrapolateLocalTime(rtp), clock.CurrentTime());

  // First frame after jump is expected on time.
  rtp += kRtpHz / k25Fps;
  clock.AdvanceTime(k25FpsDelay);
  extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_EQ(extrapolator.ExtrapolateLocalTime(rtp), clock.CurrentTime());
}

TEST(TimestampExtrapolatorTest, ExtrapolationNotAffectedByFrameOutliers) {
  SimulatedClock clock(Timestamp::Millis(1337));
  TimestampExtrapolator extrapolator(
      clock.CurrentTime(),
      CreateTestFieldTrials(
          "WebRTC-TimestampExtrapolatorConfig/outlier_rejection_stddev:3/"));

  // Stabilize filter.
  uint32_t rtp = 0;
  for (int i = 0; i < 2000; ++i) {
    rtp += kRtpHz / k25Fps;
    clock.AdvanceTime(k25FpsDelay);
    extrapolator.Update(clock.CurrentTime(), rtp);
  }

  // Last frame before outlier arrives on time.
  rtp += kRtpHz / k25Fps;
  clock.AdvanceTime(k25FpsDelay);
  extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_EQ(extrapolator.ExtrapolateLocalTime(rtp), clock.CurrentTime());

  // Outlier frame arrives 1000ms late, but is expected on time.
  rtp += kRtpHz / k25Fps;
  Timestamp expected = clock.CurrentTime() + k25FpsDelay;
  clock.AdvanceTime(TimeDelta::Millis(1000));
  extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_EQ(extrapolator.ExtrapolateLocalTime(rtp), expected);

  // Congested frames arrive back-to-back, but are expected on time.
  for (int i = 0; i < 24; ++i) {
    rtp += kRtpHz / k25Fps;
    expected += k25FpsDelay;
    extrapolator.Update(clock.CurrentTime(), rtp);
    EXPECT_EQ(extrapolator.ExtrapolateLocalTime(rtp), expected);
  }

  // Regular frame after outliers arrives on time.
  rtp += kRtpHz / k25Fps;
  clock.AdvanceTime(k25FpsDelay);
  extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_EQ(extrapolator.ExtrapolateLocalTime(rtp), clock.CurrentTime());
}

TEST(TimestampExtrapolatorTest,
     ExtrapolationAffectedByFrameOutliersAfterRejectionPeriod) {
  SimulatedClock clock(Timestamp::Millis(1337));
  TimestampExtrapolator extrapolator(
      clock.CurrentTime(),
      CreateTestFieldTrials(
          "WebRTC-TimestampExtrapolatorConfig/"
          "outlier_rejection_stddev:3,outlier_rejection_max_consecutive:20/"));

  // Stabilize filter.
  uint32_t rtp = 0;
  for (int i = 0; i < 2000; ++i) {
    rtp += kRtpHz / k25Fps;
    clock.AdvanceTime(k25FpsDelay);
    extrapolator.Update(clock.CurrentTime(), rtp);
  }

  // Last frame before outlier arrives on time.
  rtp += kRtpHz / k25Fps;
  clock.AdvanceTime(k25FpsDelay);
  extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_EQ(extrapolator.ExtrapolateLocalTime(rtp), clock.CurrentTime());

  // Outlier frame arrives 1000ms late, but is expected on time.
  rtp += kRtpHz / k25Fps;
  Timestamp expected = clock.CurrentTime() + k25FpsDelay;
  clock.AdvanceTime(TimeDelta::Millis(1000));
  extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_EQ(extrapolator.ExtrapolateLocalTime(rtp), expected);

  // Congested frames arrive back-to-back. The first 19 are expected on time.
  for (int i = 0; i < 19; ++i) {
    rtp += kRtpHz / k25Fps;
    expected += k25FpsDelay;
    extrapolator.Update(clock.CurrentTime(), rtp);
    EXPECT_EQ(extrapolator.ExtrapolateLocalTime(rtp), expected);
  }

  // After the 20 consecutive outlier frames, the filter soft resets and starts
  // expecting frames on the new baseline, which is partially congested.
  rtp += kRtpHz / k25Fps;
  extrapolator.Update(clock.CurrentTime(), rtp);
  EXPECT_EQ(extrapolator.ExtrapolateLocalTime(rtp), clock.CurrentTime());
  for (int i = 0; i < 4; ++i) {
    rtp += kRtpHz / k25Fps;
    extrapolator.Update(clock.CurrentTime(), rtp);
    EXPECT_EQ(extrapolator.ExtrapolateLocalTime(rtp),
              clock.CurrentTime() + (i + 1) * k25FpsDelay);
  }

  // Now we have caught up with realtime, but since the soft reset happened
  // 4 frames too early, the new baseline is 4 * 1000/25 = 160ms off.
  for (int i = 0; i < 10; ++i) {
    rtp += kRtpHz / k25Fps;
    clock.AdvanceTime(k25FpsDelay);
    extrapolator.Update(clock.CurrentTime(), rtp);
    EXPECT_EQ(extrapolator.ExtrapolateLocalTime(rtp),
              clock.CurrentTime() + 4 * k25FpsDelay);
  }

  // Let the filter stabilize at a realtime rate again.
  for (int i = 0; i < 2000; ++i) {
    rtp += kRtpHz / k25Fps;
    clock.AdvanceTime(k25FpsDelay);
    extrapolator.Update(clock.CurrentTime(), rtp);
  }

  // After the stabilization, the 160ms congestion offset has been canceled.
  for (int i = 0; i < 10; ++i) {
    rtp += kRtpHz / k25Fps;
    clock.AdvanceTime(k25FpsDelay);
    extrapolator.Update(clock.CurrentTime(), rtp);
    EXPECT_EQ(extrapolator.ExtrapolateLocalTime(rtp), clock.CurrentTime());
  }
}

}  // namespace webrtc
