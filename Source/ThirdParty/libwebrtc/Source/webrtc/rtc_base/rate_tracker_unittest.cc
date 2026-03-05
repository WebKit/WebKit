/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/rate_tracker.h"

#include <cstdint>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
constexpr uint32_t kBucketIntervalMs = 100;
}  // namespace

class RateTrackerForTest {
 public:
  RateTrackerForTest()
      : rate_tracker_(kBucketIntervalMs, 10u), time_(Timestamp::Zero()) {}
  void AdvanceTime(int delta) { time_ += TimeDelta::Millis(delta); }

  double ComputeRate() const { return rate_tracker_.Rate(time_); }
  double ComputeRateForInterval(int64_t interval_ms) {
    return rate_tracker_.ComputeRateForInterval(time_,
                                                TimeDelta::Millis(interval_ms));
  }
  int64_t TotalSampleCount() const { return rate_tracker_.TotalSampleCount(); }

  void AddSamples(int64_t samples_count) {
    rate_tracker_.Update(samples_count, time_);
  }

 private:
  RateTracker rate_tracker_;
  Timestamp time_;
};

TEST(RateTrackerTest, Test30FPS) {
  RateTrackerForTest tracker;

  for (int i = 0; i < 300; ++i) {
    tracker.AddSamples(1);
    tracker.AdvanceTime(33);
    if (i % 3 == 0) {
      tracker.AdvanceTime(1);
    }
  }
  EXPECT_DOUBLE_EQ(30.0, tracker.ComputeRateForInterval(50000));
}

TEST(RateTrackerTest, Test60FPS) {
  RateTrackerForTest tracker;

  for (int i = 0; i < 300; ++i) {
    tracker.AddSamples(1);
    tracker.AdvanceTime(16);
    if (i % 3 != 0) {
      tracker.AdvanceTime(1);
    }
  }
  EXPECT_DOUBLE_EQ(60.0, tracker.ComputeRateForInterval(1000));
}

TEST(RateTrackerTest, TestRateTrackerBasics) {
  RateTrackerForTest tracker;
  EXPECT_DOUBLE_EQ(0.0, tracker.ComputeRateForInterval(1000));

  // Add a sample.
  tracker.AddSamples(1234);
  // Advance the clock by less than one bucket interval (no rate returned).
  tracker.AdvanceTime(kBucketIntervalMs - 1);
  EXPECT_DOUBLE_EQ(0.0, tracker.ComputeRate());
  // Advance the clock by 100 ms (one bucket interval).
  tracker.AdvanceTime(1);
  EXPECT_DOUBLE_EQ(12340.0, tracker.ComputeRateForInterval(1000));
  EXPECT_DOUBLE_EQ(12340.0, tracker.ComputeRate());
  EXPECT_EQ(1234U, tracker.TotalSampleCount());

  // Repeat.
  tracker.AddSamples(1234);
  tracker.AdvanceTime(100);
  EXPECT_DOUBLE_EQ(12340.0, tracker.ComputeRateForInterval(1000));
  EXPECT_DOUBLE_EQ(12340.0, tracker.ComputeRate());
  EXPECT_EQ(1234U * 2, tracker.TotalSampleCount());

  // Advance the clock by 800 ms, so we've elapsed a full second.
  // units_second should now be filled in properly.
  tracker.AdvanceTime(800);
  EXPECT_DOUBLE_EQ(1234.0 * 2.0, tracker.ComputeRateForInterval(1000));
  EXPECT_DOUBLE_EQ(1234.0 * 2.0, tracker.ComputeRate());
  EXPECT_EQ(1234U * 2, tracker.TotalSampleCount());

  // Poll the tracker again immediately. The reported rate should stay the same.
  EXPECT_DOUBLE_EQ(1234.0 * 2.0, tracker.ComputeRateForInterval(1000));
  EXPECT_DOUBLE_EQ(1234.0 * 2.0, tracker.ComputeRate());
  EXPECT_EQ(1234U * 2, tracker.TotalSampleCount());

  // Do nothing and advance by a second. We should drop down to zero.
  tracker.AdvanceTime(1000);
  EXPECT_DOUBLE_EQ(0.0, tracker.ComputeRateForInterval(1000));
  EXPECT_DOUBLE_EQ(0.0, tracker.ComputeRate());
  EXPECT_EQ(1234U * 2, tracker.TotalSampleCount());

  // Send a bunch of data at a constant rate for 5.5 "seconds".
  // We should report the rate properly.
  for (int i = 0; i < 5500; i += 100) {
    tracker.AddSamples(9876U);
    tracker.AdvanceTime(100);
  }
  EXPECT_DOUBLE_EQ(9876.0 * 10.0, tracker.ComputeRateForInterval(1000));
  EXPECT_DOUBLE_EQ(9876.0 * 10.0, tracker.ComputeRate());
  EXPECT_EQ(1234U * 2 + 9876U * 55, tracker.TotalSampleCount());

  // Advance the clock by 500 ms. Since we sent nothing over this half-second,
  // the reported rate should be reduced by half.
  tracker.AdvanceTime(500);
  EXPECT_DOUBLE_EQ(9876.0 * 5.0, tracker.ComputeRateForInterval(1000));
  EXPECT_DOUBLE_EQ(9876.0 * 5.0, tracker.ComputeRate());
  EXPECT_EQ(1234U * 2 + 9876U * 55, tracker.TotalSampleCount());

  // Rate over the last half second should be zero.
  EXPECT_DOUBLE_EQ(0.0, tracker.ComputeRateForInterval(500));
}

TEST(RateTrackerTest, TestLongPeriodBetweenSamples) {
  RateTrackerForTest tracker;
  tracker.AddSamples(1);
  tracker.AdvanceTime(1000);
  EXPECT_DOUBLE_EQ(1.0, tracker.ComputeRate());

  tracker.AdvanceTime(2000);
  EXPECT_DOUBLE_EQ(0.0, tracker.ComputeRate());

  tracker.AdvanceTime(2000);
  tracker.AddSamples(1);
  EXPECT_DOUBLE_EQ(1.0, tracker.ComputeRate());
}

TEST(RateTrackerTest, TestRolloff) {
  RateTrackerForTest tracker;
  for (int i = 0; i < 10; ++i) {
    tracker.AddSamples(1U);
    tracker.AdvanceTime(100);
  }
  EXPECT_DOUBLE_EQ(10.0, tracker.ComputeRate());

  for (int i = 0; i < 10; ++i) {
    tracker.AddSamples(1U);
    tracker.AdvanceTime(50);
  }
  EXPECT_DOUBLE_EQ(15.0, tracker.ComputeRate());
  EXPECT_DOUBLE_EQ(20.0, tracker.ComputeRateForInterval(500));

  for (int i = 0; i < 10; ++i) {
    tracker.AddSamples(1U);
    tracker.AdvanceTime(50);
  }
  EXPECT_DOUBLE_EQ(20.0, tracker.ComputeRate());
}

TEST(RateTrackerTest, TestGetUnitSecondsAfterInitialValue) {
  RateTrackerForTest tracker;
  tracker.AddSamples(1234);
  tracker.AdvanceTime(1000);
  EXPECT_DOUBLE_EQ(1234.0, tracker.ComputeRateForInterval(1000));
}

TEST(RateTrackerTest, TestLargeNumbers) {
  RateTrackerForTest tracker;
  const uint64_t large_number = 0x100000000;
  tracker.AddSamples(large_number);
  tracker.AdvanceTime(1000);
  tracker.AddSamples(large_number);
  EXPECT_DOUBLE_EQ(large_number * 2, tracker.ComputeRate());
}

}  // namespace webrtc
