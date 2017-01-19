/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_video/h264/profile_level_id.h"

#include "webrtc/test/gtest.h"

namespace webrtc {
namespace H264 {

TEST(H264ProfileLevelId, TestParsingInvalid) {
  // Malformed strings.
  EXPECT_FALSE(ParseProfileLevelId(""));
  EXPECT_FALSE(ParseProfileLevelId(" 42e01f"));
  EXPECT_FALSE(ParseProfileLevelId("4242e01f"));
  EXPECT_FALSE(ParseProfileLevelId("e01f"));
  EXPECT_FALSE(ParseProfileLevelId("gggggg"));

  // Invalid level.
  EXPECT_FALSE(ParseProfileLevelId("42e000"));
  EXPECT_FALSE(ParseProfileLevelId("42e00f"));
  EXPECT_FALSE(ParseProfileLevelId("42e0ff"));

  // Invalid profile.
  EXPECT_FALSE(ParseProfileLevelId("42e11f"));
  EXPECT_FALSE(ParseProfileLevelId("58601f"));
  EXPECT_FALSE(ParseProfileLevelId("64e01f"));
}

TEST(H264ProfileLevelId, TestParsingLevel) {
  EXPECT_EQ(kLevel3_1, ParseProfileLevelId("42e01f")->level);
  EXPECT_EQ(kLevel1_1, ParseProfileLevelId("42e00b")->level);
  EXPECT_EQ(kLevel1_b, ParseProfileLevelId("42f00b")->level);
  EXPECT_EQ(kLevel4_2, ParseProfileLevelId("42C02A")->level);
  EXPECT_EQ(kLevel5_2, ParseProfileLevelId("640c34")->level);
}

TEST(H264ProfileLevelId, TestParsingConstrainedBaseline) {
  EXPECT_EQ(kProfileConstrainedBaseline,
            ParseProfileLevelId("42e01f")->profile);
  EXPECT_EQ(kProfileConstrainedBaseline,
            ParseProfileLevelId("42C02A")->profile);
  EXPECT_EQ(kProfileConstrainedBaseline,
            ParseProfileLevelId("4de01f")->profile);
  EXPECT_EQ(kProfileConstrainedBaseline,
            ParseProfileLevelId("58f01f")->profile);
}

TEST(H264ProfileLevelId, TestParsingBaseline) {
  EXPECT_EQ(kProfileBaseline, ParseProfileLevelId("42a01f")->profile);
  EXPECT_EQ(kProfileBaseline, ParseProfileLevelId("58A01F")->profile);
}

TEST(H264ProfileLevelId, TestParsingMain) {
  EXPECT_EQ(kProfileMain, ParseProfileLevelId("4D401f")->profile);
}

TEST(H264ProfileLevelId, TestParsingHigh) {
  EXPECT_EQ(kProfileHigh, ParseProfileLevelId("64001f")->profile);
}

TEST(H264ProfileLevelId, TestParsingConstrainedHigh) {
  EXPECT_EQ(kProfileConstrainedHigh, ParseProfileLevelId("640c1f")->profile);
}

TEST(H264ProfileLevelId, TestSupportedLevel) {
  EXPECT_EQ(kLevel2_1, *SupportedLevel(640 * 480, 25));
  EXPECT_EQ(kLevel3_1, *SupportedLevel(1280 * 720, 30));
  EXPECT_EQ(kLevel4_2, *SupportedLevel(1920 * 1280, 60));
}

// Test supported level below level 1 requirements.
TEST(H264ProfileLevelId, TestSupportedLevelInvalid) {
  EXPECT_FALSE(SupportedLevel(0, 0));
  // All levels support fps > 5.
  EXPECT_FALSE(SupportedLevel(1280 * 720, 5));
  // All levels support frame sizes > 183 * 137.
  EXPECT_FALSE(SupportedLevel(183 * 137, 30));
}

TEST(H264ProfileLevelId, TestToString) {
  EXPECT_EQ("42e01f", *ProfileLevelIdToString(ProfileLevelId(
                          kProfileConstrainedBaseline, kLevel3_1)));
  EXPECT_EQ("42000a",
            *ProfileLevelIdToString(ProfileLevelId(kProfileBaseline, kLevel1)));
  EXPECT_EQ("4d001f",
            ProfileLevelIdToString(ProfileLevelId(kProfileMain, kLevel3_1)));
  EXPECT_EQ("640c2a", *ProfileLevelIdToString(
                          ProfileLevelId(kProfileConstrainedHigh, kLevel4_2)));
  EXPECT_EQ("64002a",
            *ProfileLevelIdToString(ProfileLevelId(kProfileHigh, kLevel4_2)));
}

TEST(H264ProfileLevelId, TestToStringLevel1b) {
  EXPECT_EQ("42f00b", *ProfileLevelIdToString(ProfileLevelId(
                          kProfileConstrainedBaseline, kLevel1_b)));
  EXPECT_EQ("42100b", *ProfileLevelIdToString(
                          ProfileLevelId(kProfileBaseline, kLevel1_b)));
  EXPECT_EQ("4d100b",
            *ProfileLevelIdToString(ProfileLevelId(kProfileMain, kLevel1_b)));
}

TEST(H264ProfileLevelId, TestToStringRoundTrip) {
  EXPECT_EQ("42e01f", *ProfileLevelIdToString(*ParseProfileLevelId("42e01f")));
  EXPECT_EQ("42e01f", *ProfileLevelIdToString(*ParseProfileLevelId("42E01F")));
  EXPECT_EQ("4d100b", *ProfileLevelIdToString(*ParseProfileLevelId("4d100b")));
  EXPECT_EQ("4d100b", *ProfileLevelIdToString(*ParseProfileLevelId("4D100B")));
  EXPECT_EQ("640c2a", *ProfileLevelIdToString(*ParseProfileLevelId("640c2a")));
  EXPECT_EQ("640c2a", *ProfileLevelIdToString(*ParseProfileLevelId("640C2A")));
}

TEST(H264ProfileLevelId, TestToStringInvalid) {
  EXPECT_FALSE(ProfileLevelIdToString(ProfileLevelId(kProfileHigh, kLevel1_b)));
  EXPECT_FALSE(ProfileLevelIdToString(
      ProfileLevelId(kProfileConstrainedHigh, kLevel1_b)));
  EXPECT_FALSE(ProfileLevelIdToString(
      ProfileLevelId(static_cast<Profile>(255), kLevel3_1)));
}

TEST(H264ProfileLevelId, TestParseSdpProfileLevelIdEmpty) {
  const rtc::Optional<ProfileLevelId> profile_level_id =
      ParseSdpProfileLevelId(CodecParameterMap());
  EXPECT_TRUE(profile_level_id);
  EXPECT_EQ(kProfileConstrainedBaseline, profile_level_id->profile);
  EXPECT_EQ(kLevel3_1, profile_level_id->level);
}

TEST(H264ProfileLevelId, TestParseSdpProfileLevelIdConstrainedHigh) {
  CodecParameterMap params;
  params["profile-level-id"] = "640c2a";
  const rtc::Optional<ProfileLevelId> profile_level_id =
      ParseSdpProfileLevelId(params);
  EXPECT_TRUE(profile_level_id);
  EXPECT_EQ(kProfileConstrainedHigh, profile_level_id->profile);
  EXPECT_EQ(kLevel4_2, profile_level_id->level);
}

TEST(H264ProfileLevelId, TestParseSdpProfileLevelIdInvalid) {
  CodecParameterMap params;
  params["profile-level-id"] = "foobar";
  EXPECT_FALSE(ParseSdpProfileLevelId(params));
}

TEST(H264ProfileLevelId, TestGenerateProfileLevelIdForAnswerEmpty) {
  CodecParameterMap answer_params;
  GenerateProfileLevelIdForAnswer(CodecParameterMap(), CodecParameterMap(),
                                  &answer_params);
  EXPECT_TRUE(answer_params.empty());
}

TEST(H264ProfileLevelId,
     TestGenerateProfileLevelIdForAnswerLevelSymmetryCapped) {
  CodecParameterMap low_level;
  low_level["profile-level-id"] = "42e015";
  CodecParameterMap high_level;
  high_level["profile-level-id"] = "42e01f";

  // Level asymmetry is not allowed; test that answer level is the lower of the
  // local and remote levels.
  CodecParameterMap answer_params;
  GenerateProfileLevelIdForAnswer(low_level /* local_supported */,
                                  high_level /* remote_offered */,
                                  &answer_params);
  EXPECT_EQ("42e015", answer_params["profile-level-id"]);

  CodecParameterMap answer_params2;
  GenerateProfileLevelIdForAnswer(high_level /* local_supported */,
                                  low_level /* remote_offered */,
                                  &answer_params2);
  EXPECT_EQ("42e015", answer_params2["profile-level-id"]);
}

TEST(H264ProfileLevelId,
     TestGenerateProfileLevelIdForAnswerConstrainedBaselineLevelAsymmetry) {
  CodecParameterMap local_params;
  local_params["profile-level-id"] = "42e01f";
  local_params["level-asymmetry-allowed"] = "1";
  CodecParameterMap remote_params;
  remote_params["profile-level-id"] = "42e015";
  remote_params["level-asymmetry-allowed"] = "1";
  CodecParameterMap answer_params;
  GenerateProfileLevelIdForAnswer(local_params, remote_params, &answer_params);
  // When level asymmetry is allowed, we can answer a higher level than what was
  // offered.
  EXPECT_EQ("42e01f", answer_params["profile-level-id"]);
}

}  // namespace H264
}  // namespace webrtc
