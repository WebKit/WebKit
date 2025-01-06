/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "media/base/codec_comparators.h"

#include <string>

#include "api/audio_codecs/audio_format.h"
#include "api/rtp_parameters.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/vp9_profile.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using cricket::Codec;
using cricket::CreateAudioCodec;
using cricket::CreateVideoCodec;
using cricket::kH264CodecName;
using cricket::kH264FmtpPacketizationMode;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

TEST(CodecComparatorsTest, CodecMatchesItself) {
  Codec codec = cricket::CreateVideoCodec("custom");
  EXPECT_TRUE(MatchesWithCodecRules(codec, codec));
}

TEST(CodecComparatorsTest, MismatchedBasicParameters) {
  Codec codec = CreateAudioCodec(SdpAudioFormat("opus", 48000, 2));
  Codec nonmatch_codec = codec;
  nonmatch_codec.name = "g711";
  EXPECT_FALSE(MatchesWithCodecRules(nonmatch_codec, codec));
  nonmatch_codec = codec;
  nonmatch_codec.clockrate = 8000;
  EXPECT_FALSE(MatchesWithCodecRules(nonmatch_codec, codec));
  nonmatch_codec = codec;
  nonmatch_codec.channels = 1;
  EXPECT_FALSE(MatchesWithCodecRules(nonmatch_codec, codec));
}

TEST(CodecComparatorsTest, H264PacketizationModeMismatch) {
  Codec pt_mode_1 = CreateVideoCodec(kH264CodecName);
  Codec pt_mode_0 = pt_mode_1;
  pt_mode_0.SetParam(kH264FmtpPacketizationMode, "0");
  EXPECT_FALSE(MatchesWithCodecRules(pt_mode_1, pt_mode_0));
  EXPECT_FALSE(MatchesWithCodecRules(pt_mode_0, pt_mode_1));
  Codec no_pt_mode = pt_mode_1;
  no_pt_mode.RemoveParam(kH264FmtpPacketizationMode);
  EXPECT_TRUE(MatchesWithCodecRules(pt_mode_0, no_pt_mode));
  EXPECT_TRUE(MatchesWithCodecRules(no_pt_mode, pt_mode_0));
  EXPECT_FALSE(MatchesWithCodecRules(no_pt_mode, pt_mode_1));
}

TEST(CodecComparatorsTest, AudioParametersIgnored) {
  // Currently, all parameters on audio codecs are ignored for matching.
  Codec basic_opus = CreateAudioCodec(SdpAudioFormat("opus", 48000, 2));
  Codec opus_with_parameters = basic_opus;
  opus_with_parameters.SetParam("stereo", "0");
  EXPECT_TRUE(MatchesWithCodecRules(basic_opus, opus_with_parameters));
  EXPECT_TRUE(MatchesWithCodecRules(opus_with_parameters, basic_opus));
  opus_with_parameters.SetParam("nonsense", "stuff");
  EXPECT_TRUE(MatchesWithCodecRules(basic_opus, opus_with_parameters));
  EXPECT_TRUE(MatchesWithCodecRules(opus_with_parameters, basic_opus));
}

TEST(CodecComparatorsTest, StaticPayloadTypesIgnoreName) {
  // This is the IANA registered format for PT 8
  Codec codec_1 = CreateAudioCodec(8, "pcma", 8000, 1);
  Codec codec_2 = CreateAudioCodec(8, "nonsense", 8000, 1);
  EXPECT_TRUE(MatchesWithCodecRules(codec_1, codec_2));
}

struct TestParams {
  std::string name;
  SdpVideoFormat codec1;
  SdpVideoFormat codec2;
  bool expected_result;
};

using IsSameRtpCodecTest = TestWithParam<TestParams>;

TEST_P(IsSameRtpCodecTest, IsSameRtpCodec) {
  TestParams param = GetParam();
  Codec codec1 = cricket::CreateVideoCodec(param.codec1);
  Codec codec2 = cricket::CreateVideoCodec(param.codec2);

  EXPECT_EQ(IsSameRtpCodec(codec1, codec2.ToCodecParameters()),
            param.expected_result);
}

