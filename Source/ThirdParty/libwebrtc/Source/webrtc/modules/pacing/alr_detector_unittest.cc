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
#include "webrtc/modules/pacing/alr_detector.h"

namespace {

constexpr int kMeasuredIntervalMs = 110;
constexpr int kEstimatedBitrateBps = 300000;
constexpr int kBytesInIntervalAtEstimatedBitrate =
    (kEstimatedBitrateBps / 8) * kMeasuredIntervalMs / 1000;

}  // namespace

namespace webrtc {

TEST(AlrDetectorTest, ApplicationLimitedWhenLittleDataSent) {
  AlrDetector alr_detector;

  alr_detector.SetEstimatedBitrate(kEstimatedBitrateBps);
  for (int i = 0; i < 6; i++)
    alr_detector.OnBytesSent(0, kMeasuredIntervalMs);
  EXPECT_EQ(alr_detector.InApplicationLimitedRegion(), true);

  for (int i = 0; i < 6; i++)
    alr_detector.OnBytesSent(100, kMeasuredIntervalMs);
  EXPECT_EQ(alr_detector.InApplicationLimitedRegion(), true);
}

TEST(AlrDetectorTest, NetworkLimitedWhenSendingCloseToEstimate) {
  AlrDetector alr_detector;

  alr_detector.SetEstimatedBitrate(kEstimatedBitrateBps);
  for (int i = 0; i < 6; i++)
    alr_detector.OnBytesSent(kBytesInIntervalAtEstimatedBitrate,
                             kMeasuredIntervalMs);
  EXPECT_EQ(alr_detector.InApplicationLimitedRegion(), false);
}

}  // namespace webrtc
