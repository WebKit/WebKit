/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/rtp_parameters_conversion.h"

#include <cstdint>
#include <map>
#include <string>

#include "api/media_types.h"
#include "media/base/codec.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::UnorderedElementsAre;

namespace webrtc {

TEST(RtpParametersConversionTest, ToRtcpFeedback) {
  std::optional<RtcpFeedback> result = ToRtcpFeedback({"ccm", "fir"});
  EXPECT_EQ(RtcpFeedback(RtcpFeedbackType::CCM, RtcpFeedbackMessageType::FIR),
            *result);

  result = ToRtcpFeedback(cricket::FeedbackParam("goog-lntf"));
  EXPECT_EQ(RtcpFeedback(RtcpFeedbackType::LNTF), *result);

  result = ToRtcpFeedback(cricket::FeedbackParam("nack"));
  EXPECT_EQ(RtcpFeedback(RtcpFeedbackType::NACK,
                         RtcpFeedbackMessageType::GENERIC_NACK),
            *result);

  result = ToRtcpFeedback({"nack", "pli"});
  EXPECT_EQ(RtcpFeedback(RtcpFeedbackType::NACK, RtcpFeedbackMessageType::PLI),
            *result);

  result = ToRtcpFeedback(cricket::FeedbackParam("goog-remb"));
  EXPECT_EQ(RtcpFeedback(RtcpFeedbackType::REMB), *result);

  result = ToRtcpFeedback(cricket::FeedbackParam("transport-cc"));
  EXPECT_EQ(RtcpFeedback(RtcpFeedbackType::TRANSPORT_CC), *result);
}

TEST(RtpParametersConversionTest, ToRtcpFeedbackErrors) {
  // CCM with missing or invalid message type.
  std::optional<RtcpFeedback> result = ToRtcpFeedback({"ccm", "pli"});
  EXPECT_FALSE(result);

  result = ToRtcpFeedback(cricket::FeedbackParam("ccm"));
  EXPECT_FALSE(result);

  // LNTF with message type (should be left empty).
  result = ToRtcpFeedback({"goog-lntf", "pli"});
  EXPECT_FALSE(result);

  // NACK with missing or invalid message type.
  result = ToRtcpFeedback({"nack", "fir"});
  EXPECT_FALSE(result);

  // REMB with message type (should be left empty).
  result = ToRtcpFeedback({"goog-remb", "pli"});
  EXPECT_FALSE(result);

  // TRANSPORT_CC with message type (should be left empty).
  result = ToRtcpFeedback({"transport-cc", "fir"});
  EXPECT_FALSE(result);

  // Unknown message type.
  result = ToRtcpFeedback(cricket::FeedbackParam("foo"));
  EXPECT_FALSE(result);
}

TEST(RtpParametersConversionTest, ToAudioRtpCodecCapability) {
  cricket::Codec cricket_codec = cricket::CreateAudioCodec(50, "foo", 22222, 4);
  cricket_codec.params["foo"] = "bar";
  cricket_codec.feedback_params.Add(cricket::FeedbackParam("transport-cc"));
  RtpCodecCapability codec = ToRtpCodecCapability(cricket_codec);

  EXPECT_EQ("foo", codec.name);
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, codec.kind);
  EXPECT_EQ(50, codec.preferred_payload_type);
  EXPECT_EQ(22222, codec.clock_rate);
  EXPECT_EQ(4, codec.num_channels);
  ASSERT_EQ(1u, codec.parameters.size());
  EXPECT_EQ("bar", codec.parameters["foo"]);
  EXPECT_EQ(1u, codec.rtcp_feedback.size());
  EXPECT_EQ(RtcpFeedback(RtcpFeedbackType::TRANSPORT_CC),
            codec.rtcp_feedback[0]);
}