INSTANTIATE_TEST_SUITE_P(
    CodecTest,
    IsSameRtpCodecTest,
    ValuesIn<TestParams>({
        {.name = "CodecWithDifferentName",
         .codec1 = {"VP9", {}},
         .codec2 = {"VP8", {}},
         .expected_result = false},
        {.name = "Vp8WithoutParameters",
         .codec1 = {"vp8", {}},
         .codec2 = {"VP8", {}},
         .expected_result = true},
        {.name = "Vp8WithSameParameters",
         .codec1 = {"VP8", {{"x", "1"}}},
         .codec2 = {"VP8", {{"x", "1"}}},
         .expected_result = true},
        {.name = "Vp8WithDifferentParameters",
         .codec1 = {"VP8", {}},
         .codec2 = {"VP8", {{"x", "1"}}},
         .expected_result = false},
        {.name = "Av1WithoutParameters",
         .codec1 = {"AV1", {}},
         .codec2 = {"AV1", {}},
         .expected_result = true},
        {.name = "Av1WithSameProfile",
         .codec1 = {"AV1", SdpVideoFormat::AV1Profile0().parameters},
         .codec2 = {"AV1", SdpVideoFormat::AV1Profile0().parameters},
         .expected_result = true},
        {.name = "Av1WithoutParametersTreatedAsProfile0",
         .codec1 = {"AV1", SdpVideoFormat::AV1Profile0().parameters},
         .codec2 = {"AV1", {}},
         .expected_result = true},
        {.name = "Av1WithoutProfileTreatedAsProfile0",
         .codec1 = {"AV1", {{cricket::kAv1FmtpProfile, "0"}, {"x", "1"}}},
         .codec2 = {"AV1", {{"x", "1"}}},
         .expected_result = true},
        {.name = "Av1WithDifferentProfile",
         .codec1 = {"AV1", SdpVideoFormat::AV1Profile0().parameters},
         .codec2 = {"AV1", SdpVideoFormat::AV1Profile1().parameters},
         .expected_result = false},
        {.name = "Av1WithDifferentParameters",
         .codec1 = {"AV1", {{cricket::kAv1FmtpProfile, "0"}, {"x", "1"}}},
         .codec2 = {"AV1", {{cricket::kAv1FmtpProfile, "0"}, {"x", "2"}}},
         .expected_result = false},
        {.name = "Vp9WithSameProfile",
         .codec1 = {"VP9", SdpVideoFormat::VP9Profile0().parameters},
         .codec2 = {"VP9", SdpVideoFormat::VP9Profile0().parameters},
         .expected_result = true},
        {.name = "Vp9WithoutProfileTreatedAsProfile0",
         .codec1 = {"VP9", {{kVP9FmtpProfileId, "0"}, {"x", "1"}}},
         .codec2 = {"VP9", {{"x", "1"}}},
         .expected_result = true},
        {.name = "Vp9WithDifferentProfile",
         .codec1 = {"VP9", SdpVideoFormat::VP9Profile0().parameters},
         .codec2 = {"VP9", SdpVideoFormat::VP9Profile1().parameters},
         .expected_result = false},
        {.name = "H264WithSamePacketizationMode",
         .codec1 = {"H264", {{kH264FmtpPacketizationMode, "0"}}},
         .codec2 = {"H264", {{kH264FmtpPacketizationMode, "0"}}},
         .expected_result = true},
        {.name = "H264WithoutPacketizationModeTreatedAsMode0",
         .codec1 = {"H264", {{kH264FmtpPacketizationMode, "0"}, {"x", "1"}}},
         .codec2 = {"H264", {{"x", "1"}}},
         .expected_result = true},
        {.name = "H264WithDifferentPacketizationMode",
         .codec1 = {"H264", {{kH264FmtpPacketizationMode, "0"}}},
         .codec2 = {"H264", {{kH264FmtpPacketizationMode, "1"}}},
         .expected_result = false},
#ifdef RTC_ENABLE_H265
        {.name = "H265WithSameProfile",
         .codec1 = {"H265",
                    {{cricket::kH265FmtpProfileId, "1"},
                     {cricket::kH265FmtpTierFlag, "0"},
                     {cricket::kH265FmtpLevelId, "93"},
                     {cricket::kH265FmtpTxMode, "SRST"}}},
         .codec2 = {"H265",
                    {{cricket::kH265FmtpProfileId, "1"},
                     {cricket::kH265FmtpTierFlag, "0"},
                     {cricket::kH265FmtpLevelId, "93"},
                     {cricket::kH265FmtpTxMode, "SRST"}}},
         .expected_result = true},
        {.name = "H265WithoutParametersTreatedAsDefault",
         .codec1 = {"H265",
                    {{cricket::kH265FmtpProfileId, "1"},
                     {cricket::kH265FmtpTierFlag, "0"},
                     {cricket::kH265FmtpLevelId, "93"},
                     {cricket::kH265FmtpTxMode, "SRST"}}},
         .codec2 = {"H265", {}},
         .expected_result = true},
        {.name = "H265WithDifferentProfile",
         .codec1 = {"H265",
                    {{cricket::kH265FmtpProfileId, "1"},
                     {cricket::kH265FmtpTierFlag, "0"},
                     {cricket::kH265FmtpLevelId, "93"},
                     {cricket::kH265FmtpTxMode, "SRST"}}},
         .codec2 = {"H265",
                    {{cricket::kH265FmtpProfileId, "1"},
                     {cricket::kH265FmtpTierFlag, "1"},
                     {cricket::kH265FmtpLevelId, "93"},
                     {cricket::kH265FmtpTxMode, "SRST"}}},
         .expected_result = false},
#endif
    }),
    [](const testing::TestParamInfo<IsSameRtpCodecTest::ParamType>& info) {
      return info.param.name;
    });

