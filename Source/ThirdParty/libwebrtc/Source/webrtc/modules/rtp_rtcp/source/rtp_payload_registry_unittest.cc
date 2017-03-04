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

#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

using ::testing::Eq;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::_;

static const char* kTypicalPayloadName = "name";
static const size_t kTypicalChannels = 1;
static const uint32_t kTypicalFrequency = 44000;
static const CodecInst kTypicalAudioCodec = {-1 /* pltype */, "name",
                                             kTypicalFrequency, 0 /* pacsize */,
                                             kTypicalChannels};

TEST(RtpPayloadRegistryTest,
     RegistersAndRemembersVideoPayloadsUntilDeregistered) {
  RTPPayloadRegistry rtp_payload_registry;
  const uint8_t payload_type = 97;
  VideoCodec video_codec;
  video_codec.codecType = kVideoCodecVP8;
  strncpy(video_codec.plName, "VP8", RTP_PAYLOAD_NAME_SIZE);
  video_codec.plType = payload_type;

  EXPECT_EQ(0, rtp_payload_registry.RegisterReceivePayload(video_codec));

  const RtpUtility::Payload* retrieved_payload =
      rtp_payload_registry.PayloadTypeToPayload(payload_type);
  EXPECT_TRUE(retrieved_payload);

  // We should get back the corresponding payload that we registered.
  EXPECT_STREQ("VP8", retrieved_payload->name);
  EXPECT_FALSE(retrieved_payload->audio);
  EXPECT_EQ(kRtpVideoVp8, retrieved_payload->typeSpecific.Video.videoCodecType);

  // Now forget about it and verify it's gone.
  EXPECT_EQ(0, rtp_payload_registry.DeRegisterReceivePayload(payload_type));
  EXPECT_FALSE(rtp_payload_registry.PayloadTypeToPayload(payload_type));
}

TEST(RtpPayloadRegistryTest,
     RegistersAndRemembersAudioPayloadsUntilDeregistered) {
  RTPPayloadRegistry rtp_payload_registry;
  uint8_t payload_type = 97;
  bool new_payload_created = false;
  CodecInst audio_codec = kTypicalAudioCodec;
  audio_codec.pltype = payload_type;
  EXPECT_EQ(0, rtp_payload_registry.RegisterReceivePayload(
                   audio_codec, &new_payload_created));

  EXPECT_TRUE(new_payload_created) << "A new payload WAS created.";

  const RtpUtility::Payload* retrieved_payload =
      rtp_payload_registry.PayloadTypeToPayload(payload_type);
  EXPECT_TRUE(retrieved_payload);

  // We should get back the corresponding payload that we registered.
  EXPECT_STREQ(kTypicalPayloadName, retrieved_payload->name);
  EXPECT_TRUE(retrieved_payload->audio);
  EXPECT_EQ(kTypicalFrequency, retrieved_payload->typeSpecific.Audio.frequency);
  EXPECT_EQ(kTypicalChannels, retrieved_payload->typeSpecific.Audio.channels);

  // Now forget about it and verify it's gone.
  EXPECT_EQ(0, rtp_payload_registry.DeRegisterReceivePayload(payload_type));
  EXPECT_FALSE(rtp_payload_registry.PayloadTypeToPayload(payload_type));
}

TEST(RtpPayloadRegistryTest, AudioRedWorkProperly) {
  const uint8_t kRedPayloadType = 127;
  const int kRedSampleRate = 8000;
  const size_t kRedChannels = 1;

  RTPPayloadRegistry rtp_payload_registry;

  bool new_payload_created = false;
  CodecInst red_audio_codec;
  strncpy(red_audio_codec.plname, "red", RTP_PAYLOAD_NAME_SIZE);
  red_audio_codec.pltype = kRedPayloadType;
  red_audio_codec.plfreq = kRedSampleRate;
  red_audio_codec.channels = kRedChannels;
  EXPECT_EQ(0, rtp_payload_registry.RegisterReceivePayload(
                   red_audio_codec, &new_payload_created));
  EXPECT_TRUE(new_payload_created);

  EXPECT_EQ(kRedPayloadType, rtp_payload_registry.red_payload_type());

  const RtpUtility::Payload* retrieved_payload =
      rtp_payload_registry.PayloadTypeToPayload(kRedPayloadType);
  EXPECT_TRUE(retrieved_payload);
  EXPECT_TRUE(retrieved_payload->audio);
  EXPECT_STRCASEEQ("red", retrieved_payload->name);

  // Sample rate is correctly registered.
  EXPECT_EQ(kRedSampleRate,
            rtp_payload_registry.GetPayloadTypeFrequency(kRedPayloadType));
}

