/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/payload_type_picker.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "api/video_codecs/sdp_video_format.h"
#include "call/payload_type.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using testing::Eq;
using testing::Ge;
using testing::Le;
using testing::Ne;

TEST(PayloadTypePicker, PayloadTypeAssignmentWorks) {
  // Note: This behavior is due to be deprecated and removed.
  PayloadType pt_a(1);
  PayloadType pt_b = 1;  // Implicit conversion
  EXPECT_EQ(pt_a, pt_b);
  int pt_as_int = pt_a;  // Implicit conversion
  EXPECT_EQ(1, pt_as_int);
}

TEST(PayloadTypePicker, InstantiateTypes) {
  PayloadTypePicker picker;
  PayloadTypeRecorder recorder(picker);
}

TEST(PayloadTypePicker, StoreAndRecall) {
  PayloadTypePicker picker;
  PayloadTypeRecorder recorder(picker);
  const PayloadType a_payload_type(123);
  const PayloadType not_a_payload_type(44);
  Codec a_codec = CreateVideoCodec(0, "vp8");
  auto error = recorder.AddMapping(a_payload_type, a_codec);
  ASSERT_TRUE(error.ok());
  auto result = recorder.LookupCodec(a_payload_type);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), a_codec);
  auto result_pt = recorder.LookupPayloadType(a_codec);
  ASSERT_TRUE(result_pt.ok());
  EXPECT_EQ(result_pt.value(), a_payload_type);
  EXPECT_FALSE(recorder.LookupCodec(not_a_payload_type).ok());
}

TEST(PayloadTypePicker, ModifyingPtIsIgnored) {
  // Arguably a spec violation, but happens in production.
  // To be decided: Whether we should disallow codec change, fmtp change
  // or both.
  PayloadTypePicker picker;
  PayloadTypeRecorder recorder(picker);
  const PayloadType a_payload_type(123);
  Codec a_codec = CreateVideoCodec(Codec::kIdNotSet, "vp8");
  Codec b_codec = CreateVideoCodec(Codec::kIdNotSet, "vp9");
  recorder.AddMapping(a_payload_type, a_codec);
  auto error = recorder.AddMapping(a_payload_type, b_codec);
  EXPECT_TRUE(error.ok());
  auto result = recorder.LookupCodec(a_payload_type);
  // Redefinition should be accepted.
  EXPECT_EQ(result.value(), b_codec);
}

TEST(PayloadTypePicker, ModifyingPtIsAnErrorIfDisallowed) {
  PayloadTypePicker picker;
  PayloadTypeRecorder recorder(picker);
  const PayloadType a_payload_type(123);
  Codec a_codec = CreateVideoCodec(Codec::kIdNotSet, "vp8");
  Codec b_codec = CreateVideoCodec(Codec::kIdNotSet, "vp9");
  recorder.DisallowRedefinition();
  recorder.AddMapping(a_payload_type, a_codec);
  auto error = recorder.AddMapping(a_payload_type, b_codec);
  EXPECT_FALSE(error.ok());
  auto result = recorder.LookupCodec(a_payload_type);
  // Attempted redefinition should be ignored.
  EXPECT_EQ(result.value(), a_codec);
  recorder.ReallowRedefinition();
}

TEST(PayloadTypePicker, RollbackAndCommit) {
  PayloadTypePicker picker;
  PayloadTypeRecorder recorder(picker);
  const PayloadType a_payload_type(123);
  const PayloadType b_payload_type(124);
  const PayloadType not_a_payload_type(44);

  Codec a_codec = CreateVideoCodec(0, "vp8");

  Codec b_codec = CreateVideoCodec(0, "vp9");
  auto error = recorder.AddMapping(a_payload_type, a_codec);
  ASSERT_TRUE(error.ok());
  recorder.Commit();
  ASSERT_TRUE(recorder.AddMapping(b_payload_type, b_codec).ok());
  {
    auto result = recorder.LookupCodec(a_payload_type);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value(), a_codec);
  }
  {
    auto result = recorder.LookupCodec(b_payload_type);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value(), b_codec);
  }
  recorder.Rollback();
  {
    auto result = recorder.LookupCodec(a_payload_type);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value(), a_codec);
  }
  {
    auto result = recorder.LookupCodec(b_payload_type);
    ASSERT_FALSE(result.ok());
  }
  ASSERT_TRUE(recorder.AddMapping(b_payload_type, b_codec).ok());
  // Rollback after a new checkpoint has no effect.
  recorder.Commit();
  recorder.Rollback();
  {
    auto result = recorder.LookupCodec(b_payload_type);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value(), b_codec);
  }
}