TEST(CodecTest, TestCodecMatches) {
  // Test a codec with a static payload type.
  Codec c0 = cricket::CreateAudioCodec(34, "A", 44100, 1);
  EXPECT_TRUE(c0.Matches(cricket::CreateAudioCodec(34, "", 44100, 1)));
  EXPECT_TRUE(c0.Matches(cricket::CreateAudioCodec(34, "", 44100, 0)));
  EXPECT_TRUE(c0.Matches(cricket::CreateAudioCodec(34, "", 44100, 0)));
  EXPECT_TRUE(c0.Matches(cricket::CreateAudioCodec(34, "", 0, 0)));
  EXPECT_FALSE(c0.Matches(cricket::CreateAudioCodec(96, "A", 44100, 1)));
  EXPECT_FALSE(c0.Matches(cricket::CreateAudioCodec(96, "", 44100, 1)));
  EXPECT_FALSE(c0.Matches(cricket::CreateAudioCodec(95, "", 55100, 1)));
  EXPECT_FALSE(c0.Matches(cricket::CreateAudioCodec(95, "", 44100, 1)));
  EXPECT_FALSE(c0.Matches(cricket::CreateAudioCodec(95, "", 44100, 2)));
  EXPECT_FALSE(c0.Matches(cricket::CreateAudioCodec(95, "", 55100, 2)));

  // Test a codec with a dynamic payload type.
  Codec c1 = cricket::CreateAudioCodec(96, "A", 44100, 1);
  EXPECT_TRUE(c1.Matches(cricket::CreateAudioCodec(96, "A", 0, 0)));
  EXPECT_TRUE(c1.Matches(cricket::CreateAudioCodec(97, "A", 0, 0)));
  EXPECT_TRUE(c1.Matches(cricket::CreateAudioCodec(96, "a", 0, 0)));
  EXPECT_TRUE(c1.Matches(cricket::CreateAudioCodec(97, "a", 0, 0)));
  EXPECT_TRUE(c1.Matches(cricket::CreateAudioCodec(35, "a", 0, 0)));
  EXPECT_TRUE(c1.Matches(cricket::CreateAudioCodec(42, "a", 0, 0)));
  EXPECT_TRUE(c1.Matches(cricket::CreateAudioCodec(65, "a", 0, 0)));
  EXPECT_FALSE(c1.Matches(cricket::CreateAudioCodec(95, "A", 0, 0)));
  EXPECT_FALSE(c1.Matches(cricket::CreateAudioCodec(34, "A", 0, 0)));
  EXPECT_FALSE(c1.Matches(cricket::CreateAudioCodec(96, "", 44100, 2)));
  EXPECT_FALSE(c1.Matches(cricket::CreateAudioCodec(96, "A", 55100, 1)));

  // Test a codec with a dynamic payload type, and auto bitrate.
  Codec c2 = cricket::CreateAudioCodec(97, "A", 16000, 1);
  // Use default bitrate.
  EXPECT_TRUE(c2.Matches(cricket::CreateAudioCodec(97, "A", 16000, 1)));
  EXPECT_TRUE(c2.Matches(cricket::CreateAudioCodec(97, "A", 16000, 0)));
  // Use explicit bitrate.
  EXPECT_TRUE(c2.Matches(cricket::CreateAudioCodec(97, "A", 16000, 1)));
  // Backward compatibility with clients that might send "-1" (for default).
  EXPECT_TRUE(c2.Matches(cricket::CreateAudioCodec(97, "A", 16000, 1)));

  // Stereo doesn't match channels = 0.
  Codec c3 = cricket::CreateAudioCodec(96, "A", 44100, 2);
  EXPECT_TRUE(c3.Matches(cricket::CreateAudioCodec(96, "A", 44100, 2)));
  EXPECT_FALSE(c3.Matches(cricket::CreateAudioCodec(96, "A", 44100, 1)));
  EXPECT_FALSE(c3.Matches(cricket::CreateAudioCodec(96, "A", 44100, 0)));
}

