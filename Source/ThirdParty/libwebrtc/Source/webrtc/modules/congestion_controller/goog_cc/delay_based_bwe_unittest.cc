/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/delay_based_bwe.h"

#include <string>

#include "api/transport/network_types.h"
#include "modules/congestion_controller/goog_cc/acknowledged_bitrate_estimator.h"
#include "modules/congestion_controller/goog_cc/delay_based_bwe_unittest_helper.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
constexpr int kNumProbesCluster0 = 5;
constexpr int kNumProbesCluster1 = 8;
const PacedPacketInfo kPacingInfo0(0, kNumProbesCluster0, 2000);
const PacedPacketInfo kPacingInfo1(1, kNumProbesCluster1, 4000);
constexpr float kTargetUtilizationFraction = 0.95f;
}  // namespace

INSTANTIATE_TEST_SUITE_P(
    ,
    DelayBasedBweTest,
    ::testing::Values("", "WebRTC-Bwe-NewInterArrivalDelta/Disabled/"),
    [](::testing::TestParamInfo<std::string> info) {
      return info.param.empty() ? "SafetypedInterArrival"
                                : "LegacyInterArrival";
    });

TEST_P(DelayBasedBweTest, ProbeDetection) {
  int64_t now_ms = clock_.TimeInMilliseconds();

  // First burst sent at 8 * 1000 / 10 = 800 kbps.
  for (int i = 0; i < kNumProbesCluster0; ++i) {
    clock_.AdvanceTimeMilliseconds(10);
    now_ms = clock_.TimeInMilliseconds();
    IncomingFeedback(now_ms, now_ms, 1000, kPacingInfo0);
  }
  EXPECT_TRUE(bitrate_observer_.updated());

  // Second burst sent at 8 * 1000 / 5 = 1600 kbps.
  for (int i = 0; i < kNumProbesCluster1; ++i) {
    clock_.AdvanceTimeMilliseconds(5);
    now_ms = clock_.TimeInMilliseconds();
    IncomingFeedback(now_ms, now_ms, 1000, kPacingInfo1);
  }

  EXPECT_TRUE(bitrate_observer_.updated());
  EXPECT_GT(bitrate_observer_.latest_bitrate(), 1500000u);
}

TEST_P(DelayBasedBweTest, ProbeDetectionNonPacedPackets) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  // First burst sent at 8 * 1000 / 10 = 800 kbps, but with every other packet
  // not being paced which could mess things up.
  for (int i = 0; i < kNumProbesCluster0; ++i) {
    clock_.AdvanceTimeMilliseconds(5);
    now_ms = clock_.TimeInMilliseconds();
    IncomingFeedback(now_ms, now_ms, 1000, kPacingInfo0);
    // Non-paced packet, arriving 5 ms after.
    clock_.AdvanceTimeMilliseconds(5);
    IncomingFeedback(now_ms, now_ms, 100, PacedPacketInfo());
  }

  EXPECT_TRUE(bitrate_observer_.updated());
  EXPECT_GT(bitrate_observer_.latest_bitrate(), 800000u);
}

TEST_P(DelayBasedBweTest, ProbeDetectionFasterArrival) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  // First burst sent at 8 * 1000 / 10 = 800 kbps.
  // Arriving at 8 * 1000 / 5 = 1600 kbps.
  int64_t send_time_ms = 0;
  for (int i = 0; i < kNumProbesCluster0; ++i) {
    clock_.AdvanceTimeMilliseconds(1);
    send_time_ms += 10;
    now_ms = clock_.TimeInMilliseconds();
    IncomingFeedback(now_ms, send_time_ms, 1000, kPacingInfo0);
  }

  EXPECT_FALSE(bitrate_observer_.updated());
}

TEST_P(DelayBasedBweTest, ProbeDetectionSlowerArrival) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  // First burst sent at 8 * 1000 / 5 = 1600 kbps.
  // Arriving at 8 * 1000 / 7 = 1142 kbps.
  // Since the receive rate is significantly below the send rate, we expect to
  // use 95% of the estimated capacity.
  int64_t send_time_ms = 0;
  for (int i = 0; i < kNumProbesCluster1; ++i) {
    clock_.AdvanceTimeMilliseconds(7);
    send_time_ms += 5;
    now_ms = clock_.TimeInMilliseconds();
    IncomingFeedback(now_ms, send_time_ms, 1000, kPacingInfo1);
  }

  EXPECT_TRUE(bitrate_observer_.updated());
  EXPECT_NEAR(bitrate_observer_.latest_bitrate(),
              kTargetUtilizationFraction * 1140000u, 10000u);
}

