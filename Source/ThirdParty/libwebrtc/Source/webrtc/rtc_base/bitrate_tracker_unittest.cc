/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/bitrate_tracker.h"

#include <cstdlib>
#include <limits>

#include "absl/types/optional.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::AllOf;
using ::testing::Ge;
using ::testing::Le;

constexpr TimeDelta kWindow = TimeDelta::Millis(500);
constexpr TimeDelta kEpsilon = TimeDelta::Millis(1);

TEST(BitrateTrackerTest, ReturnsNulloptInitially) {
  Timestamp now = Timestamp::Seconds(12'345);
  BitrateTracker stats(kWindow);

  EXPECT_EQ(stats.Rate(now), absl::nullopt);
}

TEST(BitrateTrackerTest, ReturnsNulloptAfterSingleDataPoint) {
  Timestamp now = Timestamp::Seconds(12'345);
  BitrateTracker stats(kWindow);

  stats.Update(1'500, now);
  now += TimeDelta::Millis(10);

  EXPECT_EQ(stats.Rate(now), absl::nullopt);
}

TEST(BitrateTrackerTest, ReturnsRateAfterTwoMeasurements) {
  Timestamp now = Timestamp::Seconds(12'345);
  BitrateTracker stats(kWindow);

  stats.Update(1'500, now);
  now += TimeDelta::Millis(10);
  stats.Update(1'500, now);

  // One packet every 10ms would result in 1.2 Mbps, but until window is full,
  // it could be treated as two packets in ~10ms window, measuring twice that
  // bitrate.
  EXPECT_THAT(stats.Rate(now), AllOf(Ge(DataRate::BitsPerSec(1'200'000)),
                                     Le(DataRate::BitsPerSec(2'400'000))));
}

TEST(BitrateTrackerTest, MeasuresConstantRate) {
  const Timestamp start = Timestamp::Seconds(12'345);
  const TimeDelta kInterval = TimeDelta::Millis(10);
  const DataSize kPacketSize = DataSize::Bytes(1'500);
  const DataRate kConstantRate = kPacketSize / kInterval;

  Timestamp now = start;
  BitrateTracker stats(kWindow);

  stats.Update(kPacketSize, now);
  DataSize total_size = kPacketSize;
  DataRate last_error = DataRate::PlusInfinity();
  for (TimeDelta i = TimeDelta::Zero(); i < kWindow; i += kInterval) {
    SCOPED_TRACE(ToString(i));
    now += kInterval;
    total_size += kPacketSize;
    stats.Update(kPacketSize, now);

    // Until window is full, bitrate is measured over a smaller window and might
    // look larger than the constant rate.
    absl::optional<DataRate> bitrate = stats.Rate(now);
    ASSERT_THAT(bitrate,
                AllOf(Ge(kConstantRate), Le(total_size / (now - start))));

    // Expect the estimation error to decrease as the window is extended.
    DataRate error = *bitrate - kConstantRate;
    EXPECT_LE(error, last_error);
    last_error = error;
  }

  // Once window is full, bitrate measurment should be stable.
  for (TimeDelta i = TimeDelta::Zero(); i < kInterval;
       i += TimeDelta::Millis(1)) {
    SCOPED_TRACE(ToString(i));
    EXPECT_EQ(stats.Rate(now + i), kConstantRate);
  }
}

TEST(BitrateTrackerTest, IncreasingThenDecreasingBitrate) {
  const DataSize kLargePacketSize = DataSize::Bytes(1'500);
  const DataSize kSmallPacketSize = DataSize::Bytes(300);
  const TimeDelta kLargeInterval = TimeDelta::Millis(10);
  const TimeDelta kSmallInterval = TimeDelta::Millis(2);

  Timestamp now = Timestamp::Seconds(12'345);
  BitrateTracker stats(kWindow);

  stats.Update(kLargePacketSize, now);
  for (TimeDelta i = TimeDelta::Zero(); i < kWindow; i += kLargeInterval) {
    SCOPED_TRACE(ToString(i));
    now += kLargeInterval;
    stats.Update(kLargePacketSize, now);
  }
  absl::optional<DataRate> last_bitrate = stats.Rate(now);
  EXPECT_EQ(last_bitrate, kLargePacketSize / kLargeInterval);

  // Decrease bitrate with smaller measurments.
  for (TimeDelta i = TimeDelta::Zero(); i < kWindow; i += kLargeInterval) {
    SCOPED_TRACE(ToString(i));
    now += kLargeInterval;
    stats.Update(kSmallPacketSize, now);

    absl::optional<DataRate> bitrate = stats.Rate(now);
    EXPECT_LT(bitrate, last_bitrate);

    last_bitrate = bitrate;
  }
  EXPECT_EQ(last_bitrate, kSmallPacketSize / kLargeInterval);

  // Increase bitrate with more frequent measurments.
  for (TimeDelta i = TimeDelta::Zero(); i < kWindow; i += kSmallInterval) {
    SCOPED_TRACE(ToString(i));
    now += kSmallInterval;
    stats.Update(kSmallPacketSize, now);

    absl::optional<DataRate> bitrate = stats.Rate(now);
    EXPECT_GE(bitrate, last_bitrate);

    last_bitrate = bitrate;
  }
  EXPECT_EQ(last_bitrate, kSmallPacketSize / kSmallInterval);
}

TEST(BitrateTrackerTest, ResetAfterSilence) {
  const TimeDelta kInterval = TimeDelta::Millis(10);
  const DataSize kPacketSize = DataSize::Bytes(1'500);

  Timestamp now = Timestamp::Seconds(12'345);
  BitrateTracker stats(kWindow);

  // Feed data until window has been filled.
  stats.Update(kPacketSize, now);
  for (TimeDelta i = TimeDelta::Zero(); i < kWindow; i += kInterval) {
    now += kInterval;
    stats.Update(kPacketSize, now);
  }
  ASSERT_GT(stats.Rate(now), DataRate::Zero());

  now += kWindow + kEpsilon;
  // Silence over window size should trigger auto reset for coming sample.
  EXPECT_EQ(stats.Rate(now), absl::nullopt);
  stats.Update(kPacketSize, now);
  // Single measurment after reset is not enough to estimate the rate.
  EXPECT_EQ(stats.Rate(now), absl::nullopt);

  // Manual reset, add the same check again.
  stats.Reset();
  EXPECT_EQ(stats.Rate(now), absl::nullopt);
  now += kInterval;
  stats.Update(kPacketSize, now);
  EXPECT_EQ(stats.Rate(now), absl::nullopt);
}

TEST(BitrateTrackerTest, HandlesChangingWindowSize) {
  Timestamp now = Timestamp::Seconds(12'345);
  BitrateTracker stats(kWindow);

  // Check window size is validated.
  EXPECT_TRUE(stats.SetWindowSize(kWindow, now));
  EXPECT_FALSE(stats.SetWindowSize(kWindow + kEpsilon, now));
  EXPECT_FALSE(stats.SetWindowSize(TimeDelta::Zero(), now));
  EXPECT_TRUE(stats.SetWindowSize(kEpsilon, now));
  EXPECT_TRUE(stats.SetWindowSize(kWindow, now));

  // Fill the buffer at a rate of 10 bytes per 10 ms (8 kbps).
  const DataSize kValue = DataSize::Bytes(10);
  const TimeDelta kInterval = TimeDelta::Millis(10);
  for (TimeDelta i = TimeDelta::Zero(); i < kWindow; i += kInterval) {
    now += kInterval;
    stats.Update(kValue, now);
  }
  ASSERT_GT(stats.Rate(now), DataRate::BitsPerSec(8'000));

  // Halve the window size, rate should stay the same.
  EXPECT_TRUE(stats.SetWindowSize(kWindow / 2, now));
  EXPECT_EQ(stats.Rate(now), DataRate::BitsPerSec(8'000));

  // Double the window size again, rate should stay the same.
  // The window won't actually expand until new calls to the `Update`.
  EXPECT_TRUE(stats.SetWindowSize(kWindow, now));
  EXPECT_EQ(stats.Rate(now), DataRate::BitsPerSec(8'000));

  // Fill the now empty window half at twice the rate.
  for (TimeDelta i = TimeDelta::Zero(); i < kWindow / 2; i += kInterval) {
    now += kInterval;
    stats.Update(2 * kValue, now);
  }

  // Rate should have increased by 50%.
  EXPECT_EQ(stats.Rate(now), DataRate::BitsPerSec(12'000));
}

TEST(BitrateTrackerTest, HandlesZeroCounts) {
  const DataSize kPacketSize = DataSize::Bytes(1'500);
  const TimeDelta kInterval = TimeDelta::Millis(10);
  const Timestamp start = Timestamp::Seconds(12'345);

  Timestamp now = start;
  BitrateTracker stats(kWindow);

  stats.Update(kPacketSize, now);
  ASSERT_EQ(stats.Rate(now), absl::nullopt);
  now += kInterval;
  stats.Update(0, now);
  absl::optional<DataRate> last_bitrate = stats.Rate(now);
  EXPECT_GT(last_bitrate, DataRate::Zero());
  now += kInterval;
  while (now < start + kWindow) {
    SCOPED_TRACE(ToString(now - start));
    stats.Update(0, now);

    absl::optional<DataRate> bitrate = stats.Rate(now);
    EXPECT_GT(bitrate, DataRate::Zero());
    // As window expands, average bitrate decreases.
    EXPECT_LT(bitrate, last_bitrate);

    last_bitrate = bitrate;
    now += kInterval;
  }

  // Initial kPacketSize should be outside the window now, so overall bitrate
  // should be zero
  EXPECT_EQ(stats.Rate(now), DataRate::Zero());

  // Single measurment should be enough to get non zero rate.
  stats.Update(kPacketSize, now);
  EXPECT_EQ(stats.Rate(now), kPacketSize / kWindow);
}

TEST(BitrateTrackerTest, ReturnsNulloptWhenOverflows) {
  Timestamp now = Timestamp::Seconds(12'345);
  BitrateTracker stats(kWindow);

  int64_t very_large_number = std::numeric_limits<int64_t>::max();
  stats.Update(very_large_number, now);
  now += kEpsilon;
  stats.Update(very_large_number, now);

  EXPECT_EQ(stats.Rate(now), absl::nullopt);
}

}  // namespace
}  // namespace webrtc
