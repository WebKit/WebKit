/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/sdp_fmtp_utils.h"

#include <string.h>
#include <map>
#include <utility>

#include "rtc_base/string_to_number.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
// Max frame rate for VP8 and VP9 video.
const char kVPxFmtpMaxFrameRate[] = "max-fr";
// Max frame size for VP8 and VP9 video.
const char kVPxFmtpMaxFrameSize[] = "max-fs";
}  // namespace

TEST(SdpFmtpUtilsTest, MaxFrameRateIsMissingOrInvalid) {
  SdpVideoFormat::Parameters params;
  absl::optional<int> empty = ParseSdpForVPxMaxFrameRate(params);
  EXPECT_FALSE(empty);
  params[kVPxFmtpMaxFrameRate] = "-1";
  EXPECT_FALSE(ParseSdpForVPxMaxFrameRate(params));
  params[kVPxFmtpMaxFrameRate] = "0";
  EXPECT_FALSE(ParseSdpForVPxMaxFrameRate(params));
  params[kVPxFmtpMaxFrameRate] = "abcde";
  EXPECT_FALSE(ParseSdpForVPxMaxFrameRate(params));
}

TEST(SdpFmtpUtilsTest, MaxFrameRateIsSpecified) {
  SdpVideoFormat::Parameters params;
  params[kVPxFmtpMaxFrameRate] = "30";
  EXPECT_EQ(ParseSdpForVPxMaxFrameRate(params), 30);
  params[kVPxFmtpMaxFrameRate] = "60";
  EXPECT_EQ(ParseSdpForVPxMaxFrameRate(params), 60);
}

TEST(SdpFmtpUtilsTest, MaxFrameSizeIsMissingOrInvalid) {
  SdpVideoFormat::Parameters params;
  absl::optional<int> empty = ParseSdpForVPxMaxFrameSize(params);
  EXPECT_FALSE(empty);
  params[kVPxFmtpMaxFrameSize] = "-1";
  EXPECT_FALSE(ParseSdpForVPxMaxFrameSize(params));
  params[kVPxFmtpMaxFrameSize] = "0";
  EXPECT_FALSE(ParseSdpForVPxMaxFrameSize(params));
  params[kVPxFmtpMaxFrameSize] = "abcde";
  EXPECT_FALSE(ParseSdpForVPxMaxFrameSize(params));
}

TEST(SdpFmtpUtilsTest, MaxFrameSizeIsSpecified) {
  SdpVideoFormat::Parameters params;
  params[kVPxFmtpMaxFrameSize] = "8100";  // 1920 x 1080 / (16^2)
  EXPECT_EQ(ParseSdpForVPxMaxFrameSize(params), 1920 * 1080);
  params[kVPxFmtpMaxFrameSize] = "32400";  // 3840 x 2160 / (16^2)
  EXPECT_EQ(ParseSdpForVPxMaxFrameSize(params), 3840 * 2160);
}

}  // namespace webrtc
