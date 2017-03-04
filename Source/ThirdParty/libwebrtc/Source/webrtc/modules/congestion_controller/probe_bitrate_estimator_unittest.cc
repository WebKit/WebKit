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

constexpr int INVALID_BPS = -1;

class TestProbeBitrateEstimator : public ::testing::Test {
 public:
  TestProbeBitrateEstimator() : probe_bitrate_estimator_() {}

  // TODO(philipel): Use PacedPacketInfo when ProbeBitrateEstimator is rewritten
  //                 to use that information.
  void AddPacketFeedback(int probe_cluster_id,
                         size_t size_bytes,
                         int64_t send_time_ms,
                         int64_t arrival_time_ms) {
    PacketInfo info(arrival_time_ms, send_time_ms, 0, size_bytes,
                    PacedPacketInfo(probe_cluster_id, -1, -1));
    measured_bps_ =
        probe_bitrate_estimator_.HandleProbeAndEstimateBitrate(info);
  }

 protected:
  int measured_bps_ = INVALID_BPS;
  ProbeBitrateEstimator probe_bitrate_estimator_;
};

TEST_F(TestProbeBitrateEstimator, OneCluster) {
  AddPacketFeedback(0, 1000, 0, 10);
  AddPacketFeedback(0, 1000, 10, 20);
  AddPacketFeedback(0, 1000, 20, 30);
  AddPacketFeedback(0, 1000, 30, 40);

  EXPECT_NEAR(measured_bps_, 800000, 10);
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

  EXPECT_EQ(measured_bps_, INVALID_BPS);
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

  EXPECT_EQ(measured_bps_, INVALID_BPS);
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

  EXPECT_EQ(measured_bps_, INVALID_BPS);
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

}  // namespace webrtc