TEST(RtpPayloadRegistryTest,
     DoesNotAcceptSamePayloadTypeTwiceExceptIfPayloadIsCompatible) {
  uint8_t payload_type = 97;
  RTPPayloadRegistry rtp_payload_registry;

  bool ignored = false;
  CodecInst audio_codec = kTypicalAudioCodec;
  audio_codec.pltype = payload_type;
  EXPECT_EQ(0,
            rtp_payload_registry.RegisterReceivePayload(audio_codec, &ignored));

  CodecInst audio_codec_2 = kTypicalAudioCodec;
  audio_codec_2.pltype = payload_type;
  // Make |audio_codec_2| incompatible with |audio_codec| by changing
  // the frequency.
  audio_codec_2.plfreq = kTypicalFrequency + 1;
  EXPECT_EQ(
      -1, rtp_payload_registry.RegisterReceivePayload(audio_codec_2, &ignored))
      << "Adding incompatible codec with same payload type = bad.";

  // Change payload type.
  audio_codec_2.pltype = payload_type - 1;
  EXPECT_EQ(
      0, rtp_payload_registry.RegisterReceivePayload(audio_codec_2, &ignored))
      << "With a different payload type is fine though.";

  // Ensure both payloads are preserved.
  const RtpUtility::Payload* retrieved_payload =
      rtp_payload_registry.PayloadTypeToPayload(payload_type);
  EXPECT_TRUE(retrieved_payload);
  EXPECT_STREQ(kTypicalPayloadName, retrieved_payload->name);
  EXPECT_TRUE(retrieved_payload->audio);
  EXPECT_EQ(kTypicalFrequency, retrieved_payload->typeSpecific.Audio.frequency);
  EXPECT_EQ(kTypicalChannels, retrieved_payload->typeSpecific.Audio.channels);

  retrieved_payload =
      rtp_payload_registry.PayloadTypeToPayload(payload_type - 1);
  EXPECT_TRUE(retrieved_payload);
  EXPECT_STREQ(kTypicalPayloadName, retrieved_payload->name);
  EXPECT_TRUE(retrieved_payload->audio);
  EXPECT_EQ(kTypicalFrequency + 1,
            retrieved_payload->typeSpecific.Audio.frequency);
  EXPECT_EQ(kTypicalChannels, retrieved_payload->typeSpecific.Audio.channels);

  // Ok, update the rate for one of the codecs. If either the incoming rate or
  // the stored rate is zero it's not really an error to register the same
  // codec twice, and in that case roughly the following happens.
  EXPECT_EQ(0,
            rtp_payload_registry.RegisterReceivePayload(audio_codec, &ignored));
}

TEST(RtpPayloadRegistryTest,
     RemovesCompatibleCodecsOnRegistryIfCodecsMustBeUnique) {
  uint8_t payload_type = 97;
  RTPPayloadRegistry rtp_payload_registry;

  bool ignored = false;
  CodecInst audio_codec = kTypicalAudioCodec;
  audio_codec.pltype = payload_type;
  EXPECT_EQ(0,
            rtp_payload_registry.RegisterReceivePayload(audio_codec, &ignored));
  CodecInst audio_codec_2 = kTypicalAudioCodec;
  audio_codec_2.pltype = payload_type - 1;
  EXPECT_EQ(
      0, rtp_payload_registry.RegisterReceivePayload(audio_codec_2, &ignored));

  EXPECT_FALSE(rtp_payload_registry.PayloadTypeToPayload(payload_type))
      << "The first payload should be "
         "deregistered because the only thing that differs is payload type.";
  EXPECT_TRUE(rtp_payload_registry.PayloadTypeToPayload(payload_type - 1))
      << "The second payload should still be registered though.";

  // Now ensure non-compatible codecs aren't removed. Make |audio_codec_3|
  // incompatible by changing the frequency.
  CodecInst audio_codec_3 = kTypicalAudioCodec;
  audio_codec_3.pltype = payload_type + 1;
  audio_codec_3.plfreq = kTypicalFrequency + 1;
  EXPECT_EQ(
      0, rtp_payload_registry.RegisterReceivePayload(audio_codec_3, &ignored));

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
  CodecInst audio_codec = kTypicalAudioCodec;
  audio_codec.pltype = 34;
  EXPECT_EQ(0,
            rtp_payload_registry.RegisterReceivePayload(audio_codec, &ignored));

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
  CodecInst audio_codec;
  strncpy(audio_codec.plname, "whatever", RTP_PAYLOAD_NAME_SIZE);
  audio_codec.pltype = GetParam();
  audio_codec.plfreq = 1900;
  audio_codec.channels = 1;
  EXPECT_EQ(-1,
            rtp_payload_registry.RegisterReceivePayload(audio_codec, &ignored));
}

INSTANTIATE_TEST_CASE_P(TestKnownBadPayloadTypes,
                        ParameterizedRtpPayloadRegistryTest,
                        testing::Values(64, 72, 73, 74, 75, 76, 77, 78, 79));

class RtpPayloadRegistryGenericTest : public ::testing::TestWithParam<int> {};

