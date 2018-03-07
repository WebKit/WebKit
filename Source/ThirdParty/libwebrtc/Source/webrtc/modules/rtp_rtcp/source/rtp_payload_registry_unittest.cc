/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::Eq;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::_;

TEST(RtpPayloadRegistryTest,
     RegistersAndRemembersVideoPayloadsUntilDeregistered) {
  RTPPayloadRegistry rtp_payload_registry;
  const uint8_t payload_type = 97;
  VideoCodec video_codec;
  video_codec.codecType = kVideoCodecVP8;
  strncpy(video_codec.plName, "VP8", RTP_PAYLOAD_NAME_SIZE);
  video_codec.plType = payload_type;

  EXPECT_EQ(0, rtp_payload_registry.RegisterReceivePayload(video_codec));

  const auto retrieved_payload =
      rtp_payload_registry.PayloadTypeToPayload(payload_type);
  EXPECT_TRUE(retrieved_payload);

  // We should get back the corresponding payload that we registered.
  EXPECT_STREQ("VP8", retrieved_payload->name);
  EXPECT_TRUE(retrieved_payload->typeSpecific.is_video());
  EXPECT_EQ(kRtpVideoVp8,
            retrieved_payload->typeSpecific.video_payload().videoCodecType);

  // Now forget about it and verify it's gone.
  EXPECT_EQ(0, rtp_payload_registry.DeRegisterReceivePayload(payload_type));
  EXPECT_FALSE(rtp_payload_registry.PayloadTypeToPayload(payload_type));
}

TEST(RtpPayloadRegistryTest,
     RegistersAndRemembersAudioPayloadsUntilDeregistered) {
  RTPPayloadRegistry rtp_payload_registry;
  constexpr int payload_type = 97;
  const SdpAudioFormat audio_format("name", 44000, 1);
  bool new_payload_created = false;
  EXPECT_EQ(0, rtp_payload_registry.RegisterReceivePayload(
                   payload_type, audio_format, &new_payload_created));

  EXPECT_TRUE(new_payload_created) << "A new payload WAS created.";

  const auto retrieved_payload =
      rtp_payload_registry.PayloadTypeToPayload(payload_type);
  EXPECT_TRUE(retrieved_payload);

  // We should get back the corresponding payload that we registered.
  EXPECT_STREQ("name", retrieved_payload->name);
  EXPECT_TRUE(retrieved_payload->typeSpecific.is_audio());
  EXPECT_EQ(audio_format,
            retrieved_payload->typeSpecific.audio_payload().format);

  // Now forget about it and verify it's gone.
  EXPECT_EQ(0, rtp_payload_registry.DeRegisterReceivePayload(payload_type));
  EXPECT_FALSE(rtp_payload_registry.PayloadTypeToPayload(payload_type));
}

TEST(RtpPayloadRegistryTest, AudioRedWorkProperly) {
  RTPPayloadRegistry rtp_payload_registry;
  bool new_payload_created = false;
  const SdpAudioFormat red_format("red", 8000, 1);

  EXPECT_EQ(0, rtp_payload_registry.RegisterReceivePayload(
                   127, red_format, &new_payload_created));
  EXPECT_TRUE(new_payload_created);

  EXPECT_EQ(127, rtp_payload_registry.red_payload_type());

  const auto retrieved_payload = rtp_payload_registry.PayloadTypeToPayload(127);
  EXPECT_TRUE(retrieved_payload);
  EXPECT_TRUE(retrieved_payload->typeSpecific.is_audio());
  EXPECT_EQ(red_format, retrieved_payload->typeSpecific.audio_payload().format);
}

TEST(RtpPayloadRegistryTest,
     DoesNotAcceptSamePayloadTypeTwiceExceptIfPayloadIsCompatible) {
  constexpr int payload_type = 97;
  RTPPayloadRegistry rtp_payload_registry;

  bool ignored = false;
  const SdpAudioFormat audio_format("name", 44000, 1);
  EXPECT_EQ(0, rtp_payload_registry.RegisterReceivePayload(
                   payload_type, audio_format, &ignored));

  const SdpAudioFormat audio_format_2("name", 44001, 1);  // Not compatible.
  EXPECT_EQ(-1, rtp_payload_registry.RegisterReceivePayload(
                    payload_type, audio_format_2, &ignored))
      << "Adding incompatible codec with same payload type = bad.";

  // Change payload type.
  EXPECT_EQ(0, rtp_payload_registry.RegisterReceivePayload(
                   payload_type - 1, audio_format_2, &ignored))
      << "With a different payload type is fine though.";

  // Ensure both payloads are preserved.
  const auto retrieved_payload1 =
      rtp_payload_registry.PayloadTypeToPayload(payload_type);
  EXPECT_TRUE(retrieved_payload1);
  EXPECT_STREQ("name", retrieved_payload1->name);
  EXPECT_TRUE(retrieved_payload1->typeSpecific.is_audio());
  EXPECT_EQ(audio_format,
            retrieved_payload1->typeSpecific.audio_payload().format);

  const auto retrieved_payload2 =
      rtp_payload_registry.PayloadTypeToPayload(payload_type - 1);
  EXPECT_TRUE(retrieved_payload2);
  EXPECT_STREQ("name", retrieved_payload2->name);
  EXPECT_TRUE(retrieved_payload2->typeSpecific.is_audio());
  EXPECT_EQ(audio_format_2,
            retrieved_payload2->typeSpecific.audio_payload().format);

  // Ok, update the rate for one of the codecs. If either the incoming rate or
  // the stored rate is zero it's not really an error to register the same
  // codec twice, and in that case roughly the following happens.
  EXPECT_EQ(0, rtp_payload_registry.RegisterReceivePayload(
                   payload_type, audio_format, &ignored));
}