TEST(RtpParametersConversionTest, ToVideoRtpCodecCapability) {
  cricket::Codec cricket_codec = cricket::CreateVideoCodec(101, "VID");
  cricket_codec.clockrate = 80000;
  cricket_codec.params["foo"] = "bar";
  cricket_codec.params["ANOTHER"] = "param";
  cricket_codec.feedback_params.Add(cricket::FeedbackParam("transport-cc"));
  cricket_codec.feedback_params.Add(cricket::FeedbackParam("goog-lntf"));
  cricket_codec.feedback_params.Add({"nack", "pli"});
  RtpCodecCapability codec = ToRtpCodecCapability(cricket_codec);

  EXPECT_EQ("VID", codec.name);
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, codec.kind);
  EXPECT_EQ(101, codec.preferred_payload_type);
  EXPECT_EQ(80000, codec.clock_rate);
  ASSERT_EQ(2u, codec.parameters.size());
  EXPECT_EQ("bar", codec.parameters["foo"]);
  EXPECT_EQ("param", codec.parameters["ANOTHER"]);
  EXPECT_EQ(3u, codec.rtcp_feedback.size());
  EXPECT_EQ(RtcpFeedback(RtcpFeedbackType::TRANSPORT_CC),
            codec.rtcp_feedback[0]);
  EXPECT_EQ(RtcpFeedback(RtcpFeedbackType::LNTF), codec.rtcp_feedback[1]);
  EXPECT_EQ(RtcpFeedback(RtcpFeedbackType::NACK, RtcpFeedbackMessageType::PLI),
            codec.rtcp_feedback[2]);
}

// An unknown feedback param should just be ignored.
TEST(RtpParametersConversionTest, ToRtpCodecCapabilityUnknownFeedbackParam) {
  cricket::Codec cricket_codec = cricket::CreateAudioCodec(50, "foo", 22222, 4);
  cricket_codec.params["foo"] = "bar";
  cricket_codec.feedback_params.Add({"unknown", "param"});
  cricket_codec.feedback_params.Add(cricket::FeedbackParam("transport-cc"));
  RtpCodecCapability codec = ToRtpCodecCapability(cricket_codec);

  ASSERT_EQ(1u, codec.rtcp_feedback.size());
  EXPECT_EQ(RtcpFeedback(RtcpFeedbackType::TRANSPORT_CC),
            codec.rtcp_feedback[0]);
}

// Most of ToRtpCapabilities is tested by ToRtpCodecCapability, but we need to
// test that the result of ToRtpCodecCapability ends up in the result, and that
// the "fec" list is assembled correctly.
TEST(RtpParametersConversionTest, ToRtpCapabilities) {
  cricket::Codec vp8 = cricket::CreateVideoCodec(101, "VP8");
  vp8.clockrate = 90000;

  cricket::Codec red = cricket::CreateVideoCodec(102, "red");
  red.clockrate = 90000;

  cricket::Codec ulpfec = cricket::CreateVideoCodec(103, "ulpfec");
  ulpfec.clockrate = 90000;

  cricket::Codec flexfec = cricket::CreateVideoCodec(102, "flexfec-03");
  flexfec.clockrate = 90000;

  cricket::Codec rtx = cricket::CreateVideoRtxCodec(014, 101);

  cricket::Codec rtx2 = cricket::CreateVideoRtxCodec(105, 109);

  RtpCapabilities capabilities =
      ToRtpCapabilities({vp8, ulpfec, rtx, rtx2}, {{"uri", 1}, {"uri2", 3}});
  ASSERT_EQ(3u, capabilities.codecs.size());
  EXPECT_EQ("VP8", capabilities.codecs[0].name);
  EXPECT_EQ("ulpfec", capabilities.codecs[1].name);
  EXPECT_EQ("rtx", capabilities.codecs[2].name);
  EXPECT_EQ(0u, capabilities.codecs[2].parameters.size());
  ASSERT_EQ(2u, capabilities.header_extensions.size());
  EXPECT_EQ("uri", capabilities.header_extensions[0].uri);
  EXPECT_EQ(1, capabilities.header_extensions[0].preferred_id);
  EXPECT_EQ("uri2", capabilities.header_extensions[1].uri);
  EXPECT_EQ(3, capabilities.header_extensions[1].preferred_id);
  EXPECT_EQ(0u, capabilities.fec.size());

  capabilities = ToRtpCapabilities({vp8, red, ulpfec, rtx},
                                   cricket::RtpHeaderExtensions());
  EXPECT_EQ(4u, capabilities.codecs.size());
  EXPECT_THAT(
      capabilities.fec,
      UnorderedElementsAre(FecMechanism::RED, FecMechanism::RED_AND_ULPFEC));

  capabilities =
      ToRtpCapabilities({vp8, red, flexfec}, cricket::RtpHeaderExtensions());
  EXPECT_EQ(3u, capabilities.codecs.size());
  EXPECT_THAT(capabilities.fec,
              UnorderedElementsAre(FecMechanism::RED, FecMechanism::FLEXFEC));
}

}  // namespace webrtc
