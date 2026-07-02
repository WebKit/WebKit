/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/rate_utilization_tracker.h"

#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Not;

constexpr int kDefaultMaxDataPoints = 10;
constexpr TimeDelta kDefaultTimeWindow = TimeDelta::Seconds(1);
constexpr Timestamp kStartTime = Timestamp::Millis(9876654);
constexpr double kAllowedError = 0.002;  // 0.2% error allowed.

MATCHER_P(PrettyCloseTo, expected, "") {
  return arg && std::abs(*arg - expected) < kAllowedError;
}

TEST(RateUtilizationTrackerTest, NoDataInNoDataOut) {
  RateUtilizationTracker tracker(kDefaultMaxDataPoints, kDefaultTimeWindow);
  EXPECT_FALSE(tracker.GetRateUtilizationFactor(kStartTime).has_value());
}

TEST(RateUtilizationTrackerTest, NoUtilizationWithoutDataPoints) {
  RateUtilizationTracker tracker(kDefaultMaxDataPoints, kDefaultTimeWindow);
  tracker.OnDataRateChanged(DataRate::KilobitsPerSec(100), kStartTime);
  EXPECT_FALSE(tracker.GetRateUtilizationFactor(kStartTime).has_value());
}

TEST(RateUtilizationTrackerTest, NoUtilizationWithoutRateUpdates) {
  RateUtilizationTracker tracker(kDefaultMaxDataPoints, kDefaultTimeWindow);
  tracker.OnDataProduced(DataSize::Bytes(100), kStartTime);
  EXPECT_FALSE(tracker.GetRateUtilizationFactor(kStartTime).has_value());
}

TEST(RateUtilizationTrackerTest, SingleDataPoint) {
  RateUtilizationTracker tracker(kDefaultMaxDataPoints, kDefaultTimeWindow);
  constexpr TimeDelta kFrameInterval = TimeDelta::Seconds(1) / 33;
  constexpr DataRate kTargetRate = DataRate::KilobitsPerSec(100);
  constexpr DataSize kIdealFrameSize = kTargetRate * kFrameInterval;

  tracker.OnDataRateChanged(kTargetRate, kStartTime);
  tracker.OnDataProduced(kIdealFrameSize, kStartTime);

  // From the start, the window is extended to cover the expected duration for
  // the last frame - resulting in 100% utilization.
  EXPECT_THAT(tracker.GetRateUtilizationFactor(kStartTime), PrettyCloseTo(1.0));

  // At the expected frame interval the utilization is still 100%.
  EXPECT_THAT(tracker.GetRateUtilizationFactor(kStartTime + kFrameInterval),
              PrettyCloseTo(1.0));

  // After two frame intervals the utilization is half the expected.
  EXPECT_THAT(tracker.GetRateUtilizationFactor(kStartTime + 2 * kFrameInterval),
              PrettyCloseTo(0.5));
}

TEST(RateUtilizationTrackerTest, TwoDataPoints) {
  RateUtilizationTracker tracker(kDefaultMaxDataPoints, kDefaultTimeWindow);
  constexpr TimeDelta kFrameInterval = TimeDelta::Seconds(1) / 33;
  constexpr DataRate kTargetRate = DataRate::KilobitsPerSec(100);
  constexpr DataSize kIdealFrameSize = kTargetRate * kFrameInterval;

  tracker.OnDataRateChanged(kTargetRate, kStartTime);
  tracker.OnDataProduced(kIdealFrameSize, kStartTime);
  tracker.OnDataProduced(kIdealFrameSize, kStartTime + kFrameInterval);

  EXPECT_THAT(tracker.GetRateUtilizationFactor(kStartTime + 2 * kFrameInterval),
              PrettyCloseTo(1.0));

  // After two three frame interval we have two utilizated intervals and one
  // unitilzed => 2/3 utilization.
  EXPECT_THAT(tracker.GetRateUtilizationFactor(kStartTime + 3 * kFrameInterval),
              PrettyCloseTo(2.0 / 3.0));
}

TEST(RateUtilizationTrackerTest, TwoDataPointsConsistentOveruse) {
  RateUtilizationTracker tracker(kDefaultMaxDataPoints, kDefaultTimeWindow);
  constexpr TimeDelta kFrameInterval = TimeDelta::Seconds(1) / 33;
  constexpr DataRate kTargetRate = DataRate::KilobitsPerSec(100);
  constexpr DataSize kIdealFrameSize = kTargetRate * kFrameInterval;

  tracker.OnDataRateChanged(kTargetRate, kStartTime);
  tracker.OnDataProduced(kIdealFrameSize * 2, kStartTime);
  tracker.OnDataProduced(kIdealFrameSize * 2, kStartTime + kFrameInterval);

  // Note that the last data point is presumed to be sent at the designated rate
  // and no new data points produced until the buffers empty. Thus the
  // overshoot is just 4/3 unstead of 4/2.
  EXPECT_THAT(tracker.GetRateUtilizationFactor(kStartTime + 2 * kFrameInterval),
              PrettyCloseTo(4.0 / 3.0));
}