TEST(RtpPayloadRegistryTest,
     RemovesCompatibleCodecsOnRegistryIfCodecsMustBeUnique) {
  constexpr int payload_type = 97;
  RTPPayloadRegistry rtp_payload_registry;

  bool ignored = false;
  const SdpAudioFormat audio_format("name", 44000, 1);
  EXPECT_EQ(0, rtp_payload_registry.RegisterReceivePayload(
                   payload_type, audio_format, &ignored));
  EXPECT_EQ(0, rtp_payload_registry.RegisterReceivePayload(
                   payload_type - 1, audio_format, &ignored));

  EXPECT_FALSE(rtp_payload_registry.PayloadTypeToPayload(payload_type))
      << "The first payload should be "
         "deregistered because the only thing that differs is payload type.";
  EXPECT_TRUE(rtp_payload_registry.PayloadTypeToPayload(payload_type - 1))
      << "The second payload should still be registered though.";

  // Now ensure non-compatible codecs aren't removed. Make |audio_format_2|
  // incompatible by changing the frequency.
  const SdpAudioFormat audio_format_2("name", 44001, 1);
  EXPECT_EQ(0, rtp_payload_registry.RegisterReceivePayload(
                   payload_type + 1, audio_format_2, &ignored));

  EXPECT_TRUE(rtp_payload_registry.PayloadTypeToPayload(payload_type - 1))
      << "Not compatible; both payloads should be kept.";
  EXPECT_TRUE(rtp_payload_registry.PayloadTypeToPayload(payload_type + 1))
      << "Not compatible; both payloads should be kept.";
}

TEST(RtpPayloadRegistryTest,
     LastReceivedCodecTypesAreResetWhenRegisteringNewPayloadTypes) {
  RTPPayloadRegistry rtp_payload_registry;
  rtp_payload_registry.set_last_received_payload_type(17);
  EXPECT_EQ(17, rtp_payload_registry.last_received_payload_type());

  bool media_type_unchanged = rtp_payload_registry.ReportMediaPayloadType(18);
  EXPECT_FALSE(media_type_unchanged);
  media_type_unchanged = rtp_payload_registry.ReportMediaPayloadType(18);
  EXPECT_TRUE(media_type_unchanged);

  bool ignored;
  constexpr int payload_type = 34;
  const SdpAudioFormat audio_format("name", 44000, 1);
  EXPECT_EQ(0, rtp_payload_registry.RegisterReceivePayload(
                   payload_type, audio_format, &ignored));

  EXPECT_EQ(-1, rtp_payload_registry.last_received_payload_type());
  media_type_unchanged = rtp_payload_registry.ReportMediaPayloadType(18);
  EXPECT_FALSE(media_type_unchanged);
}

class ParameterizedRtpPayloadRegistryTest
    : public ::testing::TestWithParam<int> {};

TEST_P(ParameterizedRtpPayloadRegistryTest,
       FailsToRegisterKnownPayloadsWeAreNotInterestedIn) {
  RTPPayloadRegistry rtp_payload_registry;

  bool ignored;
  const int payload_type = GetParam();
  const SdpAudioFormat audio_format("whatever", 1900, 1);
  EXPECT_EQ(-1, rtp_payload_registry.RegisterReceivePayload(
                    payload_type, audio_format, &ignored));
}

INSTANTIATE_TEST_CASE_P(TestKnownBadPayloadTypes,
                        ParameterizedRtpPayloadRegistryTest,
                        testing::Values(64, 72, 73, 74, 75, 76, 77, 78, 79));

class RtpPayloadRegistryGenericTest : public ::testing::TestWithParam<int> {};

TEST_P(RtpPayloadRegistryGenericTest, RegisterGenericReceivePayloadType) {
  RTPPayloadRegistry rtp_payload_registry;

  bool ignored;
  const int payload_type = GetParam();
  const SdpAudioFormat audio_format("generic-codec", 1900, 1);  // Dummy values.
  EXPECT_EQ(0, rtp_payload_registry.RegisterReceivePayload(
                   payload_type, audio_format, &ignored));
}

INSTANTIATE_TEST_CASE_P(TestDynamicRange,
                        RtpPayloadRegistryGenericTest,
                        testing::Range(96, 127 + 1));

}  // namespace webrtc
