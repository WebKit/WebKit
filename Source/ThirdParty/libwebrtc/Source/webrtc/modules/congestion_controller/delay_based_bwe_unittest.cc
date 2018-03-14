/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/delay_based_bwe.h"
#include "modules/congestion_controller/delay_based_bwe_unittest_helper.h"
#include "modules/pacing/paced_sender.h"
#include "rtc_base/constructormagic.h"
#include "system_wrappers/include/clock.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
constexpr int kNumProbesCluster0 = 5;
constexpr int kNumProbesCluster1 = 8;
const PacedPacketInfo kPacingInfo0(0, kNumProbesCluster0, 2000);
const PacedPacketInfo kPacingInfo1(1, kNumProbesCluster1, 4000);
constexpr float kTargetUtilizationFraction = 0.95f;
}  // namespace

TEST_F(DelayBasedBweTest, NoCrashEmptyFeedback) {
  std::vector<PacketFeedback> packet_feedback_vector;
  bitrate_estimator_->IncomingPacketFeedbackVector(packet_feedback_vector,
                                                   rtc::nullopt);
}

TEST_F(DelayBasedBweTest, NoCrashOnlyLostFeedback) {
  std::vector<PacketFeedback> packet_feedback_vector;
  packet_feedback_vector.push_back(PacketFeedback(PacketFeedback::kNotReceived,
                                                  PacketFeedback::kNoSendTime,
                                                  0, 1500, PacedPacketInfo()));
  packet_feedback_vector.push_back(PacketFeedback(PacketFeedback::kNotReceived,
                                                  PacketFeedback::kNoSendTime,
                                                  1, 1500, PacedPacketInfo()));
  bitrate_estimator_->IncomingPacketFeedbackVector(packet_feedback_vector,
                                                   rtc::nullopt);
}

TEST_F(DelayBasedBweTest, ProbeDetection) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  uint16_t seq_num = 0;

  // First burst sent at 8 * 1000 / 10 = 800 kbps.
  for (int i = 0; i < kNumProbesCluster0; ++i) {
    clock_.AdvanceTimeMilliseconds(10);
    now_ms = clock_.TimeInMilliseconds();
    IncomingFeedback(now_ms, now_ms, seq_num++, 1000, kPacingInfo0);
  }
  EXPECT_TRUE(bitrate_observer_.updated());

  // Second burst sent at 8 * 1000 / 5 = 1600 kbps.
  for (int i = 0; i < kNumProbesCluster1; ++i) {
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
  for (int i = 0; i < kNumProbesCluster0; ++i) {
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
  for (int i = 0; i < kNumProbesCluster0; ++i) {
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
  // Since the receive rate is significantly below the send rate, we expect to
  // use 95% of the estimated capacity.
  int64_t send_time_ms = 0;
  for (int i = 0; i < kNumProbesCluster1; ++i) {
    clock_.AdvanceTimeMilliseconds(7);
    send_time_ms += 5;
    now_ms = clock_.TimeInMilliseconds();
    IncomingFeedback(now_ms, send_time_ms, seq_num++, 1000, kPacingInfo1);
  }

  EXPECT_TRUE(bitrate_observer_.updated());
  EXPECT_NEAR(bitrate_observer_.latest_bitrate(),
              kTargetUtilizationFraction * 1140000u, 10000u);
}

TEST_F(DelayBasedBweTest, ProbeDetectionSlowerArrivalHighBitrate) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  uint16_t seq_num = 0;
  // Burst sent at 8 * 1000 / 1 = 8000 kbps.
  // Arriving at 8 * 1000 / 2 = 4000 kbps.
  // Since the receive rate is significantly below the send rate, we expect to
  // use 95% of the estimated capacity.
  int64_t send_time_ms = 0;
  for (int i = 0; i < kNumProbesCluster1; ++i) {
    clock_.AdvanceTimeMilliseconds(2);
    send_time_ms += 1;
    now_ms = clock_.TimeInMilliseconds();
    IncomingFeedback(now_ms, send_time_ms, seq_num++, 1000, kPacingInfo1);
  }

  EXPECT_TRUE(bitrate_observer_.updated());
  EXPECT_NEAR(bitrate_observer_.latest_bitrate(),
              kTargetUtilizationFraction * 4000000u, 10000u);
}

TEST_F(DelayBasedBweTest, GetExpectedBwePeriodMs) {
  int64_t default_interval_ms = bitrate_estimator_->GetExpectedBwePeriodMs();
  EXPECT_GT(default_interval_ms, 0);
  CapacityDropTestHelper(1, true, 333, 0);
  int64_t interval_ms = bitrate_estimator_->GetExpectedBwePeriodMs();
  EXPECT_GT(interval_ms, 0);
  EXPECT_NE(interval_ms, default_interval_ms);
}

TEST_F(DelayBasedBweTest, InitialBehavior) {
  InitialBehaviorTestHelper(730000);
}

TEST_F(DelayBasedBweTest, RateIncreaseReordering) {
  RateIncreaseReorderingTestHelper(730000);
}
TEST_F(DelayBasedBweTest, RateIncreaseRtpTimestamps) {
  RateIncreaseRtpTimestampsTestHelper(627);
}

TEST_F(DelayBasedBweTest, CapacityDropOneStream) {
  CapacityDropTestHelper(1, false, 300, 0);
}

TEST_F(DelayBasedBweTest, CapacityDropPosOffsetChange) {
  CapacityDropTestHelper(1, false, 867, 30000);
}

TEST_F(DelayBasedBweTest, CapacityDropNegOffsetChange) {
  CapacityDropTestHelper(1, false, 933, -30000);
}

TEST_F(DelayBasedBweTest, CapacityDropOneStreamWrap) {
  CapacityDropTestHelper(1, true, 333, 0);
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

TEST_F(DelayBasedBweTest, TestInitialOveruse) {
  const uint32_t kStartBitrate = 300e3;
  const uint32_t kInitialCapacityBps = 200e3;
  const uint32_t kDummySsrc = 0;
  // High FPS to ensure that we send a lot of packets in a short time.
  const int kFps = 90;

  stream_generator_->AddStream(new test::RtpStream(kFps, kStartBitrate));
  stream_generator_->set_capacity_bps(kInitialCapacityBps);

  // Needed to initialize the AimdRateControl.
  bitrate_estimator_->SetStartBitrate(kStartBitrate);

  // Produce 30 frames (in 1/3 second) and give them to the estimator.
  uint32_t bitrate_bps = kStartBitrate;
  bool seen_overuse = false;
  for (int i = 0; i < 30; ++i) {
    bool overuse = GenerateAndProcessFrame(kDummySsrc, bitrate_bps);
    // The purpose of this test is to ensure that we back down even if we don't
    // have any acknowledged bitrate estimate yet. Hence, if the test works
    // as expected, we should not have a measured bitrate yet.
    EXPECT_FALSE(acknowledged_bitrate_estimator_->bitrate_bps().has_value());
    if (overuse) {
      EXPECT_TRUE(bitrate_observer_.updated());
      EXPECT_NEAR(bitrate_observer_.latest_bitrate(), kStartBitrate / 2, 15000);
      bitrate_bps = bitrate_observer_.latest_bitrate();
      seen_overuse = true;
      break;
    } else if (bitrate_observer_.updated()) {
      bitrate_bps = bitrate_observer_.latest_bitrate();
      bitrate_observer_.Reset();
    }
  }
  EXPECT_TRUE(seen_overuse);
  EXPECT_NEAR(bitrate_observer_.latest_bitrate(), kStartBitrate / 2, 15000);
}

}  // namespace webrtc
