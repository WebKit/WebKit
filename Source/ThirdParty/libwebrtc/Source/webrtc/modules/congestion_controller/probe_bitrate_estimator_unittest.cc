/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/congestion_controller/probe_bitrate_estimator.h"

#include <vector>
#include <utility>

#include "webrtc/modules/remote_bitrate_estimator/aimd_rate_control.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {
constexpr int kInvalidBitrate = -1;
constexpr int kDefaultMinProbes = 5;
constexpr int kDefaultMinBytes = 5000;
}  // anonymous namespace

class TestProbeBitrateEstimator : public ::testing::Test {
 public:
  TestProbeBitrateEstimator() : probe_bitrate_estimator_(nullptr) {}

  // TODO(philipel): Use PacedPacketInfo when ProbeBitrateEstimator is rewritten
  //                 to use that information.
  void AddPacketFeedback(int probe_cluster_id,
                         size_t size_bytes,
                         int64_t send_time_ms,
                         int64_t arrival_time_ms,
                         int min_probes = kDefaultMinProbes,
                         int min_bytes = kDefaultMinBytes) {
    PacedPacketInfo pacing_info(probe_cluster_id, min_probes, min_bytes);
    PacketFeedback packet_feedback(arrival_time_ms, send_time_ms, 0, size_bytes,
                                   pacing_info);
    measured_bps_ =
        probe_bitrate_estimator_.HandleProbeAndEstimateBitrate(packet_feedback);
  }

 protected:
  int measured_bps_ = kInvalidBitrate;
  ProbeBitrateEstimator probe_bitrate_estimator_;
};

TEST_F(TestProbeBitrateEstimator, OneCluster) {
  AddPacketFeedback(0, 1000, 0, 10);
  AddPacketFeedback(0, 1000, 10, 20);
  AddPacketFeedback(0, 1000, 20, 30);
  AddPacketFeedback(0, 1000, 30, 40);

  EXPECT_NEAR(measured_bps_, 800000, 10);
}

TEST_F(TestProbeBitrateEstimator, OneClusterTooFewProbes) {
  AddPacketFeedback(0, 2000, 0, 10);
  AddPacketFeedback(0, 2000, 10, 20);
  AddPacketFeedback(0, 2000, 20, 30);

  EXPECT_EQ(kInvalidBitrate, measured_bps_);
}

TEST_F(TestProbeBitrateEstimator, OneClusterTooFewBytes) {
  const int kMinBytes = 6000;
  AddPacketFeedback(0, 800, 0, 10, kDefaultMinProbes, kMinBytes);
  AddPacketFeedback(0, 800, 10, 20, kDefaultMinProbes, kMinBytes);
  AddPacketFeedback(0, 800, 20, 30, kDefaultMinProbes, kMinBytes);
  AddPacketFeedback(0, 800, 30, 40, kDefaultMinProbes, kMinBytes);
  AddPacketFeedback(0, 800, 40, 50, kDefaultMinProbes, kMinBytes);

  EXPECT_EQ(kInvalidBitrate, measured_bps_);
}

TEST_F(TestProbeBitrateEstimator, SmallCluster) {
  const int kMinBytes = 1000;
  AddPacketFeedback(0, 150, 0, 10, kDefaultMinProbes, kMinBytes);
  AddPacketFeedback(0, 150, 10, 20, kDefaultMinProbes, kMinBytes);
  AddPacketFeedback(0, 150, 20, 30, kDefaultMinProbes, kMinBytes);
  AddPacketFeedback(0, 150, 30, 40, kDefaultMinProbes, kMinBytes);
  AddPacketFeedback(0, 150, 40, 50, kDefaultMinProbes, kMinBytes);
  AddPacketFeedback(0, 150, 50, 60, kDefaultMinProbes, kMinBytes);
  EXPECT_NEAR(measured_bps_, 120000, 10);
}

TEST_F(TestProbeBitrateEstimator, LargeCluster) {
  const int kMinProbes = 30;
  const int kMinBytes = 312500;

  int64_t send_time = 0;
  int64_t receive_time = 5;
  for (int i = 0; i < 25; ++i) {
    AddPacketFeedback(0, 12500, send_time, receive_time, kMinProbes, kMinBytes);
    ++send_time;
    ++receive_time;
  }
  EXPECT_NEAR(measured_bps_, 100000000, 10);
}

TEST_F(TestProbeBitrateEstimator, FastReceive) {
  AddPacketFeedback(0, 1000, 0, 15);
  AddPacketFeedback(0, 1000, 10, 30);
  AddPacketFeedback(0, 1000, 20, 35);
  AddPacketFeedback(0, 1000, 30, 40);

  EXPECT_NEAR(measured_bps_, 800000, 10);
}