TEST(CodecTest, TestOpusAudioCodecWithDifferentParameters) {
  Codec opus_with_fec = cricket::CreateAudioCodec(96, "opus", 48000, 2);
  opus_with_fec.params["useinbandfec"] = "1";
  Codec opus_without_fec = cricket::CreateAudioCodec(96, "opus", 48000, 2);

  EXPECT_TRUE(opus_with_fec != opus_without_fec);
  // Matches does not compare parameters for audio.
  EXPECT_TRUE(opus_with_fec.Matches(opus_without_fec));

  webrtc::RtpCodecParameters rtp_opus_with_fec =
      opus_with_fec.ToCodecParameters();
  // MatchesRtpCodec takes parameters into account.
  EXPECT_TRUE(opus_with_fec.MatchesRtpCodec(rtp_opus_with_fec));
  EXPECT_FALSE(opus_without_fec.MatchesRtpCodec(rtp_opus_with_fec));
}

TEST(CodecTest, TestVideoCodecMatches) {
  // Test a codec with a static payload type.
  Codec c0 = cricket::CreateVideoCodec(34, "V");
  EXPECT_TRUE(c0.Matches(cricket::CreateVideoCodec(34, "")));
  EXPECT_FALSE(c0.Matches(cricket::CreateVideoCodec(96, "")));
  EXPECT_FALSE(c0.Matches(cricket::CreateVideoCodec(96, "V")));

  // Test a codec with a dynamic payload type.
  Codec c1 = cricket::CreateVideoCodec(96, "V");
  EXPECT_TRUE(c1.Matches(cricket::CreateVideoCodec(96, "V")));
  EXPECT_TRUE(c1.Matches(cricket::CreateVideoCodec(97, "V")));
  EXPECT_TRUE(c1.Matches(cricket::CreateVideoCodec(96, "v")));
  EXPECT_TRUE(c1.Matches(cricket::CreateVideoCodec(97, "v")));
  EXPECT_TRUE(c1.Matches(cricket::CreateVideoCodec(35, "v")));
  EXPECT_TRUE(c1.Matches(cricket::CreateVideoCodec(42, "v")));
  EXPECT_TRUE(c1.Matches(cricket::CreateVideoCodec(65, "v")));
  EXPECT_FALSE(c1.Matches(cricket::CreateVideoCodec(96, "")));
  EXPECT_FALSE(c1.Matches(cricket::CreateVideoCodec(95, "V")));
  EXPECT_FALSE(c1.Matches(cricket::CreateVideoCodec(34, "V")));
}

TEST(CodecTest, TestVideoCodecMatchesWithDifferentPacketization) {
  Codec c0 = cricket::CreateVideoCodec(100, cricket::kVp8CodecName);
  Codec c1 = cricket::CreateVideoCodec(101, cricket::kVp8CodecName);
  c1.packetization = "raw";

  EXPECT_TRUE(c0.Matches(c1));
  EXPECT_TRUE(c1.Matches(c0));
}

