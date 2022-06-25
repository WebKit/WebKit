/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/sdp_video_format_utils.h"

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

TEST(SdpVideoFormatUtilsTest, TestH264GenerateProfileLevelIdForAnswerEmpty) {
  SdpVideoFormat::Parameters answer_params;
  H264GenerateProfileLevelIdForAnswer(SdpVideoFormat::Parameters(),
                                      SdpVideoFormat::Parameters(),
                                      &answer_params);
  EXPECT_TRUE(answer_params.empty());
}

TEST(SdpVideoFormatUtilsTest,
     TestH264GenerateProfileLevelIdForAnswerLevelSymmetryCapped) {
  SdpVideoFormat::Parameters low_level;
  low_level["profile-level-id"] = "42e015";
  SdpVideoFormat::Parameters high_level;
  high_level["profile-level-id"] = "42e01f";

  // Level asymmetry is not allowed; test that answer level is the lower of the
  // local and remote levels.
  SdpVideoFormat::Parameters answer_params;
  H264GenerateProfileLevelIdForAnswer(low_level /* local_supported */,
                                      high_level /* remote_offered */,
                                      &answer_params);
  EXPECT_EQ("42e015", answer_params["profile-level-id"]);

  SdpVideoFormat::Parameters answer_params2;
  H264GenerateProfileLevelIdForAnswer(high_level /* local_supported */,
                                      low_level /* remote_offered */,
                                      &answer_params2);
  EXPECT_EQ("42e015", answer_params2["profile-level-id"]);
}

TEST(SdpVideoFormatUtilsTest,
     TestH264GenerateProfileLevelIdForAnswerConstrainedBaselineLevelAsymmetry) {
  SdpVideoFormat::Parameters local_params;
  local_params["profile-level-id"] = "42e01f";
  local_params["level-asymmetry-allowed"] = "1";
  SdpVideoFormat::Parameters remote_params;
  remote_params["profile-level-id"] = "42e015";
  remote_params["level-asymmetry-allowed"] = "1";
  SdpVideoFormat::Parameters answer_params;
  H264GenerateProfileLevelIdForAnswer(local_params, remote_params,
                                      &answer_params);
  // When level asymmetry is allowed, we can answer a higher level than what was
  // offered.
  EXPECT_EQ("42e01f", answer_params["profile-level-id"]);
}

TEST(SdpVideoFormatUtilsTest, MaxFrameRateIsMissingOrInvalid) {
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

TEST(SdpVideoFormatUtilsTest, MaxFrameRateIsSpecified) {
  SdpVideoFormat::Parameters params;
  params[kVPxFmtpMaxFrameRate] = "30";
  EXPECT_EQ(ParseSdpForVPxMaxFrameRate(params), 30);
  params[kVPxFmtpMaxFrameRate] = "60";
  EXPECT_EQ(ParseSdpForVPxMaxFrameRate(params), 60);
}

TEST(SdpVideoFormatUtilsTest, MaxFrameSizeIsMissingOrInvalid) {
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

TEST(SdpVideoFormatUtilsTest, MaxFrameSizeIsSpecified) {
  SdpVideoFormat::Parameters params;
  params[kVPxFmtpMaxFrameSize] = "8100";  // 1920 x 1080 / (16^2)
  EXPECT_EQ(ParseSdpForVPxMaxFrameSize(params), 1920 * 1080);
  params[kVPxFmtpMaxFrameSize] = "32400";  // 3840 x 2160 / (16^2)
  EXPECT_EQ(ParseSdpForVPxMaxFrameSize(params), 3840 * 2160);
}

}  // namespace webrtc