TEST_F(TestProbeBitrateEstimator, TooFastReceive) {
  AddPacketFeedback(0, 1000, 0, 19);
  AddPacketFeedback(0, 1000, 10, 22);
  AddPacketFeedback(0, 1000, 20, 25);
  AddPacketFeedback(0, 1000, 40, 27);

  EXPECT_EQ(measured_bps_, kInvalidBitrate);
}

TEST_F(TestProbeBitrateEstimator, SlowReceive) {
  AddPacketFeedback(0, 1000, 0, 10);
  AddPacketFeedback(0, 1000, 10, 40);
  AddPacketFeedback(0, 1000, 20, 70);
  AddPacketFeedback(0, 1000, 30, 85);

  EXPECT_NEAR(measured_bps_, 320000, 10);
}

TEST_F(TestProbeBitrateEstimator, BurstReceive) {
  AddPacketFeedback(0, 1000, 0, 50);
  AddPacketFeedback(0, 1000, 10, 50);
  AddPacketFeedback(0, 1000, 20, 50);
  AddPacketFeedback(0, 1000, 40, 50);

  EXPECT_EQ(measured_bps_, kInvalidBitrate);
}

TEST_F(TestProbeBitrateEstimator, MultipleClusters) {
  AddPacketFeedback(0, 1000, 0, 10);
  AddPacketFeedback(0, 1000, 10, 20);
  AddPacketFeedback(0, 1000, 20, 30);
  AddPacketFeedback(0, 1000, 40, 60);
  EXPECT_NEAR(measured_bps_, 480000, 10);

  AddPacketFeedback(0, 1000, 50, 60);
  EXPECT_NEAR(measured_bps_, 640000, 10);

  AddPacketFeedback(1, 1000, 60, 70);
  AddPacketFeedback(1, 1000, 65, 77);
  AddPacketFeedback(1, 1000, 70, 84);
  AddPacketFeedback(1, 1000, 75, 90);

  EXPECT_NEAR(measured_bps_, 1200000, 10);
}

TEST_F(TestProbeBitrateEstimator, IgnoreOldClusters) {
  AddPacketFeedback(0, 1000, 0, 10);
  AddPacketFeedback(0, 1000, 10, 20);
  AddPacketFeedback(0, 1000, 20, 30);

  AddPacketFeedback(1, 1000, 60, 70);
  AddPacketFeedback(1, 1000, 65, 77);
  AddPacketFeedback(1, 1000, 70, 84);
  AddPacketFeedback(1, 1000, 75, 90);

  EXPECT_NEAR(measured_bps_, 1200000, 10);

  // Coming in 6s later
  AddPacketFeedback(0, 1000, 40 + 6000, 60 + 6000);

  EXPECT_EQ(measured_bps_, kInvalidBitrate);
}

TEST_F(TestProbeBitrateEstimator, IgnoreSizeLastSendPacket) {
  AddPacketFeedback(0, 1000, 0, 10);
  AddPacketFeedback(0, 1000, 10, 20);
  AddPacketFeedback(0, 1000, 20, 30);
  AddPacketFeedback(0, 1000, 30, 40);
  AddPacketFeedback(0, 1500, 40, 50);

  EXPECT_NEAR(measured_bps_, 800000, 10);
}

TEST_F(TestProbeBitrateEstimator, IgnoreSizeFirstReceivePacket) {
  AddPacketFeedback(0, 1500, 0, 10);
  AddPacketFeedback(0, 1000, 10, 20);
  AddPacketFeedback(0, 1000, 20, 30);
  AddPacketFeedback(0, 1000, 30, 40);

  EXPECT_NEAR(measured_bps_, 800000, 10);
}

TEST_F(TestProbeBitrateEstimator, NoLastEstimatedBitrateBps) {
  EXPECT_FALSE(probe_bitrate_estimator_.FetchAndResetLastEstimatedBitrateBps());
}

TEST_F(TestProbeBitrateEstimator, FetchLastEstimatedBitrateBps) {
  AddPacketFeedback(0, 1000, 0, 10);
  AddPacketFeedback(0, 1000, 10, 20);
  AddPacketFeedback(0, 1000, 20, 30);
  AddPacketFeedback(0, 1000, 30, 40);

  auto estimated_bitrate_bps =
      probe_bitrate_estimator_.FetchAndResetLastEstimatedBitrateBps();
  EXPECT_TRUE(estimated_bitrate_bps);
  EXPECT_NEAR(*estimated_bitrate_bps, 800000, 10);
  EXPECT_FALSE(probe_bitrate_estimator_.FetchAndResetLastEstimatedBitrateBps());
}

}  // namespace webrtc
