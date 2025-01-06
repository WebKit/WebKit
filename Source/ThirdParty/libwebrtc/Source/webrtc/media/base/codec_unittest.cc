/*
 *  Copyright (c) 2009 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/codec.h"

#include <optional>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "api/media_types.h"
#include "api/rtp_parameters.h"
#include "api/video_codecs/h264_profile_level_id.h"
#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/vp9_profile.h"
#include "media/base/media_constants.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "test/gtest.h"

using cricket::Codec;
using cricket::FeedbackParam;
using cricket::kCodecParamAssociatedPayloadType;
using cricket::kCodecParamMaxBitrate;
using cricket::kCodecParamMinBitrate;

class TestCodec : public Codec {
 public:
  TestCodec(int id, const std::string& name, int clockrate)
      : Codec(Type::kAudio, id, name, clockrate) {}
  TestCodec() : Codec(Type::kAudio) {}
  TestCodec(const TestCodec& c) = default;
  TestCodec& operator=(const TestCodec& c) = default;
};

TEST(CodecTest, TestCodecOperators) {
  TestCodec c0(96, "D", 1000);
  c0.SetParam("a", 1);

  TestCodec c1 = c0;
  EXPECT_TRUE(c1 == c0);

  int param_value0;
  int param_value1;
  EXPECT_TRUE(c0.GetParam("a", &param_value0));
  EXPECT_TRUE(c1.GetParam("a", &param_value1));
  EXPECT_EQ(param_value0, param_value1);

  c1.id = 86;
  EXPECT_TRUE(c0 != c1);

  c1 = c0;
  c1.name = "x";
  EXPECT_TRUE(c0 != c1);

  c1 = c0;
  c1.clockrate = 2000;
  EXPECT_TRUE(c0 != c1);

  c1 = c0;
  c1.SetParam("a", 2);
  EXPECT_TRUE(c0 != c1);

  TestCodec c5;
  TestCodec c6(Codec::kIdNotSet, "", 0);
  EXPECT_TRUE(c5 == c6);
}

TEST(CodecTest, TestAudioCodecOperators) {
  Codec c0 = cricket::CreateAudioCodec(96, "A", 44100, 2);
  Codec c1 = cricket::CreateAudioCodec(95, "A", 44100, 2);
  Codec c2 = cricket::CreateAudioCodec(96, "x", 44100, 2);
  Codec c3 = cricket::CreateAudioCodec(96, "A", 48000, 2);
  Codec c4 = cricket::CreateAudioCodec(96, "A", 44100, 2);
  c4.bitrate = 10000;
  Codec c5 = cricket::CreateAudioCodec(96, "A", 44100, 1);
  EXPECT_NE(c0, c1);
  EXPECT_NE(c0, c2);
  EXPECT_NE(c0, c3);
  EXPECT_NE(c0, c4);
  EXPECT_NE(c0, c5);

  Codec c8 = cricket::CreateAudioCodec(0, "", 0, 0);
  Codec c9 = c0;
  EXPECT_EQ(c9, c0);

  Codec c10(c0);
  Codec c11(c0);
  Codec c12(c0);
  Codec c13(c0);
  c10.params["x"] = "abc";
  c11.params["x"] = "def";
  c12.params["y"] = "abc";
  c13.params["x"] = "abc";
  EXPECT_NE(c10, c0);
  EXPECT_NE(c11, c0);
  EXPECT_NE(c11, c10);
  EXPECT_NE(c12, c0);
  EXPECT_NE(c12, c10);
  EXPECT_NE(c12, c11);
  EXPECT_EQ(c13, c10);
}

TEST(CodecTest, TestVideoCodecOperators) {
  Codec c0 = cricket::CreateVideoCodec(96, "V");
  Codec c1 = cricket::CreateVideoCodec(95, "V");
  Codec c2 = cricket::CreateVideoCodec(96, "x");

  EXPECT_TRUE(c0 != c1);
  EXPECT_TRUE(c0 != c2);

  Codec c8 = cricket::CreateVideoCodec(0, "");
  Codec c9 = c0;
  EXPECT_TRUE(c9 == c0);

  Codec c10(c0);
  Codec c11(c0);
  Codec c12(c0);
  Codec c13(c0);
  c10.params["x"] = "abc";
  c11.params["x"] = "def";
  c12.params["y"] = "abc";
  c13.params["x"] = "abc";
  EXPECT_TRUE(c10 != c0);
  EXPECT_TRUE(c11 != c0);
  EXPECT_TRUE(c11 != c10);
  EXPECT_TRUE(c12 != c0);
  EXPECT_TRUE(c12 != c10);
  EXPECT_TRUE(c12 != c11);
  EXPECT_TRUE(c13 == c10);
}

TEST(CodecTest, TestVideoCodecEqualsWithDifferentPacketization) {
  Codec c0 = cricket::CreateVideoCodec(100, cricket::kVp8CodecName);
  Codec c1 = cricket::CreateVideoCodec(100, cricket::kVp8CodecName);
  Codec c2 = cricket::CreateVideoCodec(100, cricket::kVp8CodecName);
  c2.packetization = "raw";

  EXPECT_EQ(c0, c1);
  EXPECT_NE(c0, c2);
  EXPECT_NE(c2, c0);
  EXPECT_EQ(c2, c2);
}


TEST(CodecTest, TestSetParamGetParamAndRemoveParam) {
  Codec codec = cricket::CreateAudioCodec(0, "foo", 22222, 2);
  codec.SetParam("a", "1");
  codec.SetParam("b", "x");

  int int_value = 0;
  EXPECT_TRUE(codec.GetParam("a", &int_value));
  EXPECT_EQ(1, int_value);
  EXPECT_FALSE(codec.GetParam("b", &int_value));
  EXPECT_FALSE(codec.GetParam("c", &int_value));

  std::string str_value;
  EXPECT_TRUE(codec.GetParam("a", &str_value));
  EXPECT_EQ("1", str_value);
  EXPECT_TRUE(codec.GetParam("b", &str_value));
  EXPECT_EQ("x", str_value);
  EXPECT_FALSE(codec.GetParam("c", &str_value));
  EXPECT_TRUE(codec.RemoveParam("a"));
  EXPECT_FALSE(codec.RemoveParam("c"));
}

TEST(CodecTest, TestIntersectFeedbackParams) {
  const FeedbackParam a1("a", "1");
  const FeedbackParam b2("b", "2");
  const FeedbackParam b3("b", "3");
  const FeedbackParam c3("c", "3");
  TestCodec c1;
  c1.AddFeedbackParam(a1);  // Only match with c2.
  c1.AddFeedbackParam(b2);  // Same param different values.
  c1.AddFeedbackParam(c3);  // Not in c2.
  TestCodec c2;
  c2.AddFeedbackParam(a1);
  c2.AddFeedbackParam(b3);

  c1.IntersectFeedbackParams(c2);
  EXPECT_TRUE(c1.HasFeedbackParam(a1));
  EXPECT_FALSE(c1.HasFeedbackParam(b2));
  EXPECT_FALSE(c1.HasFeedbackParam(c3));
}

TEST(CodecTest, TestGetCodecType) {
  // Codec type comparison should be case insensitive on names.
  const Codec codec = cricket::CreateVideoCodec(96, "V");
  const Codec rtx_codec = cricket::CreateVideoCodec(96, "rTx");
  const Codec ulpfec_codec = cricket::CreateVideoCodec(96, "ulpFeC");
  const Codec flexfec_codec = cricket::CreateVideoCodec(96, "FlExFeC-03");
  const Codec red_codec = cricket::CreateVideoCodec(96, "ReD");
  EXPECT_TRUE(codec.IsMediaCodec());
  EXPECT_EQ(codec.GetResiliencyType(), Codec::ResiliencyType::kNone);
  EXPECT_EQ(rtx_codec.GetResiliencyType(), Codec::ResiliencyType::kRtx);
  EXPECT_EQ(ulpfec_codec.GetResiliencyType(), Codec::ResiliencyType::kUlpfec);
  EXPECT_EQ(flexfec_codec.GetResiliencyType(), Codec::ResiliencyType::kFlexfec);
  EXPECT_EQ(red_codec.GetResiliencyType(), Codec::ResiliencyType::kRed);
}

TEST(CodecTest, TestCreateRtxCodec) {
  const Codec rtx_codec = cricket::CreateVideoRtxCodec(96, 120);
  EXPECT_EQ(96, rtx_codec.id);
  EXPECT_EQ(rtx_codec.GetResiliencyType(), Codec::ResiliencyType::kRtx);
  int associated_payload_type;
  ASSERT_TRUE(rtx_codec.GetParam(kCodecParamAssociatedPayloadType,
                                 &associated_payload_type));
  EXPECT_EQ(120, associated_payload_type);
}

TEST(CodecTest, TestValidateCodecFormat) {
  const Codec codec = cricket::CreateVideoCodec(96, "V");
  ASSERT_TRUE(codec.ValidateCodecFormat());

  // Accept 0-127 as payload types.
  Codec low_payload_type = codec;
  low_payload_type.id = 0;
  Codec high_payload_type = codec;
  high_payload_type.id = 127;
  ASSERT_TRUE(low_payload_type.ValidateCodecFormat());
  EXPECT_TRUE(high_payload_type.ValidateCodecFormat());

  // Reject negative payloads.
  Codec negative_payload_type = codec;
  negative_payload_type.id = -1;
  EXPECT_FALSE(negative_payload_type.ValidateCodecFormat());

  // Reject too-high payloads.
  Codec too_high_payload_type = codec;
  too_high_payload_type.id = 128;
  EXPECT_FALSE(too_high_payload_type.ValidateCodecFormat());

  // Reject codecs with min bitrate > max bitrate.
  Codec incorrect_bitrates = codec;
  incorrect_bitrates.params[kCodecParamMinBitrate] = "100";
  incorrect_bitrates.params[kCodecParamMaxBitrate] = "80";
  EXPECT_FALSE(incorrect_bitrates.ValidateCodecFormat());

  // Accept min bitrate == max bitrate.
  Codec equal_bitrates = codec;
  equal_bitrates.params[kCodecParamMinBitrate] = "100";
  equal_bitrates.params[kCodecParamMaxBitrate] = "100";
  EXPECT_TRUE(equal_bitrates.ValidateCodecFormat());

  // Accept min bitrate < max bitrate.
  Codec different_bitrates = codec;
  different_bitrates.params[kCodecParamMinBitrate] = "99";
  different_bitrates.params[kCodecParamMaxBitrate] = "100";
  EXPECT_TRUE(different_bitrates.ValidateCodecFormat());
}

TEST(CodecTest, TestToCodecParameters) {
  Codec v = cricket::CreateVideoCodec(96, "V");
  v.SetParam("p1", "v1");
  webrtc::RtpCodecParameters codec_params_1 = v.ToCodecParameters();
  EXPECT_EQ(96, codec_params_1.payload_type);
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, codec_params_1.kind);
  EXPECT_EQ("V", codec_params_1.name);
  EXPECT_EQ(cricket::kVideoCodecClockrate, codec_params_1.clock_rate);
  EXPECT_EQ(std::nullopt, codec_params_1.num_channels);
  ASSERT_EQ(1u, codec_params_1.parameters.size());
  EXPECT_EQ("p1", codec_params_1.parameters.begin()->first);
  EXPECT_EQ("v1", codec_params_1.parameters.begin()->second);

  Codec a = cricket::CreateAudioCodec(97, "A", 44100, 2);
  a.SetParam("p1", "a1");
  webrtc::RtpCodecParameters codec_params_2 = a.ToCodecParameters();
  EXPECT_EQ(97, codec_params_2.payload_type);
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, codec_params_2.kind);
  EXPECT_EQ("A", codec_params_2.name);
  EXPECT_EQ(44100, codec_params_2.clock_rate);
  EXPECT_EQ(2, codec_params_2.num_channels);
  ASSERT_EQ(1u, codec_params_2.parameters.size());
  EXPECT_EQ("p1", codec_params_2.parameters.begin()->first);
  EXPECT_EQ("a1", codec_params_2.parameters.begin()->second);
}

TEST(CodecTest, H264CostrainedBaselineIsAddedIfH264IsSupported) {
  const std::vector<webrtc::SdpVideoFormat> kExplicitlySupportedFormats = {
      webrtc::CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                               webrtc::H264Level::kLevel3_1, "1"),
      webrtc::CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                               webrtc::H264Level::kLevel3_1, "0")};

  std::vector<webrtc::SdpVideoFormat> supported_formats =
      kExplicitlySupportedFormats;
  cricket::AddH264ConstrainedBaselineProfileToSupportedFormats(
      &supported_formats);

  const webrtc::SdpVideoFormat kH264ConstrainedBasedlinePacketization1 =
      webrtc::CreateH264Format(webrtc::H264Profile::kProfileConstrainedBaseline,
                               webrtc::H264Level::kLevel3_1, "1");
  const webrtc::SdpVideoFormat kH264ConstrainedBasedlinePacketization0 =
      webrtc::CreateH264Format(webrtc::H264Profile::kProfileConstrainedBaseline,
                               webrtc::H264Level::kLevel3_1, "0");

  EXPECT_EQ(supported_formats[0], kExplicitlySupportedFormats[0]);
  EXPECT_EQ(supported_formats[1], kExplicitlySupportedFormats[1]);
  EXPECT_EQ(supported_formats[2], kH264ConstrainedBasedlinePacketization1);
  EXPECT_EQ(supported_formats[3], kH264ConstrainedBasedlinePacketization0);
}

TEST(CodecTest, H264CostrainedBaselineIsNotAddedIfH264IsUnsupported) {
  const std::vector<webrtc::SdpVideoFormat> kExplicitlySupportedFormats = {
      {cricket::kVp9CodecName,
       {{webrtc::kVP9FmtpProfileId,
         VP9ProfileToString(webrtc::VP9Profile::kProfile0)}}}};

  std::vector<webrtc::SdpVideoFormat> supported_formats =
      kExplicitlySupportedFormats;
  cricket::AddH264ConstrainedBaselineProfileToSupportedFormats(
      &supported_formats);

  EXPECT_EQ(supported_formats[0], kExplicitlySupportedFormats[0]);
  EXPECT_EQ(supported_formats.size(), kExplicitlySupportedFormats.size());
}

TEST(CodecTest, H264CostrainedBaselineNotAddedIfAlreadySpecified) {
  const std::vector<webrtc::SdpVideoFormat> kExplicitlySupportedFormats = {
      webrtc::CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                               webrtc::H264Level::kLevel3_1, "1"),
      webrtc::CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                               webrtc::H264Level::kLevel3_1, "0"),
      webrtc::CreateH264Format(webrtc::H264Profile::kProfileConstrainedBaseline,
                               webrtc::H264Level::kLevel3_1, "1"),
      webrtc::CreateH264Format(webrtc::H264Profile::kProfileConstrainedBaseline,
                               webrtc::H264Level::kLevel3_1, "0")};

  std::vector<webrtc::SdpVideoFormat> supported_formats =
      kExplicitlySupportedFormats;
  cricket::AddH264ConstrainedBaselineProfileToSupportedFormats(
      &supported_formats);

  EXPECT_EQ(supported_formats[0], kExplicitlySupportedFormats[0]);
  EXPECT_EQ(supported_formats[1], kExplicitlySupportedFormats[1]);
  EXPECT_EQ(supported_formats[2], kExplicitlySupportedFormats[2]);
  EXPECT_EQ(supported_formats[3], kExplicitlySupportedFormats[3]);
  EXPECT_EQ(supported_formats.size(), kExplicitlySupportedFormats.size());
}

TEST(CodecTest, AbslStringify) {
  Codec codec = cricket::CreateAudioCodec(47, "custom-audio", 48000, 2);
  EXPECT_EQ(absl::StrCat(codec), "[47:audio/custom-audio/48000/2]");
  codec.params["key"] = "value";
  EXPECT_EQ(absl::StrCat(codec), "[47:audio/custom-audio/48000/2;key=value]");
}