TEST(PayloadTypePicker, StaticValueIsGood) {
  PayloadTypePicker picker;
  Codec a_codec = CreateAudioCodec(-1, kPcmuCodecName, 8000, 1);
  auto result = picker.SuggestMapping(a_codec, nullptr);
  // In the absence of existing mappings, PCMU always has 0 as PT.
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value(), PayloadType(0));
}

TEST(PayloadTypePicker, DynamicValueIsGood) {
  PayloadTypePicker picker;
  Codec a_codec = CreateAudioCodec(-1, "lyra", 8000, 1);
  auto result = picker.SuggestMapping(a_codec, nullptr);
  // This should result in a value from the dynamic range; since this is the
  // first assignment, it should be in the upper range.
  ASSERT_TRUE(result.ok());
  EXPECT_GE(result.value(), PayloadType(96));
  EXPECT_LE(result.value(), PayloadType(127));
}

TEST(PayloadTypePicker, RecordedValueReturned) {
  PayloadTypePicker picker;
  PayloadTypeRecorder recorder(picker);
  Codec a_codec = CreateAudioCodec(-1, "lyra", 8000, 1);
  recorder.AddMapping(47, a_codec);
  auto result = picker.SuggestMapping(a_codec, &recorder);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(47, result.value());
}

TEST(PayloadTypePicker, RecordedValueExcluded) {
  PayloadTypePicker picker;
  PayloadTypeRecorder recorder1(picker);
  PayloadTypeRecorder recorder2(picker);
  Codec a_codec = CreateAudioCodec(-1, "lyra", 8000, 1);
  Codec b_codec = CreateAudioCodec(-1, "mlcodec", 8000, 1);
  recorder1.AddMapping(47, a_codec);
  recorder2.AddMapping(47, b_codec);
  auto result = picker.SuggestMapping(b_codec, &recorder1);
  ASSERT_TRUE(result.ok());
  EXPECT_NE(47, result.value());
}

TEST(PayloadTypePicker, AudioGetsHigherRange) {
  PayloadTypePicker picker;
  Codec an_audio_codec = CreateAudioCodec(-1, "lyra", 8000, 1);
  auto result = picker.SuggestMapping(an_audio_codec, nullptr).value();
  EXPECT_THAT(result, Ge(96));
}

TEST(PayloadTypePicker, AudioRedGetsLowerRange) {
  PayloadTypePicker picker;
  Codec an_audio_codec = CreateAudioCodec(-1, "red", 48000, 2);
  auto result = picker.SuggestMapping(an_audio_codec, nullptr).value();
  EXPECT_THAT(result, Le(63));
}

