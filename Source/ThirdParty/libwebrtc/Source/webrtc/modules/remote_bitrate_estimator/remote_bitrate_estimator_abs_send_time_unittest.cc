/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.h"

#include "modules/remote_bitrate_estimator/remote_bitrate_estimator_unittest_helper.h"
#include "rtc_base/constructor_magic.h"
#include "test/gtest.h"

namespace webrtc {

class RemoteBitrateEstimatorAbsSendTimeTest
    : public RemoteBitrateEstimatorTest {
 public:
  RemoteBitrateEstimatorAbsSendTimeTest() {}
  virtual void SetUp() {
    bitrate_estimator_.reset(new RemoteBitrateEstimatorAbsSendTime(
        bitrate_observer_.get(), &clock_));
  }

 protected:
  RTC_DISALLOW_COPY_AND_ASSIGN(RemoteBitrateEstimatorAbsSendTimeTest);
};

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, InitialBehavior) {
  InitialBehaviorTestHelper(674840);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, RateIncreaseReordering) {
  RateIncreaseReorderingTestHelper(674840);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, RateIncreaseRtpTimestamps) {
  RateIncreaseRtpTimestampsTestHelper(1237);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, CapacityDropOneStream) {
  CapacityDropTestHelper(1, false, 633, 0);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, CapacityDropPosOffsetChange) {
  CapacityDropTestHelper(1, false, 267, 30000);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, CapacityDropNegOffsetChange) {
  CapacityDropTestHelper(1, false, 267, -30000);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, CapacityDropOneStreamWrap) {
  CapacityDropTestHelper(1, true, 633, 0);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, CapacityDropTwoStreamsWrap) {
  CapacityDropTestHelper(2, true, 700, 0);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, CapacityDropThreeStreamsWrap) {
  CapacityDropTestHelper(3, true, 633, 0);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, CapacityDropThirteenStreamsWrap) {
  CapacityDropTestHelper(13, true, 667, 0);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, CapacityDropNineteenStreamsWrap) {
  CapacityDropTestHelper(19, true, 667, 0);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, CapacityDropThirtyStreamsWrap) {
  CapacityDropTestHelper(30, true, 667, 0);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, TestTimestampGrouping) {
  TestTimestampGroupingTestHelper();
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, TestShortTimeoutAndWrap) {
  // Simulate a client leaving and rejoining the call after 35 seconds. This
  // will make abs send time wrap, so if streams aren't timed out properly
  // the next 30 seconds of packets will be out of order.
  TestWrappingHelper(35);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, TestLongTimeoutAndWrap) {
  // Simulate a client leaving and rejoining the call after some multiple of
  // 64 seconds later. This will cause a zero difference in abs send times due
  // to the wrap, but a big difference in arrival time, if streams aren't
  // properly timed out.
  TestWrappingHelper(10 * 64);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, TestProcessAfterTimeout) {
  // This time constant must be equal to the ones defined for the
  // RemoteBitrateEstimator.
  const int64_t kStreamTimeOutMs = 2000;
  const int64_t kProcessIntervalMs = 1000;
  IncomingPacket(0, 1000, clock_.TimeInMilliseconds(), 0, 0);
  clock_.AdvanceTimeMilliseconds(kStreamTimeOutMs + 1);
  // Trigger timeout.
  bitrate_estimator_->Process();
  clock_.AdvanceTimeMilliseconds(kProcessIntervalMs);
  // This shouldn't crash.
  bitrate_estimator_->Process();
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, TestProbeDetection) {
  const int kProbeLength = 5;
  int64_t now_ms = clock_.TimeInMilliseconds();
  // First burst sent at 8 * 1000 / 10 = 800 kbps.
  for (int i = 0; i < kProbeLength; ++i) {
    clock_.AdvanceTimeMilliseconds(10);
    now_ms = clock_.TimeInMilliseconds();
    IncomingPacket(0, 1000, now_ms, 90 * now_ms, AbsSendTime(now_ms, 1000));
  }

  // Second burst sent at 8 * 1000 / 5 = 1600 kbps.
  for (int i = 0; i < kProbeLength; ++i) {
    clock_.AdvanceTimeMilliseconds(5);
    now_ms = clock_.TimeInMilliseconds();
    IncomingPacket(0, 1000, now_ms, 90 * now_ms, AbsSendTime(now_ms, 1000));
  }

  bitrate_estimator_->Process();
  EXPECT_TRUE(bitrate_observer_->updated());
  EXPECT_GT(bitrate_observer_->latest_bitrate(), 1500000u);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest,
       TestProbeDetectionNonPacedPackets) {
  const int kProbeLength = 5;
  int64_t now_ms = clock_.TimeInMilliseconds();
  // First burst sent at 8 * 1000 / 10 = 800 kbps, but with every other packet
  // not being paced which could mess things up.
  for (int i = 0; i < kProbeLength; ++i) {
    clock_.AdvanceTimeMilliseconds(5);
    now_ms = clock_.TimeInMilliseconds();
    IncomingPacket(0, 1000, now_ms, 90 * now_ms, AbsSendTime(now_ms, 1000));
    // Non-paced packet, arriving 5 ms after.
    clock_.AdvanceTimeMilliseconds(5);
    IncomingPacket(0, 100, now_ms, 90 * now_ms, AbsSendTime(now_ms, 1000));
  }

  bitrate_estimator_->Process();
  EXPECT_TRUE(bitrate_observer_->updated());
  EXPECT_GT(bitrate_observer_->latest_bitrate(), 800000u);
}