TEST_P(DelayBasedBweTest, ProbeDetectionSlowerArrivalHighBitrate) {
  int64_t now_ms = clock_.TimeInMilliseconds();
  // Burst sent at 8 * 1000 / 1 = 8000 kbps.
  // Arriving at 8 * 1000 / 2 = 4000 kbps.
  // Since the receive rate is significantly below the send rate, we expect to
  // use 95% of the estimated capacity.
  int64_t send_time_ms = 0;
  for (int i = 0; i < kNumProbesCluster1; ++i) {
    clock_.AdvanceTimeMilliseconds(2);
    send_time_ms += 1;
    now_ms = clock_.TimeInMilliseconds();
    IncomingFeedback(now_ms, send_time_ms, 1000, kPacingInfo1);
  }

  EXPECT_TRUE(bitrate_observer_.updated());
  EXPECT_NEAR(bitrate_observer_.latest_bitrate(),
              kTargetUtilizationFraction * 4000000u, 10000u);
}

TEST_P(DelayBasedBweTest, GetExpectedBwePeriodMs) {
  auto default_interval = bitrate_estimator_->GetExpectedBwePeriod();
  EXPECT_GT(default_interval.ms(), 0);
  CapacityDropTestHelper(1, true, 333, 0);
  auto interval = bitrate_estimator_->GetExpectedBwePeriod();
  EXPECT_GT(interval.ms(), 0);
  EXPECT_NE(interval.ms(), default_interval.ms());
}

TEST_P(DelayBasedBweTest, InitialBehavior) {
  InitialBehaviorTestHelper(730000);
}

TEST_P(DelayBasedBweTest, RateIncreaseReordering) {
  RateIncreaseReorderingTestHelper(730000);
}
TEST_P(DelayBasedBweTest, RateIncreaseRtpTimestamps) {
  RateIncreaseRtpTimestampsTestHelper(622);
}

TEST_P(DelayBasedBweTest, CapacityDropOneStream) {
  CapacityDropTestHelper(1, false, 300, 0);
}

TEST_P(DelayBasedBweTest, CapacityDropPosOffsetChange) {
  CapacityDropTestHelper(1, false, 867, 30000);
}

TEST_P(DelayBasedBweTest, CapacityDropNegOffsetChange) {
  CapacityDropTestHelper(1, false, 933, -30000);
}

TEST_P(DelayBasedBweTest, CapacityDropOneStreamWrap) {
  CapacityDropTestHelper(1, true, 333, 0);
}

TEST_P(DelayBasedBweTest, TestTimestampGrouping) {
  TestTimestampGroupingTestHelper();
}

TEST_P(DelayBasedBweTest, TestShortTimeoutAndWrap) {
  // Simulate a client leaving and rejoining the call after 35 seconds. This
  // will make abs send time wrap, so if streams aren't timed out properly
  // the next 30 seconds of packets will be out of order.
  TestWrappingHelper(35);
}

TEST_P(DelayBasedBweTest, TestLongTimeoutAndWrap) {
  // Simulate a client leaving and rejoining the call after some multiple of
  // 64 seconds later. This will cause a zero difference in abs send times due
  // to the wrap, but a big difference in arrival time, if streams aren't
  // properly timed out.
  TestWrappingHelper(10 * 64);
}