// AV1 codecs compare profile information.
TEST(CodecTest, TestAV1CodecMatches) {
  const char kProfile0[] = "0";
  const char kProfile1[] = "1";
  const char kProfile2[] = "2";

  Codec c_no_profile = cricket::CreateVideoCodec(95, cricket::kAv1CodecName);
  Codec c_profile0 = cricket::CreateVideoCodec(95, cricket::kAv1CodecName);
  c_profile0.params[cricket::kAv1FmtpProfile] = kProfile0;
  Codec c_profile1 = cricket::CreateVideoCodec(95, cricket::kAv1CodecName);
  c_profile1.params[cricket::kAv1FmtpProfile] = kProfile1;
  Codec c_profile2 = cricket::CreateVideoCodec(95, cricket::kAv1CodecName);
  c_profile2.params[cricket::kAv1FmtpProfile] = kProfile2;

  // An AV1 entry with no profile specified should be treated as profile-0.
  EXPECT_TRUE(c_profile0.Matches(c_no_profile));

  {
    // Two AV1 entries without a profile specified are treated as duplicates.
    Codec c_no_profile_eq =
        cricket::CreateVideoCodec(95, cricket::kAv1CodecName);
    EXPECT_TRUE(c_no_profile.Matches(c_no_profile_eq));
  }

  {
    // Two AV1 entries with profile 0 specified are treated as duplicates.
    Codec c_profile0_eq = cricket::CreateVideoCodec(95, cricket::kAv1CodecName);
    c_profile0_eq.params[cricket::kAv1FmtpProfile] = kProfile0;
    EXPECT_TRUE(c_profile0.Matches(c_profile0_eq));
  }

  {
    // Two AV1 entries with profile 1 specified are treated as duplicates.
    Codec c_profile1_eq = cricket::CreateVideoCodec(95, cricket::kAv1CodecName);
    c_profile1_eq.params[cricket::kAv1FmtpProfile] = kProfile1;
    EXPECT_TRUE(c_profile1.Matches(c_profile1_eq));
  }

  // AV1 entries with different profiles (0 and 1) are seen as distinct.
  EXPECT_FALSE(c_profile0.Matches(c_profile1));
  EXPECT_FALSE(c_no_profile.Matches(c_profile1));

  // AV1 entries with different profiles (0 and 2) are seen as distinct.
  EXPECT_FALSE(c_profile0.Matches(c_profile2));
  EXPECT_FALSE(c_no_profile.Matches(c_profile2));
}

// VP9 codecs compare profile information.
TEST(CodecTest, TestVP9CodecMatches) {
  const char kProfile0[] = "0";
  const char kProfile2[] = "2";

  Codec c_no_profile = cricket::CreateVideoCodec(95, cricket::kVp9CodecName);
  Codec c_profile0 = cricket::CreateVideoCodec(95, cricket::kVp9CodecName);
  c_profile0.params[webrtc::kVP9FmtpProfileId] = kProfile0;

  EXPECT_TRUE(c_profile0.Matches(c_no_profile));

  {
    Codec c_profile0_eq = cricket::CreateVideoCodec(95, cricket::kVp9CodecName);
    c_profile0_eq.params[webrtc::kVP9FmtpProfileId] = kProfile0;
    EXPECT_TRUE(c_profile0.Matches(c_profile0_eq));
  }

  {
    Codec c_profile2 = cricket::CreateVideoCodec(95, cricket::kVp9CodecName);
    c_profile2.params[webrtc::kVP9FmtpProfileId] = kProfile2;
    EXPECT_FALSE(c_profile0.Matches(c_profile2));
    EXPECT_FALSE(c_no_profile.Matches(c_profile2));
  }

  {
    Codec c_no_profile_eq =
        cricket::CreateVideoCodec(95, cricket::kVp9CodecName);
    EXPECT_TRUE(c_no_profile.Matches(c_no_profile_eq));
  }
}

// Matching H264 codecs also need to have matching profile-level-id and
// packetization-mode.
TEST(CodecTest, TestH264CodecMatches) {
  const char kProfileLevelId1[] = "42e01f";
  const char kProfileLevelId2[] = "42a01e";
  const char kProfileLevelId3[] = "42e01e";

  Codec pli_1_pm_0 = cricket::CreateVideoCodec(95, "H264");
  pli_1_pm_0.params[cricket::kH264FmtpProfileLevelId] = kProfileLevelId1;
  pli_1_pm_0.params[cricket::kH264FmtpPacketizationMode] = "0";

  {
    Codec pli_1_pm_blank = cricket::CreateVideoCodec(95, "H264");
    pli_1_pm_blank.params[cricket::kH264FmtpProfileLevelId] = kProfileLevelId1;
    pli_1_pm_blank.params.erase(
        pli_1_pm_blank.params.find(cricket::kH264FmtpPacketizationMode));

    // Matches since if packetization-mode is not specified it defaults to "0".
    EXPECT_TRUE(pli_1_pm_0.Matches(pli_1_pm_blank));

    // MatchesRtpCodec does exact comparison of parameters.
    EXPECT_FALSE(
        pli_1_pm_0.MatchesRtpCodec(pli_1_pm_blank.ToCodecParameters()));
  }

  {
    Codec pli_1_pm_1 = cricket::CreateVideoCodec(95, "H264");
    pli_1_pm_1.params[cricket::kH264FmtpProfileLevelId] = kProfileLevelId1;
    pli_1_pm_1.params[cricket::kH264FmtpPacketizationMode] = "1";

    // Does not match since packetization-mode is different.
    EXPECT_FALSE(pli_1_pm_0.Matches(pli_1_pm_1));

    EXPECT_FALSE(pli_1_pm_0.MatchesRtpCodec(pli_1_pm_1.ToCodecParameters()));
  }

  {
    Codec pli_2_pm_0 = cricket::CreateVideoCodec(95, "H264");
    pli_2_pm_0.params[cricket::kH264FmtpProfileLevelId] = kProfileLevelId2;
    pli_2_pm_0.params[cricket::kH264FmtpPacketizationMode] = "0";

    // Does not match since profile-level-id is different.
    EXPECT_FALSE(pli_1_pm_0.Matches(pli_2_pm_0));

    EXPECT_FALSE(pli_1_pm_0.MatchesRtpCodec(pli_2_pm_0.ToCodecParameters()));
  }

  {
    Codec pli_3_pm_0_asym = cricket::CreateVideoCodec(95, "H264");
    pli_3_pm_0_asym.params[cricket::kH264FmtpProfileLevelId] = kProfileLevelId3;
    pli_3_pm_0_asym.params[cricket::kH264FmtpPacketizationMode] = "0";

    // Does match, profile-level-id is different but the level is not compared.
    // and the profile matches.
    EXPECT_TRUE(pli_1_pm_0.Matches(pli_3_pm_0_asym));

    EXPECT_FALSE(
        pli_1_pm_0.MatchesRtpCodec(pli_3_pm_0_asym.ToCodecParameters()));

    //
  }
}

