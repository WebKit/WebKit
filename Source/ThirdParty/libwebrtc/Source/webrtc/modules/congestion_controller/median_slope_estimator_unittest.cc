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
#include "webrtc/base/random.h"
#include "webrtc/modules/congestion_controller/median_slope_estimator.h"

namespace webrtc {

namespace {
constexpr size_t kWindowSize = 20;
constexpr double kGain = 1;
constexpr int64_t kAvgTimeBetweenPackets = 10;
constexpr size_t kPacketCount = 2 * kWindowSize + 1;

void TestEstimator(double slope, double jitter_stddev, double tolerance) {
  MedianSlopeEstimator estimator(kWindowSize, kGain);
  Random random(0x1234567);
  int64_t send_times[kPacketCount];
  int64_t recv_times[kPacketCount];
  int64_t send_start_time = random.Rand(1000000);
  int64_t recv_start_time = random.Rand(1000000);
  for (size_t i = 0; i < kPacketCount; ++i) {
    send_times[i] = send_start_time + i * kAvgTimeBetweenPackets;
    double latency = i * kAvgTimeBetweenPackets / (1 - slope);
    double jitter = random.Gaussian(0, jitter_stddev);
    recv_times[i] = recv_start_time + latency + jitter;
  }
  for (size_t i = 1; i < kPacketCount; ++i) {
    double recv_delta = recv_times[i] - recv_times[i - 1];
    double send_delta = send_times[i] - send_times[i - 1];
    estimator.Update(recv_delta, send_delta, recv_times[i]);
    if (i < kWindowSize)
      EXPECT_NEAR(estimator.trendline_slope(), 0, 0.001);
    else
      EXPECT_NEAR(estimator.trendline_slope(), slope, tolerance);
  }
}
}  // namespace

TEST(MedianSlopeEstimator, PerfectLineSlopeOneHalf) {
  TestEstimator(0.5, 0, 0.001);
}

TEST(MedianSlopeEstimator, PerfectLineSlopeMinusOne) {
  TestEstimator(-1, 0, 0.001);
}

TEST(MedianSlopeEstimator, PerfectLineSlopeZero) {
  TestEstimator(0, 0, 0.001);
}

TEST(MedianSlopeEstimator, JitteryLineSlopeOneHalf) {
  TestEstimator(0.5, kAvgTimeBetweenPackets / 3.0, 0.01);
}

TEST(MedianSlopeEstimator, JitteryLineSlopeMinusOne) {
  TestEstimator(-1, kAvgTimeBetweenPackets / 3.0, 0.05);
}

TEST(MedianSlopeEstimator, JitteryLineSlopeZero) {
  TestEstimator(0, kAvgTimeBetweenPackets / 3.0, 0.02);
}

}  // namespace webrtc