TEST(RateUtilizationTrackerTest, OveruseWithFrameDrop) {
  RateUtilizationTracker tracker(kDefaultMaxDataPoints, kDefaultTimeWindow);
  constexpr TimeDelta kFrameInterval = TimeDelta::Seconds(1) / 33;
  constexpr DataRate kTargetRate = DataRate::KilobitsPerSec(100);
  constexpr DataSize kIdealFrameSize = kTargetRate * kFrameInterval;

  // First frame is 2x larger than it should be.
  tracker.OnDataRateChanged(kTargetRate, kStartTime);
  tracker.OnDataProduced(kIdealFrameSize * 2, kStartTime);
  // Compensate by dropping a frame before the next nominal-size one.
  tracker.OnDataProduced(kIdealFrameSize, kStartTime + 2 * kFrameInterval);

  EXPECT_THAT(tracker.GetRateUtilizationFactor(kStartTime + 3 * kFrameInterval),
              PrettyCloseTo(1.0));
}

TEST(RateUtilizationTrackerTest, VaryingRate) {
  RateUtilizationTracker tracker(kDefaultMaxDataPoints, kDefaultTimeWindow);
  constexpr TimeDelta kFrameInterval = TimeDelta::Seconds(1) / 33;
  constexpr DataRate kTargetRate = DataRate::KilobitsPerSec(100);
  constexpr DataSize kIdealFrameSize = kTargetRate * kFrameInterval;

  // Rate goes up, rate comes down...
  tracker.OnDataRateChanged(kTargetRate, kStartTime);
  tracker.OnDataProduced(kIdealFrameSize, kStartTime);
  tracker.OnDataRateChanged(kTargetRate * 2, kStartTime + kFrameInterval);
  tracker.OnDataProduced(kIdealFrameSize * 2, kStartTime + kFrameInterval);
  tracker.OnDataRateChanged(kTargetRate, kStartTime + 2 * kFrameInterval);
  tracker.OnDataProduced(kIdealFrameSize, kStartTime + 2 * kFrameInterval);

  EXPECT_THAT(tracker.GetRateUtilizationFactor(kStartTime + 3 * kFrameInterval),
              PrettyCloseTo(1.0));
}

TEST(RateUtilizationTrackerTest, VaryingRateMidFrameInterval) {
  RateUtilizationTracker tracker(kDefaultMaxDataPoints, kDefaultTimeWindow);
  constexpr TimeDelta kFrameInterval = TimeDelta::Seconds(1) / 33;
  constexpr DataRate kTargetRate = DataRate::KilobitsPerSec(100);
  constexpr DataSize kIdealFrameSize = kTargetRate * kFrameInterval;

  // First frame 1/3 too large
  tracker.OnDataRateChanged(kTargetRate, kStartTime);
  tracker.OnDataProduced(kIdealFrameSize * (3.0 / 2.0), kStartTime);

  // Mid frame interval double the target rate. Should lead to no overshoot.
  tracker.OnDataRateChanged(kTargetRate * 2, kStartTime + kFrameInterval / 2);
  EXPECT_THAT(tracker.GetRateUtilizationFactor(kStartTime + kFrameInterval),
              PrettyCloseTo(1.0));
}

TEST(RateUtilizationTrackerTest, VaryingRateAfterLastDataPoint) {
  RateUtilizationTracker tracker(kDefaultMaxDataPoints, kDefaultTimeWindow);
  constexpr TimeDelta kFrameInterval = TimeDelta::Seconds(1) / 33;
  constexpr DataRate kTargetRate = DataRate::KilobitsPerSec(100);
  constexpr DataSize kIdealFrameSize = kTargetRate * kFrameInterval;

  tracker.OnDataRateChanged(kTargetRate, kStartTime);
  // Data point is just after the rate update.
  tracker.OnDataProduced(kIdealFrameSize, kStartTime + TimeDelta::Micros(1));

  // Half an interval past the last frame double the target rate.
  tracker.OnDataRateChanged(kTargetRate * 2, kStartTime + kFrameInterval / 2);

  // The last data point should now extend only to 2/3 the way to the next frame
  // interval.
  EXPECT_THAT(tracker.GetRateUtilizationFactor(kStartTime +
                                               kFrameInterval * (2.0 / 3.0)),
              PrettyCloseTo(1.0));
  EXPECT_THAT(tracker.GetRateUtilizationFactor(kStartTime +
                                               kFrameInterval * (2.3 / 3.0)),
              Not(PrettyCloseTo(1.0)));
}

