/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/gtest.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/congestion_controller/delay_based_bwe.h"
#include "webrtc/modules/congestion_controller/delay_based_bwe_unittest_helper.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/field_trial.h"

namespace webrtc {

namespace {
const PacedPacketInfo kPacingInfo0(0, 5, 2000);
const PacedPacketInfo kPacingInfo1(1, 8, 4000);
constexpr int kNumProbes = 5;
}  // namespace

TEST_F(DelayBasedBweTest, ProbeDetection) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  uint16_t seq_num = 0;

  // First burst sent at 8 * 1000 / 10 = 800 kbps.
  for (int i = 0; i < kNumProbes; ++i) {
    clock_.AdvanceTimeMilliseconds(10);
    now_ms = clock_.TimeInMilliseconds();
    IncomingFeedback(now_ms, now_ms, seq_num++, 1000, kPacingInfo0);
  }
  EXPECT_TRUE(bitrate_observer_.updated());

  // Second burst sent at 8 * 1000 / 5 = 1600 kbps.
  for (int i = 0; i < kNumProbes; ++i) {
    clock_.AdvanceTimeMilliseconds(5);
    now_ms = clock_.TimeInMilliseconds();
    IncomingFeedback(now_ms, now_ms, seq_num++, 1000, kPacingInfo1);
  }

  EXPECT_TRUE(bitrate_observer_.updated());
  EXPECT_GT(bitrate_observer_.latest_bitrate(), 1500000u);
}

TEST_F(DelayBasedBweTest, ProbeDetectionNonPacedPackets) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  uint16_t seq_num = 0;
  // First burst sent at 8 * 1000 / 10 = 800 kbps, but with every other packet
  // not being paced which could mess things up.
  for (int i = 0; i < kNumProbes; ++i) {
    clock_.AdvanceTimeMilliseconds(5);
    now_ms = clock_.TimeInMilliseconds();
    IncomingFeedback(now_ms, now_ms, seq_num++, 1000, kPacingInfo0);
    // Non-paced packet, arriving 5 ms after.
    clock_.AdvanceTimeMilliseconds(5);
    IncomingFeedback(now_ms, now_ms, seq_num++, 100, PacedPacketInfo());
  }

  EXPECT_TRUE(bitrate_observer_.updated());
  EXPECT_GT(bitrate_observer_.latest_bitrate(), 800000u);
}

TEST_F(DelayBasedBweTest, ProbeDetectionFasterArrival) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  uint16_t seq_num = 0;
  // First burst sent at 8 * 1000 / 10 = 800 kbps.
  // Arriving at 8 * 1000 / 5 = 1600 kbps.
  int64_t send_time_ms = 0;
  for (int i = 0; i < kNumProbes; ++i) {
    clock_.AdvanceTimeMilliseconds(1);
    send_time_ms += 10;
    now_ms = clock_.TimeInMilliseconds();
    IncomingFeedback(now_ms, send_time_ms, seq_num++, 1000, kPacingInfo0);
  }

  EXPECT_FALSE(bitrate_observer_.updated());
}

TEST_F(DelayBasedBweTest, ProbeDetectionSlowerArrival) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  uint16_t seq_num = 0;
  // First burst sent at 8 * 1000 / 5 = 1600 kbps.
  // Arriving at 8 * 1000 / 7 = 1142 kbps.
  int64_t send_time_ms = 0;
  for (int i = 0; i < kNumProbes; ++i) {
    clock_.AdvanceTimeMilliseconds(7);
    send_time_ms += 5;
    now_ms = clock_.TimeInMilliseconds();
    IncomingFeedback(now_ms, send_time_ms, seq_num++, 1000, kPacingInfo1);
  }

  EXPECT_TRUE(bitrate_observer_.updated());
  EXPECT_NEAR(bitrate_observer_.latest_bitrate(), 1140000u, 10000u);
}

TEST_F(DelayBasedBweTest, ProbeDetectionSlowerArrivalHighBitrate) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  uint16_t seq_num = 0;
  // Burst sent at 8 * 1000 / 1 = 8000 kbps.
  // Arriving at 8 * 1000 / 2 = 4000 kbps.
  int64_t send_time_ms = 0;
  for (int i = 0; i < kNumProbes; ++i) {
    clock_.AdvanceTimeMilliseconds(2);
    send_time_ms += 1;
    now_ms = clock_.TimeInMilliseconds();
    IncomingFeedback(now_ms, send_time_ms, seq_num++, 1000, kPacingInfo1);
  }

  EXPECT_TRUE(bitrate_observer_.updated());
  EXPECT_NEAR(bitrate_observer_.latest_bitrate(), 4000000u, 10000u);
}

TEST_F(DelayBasedBweTest, GetProbingInterval) {
  int64_t default_interval_ms = bitrate_estimator_->GetProbingIntervalMs();
  EXPECT_GT(default_interval_ms, 0);
  CapacityDropTestHelper(1, true, 567, 0);
  int64_t interval_ms = bitrate_estimator_->GetProbingIntervalMs();
  EXPECT_GT(interval_ms, 0);
  EXPECT_NE(interval_ms, default_interval_ms);
}

TEST_F(DelayBasedBweTest, InitialBehavior) {
  InitialBehaviorTestHelper(674840);
}

TEST_F(DelayBasedBweTest, RateIncreaseReordering) {
  RateIncreaseReorderingTestHelper(674840);
}

