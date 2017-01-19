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

#include "webrtc/modules/rtp_rtcp/include/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/mock/mock_rtp_payload_strategy.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

using ::testing::Eq;
using ::testing::Return;
using ::testing::_;

static const char* kTypicalPayloadName = "name";
static const size_t kTypicalChannels = 1;
static const int kTypicalFrequency = 44000;
static const int kTypicalRate = 32 * 1024;

class RtpPayloadRegistryTest : public ::testing::Test {
 public:
  void SetUp() {
    // Note: the payload registry takes ownership of the strategy.
    mock_payload_strategy_ = new testing::NiceMock<MockRTPPayloadStrategy>();
    rtp_payload_registry_.reset(new RTPPayloadRegistry(mock_payload_strategy_));
  }

 protected:
  RtpUtility::Payload* ExpectReturnOfTypicalAudioPayload(uint8_t payload_type,
                                                         uint32_t rate) {
    bool audio = true;
    RtpUtility::Payload returned_payload = {
        "name",
        audio,
        {// Initialize the audio struct in this case.
         {kTypicalFrequency, kTypicalChannels, rate}}};

    // Note: we return a new payload since the payload registry takes ownership
    // of the created object.
    RtpUtility::Payload* returned_payload_on_heap =
        new RtpUtility::Payload(returned_payload);
    EXPECT_CALL(*mock_payload_strategy_,
                CreatePayloadType(kTypicalPayloadName, payload_type,
                                  kTypicalFrequency, kTypicalChannels, rate))
        .WillOnce(Return(returned_payload_on_heap));
    return returned_payload_on_heap;
  }

  std::unique_ptr<RTPPayloadRegistry> rtp_payload_registry_;
  testing::NiceMock<MockRTPPayloadStrategy>* mock_payload_strategy_;
};

TEST_F(RtpPayloadRegistryTest, RegistersAndRemembersPayloadsUntilDeregistered) {
  uint8_t payload_type = 97;
  RtpUtility::Payload* returned_payload_on_heap =
      ExpectReturnOfTypicalAudioPayload(payload_type, kTypicalRate);

  bool new_payload_created = false;
  EXPECT_EQ(0, rtp_payload_registry_->RegisterReceivePayload(
                   kTypicalPayloadName, payload_type, kTypicalFrequency,
                   kTypicalChannels, kTypicalRate, &new_payload_created));

  EXPECT_TRUE(new_payload_created) << "A new payload WAS created.";

  const RtpUtility::Payload* retrieved_payload =
      rtp_payload_registry_->PayloadTypeToPayload(payload_type);
  EXPECT_TRUE(retrieved_payload);

  // We should get back the exact pointer to the payload returned by the
  // payload strategy.
  EXPECT_EQ(returned_payload_on_heap, retrieved_payload);

  // Now forget about it and verify it's gone.
  EXPECT_EQ(0, rtp_payload_registry_->DeRegisterReceivePayload(payload_type));
  EXPECT_FALSE(rtp_payload_registry_->PayloadTypeToPayload(payload_type));
}

TEST_F(RtpPayloadRegistryTest, AudioRedWorkProperly) {
  const uint8_t kRedPayloadType = 127;
  const int kRedSampleRate = 8000;
  const size_t kRedChannels = 1;
  const int kRedBitRate = 0;

  // This creates an audio RTP payload strategy.
  rtp_payload_registry_.reset(
      new RTPPayloadRegistry(RTPPayloadStrategy::CreateStrategy(true)));

  bool new_payload_created = false;
  EXPECT_EQ(0, rtp_payload_registry_->RegisterReceivePayload(
                   "red", kRedPayloadType, kRedSampleRate, kRedChannels,
                   kRedBitRate, &new_payload_created));
  EXPECT_TRUE(new_payload_created);

  EXPECT_EQ(kRedPayloadType, rtp_payload_registry_->red_payload_type());

  const RtpUtility::Payload* retrieved_payload =
      rtp_payload_registry_->PayloadTypeToPayload(kRedPayloadType);
  EXPECT_TRUE(retrieved_payload);
  EXPECT_TRUE(retrieved_payload->audio);
  EXPECT_STRCASEEQ("red", retrieved_payload->name);

  // Sample rate is correctly registered.
  EXPECT_EQ(kRedSampleRate,
            rtp_payload_registry_->GetPayloadTypeFrequency(kRedPayloadType));
}

