/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/capture_clock_offset_updater.h"

#include "system_wrappers/include/ntp_time.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

TEST(AbsoluteCaptureTimeReceiverTest,
     SkipEstimatedCaptureClockOffsetIfRemoteToLocalClockOffsetIsUnknown) {
  static const absl::optional<int64_t> kRemoteCaptureClockOffset =
      Int64MsToQ32x32(-350);
  CaptureClockOffsetUpdater updater;
  updater.SetRemoteToLocalClockOffset(absl::nullopt);
  EXPECT_EQ(
      updater.AdjustEstimatedCaptureClockOffset(kRemoteCaptureClockOffset),
      absl::nullopt);
}

TEST(AbsoluteCaptureTimeReceiverTest,
     SkipEstimatedCaptureClockOffsetIfRemoteCaptureClockOffsetIsUnknown) {
  static const absl::optional<int64_t> kCaptureClockOffsetNull = absl::nullopt;
  CaptureClockOffsetUpdater updater;
  updater.SetRemoteToLocalClockOffset(0);
  EXPECT_EQ(updater.AdjustEstimatedCaptureClockOffset(kCaptureClockOffsetNull),
            kCaptureClockOffsetNull);

  static const absl::optional<int64_t> kRemoteCaptureClockOffset =
      Int64MsToQ32x32(-350);
  EXPECT_EQ(
      updater.AdjustEstimatedCaptureClockOffset(kRemoteCaptureClockOffset),
      kRemoteCaptureClockOffset);
}

TEST(AbsoluteCaptureTimeReceiverTest, EstimatedCaptureClockOffsetArithmetic) {
  static const absl::optional<int64_t> kRemoteCaptureClockOffset =
      Int64MsToQ32x32(-350);
  static const absl::optional<int64_t> kRemoteToLocalClockOffset =
      Int64MsToQ32x32(-7000007);
  CaptureClockOffsetUpdater updater;
  updater.SetRemoteToLocalClockOffset(kRemoteToLocalClockOffset);
  EXPECT_THAT(
      updater.AdjustEstimatedCaptureClockOffset(kRemoteCaptureClockOffset),
      ::testing::Optional(::testing::Eq(*kRemoteCaptureClockOffset +
                                        *kRemoteToLocalClockOffset)));
}

}  // namespace webrtc