TEST_F(DelayBasedBweTest, RateIncreaseRtpTimestamps) {
  RateIncreaseRtpTimestampsTestHelper(1240);
}

TEST_F(DelayBasedBweTest, CapacityDropOneStream) {
  CapacityDropTestHelper(1, false, 567, 0);
}

TEST_F(DelayBasedBweTest, CapacityDropPosOffsetChange) {
  CapacityDropTestHelper(1, false, 200, 30000);
}

TEST_F(DelayBasedBweTest, CapacityDropNegOffsetChange) {
  CapacityDropTestHelper(1, false, 733, -30000);
}

TEST_F(DelayBasedBweTest, CapacityDropOneStreamWrap) {
  CapacityDropTestHelper(1, true, 567, 0);
}

TEST_F(DelayBasedBweTest, TestTimestampGrouping) {
  TestTimestampGroupingTestHelper();
}

TEST_F(DelayBasedBweTest, TestShortTimeoutAndWrap) {
  // Simulate a client leaving and rejoining the call after 35 seconds. This
  // will make abs send time wrap, so if streams aren't timed out properly
  // the next 30 seconds of packets will be out of order.
  TestWrappingHelper(35);
}

TEST_F(DelayBasedBweTest, TestLongTimeoutAndWrap) {
  // Simulate a client leaving and rejoining the call after some multiple of
  // 64 seconds later. This will cause a zero difference in abs send times due
  // to the wrap, but a big difference in arrival time, if streams aren't
  // properly timed out.
  TestWrappingHelper(10 * 64);
}

class DelayBasedBweExperimentTest : public DelayBasedBweTest {
 public:
  DelayBasedBweExperimentTest()
      : override_field_trials_("WebRTC-ImprovedBitrateEstimate/Enabled/") {
    bitrate_estimator_.reset(new DelayBasedBwe(nullptr, &clock_));
  }

 private:
  test::ScopedFieldTrials override_field_trials_;
};

TEST_F(DelayBasedBweExperimentTest, RateIncreaseRtpTimestamps) {
  RateIncreaseRtpTimestampsTestHelper(1288);
}

TEST_F(DelayBasedBweExperimentTest, CapacityDropOneStream) {
  CapacityDropTestHelper(1, false, 333, 0);
}

TEST_F(DelayBasedBweExperimentTest, CapacityDropPosOffsetChange) {
  CapacityDropTestHelper(1, false, 300, 30000);
}

TEST_F(DelayBasedBweExperimentTest, CapacityDropNegOffsetChange) {
  CapacityDropTestHelper(1, false, 300, -30000);
}

TEST_F(DelayBasedBweExperimentTest, CapacityDropOneStreamWrap) {
  CapacityDropTestHelper(1, true, 333, 0);
}

class DelayBasedBweTrendlineExperimentTest : public DelayBasedBweTest {
 public:
  DelayBasedBweTrendlineExperimentTest()
      : override_field_trials_("WebRTC-BweTrendlineFilter/Enabled-15,0.9,4/") {
    bitrate_estimator_.reset(new DelayBasedBwe(nullptr, &clock_));
  }

 private:
  test::ScopedFieldTrials override_field_trials_;
};

TEST_F(DelayBasedBweTrendlineExperimentTest, RateIncreaseRtpTimestamps) {
  RateIncreaseRtpTimestampsTestHelper(1240);
}

TEST_F(DelayBasedBweTrendlineExperimentTest, CapacityDropOneStream) {
  CapacityDropTestHelper(1, false, 600, 0);
}

TEST_F(DelayBasedBweTrendlineExperimentTest, CapacityDropPosOffsetChange) {
  CapacityDropTestHelper(1, false, 600, 30000);
}

TEST_F(DelayBasedBweTrendlineExperimentTest, CapacityDropNegOffsetChange) {
  CapacityDropTestHelper(1, false, 1267, -30000);
}

TEST_F(DelayBasedBweTrendlineExperimentTest, CapacityDropOneStreamWrap) {
  CapacityDropTestHelper(1, true, 600, 0);
}

class DelayBasedBweMedianSlopeExperimentTest : public DelayBasedBweTest {
 public:
  DelayBasedBweMedianSlopeExperimentTest()
      : override_field_trials_("WebRTC-BweMedianSlopeFilter/Enabled-20,4/") {
    bitrate_estimator_.reset(new DelayBasedBwe(nullptr, &clock_));
  }

 private:
  test::ScopedFieldTrials override_field_trials_;
};

TEST_F(DelayBasedBweMedianSlopeExperimentTest, RateIncreaseRtpTimestamps) {
  RateIncreaseRtpTimestampsTestHelper(1240);
}

TEST_F(DelayBasedBweMedianSlopeExperimentTest, CapacityDropOneStream) {
  CapacityDropTestHelper(1, false, 600, 0);
}

TEST_F(DelayBasedBweMedianSlopeExperimentTest, CapacityDropPosOffsetChange) {
  CapacityDropTestHelper(1, false, 600, 30000);
}

TEST_F(DelayBasedBweMedianSlopeExperimentTest, CapacityDropNegOffsetChange) {
  CapacityDropTestHelper(1, false, 1267, -30000);
}

TEST_F(DelayBasedBweMedianSlopeExperimentTest, CapacityDropOneStreamWrap) {
  CapacityDropTestHelper(1, true, 600, 0);
}

}  // namespace webrtc