TEST_P(RtpPayloadRegistryGenericTest, RegisterGenericReceivePayloadType) {
  RTPPayloadRegistry rtp_payload_registry;

  bool ignored;
  CodecInst audio_codec;
  // Dummy values, except for payload_type.
  strncpy(audio_codec.plname, "generic-codec", RTP_PAYLOAD_NAME_SIZE);
  audio_codec.pltype = GetParam();
  audio_codec.plfreq = 1900;
  audio_codec.channels = 1;
  EXPECT_EQ(0,
            rtp_payload_registry.RegisterReceivePayload(audio_codec, &ignored));
}

// Generates an RTX packet for the given length and original sequence number.
// The RTX sequence number and ssrc will use the default value of 9999. The
// caller takes ownership of the returned buffer.
const uint8_t* GenerateRtxPacket(size_t header_length,
                                 size_t payload_length,
                                 uint16_t original_sequence_number) {
  uint8_t* packet =
      new uint8_t[kRtxHeaderSize + header_length + payload_length]();
  // Write the RTP version to the first byte, so the resulting header can be
  // parsed.
  static const int kRtpExpectedVersion = 2;
  packet[0] = static_cast<uint8_t>(kRtpExpectedVersion << 6);
  // Write a junk sequence number. It should be thrown away when the packet is
  // restored.
  ByteWriter<uint16_t>::WriteBigEndian(packet + 2, 9999);
  // Write a junk ssrc. It should also be thrown away when the packet is
  // restored.
  ByteWriter<uint32_t>::WriteBigEndian(packet + 8, 9999);

  // Now write the RTX header. It occurs at the start of the payload block, and
  // contains just the sequence number.
  ByteWriter<uint16_t>::WriteBigEndian(packet + header_length,
                                       original_sequence_number);
  return packet;
}

void TestRtxPacket(RTPPayloadRegistry* rtp_payload_registry,
                   int rtx_payload_type,
                   int expected_payload_type,
                   bool should_succeed) {
  size_t header_length = 100;
  size_t payload_length = 200;
  size_t original_length = header_length + payload_length + kRtxHeaderSize;

  RTPHeader header;
  header.ssrc = 1000;
  header.sequenceNumber = 100;
  header.payloadType = rtx_payload_type;
  header.headerLength = header_length;

  uint16_t original_sequence_number = 1234;
  uint32_t original_ssrc = 500;

  std::unique_ptr<const uint8_t[]> packet(GenerateRtxPacket(
      header_length, payload_length, original_sequence_number));
  std::unique_ptr<uint8_t[]> restored_packet(
      new uint8_t[header_length + payload_length]);
  size_t length = original_length;
  bool success = rtp_payload_registry->RestoreOriginalPacket(
      restored_packet.get(), packet.get(), &length, original_ssrc, header);
  EXPECT_EQ(should_succeed, success)
      << "Test success should match should_succeed.";
  if (!success) {
    return;
  }

  EXPECT_EQ(original_length - kRtxHeaderSize, length)
      << "The restored packet should be exactly kRtxHeaderSize smaller.";

  std::unique_ptr<RtpHeaderParser> header_parser(RtpHeaderParser::Create());
  RTPHeader restored_header;
  ASSERT_TRUE(
      header_parser->Parse(restored_packet.get(), length, &restored_header));
  EXPECT_EQ(original_sequence_number, restored_header.sequenceNumber)
      << "The restored packet should have the original sequence number "
      << "in the correct location in the RTP header.";
  EXPECT_EQ(expected_payload_type, restored_header.payloadType)
      << "The restored packet should have the correct payload type.";
  EXPECT_EQ(original_ssrc, restored_header.ssrc)
      << "The restored packet should have the correct ssrc.";
}

TEST(RtpPayloadRegistryTest, MultipleRtxPayloadTypes) {
  RTPPayloadRegistry rtp_payload_registry;
  // Set the incoming payload type to 90.
  RTPHeader header;
  header.payloadType = 90;
  header.ssrc = 1;
  rtp_payload_registry.SetIncomingPayloadType(header);
  rtp_payload_registry.SetRtxSsrc(100);
  // Map two RTX payload types.
  rtp_payload_registry.SetRtxPayloadType(105, 95);
  rtp_payload_registry.SetRtxPayloadType(106, 96);

  TestRtxPacket(&rtp_payload_registry, 105, 95, true);
  TestRtxPacket(&rtp_payload_registry, 106, 96, true);
}

TEST(RtpPayloadRegistryTest, InvalidRtxConfiguration) {
  RTPPayloadRegistry rtp_payload_registry;
  rtp_payload_registry.SetRtxSsrc(100);
  // Fails because no mappings exist and the incoming payload type isn't known.
  TestRtxPacket(&rtp_payload_registry, 105, 0, false);
  // Succeeds when the mapping is used, but fails for the implicit fallback.
  rtp_payload_registry.SetRtxPayloadType(105, 95);
  TestRtxPacket(&rtp_payload_registry, 105, 95, true);
  TestRtxPacket(&rtp_payload_registry, 106, 0, false);
}

INSTANTIATE_TEST_CASE_P(TestDynamicRange,
                        RtpPayloadRegistryGenericTest,
                        testing::Range(96, 127 + 1));

}  // namespace webrtc
