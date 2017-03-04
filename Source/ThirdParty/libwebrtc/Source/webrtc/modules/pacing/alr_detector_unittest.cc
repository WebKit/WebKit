/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/pacing/alr_detector.h"

#include "webrtc/test/gtest.h"

namespace {

constexpr int kEstimatedBitrateBps = 300000;

}  // namespace

namespace webrtc {

class AlrDetectorTest : public testing::Test {
 public:
  void SetUp() override {
    alr_detector_.SetEstimatedBitrate(kEstimatedBitrateBps);
  }

  void SimulateOutgoingTraffic(int interval_ms, int usage_percentage) {
    const int kTimeStepMs = 10;
    for (int t = 0; t < interval_ms; t += kTimeStepMs) {
      now_ms += kTimeStepMs;
      alr_detector_.OnBytesSent(kEstimatedBitrateBps * usage_percentage *
                                    kTimeStepMs / (8 * 100 * 1000),
                                now_ms);
    }

    int remainder_ms = interval_ms % kTimeStepMs;
    now_ms += remainder_ms;
    if (remainder_ms > 0) {
      alr_detector_.OnBytesSent(kEstimatedBitrateBps * usage_percentage *
                                    remainder_ms / (8 * 100 * 1000),
                                remainder_ms);
    }
  }

 protected:
  AlrDetector alr_detector_;
  int64_t now_ms = 1;
};

TEST_F(AlrDetectorTest, AlrDetection) {
  // Start in non-ALR state.
  EXPECT_FALSE(alr_detector_.GetApplicationLimitedRegionStartTime());

  // Stay in non-ALR state when usage is close to 100%.
  SimulateOutgoingTraffic(500, 90);
  EXPECT_FALSE(alr_detector_.GetApplicationLimitedRegionStartTime());

  // Verify that we ALR starts when bitrate drops below 20%.
  SimulateOutgoingTraffic(500, 20);
  EXPECT_TRUE(alr_detector_.GetApplicationLimitedRegionStartTime());

  // Verify that we remain in ALR state while usage is still below 70%.
  SimulateOutgoingTraffic(500, 69);
  EXPECT_TRUE(alr_detector_.GetApplicationLimitedRegionStartTime());

  // Verify that ALR ends when usage is above 70%.
  SimulateOutgoingTraffic(500, 75);
  EXPECT_FALSE(alr_detector_.GetApplicationLimitedRegionStartTime());
}

TEST_F(AlrDetectorTest, ShortSpike) {
  // Start in non-ALR state.
  EXPECT_FALSE(alr_detector_.GetApplicationLimitedRegionStartTime());

  // Verify that we ALR starts when bitrate drops below 20%.
  SimulateOutgoingTraffic(500, 20);
  EXPECT_TRUE(alr_detector_.GetApplicationLimitedRegionStartTime());

  // Verify that we stay in ALR region even after a short bitrate spike.
  SimulateOutgoingTraffic(100, 150);
  EXPECT_TRUE(alr_detector_.GetApplicationLimitedRegionStartTime());

  SimulateOutgoingTraffic(200, 20);
  EXPECT_TRUE(alr_detector_.GetApplicationLimitedRegionStartTime());

  // ALR ends when usage is above 70%.
  SimulateOutgoingTraffic(500, 75);
  EXPECT_FALSE(alr_detector_.GetApplicationLimitedRegionStartTime());
}

TEST_F(AlrDetectorTest, BandwidthEstimateChanges) {
  // Start in non-ALR state.
  EXPECT_FALSE(alr_detector_.GetApplicationLimitedRegionStartTime());

  // ALR starts when bitrate drops below 20%.
  SimulateOutgoingTraffic(500, 20);
  EXPECT_TRUE(alr_detector_.GetApplicationLimitedRegionStartTime());

  // When bandwidth estimate drops the detector should stay in ALR mode and quit
  // it shortly afterwards as the sender continues sending the same amount of
  // traffic. This is necessary to ensure that ProbeController can still react
  // to the BWE drop by initiating a new probe.
  alr_detector_.SetEstimatedBitrate(kEstimatedBitrateBps / 5);
  EXPECT_TRUE(alr_detector_.GetApplicationLimitedRegionStartTime());
  SimulateOutgoingTraffic(10, 20);
  EXPECT_FALSE(alr_detector_.GetApplicationLimitedRegionStartTime());
}

}  // namespace webrtc