// Packets will require 5 ms to be transmitted to the receiver, causing packets
// of the second probe to be dispersed.
TEST_F(RemoteBitrateEstimatorAbsSendTimeTest,
       TestProbeDetectionTooHighBitrate) {
  const int kProbeLength = 5;
  int64_t now_ms = clock_.TimeInMilliseconds();
  int64_t send_time_ms = 0;
  // First burst sent at 8 * 1000 / 10 = 800 kbps.
  for (int i = 0; i < kProbeLength; ++i) {
    clock_.AdvanceTimeMilliseconds(10);
    now_ms = clock_.TimeInMilliseconds();
    send_time_ms += 10;
    IncomingPacket(0, 1000, now_ms, 90 * send_time_ms,
                   AbsSendTime(send_time_ms, 1000));
  }

  // Second burst sent at 8 * 1000 / 5 = 1600 kbps, arriving at 8 * 1000 / 8 =
  // 1000 kbps.
  for (int i = 0; i < kProbeLength; ++i) {
    clock_.AdvanceTimeMilliseconds(8);
    now_ms = clock_.TimeInMilliseconds();
    send_time_ms += 5;
    IncomingPacket(0, 1000, now_ms, send_time_ms,
                   AbsSendTime(send_time_ms, 1000));
  }

  bitrate_estimator_->Process();
  EXPECT_TRUE(bitrate_observer_->updated());
  EXPECT_NEAR(bitrate_observer_->latest_bitrate(), 800000u, 10000);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest,
       TestProbeDetectionSlightlyFasterArrival) {
  const int kProbeLength = 5;
  int64_t now_ms = clock_.TimeInMilliseconds();
  // First burst sent at 8 * 1000 / 10 = 800 kbps.
  // Arriving at 8 * 1000 / 5 = 1600 kbps.
  int64_t send_time_ms = 0;
  for (int i = 0; i < kProbeLength; ++i) {
    clock_.AdvanceTimeMilliseconds(5);
    send_time_ms += 10;
    now_ms = clock_.TimeInMilliseconds();
    IncomingPacket(0, 1000, now_ms, 90 * send_time_ms,
                   AbsSendTime(send_time_ms, 1000));
  }

  bitrate_estimator_->Process();
  EXPECT_TRUE(bitrate_observer_->updated());
  EXPECT_GT(bitrate_observer_->latest_bitrate(), 800000u);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, TestProbeDetectionFasterArrival) {
  const int kProbeLength = 5;
  int64_t now_ms = clock_.TimeInMilliseconds();
  // First burst sent at 8 * 1000 / 10 = 800 kbps.
  // Arriving at 8 * 1000 / 5 = 1600 kbps.
  int64_t send_time_ms = 0;
  for (int i = 0; i < kProbeLength; ++i) {
    clock_.AdvanceTimeMilliseconds(1);
    send_time_ms += 10;
    now_ms = clock_.TimeInMilliseconds();
    IncomingPacket(0, 1000, now_ms, 90 * send_time_ms,
                   AbsSendTime(send_time_ms, 1000));
  }

  bitrate_estimator_->Process();
  EXPECT_FALSE(bitrate_observer_->updated());
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, TestProbeDetectionSlowerArrival) {
  const int kProbeLength = 5;
  int64_t now_ms = clock_.TimeInMilliseconds();
  // First burst sent at 8 * 1000 / 5 = 1600 kbps.
  // Arriving at 8 * 1000 / 7 = 1142 kbps.
  int64_t send_time_ms = 0;
  for (int i = 0; i < kProbeLength; ++i) {
    clock_.AdvanceTimeMilliseconds(7);
    send_time_ms += 5;
    now_ms = clock_.TimeInMilliseconds();
    IncomingPacket(0, 1000, now_ms, 90 * send_time_ms,
                   AbsSendTime(send_time_ms, 1000));
  }

  bitrate_estimator_->Process();
  EXPECT_TRUE(bitrate_observer_->updated());
  EXPECT_NEAR(bitrate_observer_->latest_bitrate(), 1140000, 10000);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest,
       TestProbeDetectionSlowerArrivalHighBitrate) {
  const int kProbeLength = 5;
  int64_t now_ms = clock_.TimeInMilliseconds();
  // Burst sent at 8 * 1000 / 1 = 8000 kbps.
  // Arriving at 8 * 1000 / 2 = 4000 kbps.
  int64_t send_time_ms = 0;
  for (int i = 0; i < kProbeLength; ++i) {
    clock_.AdvanceTimeMilliseconds(2);
    send_time_ms += 1;
    now_ms = clock_.TimeInMilliseconds();
    IncomingPacket(0, 1000, now_ms, 90 * send_time_ms,
                   AbsSendTime(send_time_ms, 1000));
  }

  bitrate_estimator_->Process();
  EXPECT_TRUE(bitrate_observer_->updated());
  EXPECT_NEAR(bitrate_observer_->latest_bitrate(), 4000000u, 10000);
}