TEST_F(RtpPayloadRegistryTest,
       DoesNotAcceptSamePayloadTypeTwiceExceptIfPayloadIsCompatible) {
  uint8_t payload_type = 97;

  bool ignored = false;
  RtpUtility::Payload* first_payload_on_heap =
      ExpectReturnOfTypicalAudioPayload(payload_type, kTypicalRate);
  EXPECT_EQ(0, rtp_payload_registry_->RegisterReceivePayload(
                   kTypicalPayloadName, payload_type, kTypicalFrequency,
                   kTypicalChannels, kTypicalRate, &ignored));

  EXPECT_EQ(-1, rtp_payload_registry_->RegisterReceivePayload(
                    kTypicalPayloadName, payload_type, kTypicalFrequency,
                    kTypicalChannels, kTypicalRate, &ignored))
      << "Adding same codec twice = bad.";

  RtpUtility::Payload* second_payload_on_heap =
      ExpectReturnOfTypicalAudioPayload(payload_type - 1, kTypicalRate);
  EXPECT_EQ(0, rtp_payload_registry_->RegisterReceivePayload(
                   kTypicalPayloadName, payload_type - 1, kTypicalFrequency,
                   kTypicalChannels, kTypicalRate, &ignored))
      << "With a different payload type is fine though.";

  // Ensure both payloads are preserved.
  const RtpUtility::Payload* retrieved_payload =
      rtp_payload_registry_->PayloadTypeToPayload(payload_type);
  EXPECT_TRUE(retrieved_payload);
  EXPECT_EQ(first_payload_on_heap, retrieved_payload);
  retrieved_payload =
      rtp_payload_registry_->PayloadTypeToPayload(payload_type - 1);
  EXPECT_TRUE(retrieved_payload);
  EXPECT_EQ(second_payload_on_heap, retrieved_payload);

  // Ok, update the rate for one of the codecs. If either the incoming rate or
  // the stored rate is zero it's not really an error to register the same
  // codec twice, and in that case roughly the following happens.
  ON_CALL(*mock_payload_strategy_, PayloadIsCompatible(_, _, _, _))
      .WillByDefault(Return(true));
  EXPECT_CALL(*mock_payload_strategy_,
              UpdatePayloadRate(first_payload_on_heap, kTypicalRate));
  EXPECT_EQ(0, rtp_payload_registry_->RegisterReceivePayload(
                   kTypicalPayloadName, payload_type, kTypicalFrequency,
                   kTypicalChannels, kTypicalRate, &ignored));
}

TEST_F(RtpPayloadRegistryTest,
       RemovesCompatibleCodecsOnRegistryIfCodecsMustBeUnique) {
  ON_CALL(*mock_payload_strategy_, PayloadIsCompatible(_, _, _, _))
      .WillByDefault(Return(true));
  ON_CALL(*mock_payload_strategy_, CodecsMustBeUnique())
      .WillByDefault(Return(true));

  uint8_t payload_type = 97;

  bool ignored = false;
  ExpectReturnOfTypicalAudioPayload(payload_type, kTypicalRate);
  EXPECT_EQ(0, rtp_payload_registry_->RegisterReceivePayload(
                   kTypicalPayloadName, payload_type, kTypicalFrequency,
                   kTypicalChannels, kTypicalRate, &ignored));
  ExpectReturnOfTypicalAudioPayload(payload_type - 1, kTypicalRate);
  EXPECT_EQ(0, rtp_payload_registry_->RegisterReceivePayload(
                   kTypicalPayloadName, payload_type - 1, kTypicalFrequency,
                   kTypicalChannels, kTypicalRate, &ignored));

  EXPECT_FALSE(rtp_payload_registry_->PayloadTypeToPayload(payload_type))
      << "The first payload should be "
         "deregistered because the only thing that differs is payload type.";
  EXPECT_TRUE(rtp_payload_registry_->PayloadTypeToPayload(payload_type - 1))
      << "The second payload should still be registered though.";

  // Now ensure non-compatible codecs aren't removed.
  ON_CALL(*mock_payload_strategy_, PayloadIsCompatible(_, _, _, _))
      .WillByDefault(Return(false));
  ExpectReturnOfTypicalAudioPayload(payload_type + 1, kTypicalRate);
  EXPECT_EQ(0, rtp_payload_registry_->RegisterReceivePayload(
                   kTypicalPayloadName, payload_type + 1, kTypicalFrequency,
                   kTypicalChannels, kTypicalRate, &ignored));

  EXPECT_TRUE(rtp_payload_registry_->PayloadTypeToPayload(payload_type - 1))
      << "Not compatible; both payloads should be kept.";
  EXPECT_TRUE(rtp_payload_registry_->PayloadTypeToPayload(payload_type + 1))
      << "Not compatible; both payloads should be kept.";
}

