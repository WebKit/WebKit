/*
 *  Copyright (c) 2009 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/gunit.h"
#include "webrtc/media/base/codec.h"

using cricket::AudioCodec;
using cricket::Codec;
using cricket::DataCodec;
using cricket::FeedbackParam;
using cricket::VideoCodec;
using cricket::kCodecParamAssociatedPayloadType;
using cricket::kCodecParamMaxBitrate;
using cricket::kCodecParamMinBitrate;

class TestCodec : public Codec {
 public:
  TestCodec(int id, const std::string name, int clockrate)
      : Codec(id, name, clockrate) {}
  TestCodec() : Codec() {}
  TestCodec(const TestCodec& c) : Codec(c) {}
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
  TestCodec c6(0, "", 0);
  EXPECT_TRUE(c5 == c6);
}

TEST(CodecTest, TestAudioCodecOperators) {
  AudioCodec c0(96, "A", 44100, 20000, 2);
  AudioCodec c1(95, "A", 44100, 20000, 2);
  AudioCodec c2(96, "x", 44100, 20000, 2);
  AudioCodec c3(96, "A", 48000, 20000, 2);
  AudioCodec c4(96, "A", 44100, 10000, 2);
  AudioCodec c5(96, "A", 44100, 20000, 1);
  EXPECT_NE(c0, c1);
  EXPECT_NE(c0, c2);
  EXPECT_NE(c0, c3);
  EXPECT_NE(c0, c4);
  EXPECT_NE(c0, c5);

  AudioCodec c7;
  AudioCodec c8(0, "", 0, 0, 0);
  AudioCodec c9 = c0;
  EXPECT_EQ(c8, c7);
  EXPECT_NE(c9, c7);
  EXPECT_EQ(c9, c0);

  AudioCodec c10(c0);
  AudioCodec c11(c0);
  AudioCodec c12(c0);
  AudioCodec c13(c0);
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

TEST(CodecTest, TestAudioCodecMatches) {
  // Test a codec with a static payload type.
  AudioCodec c0(95, "A", 44100, 20000, 1);
  EXPECT_TRUE(c0.Matches(AudioCodec(95, "", 44100, 20000, 1)));
  EXPECT_TRUE(c0.Matches(AudioCodec(95, "", 44100, 20000, 0)));
  EXPECT_TRUE(c0.Matches(AudioCodec(95, "", 44100, 0, 0)));
  EXPECT_TRUE(c0.Matches(AudioCodec(95, "", 0, 0, 0)));
  EXPECT_FALSE(c0.Matches(AudioCodec(96, "", 44100, 20000, 1)));
  EXPECT_FALSE(c0.Matches(AudioCodec(95, "", 55100, 20000, 1)));
  EXPECT_FALSE(c0.Matches(AudioCodec(95, "", 44100, 30000, 1)));
  EXPECT_FALSE(c0.Matches(AudioCodec(95, "", 44100, 20000, 2)));
  EXPECT_FALSE(c0.Matches(AudioCodec(95, "", 55100, 30000, 2)));

  // Test a codec with a dynamic payload type.
  AudioCodec c1(96, "A", 44100, 20000, 1);
  EXPECT_TRUE(c1.Matches(AudioCodec(96, "A", 0, 0, 0)));
  EXPECT_TRUE(c1.Matches(AudioCodec(97, "A", 0, 0, 0)));
  EXPECT_TRUE(c1.Matches(AudioCodec(96, "a", 0, 0, 0)));
  EXPECT_TRUE(c1.Matches(AudioCodec(97, "a", 0, 0, 0)));
  EXPECT_FALSE(c1.Matches(AudioCodec(95, "A", 0, 0, 0)));
  EXPECT_FALSE(c1.Matches(AudioCodec(96, "", 44100, 20000, 2)));
  EXPECT_FALSE(c1.Matches(AudioCodec(96, "A", 55100, 30000, 1)));

  // Test a codec with a dynamic payload type, and auto bitrate.
  AudioCodec c2(97, "A", 16000, 0, 1);
  // Use default bitrate.
  EXPECT_TRUE(c2.Matches(AudioCodec(97, "A", 16000, 0, 1)));
  EXPECT_TRUE(c2.Matches(AudioCodec(97, "A", 16000, 0, 0)));
  // Use explicit bitrate.
  EXPECT_TRUE(c2.Matches(AudioCodec(97, "A", 16000, 32000, 1)));
  // Backward compatibility with clients that might send "-1" (for default).
  EXPECT_TRUE(c2.Matches(AudioCodec(97, "A", 16000, -1, 1)));

  // Stereo doesn't match channels = 0.
  AudioCodec c3(96, "A", 44100, 20000, 2);
  EXPECT_TRUE(c3.Matches(AudioCodec(96, "A", 44100, 20000, 2)));
  EXPECT_FALSE(c3.Matches(AudioCodec(96, "A", 44100, 20000, 1)));
  EXPECT_FALSE(c3.Matches(AudioCodec(96, "A", 44100, 20000, 0)));
}

TEST(CodecTest, TestVideoCodecOperators) {
  VideoCodec c0(96, "V");
  VideoCodec c1(95, "V");
  VideoCodec c2(96, "x");

  EXPECT_TRUE(c0 != c1);
  EXPECT_TRUE(c0 != c2);

  VideoCodec c7;
  VideoCodec c8(0, "");
  VideoCodec c9 = c0;
  EXPECT_TRUE(c8 == c7);
  EXPECT_TRUE(c9 != c7);
  EXPECT_TRUE(c9 == c0);

  VideoCodec c10(c0);
  VideoCodec c11(c0);
  VideoCodec c12(c0);
  VideoCodec c13(c0);
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

TEST(CodecTest, TestVideoCodecMatches) {
  // Test a codec with a static payload type.
  VideoCodec c0(95, "V");
  EXPECT_TRUE(c0.Matches(VideoCodec(95, "")));
  EXPECT_FALSE(c0.Matches(VideoCodec(96, "")));

  // Test a codec with a dynamic payload type.
  VideoCodec c1(96, "V");
  EXPECT_TRUE(c1.Matches(VideoCodec(96, "V")));
  EXPECT_TRUE(c1.Matches(VideoCodec(97, "V")));
  EXPECT_TRUE(c1.Matches(VideoCodec(96, "v")));
  EXPECT_TRUE(c1.Matches(VideoCodec(97, "v")));
  EXPECT_FALSE(c1.Matches(VideoCodec(96, "")));
  EXPECT_FALSE(c1.Matches(VideoCodec(95, "V")));
}

TEST(CodecTest, TestDataCodecMatches) {
  // Test a codec with a static payload type.
  DataCodec c0(95, "D");
  EXPECT_TRUE(c0.Matches(DataCodec(95, "")));
  EXPECT_FALSE(c0.Matches(DataCodec(96, "")));

  // Test a codec with a dynamic payload type.
  DataCodec c1(96, "D");
  EXPECT_TRUE(c1.Matches(DataCodec(96, "D")));
  EXPECT_TRUE(c1.Matches(DataCodec(97, "D")));
  EXPECT_TRUE(c1.Matches(DataCodec(96, "d")));
  EXPECT_TRUE(c1.Matches(DataCodec(97, "d")));
  EXPECT_FALSE(c1.Matches(DataCodec(96, "")));
  EXPECT_FALSE(c1.Matches(DataCodec(95, "D")));
}

TEST(CodecTest, TestSetParamGetParamAndRemoveParam) {
  AudioCodec codec;
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
  c1.AddFeedbackParam(a1); // Only match with c2.
  c1.AddFeedbackParam(b2); // Same param different values.
  c1.AddFeedbackParam(c3); // Not in c2.
  TestCodec c2;
  c2.AddFeedbackParam(a1);
  c2.AddFeedbackParam(b3);

  c1.IntersectFeedbackParams(c2);
  EXPECT_TRUE(c1.HasFeedbackParam(a1));
  EXPECT_FALSE(c1.HasFeedbackParam(b2));
  EXPECT_FALSE(c1.HasFeedbackParam(c3));
}

TEST(CodecTest, TestGetCodecType) {
  // Codec type comparison should be case insenstive on names.
  const VideoCodec codec(96, "V");
  const VideoCodec rtx_codec(96, "rTx");
  const VideoCodec ulpfec_codec(96, "ulpFeC");
  const VideoCodec flexfec_codec(96, "FlExFeC-03");
  const VideoCodec red_codec(96, "ReD");
  EXPECT_EQ(VideoCodec::CODEC_VIDEO, codec.GetCodecType());
  EXPECT_EQ(VideoCodec::CODEC_RTX, rtx_codec.GetCodecType());
  EXPECT_EQ(VideoCodec::CODEC_ULPFEC, ulpfec_codec.GetCodecType());
  EXPECT_EQ(VideoCodec::CODEC_FLEXFEC, flexfec_codec.GetCodecType());
  EXPECT_EQ(VideoCodec::CODEC_RED, red_codec.GetCodecType());
}

TEST(CodecTest, TestCreateRtxCodec) {
  VideoCodec rtx_codec = VideoCodec::CreateRtxCodec(96, 120);
  EXPECT_EQ(96, rtx_codec.id);
  EXPECT_EQ(VideoCodec::CODEC_RTX, rtx_codec.GetCodecType());
  int associated_payload_type;
  ASSERT_TRUE(rtx_codec.GetParam(kCodecParamAssociatedPayloadType,
                                 &associated_payload_type));
  EXPECT_EQ(120, associated_payload_type);
}

TEST(CodecTest, TestValidateCodecFormat) {
  const VideoCodec codec(96, "V");
  ASSERT_TRUE(codec.ValidateCodecFormat());

  // Accept 0-127 as payload types.
  VideoCodec low_payload_type = codec;
  low_payload_type.id = 0;
  VideoCodec high_payload_type = codec;
  high_payload_type.id = 127;
  ASSERT_TRUE(low_payload_type.ValidateCodecFormat());
  EXPECT_TRUE(high_payload_type.ValidateCodecFormat());

  // Reject negative payloads.
  VideoCodec negative_payload_type = codec;
  negative_payload_type.id = -1;
  EXPECT_FALSE(negative_payload_type.ValidateCodecFormat());

  // Reject too-high payloads.
  VideoCodec too_high_payload_type = codec;
  too_high_payload_type.id = 128;
  EXPECT_FALSE(too_high_payload_type.ValidateCodecFormat());

  // Reject codecs with min bitrate > max bitrate.
  VideoCodec incorrect_bitrates = codec;
  incorrect_bitrates.params[kCodecParamMinBitrate] = "100";
  incorrect_bitrates.params[kCodecParamMaxBitrate] = "80";
  EXPECT_FALSE(incorrect_bitrates.ValidateCodecFormat());

  // Accept min bitrate == max bitrate.
  VideoCodec equal_bitrates = codec;
  equal_bitrates.params[kCodecParamMinBitrate] = "100";
  equal_bitrates.params[kCodecParamMaxBitrate] = "100";
  EXPECT_TRUE(equal_bitrates.ValidateCodecFormat());

  // Accept min bitrate < max bitrate.
  VideoCodec different_bitrates = codec;
  different_bitrates.params[kCodecParamMinBitrate] = "99";
  different_bitrates.params[kCodecParamMaxBitrate] = "100";
  EXPECT_TRUE(different_bitrates.ValidateCodecFormat());
}

TEST(CodecTest, TestToCodecParameters) {
  const VideoCodec v(96, "V");
  webrtc::RtpCodecParameters codec_params_1 = v.ToCodecParameters();
  EXPECT_EQ(96, codec_params_1.payload_type);
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, codec_params_1.kind);
  EXPECT_EQ("V", codec_params_1.name);
  EXPECT_EQ(rtc::Optional<int>(cricket::kVideoCodecClockrate),
            codec_params_1.clock_rate);
  EXPECT_EQ(rtc::Optional<int>(), codec_params_1.num_channels);

  const AudioCodec a(97, "A", 44100, 20000, 2);
  webrtc::RtpCodecParameters codec_params_2 = a.ToCodecParameters();
  EXPECT_EQ(97, codec_params_2.payload_type);
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, codec_params_2.kind);
  EXPECT_EQ("A", codec_params_2.name);
  EXPECT_EQ(rtc::Optional<int>(44100), codec_params_2.clock_rate);
  EXPECT_EQ(rtc::Optional<int>(2), codec_params_2.num_channels);
}