TEST(PayloadTypePicker, VideoGetsTreatedSpecially) {
  PayloadTypePicker picker;
  Codec h264_constrained = CreateVideoCodec(
      SdpVideoFormat(kH264CodecName, {{kH264FmtpProfileLevelId, "42e01f"},
                                      {kH264FmtpLevelAsymmetryAllowed, "1"},
                                      {kH264FmtpPacketizationMode, "1"}}));
  Codec h264_yuv444 = CreateVideoCodec(
      SdpVideoFormat(kH264CodecName, {{kH264FmtpProfileLevelId, "f4001f"},
                                      {kH264FmtpLevelAsymmetryAllowed, "1"},
                                      {kH264FmtpPacketizationMode, "1"}}));
  Codec vp9_profile_2 =
      CreateVideoCodec({kVp9CodecName, {{kVP9ProfileId, "2"}}});
  Codec vp9_profile_3 =
      CreateVideoCodec({kVp9CodecName, {{kVP9ProfileId, "3"}}});
  Codec h265 = CreateVideoCodec(
      SdpVideoFormat(kH265CodecName, {{kH265FmtpProfileId, "1"},
                                      {kH265FmtpTierFlag, "0"},
                                      {kH265FmtpLevelId, "93"},
                                      {kH265FmtpTxMode, "SRST"}}));
  // Valid for high range only.
  EXPECT_THAT(picker.SuggestMapping(h264_constrained, nullptr).value(), Ge(96));
  EXPECT_THAT(picker.SuggestMapping(vp9_profile_2, nullptr).value(), Ge(96));
  // Valid for lower range.
  EXPECT_THAT(picker.SuggestMapping(h264_yuv444, nullptr).value(), Le(63));
  EXPECT_THAT(picker.SuggestMapping(vp9_profile_3, nullptr).value(), Le(63));
  EXPECT_THAT(picker.SuggestMapping(h265, nullptr).value(), Le(63));

  // RTX with a primary codec in the lower range is valid for lower range.
  Codec lower_range_rtx = CreateVideoRtxCodec(Codec::kIdNotSet, 63);
  EXPECT_THAT(picker.SuggestMapping(lower_range_rtx, nullptr).value(), Le(63));
}

TEST(PayloadTypePicker, ChoosingH264Profiles) {
  // No opinion on whether these are right or wrong, just that their
  // behavior is consistent.
  PayloadTypePicker picker;
  Codec h264_constrained = CreateVideoCodec(
      SdpVideoFormat(kH264CodecName, {{kH264FmtpProfileLevelId, "42e01f"},
                                      {kH264FmtpLevelAsymmetryAllowed, "1"},
                                      {kH264FmtpPacketizationMode, "1"}}));
  Codec h264_high_1f = CreateVideoCodec(
      SdpVideoFormat(kH264CodecName, {{kH264FmtpProfileLevelId, "640c1f"},
                                      {kH264FmtpLevelAsymmetryAllowed, "1"},
                                      {kH264FmtpPacketizationMode, "1"}}));
  Codec h264_high_2a = CreateVideoCodec(
      SdpVideoFormat(kH264CodecName, {{kH264FmtpProfileLevelId, "640c2a"},
                                      {kH264FmtpLevelAsymmetryAllowed, "1"},
                                      {kH264FmtpPacketizationMode, "1"}}));
  PayloadType pt_constrained =
      picker.SuggestMapping(h264_constrained, nullptr).value();
  PayloadType pt_high_1f = picker.SuggestMapping(h264_high_1f, nullptr).value();
  PayloadType pt_high_2a = picker.SuggestMapping(h264_high_2a, nullptr).value();
  EXPECT_THAT(pt_constrained, Ne(pt_high_1f));
  EXPECT_THAT(pt_high_1f, Eq(pt_high_2a));
}

TEST(PayloadTypePicker, AbslStringify) {
  PayloadTypePicker picker;
  Codec a_codec = CreateAudioCodec(-1, "lyra", 8000, 1);
  Codec b_codec = CreateVideoCodec(-1, "vp8");

  picker.AddMapping(47, a_codec);
  picker.AddMapping(100, b_codec);

  std::string s = absl::StrCat(picker);

  EXPECT_THAT(s, testing::HasSubstr("Reserved:"));
  EXPECT_THAT(s, testing::HasSubstr(" 47"));
  EXPECT_THAT(s, testing::HasSubstr(" 100"));
  EXPECT_THAT(s, testing::HasSubstr("Entries:"));
  EXPECT_THAT(s, testing::HasSubstr("\n 47:[-1:audio/lyra/8000/1]"));
  EXPECT_THAT(s, testing::HasSubstr("\n 100:[-1:video/vp8/90000/0]"));
}

}  // namespace webrtc