TEST_F(RtpPayloadRegistryTest,
       LastReceivedCodecTypesAreResetWhenRegisteringNewPayloadTypes) {
  rtp_payload_registry_->set_last_received_payload_type(17);
  EXPECT_EQ(17, rtp_payload_registry_->last_received_payload_type());

  bool media_type_unchanged = rtp_payload_registry_->ReportMediaPayloadType(18);
  EXPECT_FALSE(media_type_unchanged);
  media_type_unchanged = rtp_payload_registry_->ReportMediaPayloadType(18);
  EXPECT_TRUE(media_type_unchanged);

  bool ignored;
  ExpectReturnOfTypicalAudioPayload(34, kTypicalRate);
  EXPECT_EQ(0, rtp_payload_registry_->RegisterReceivePayload(
                   kTypicalPayloadName, 34, kTypicalFrequency, kTypicalChannels,
                   kTypicalRate, &ignored));

  EXPECT_EQ(-1, rtp_payload_registry_->last_received_payload_type());
  media_type_unchanged = rtp_payload_registry_->ReportMediaPayloadType(18);
  EXPECT_FALSE(media_type_unchanged);
}

class ParameterizedRtpPayloadRegistryTest
    : public RtpPayloadRegistryTest,
      public ::testing::WithParamInterface<int> {};

TEST_P(ParameterizedRtpPayloadRegistryTest,
       FailsToRegisterKnownPayloadsWeAreNotInterestedIn) {
  int payload_type = GetParam();

  bool ignored;
  EXPECT_EQ(-1, rtp_payload_registry_->RegisterReceivePayload(
                    "whatever", static_cast<uint8_t>(payload_type), 19, 1, 17,
                    &ignored));
}

INSTANTIATE_TEST_CASE_P(TestKnownBadPayloadTypes,
                        ParameterizedRtpPayloadRegistryTest,
                        testing::Values(64, 72, 73, 74, 75, 76, 77, 78, 79));

class RtpPayloadRegistryGenericTest
    : public RtpPayloadRegistryTest,
      public ::testing::WithParamInterface<int> {};

TEST_P(RtpPayloadRegistryGenericTest, RegisterGenericReceivePayloadType) {
  int payload_type = GetParam();

  bool ignored;

  EXPECT_EQ(0, rtp_payload_registry_->RegisterReceivePayload(
                   "generic-codec", static_cast<int8_t>(payload_type), 19, 1,
                   17, &ignored));  // dummy values, except for payload_type
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

TEST_F(RtpPayloadRegistryTest, MultipleRtxPayloadTypes) {
  // Set the incoming payload type to 90.
  RTPHeader header;
  header.payloadType = 90;
  header.ssrc = 1;
  rtp_payload_registry_->SetIncomingPayloadType(header);
  rtp_payload_registry_->SetRtxSsrc(100);
  // Map two RTX payload types.
  rtp_payload_registry_->SetRtxPayloadType(105, 95);
  rtp_payload_registry_->SetRtxPayloadType(106, 96);

  TestRtxPacket(rtp_payload_registry_.get(), 105, 95, true);
  TestRtxPacket(rtp_payload_registry_.get(), 106, 96, true);
}

TEST_F(RtpPayloadRegistryTest, InvalidRtxConfiguration) {
  rtp_payload_registry_->SetRtxSsrc(100);
  // Fails because no mappings exist and the incoming payload type isn't known.
  TestRtxPacket(rtp_payload_registry_.get(), 105, 0, false);
  // Succeeds when the mapping is used, but fails for the implicit fallback.
  rtp_payload_registry_->SetRtxPayloadType(105, 95);
  TestRtxPacket(rtp_payload_registry_.get(), 105, 95, true);
  TestRtxPacket(rtp_payload_registry_.get(), 106, 0, false);
}

INSTANTIATE_TEST_CASE_P(TestDynamicRange,
                        RtpPayloadRegistryGenericTest,
                        testing::Range(96, 127 + 1));

}  // namespace webrtc
