/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/quality_limitation_reason_tracker.h"

#include "common_video/include/quality_limitation_reason.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {

class QualityLimitationReasonTrackerTest : public ::testing::Test {
 public:
  QualityLimitationReasonTrackerTest()
      : fake_clock_(1234), tracker_(&fake_clock_) {}

 protected:
  SimulatedClock fake_clock_;
  QualityLimitationReasonTracker tracker_;
};

TEST_F(QualityLimitationReasonTrackerTest, DefaultValues) {
  EXPECT_EQ(QualityLimitationReason::kNone, tracker_.current_reason());
  auto durations_ms = tracker_.DurationsMs();
  EXPECT_EQ(4u, durations_ms.size());
  EXPECT_EQ(0, durations_ms.find(QualityLimitationReason::kNone)->second);
  EXPECT_EQ(0, durations_ms.find(QualityLimitationReason::kCpu)->second);
  EXPECT_EQ(0, durations_ms.find(QualityLimitationReason::kBandwidth)->second);
  EXPECT_EQ(0, durations_ms.find(QualityLimitationReason::kOther)->second);
}

TEST_F(QualityLimitationReasonTrackerTest, NoneDurationIncreasesByDefault) {
  int64_t initial_duration_ms =
      tracker_.DurationsMs()[QualityLimitationReason::kNone];
  fake_clock_.AdvanceTimeMilliseconds(9999);
  EXPECT_EQ(initial_duration_ms + 9999,
            tracker_.DurationsMs()[QualityLimitationReason::kNone]);
}

TEST_F(QualityLimitationReasonTrackerTest,
       RememberDurationAfterSwitchingReason) {
  tracker_.SetReason(QualityLimitationReason::kCpu);
  int64_t initial_duration_ms =
      tracker_.DurationsMs()[QualityLimitationReason::kCpu];
  fake_clock_.AdvanceTimeMilliseconds(50);
  tracker_.SetReason(QualityLimitationReason::kOther);
  fake_clock_.AdvanceTimeMilliseconds(50);
  EXPECT_EQ(initial_duration_ms + 50,
            tracker_.DurationsMs()[QualityLimitationReason::kCpu]);
}

class QualityLimitationReasonTrackerTestWithParamReason
    : public QualityLimitationReasonTrackerTest,
      public ::testing::WithParamInterface<QualityLimitationReason> {
 public:
  QualityLimitationReasonTrackerTestWithParamReason()
      : reason_(GetParam()),
        different_reason_(reason_ != QualityLimitationReason::kCpu
                              ? QualityLimitationReason::kCpu
                              : QualityLimitationReason::kOther) {}

 protected:
  QualityLimitationReason reason_;
  QualityLimitationReason different_reason_;
};

TEST_P(QualityLimitationReasonTrackerTestWithParamReason,
       DurationIncreasesOverTime) {
  int64_t initial_duration_ms = tracker_.DurationsMs()[reason_];
  tracker_.SetReason(reason_);
  EXPECT_EQ(initial_duration_ms, tracker_.DurationsMs()[reason_]);
  fake_clock_.AdvanceTimeMilliseconds(4321);
  EXPECT_EQ(initial_duration_ms + 4321, tracker_.DurationsMs()[reason_]);
}

TEST_P(QualityLimitationReasonTrackerTestWithParamReason,
       SwitchBetweenReasonsBackAndForth) {
  int64_t initial_duration_ms = tracker_.DurationsMs()[reason_];
  // Spend 100 ms in `different_reason_`.
  tracker_.SetReason(different_reason_);
  fake_clock_.AdvanceTimeMilliseconds(100);
  EXPECT_EQ(initial_duration_ms, tracker_.DurationsMs()[reason_]);
  // Spend 50 ms in `reason_`.
  tracker_.SetReason(reason_);
  fake_clock_.AdvanceTimeMilliseconds(50);
  EXPECT_EQ(initial_duration_ms + 50, tracker_.DurationsMs()[reason_]);
  // Spend another 1000 ms in `different_reason_`.
  tracker_.SetReason(different_reason_);
  fake_clock_.AdvanceTimeMilliseconds(1000);
  EXPECT_EQ(initial_duration_ms + 50, tracker_.DurationsMs()[reason_]);
  // Spend another 100 ms in `reason_`.
  tracker_.SetReason(reason_);
  fake_clock_.AdvanceTimeMilliseconds(100);
  EXPECT_EQ(initial_duration_ms + 150, tracker_.DurationsMs()[reason_]);
  // Change reason one last time without advancing time.
  tracker_.SetReason(different_reason_);
  EXPECT_EQ(initial_duration_ms + 150, tracker_.DurationsMs()[reason_]);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    QualityLimitationReasonTrackerTestWithParamReason,
    ::testing::Values(QualityLimitationReason::kNone,       // "/0"
                      QualityLimitationReason::kCpu,        // "/1"
                      QualityLimitationReason::kBandwidth,  // "/2"
                      QualityLimitationReason::kOther));    // "/3"

}  // namespace webrtc