TEST_P(DelayBasedBweTest, TestInitialOveruse) {
  const DataRate kStartBitrate = DataRate::KilobitsPerSec(300);
  const DataRate kInitialCapacity = DataRate::KilobitsPerSec(200);
  const uint32_t kDummySsrc = 0;
  // High FPS to ensure that we send a lot of packets in a short time.
  const int kFps = 90;

  stream_generator_->AddStream(new test::RtpStream(kFps, kStartBitrate.bps()));
  stream_generator_->set_capacity_bps(kInitialCapacity.bps());

  // Needed to initialize the AimdRateControl.
  bitrate_estimator_->SetStartBitrate(kStartBitrate);

  // Produce 30 frames (in 1/3 second) and give them to the estimator.
  int64_t bitrate_bps = kStartBitrate.bps();
  bool seen_overuse = false;
  for (int i = 0; i < 30; ++i) {
    bool overuse = GenerateAndProcessFrame(kDummySsrc, bitrate_bps);
    // The purpose of this test is to ensure that we back down even if we don't
    // have any acknowledged bitrate estimate yet. Hence, if the test works
    // as expected, we should not have a measured bitrate yet.
    EXPECT_FALSE(acknowledged_bitrate_estimator_->bitrate().has_value());
    if (overuse) {
      EXPECT_TRUE(bitrate_observer_.updated());
      EXPECT_NEAR(bitrate_observer_.latest_bitrate(), kStartBitrate.bps() / 2,
                  15000);
      bitrate_bps = bitrate_observer_.latest_bitrate();
      seen_overuse = true;
      break;
    } else if (bitrate_observer_.updated()) {
      bitrate_bps = bitrate_observer_.latest_bitrate();
      bitrate_observer_.Reset();
    }
  }
  EXPECT_TRUE(seen_overuse);
  EXPECT_NEAR(bitrate_observer_.latest_bitrate(), kStartBitrate.bps() / 2,
              15000);
}

class DelayBasedBweTestWithBackoffTimeoutExperiment : public DelayBasedBweTest {
};

INSTANTIATE_TEST_SUITE_P(
    ,
    DelayBasedBweTestWithBackoffTimeoutExperiment,
    ::testing::Values(
        "WebRTC-BweAimdRateControlConfig/initial_backoff_interval:200ms/"));

// This test subsumes and improves DelayBasedBweTest.TestInitialOveruse above.
TEST_P(DelayBasedBweTestWithBackoffTimeoutExperiment, TestInitialOveruse) {
  const DataRate kStartBitrate = DataRate::KilobitsPerSec(300);
  const DataRate kInitialCapacity = DataRate::KilobitsPerSec(200);
  const uint32_t kDummySsrc = 0;
  // High FPS to ensure that we send a lot of packets in a short time.
  const int kFps = 90;

  stream_generator_->AddStream(new test::RtpStream(kFps, kStartBitrate.bps()));
  stream_generator_->set_capacity_bps(kInitialCapacity.bps());

  // Needed to initialize the AimdRateControl.
  bitrate_estimator_->SetStartBitrate(kStartBitrate);

  // Produce 30 frames (in 1/3 second) and give them to the estimator.
  int64_t bitrate_bps = kStartBitrate.bps();
  bool seen_overuse = false;
  for (int frames = 0; frames < 30 && !seen_overuse; ++frames) {
    bool overuse = GenerateAndProcessFrame(kDummySsrc, bitrate_bps);
    // The purpose of this test is to ensure that we back down even if we don't
    // have any acknowledged bitrate estimate yet. Hence, if the test works
    // as expected, we should not have a measured bitrate yet.
    EXPECT_FALSE(acknowledged_bitrate_estimator_->bitrate().has_value());
    if (overuse) {
      EXPECT_TRUE(bitrate_observer_.updated());
      EXPECT_NEAR(bitrate_observer_.latest_bitrate(), kStartBitrate.bps() / 2,
                  15000);
      bitrate_bps = bitrate_observer_.latest_bitrate();
      seen_overuse = true;
    } else if (bitrate_observer_.updated()) {
      bitrate_bps = bitrate_observer_.latest_bitrate();
      bitrate_observer_.Reset();
    }
  }
  EXPECT_TRUE(seen_overuse);
  // Continue generating an additional 15 frames (equivalent to 167 ms) and
  // verify that we don't back down further.
  for (int frames = 0; frames < 15 && seen_overuse; ++frames) {
    bool overuse = GenerateAndProcessFrame(kDummySsrc, bitrate_bps);
    EXPECT_FALSE(overuse);
    if (bitrate_observer_.updated()) {
      bitrate_bps = bitrate_observer_.latest_bitrate();
      EXPECT_GE(bitrate_bps, kStartBitrate.bps() / 2 - 15000);
      EXPECT_LE(bitrate_bps, kInitialCapacity.bps() + 15000);
      bitrate_observer_.Reset();
    }
  }
}

}  // namespace webrtc