TEST(RateUtilizationTrackerTest, DataPointLimit) {
  // Set max data points to two.
  RateUtilizationTracker tracker(/*max_data_points=*/2, kDefaultTimeWindow);
  constexpr TimeDelta kFrameInterval = TimeDelta::Seconds(1) / 33;
  constexpr DataRate kTargetRate = DataRate::KilobitsPerSec(100);
  constexpr DataSize kIdealFrameSize = kTargetRate * kFrameInterval;

  // Insert two frames that are too large.
  tracker.OnDataRateChanged(kTargetRate, kStartTime);
  tracker.OnDataProduced(kIdealFrameSize * 2, kStartTime);
  tracker.OnDataProduced(kIdealFrameSize * 2, kStartTime + 1 * kFrameInterval);
  EXPECT_THAT(tracker.GetRateUtilizationFactor(kStartTime + 1 * kFrameInterval),
              Not(PrettyCloseTo(1.0)));

  // Insert two frames of the correct size. Past grievances have been forgotten.
  tracker.OnDataProduced(kIdealFrameSize, kStartTime + 2 * kFrameInterval);
  tracker.OnDataProduced(kIdealFrameSize, kStartTime + 3 * kFrameInterval);
  EXPECT_THAT(tracker.GetRateUtilizationFactor(kStartTime + 3 * kFrameInterval),
              PrettyCloseTo(1.0));
}

TEST(RateUtilizationTrackerTest, WindowSizeLimit) {
  constexpr TimeDelta kFrameInterval = TimeDelta::Seconds(1) / 33;
  constexpr DataRate kTargetRate = DataRate::KilobitsPerSec(100);
  constexpr DataSize kIdealFrameSize = kTargetRate * kFrameInterval;
  // Number of data points enough, but time window too small.
  RateUtilizationTracker tracker(/*max_data_points=*/4, /*time_window=*/
                                 2 * kFrameInterval - TimeDelta::Millis(1));

  // Insert two frames that are too large.
  tracker.OnDataRateChanged(kTargetRate, kStartTime);
  tracker.OnDataProduced(kIdealFrameSize * 2, kStartTime);
  tracker.OnDataProduced(kIdealFrameSize * 2, kStartTime + 1 * kFrameInterval);
  EXPECT_THAT(tracker.GetRateUtilizationFactor(kStartTime + 1 * kFrameInterval),
              Not(PrettyCloseTo(1.0)));

  // Insert two frames of the correct size. Past grievances have been forgotten.
  tracker.OnDataProduced(kIdealFrameSize, kStartTime + 2 * kFrameInterval);
  tracker.OnDataProduced(kIdealFrameSize, kStartTime + 3 * kFrameInterval);
  EXPECT_THAT(tracker.GetRateUtilizationFactor(kStartTime + 3 * kFrameInterval),
              PrettyCloseTo(1.0));
}

TEST(RateUtilizationTrackerTest, EqualTimestampsTreatedAtSameDataPoint) {
  // Set max data points to two.
  RateUtilizationTracker tracker(/*max_data_points=*/2, kDefaultTimeWindow);
  constexpr TimeDelta kFrameInterval = TimeDelta::Seconds(1) / 33;
  constexpr DataRate kTargetRate = DataRate::KilobitsPerSec(100);
  constexpr DataSize kIdealFrameSize = kTargetRate * kFrameInterval;

  tracker.OnDataRateChanged(kTargetRate, kStartTime);
  tracker.OnDataProduced(kIdealFrameSize, kStartTime);
  EXPECT_THAT(tracker.GetRateUtilizationFactor(kStartTime), PrettyCloseTo(1.0));

  // This is viewed as an undershoot.
  tracker.OnDataProduced(kIdealFrameSize, kStartTime + (kFrameInterval * 2));
  EXPECT_THAT(
      tracker.GetRateUtilizationFactor(kStartTime + (kFrameInterval * 2)),
      PrettyCloseTo(2.0 / 3.0));

  // Add the same data point again. Treated as layered frame so will accumulate
  // in the same data point. This is expected to have a send time twice as long
  // now, reducing the undershoot.
  tracker.OnDataProduced(kIdealFrameSize, kStartTime + (kFrameInterval * 2));
  EXPECT_THAT(
      tracker.GetRateUtilizationFactor(kStartTime + (kFrameInterval * 2)),
      PrettyCloseTo(3.0 / 4.0));
}

TEST(RateUtilizationTrackerTest, FullRateAfterLastDataPoint) {
  RateUtilizationTracker tracker(kDefaultMaxDataPoints, kDefaultTimeWindow);
  constexpr TimeDelta kFrameInterval = TimeDelta::Seconds(1) / 33;
  constexpr DataRate kTargetRate = DataRate::KilobitsPerSec(100);
  constexpr DataSize kIdealFrameSize = kTargetRate * kFrameInterval;

  tracker.OnDataRateChanged(kTargetRate, kStartTime);
  tracker.OnDataProduced(kIdealFrameSize, kStartTime);

  // New rate update, but accumulated rate for last data point fully saturated
  // by next to last rate update.
  tracker.OnDataRateChanged(kTargetRate, kStartTime + kFrameInterval * 2);

  EXPECT_THAT(tracker.GetRateUtilizationFactor(kStartTime + kFrameInterval * 3),
              PrettyCloseTo(1.0 / 3.0));
}

}  // namespace
}  // namespace webrtc