TEST_F(RemoteBitrateEstimatorAbsSendTimeTest, ProbingIgnoresSmallPackets) {
  const int kProbeLength = 5;
  int64_t now_ms = clock_.TimeInMilliseconds();
  // Probing with 200 bytes every 10 ms, should be ignored by the probe
  // detection.
  for (int i = 0; i < kProbeLength; ++i) {
    clock_.AdvanceTimeMilliseconds(10);
    now_ms = clock_.TimeInMilliseconds();
    IncomingPacket(0, 200, now_ms, 90 * now_ms, AbsSendTime(now_ms, 1000));
  }

  bitrate_estimator_->Process();
  EXPECT_FALSE(bitrate_observer_->updated());

  // Followed by a probe with 1000 bytes packets, should be detected as a
  // probe.
  for (int i = 0; i < kProbeLength; ++i) {
    clock_.AdvanceTimeMilliseconds(10);
    now_ms = clock_.TimeInMilliseconds();
    IncomingPacket(0, 1000, now_ms, 90 * now_ms, AbsSendTime(now_ms, 1000));
  }

  // Wait long enough so that we can call Process again.
  clock_.AdvanceTimeMilliseconds(1000);

  bitrate_estimator_->Process();
  EXPECT_TRUE(bitrate_observer_->updated());
  EXPECT_NEAR(bitrate_observer_->latest_bitrate(), 800000u, 10000);
}
}  // namespace webrtc