#ifdef RTC_ENABLE_H265
// Matching H.265 codecs should have matching profile/tier/level and tx-mode.
TEST(CodecTest, TestH265CodecMatches) {
  constexpr char kProfile1[] = "1";
  constexpr char kTier1[] = "1";
  constexpr char kLevel3_1[] = "93";
  constexpr char kLevel4[] = "120";
  constexpr char kTxMrst[] = "MRST";

  Codec c_ptl_blank = cricket::CreateVideoCodec(95, cricket::kH265CodecName);

  {
    Codec c_profile_1 = cricket::CreateVideoCodec(95, cricket::kH265CodecName);
    c_profile_1.params[cricket::kH265FmtpProfileId] = kProfile1;

    // Matches since profile-id unspecified defaults to "1".
    EXPECT_TRUE(c_ptl_blank.Matches(c_profile_1));
  }

  {
    Codec c_tier_flag_1 =
        cricket::CreateVideoCodec(95, cricket::kH265CodecName);
    c_tier_flag_1.params[cricket::kH265FmtpTierFlag] = kTier1;

    // Does not match since profile-space unspecified defaults to "0".
    EXPECT_FALSE(c_ptl_blank.Matches(c_tier_flag_1));
  }

  {
    Codec c_level_id_3_1 =
        cricket::CreateVideoCodec(95, cricket::kH265CodecName);
    c_level_id_3_1.params[cricket::kH265FmtpLevelId] = kLevel3_1;

    // Matches since level-id unspecified defaults to "93".
    EXPECT_TRUE(c_ptl_blank.Matches(c_level_id_3_1));
  }

  {
    Codec c_level_id_4 = cricket::CreateVideoCodec(95, cricket::kH265CodecName);
    c_level_id_4.params[cricket::kH265FmtpLevelId] = kLevel4;

    // Matches since we ignore level-id when matching H.265 codecs.
    EXPECT_TRUE(c_ptl_blank.Matches(c_level_id_4));
  }

  {
    Codec c_tx_mode_mrst =
        cricket::CreateVideoCodec(95, cricket::kH265CodecName);
    c_tx_mode_mrst.params[cricket::kH265FmtpTxMode] = kTxMrst;

    // Does not match since tx-mode implies to "SRST" and must be not specified
    // when it is the only mode supported:
    // https://datatracker.ietf.org/doc/html/draft-ietf-avtcore-hevc-webrtc
    EXPECT_FALSE(c_ptl_blank.Matches(c_tx_mode_mrst));
  }
}
#endif

TEST(CodecTest, TestMatchesRtpCodecRtx) {
  const Codec rtx_codec_1 = cricket::CreateVideoRtxCodec(96, 120);
  const Codec rtx_codec_2 = cricket::CreateVideoRtxCodec(96, 121);
  EXPECT_TRUE(rtx_codec_1.Matches(rtx_codec_2));
  // MatchesRtpCodec ignores the different associated payload type (apt) for
  // RTX.
  EXPECT_TRUE(rtx_codec_1.MatchesRtpCodec(rtx_codec_2.ToCodecParameters()));
}

}  // namespace webrtc
