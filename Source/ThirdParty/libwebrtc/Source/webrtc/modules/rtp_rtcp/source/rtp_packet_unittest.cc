/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_received.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_to_send.h"

#include "webrtc/base/random.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extension.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

using testing::ElementsAreArray;
using testing::make_tuple;

namespace webrtc {
namespace {
constexpr int8_t kPayloadType = 100;
constexpr uint32_t kSsrc = 0x12345678;
constexpr uint16_t kSeqNum = 88;
constexpr uint32_t kTimestamp = 0x65431278;
constexpr uint8_t kTransmissionOffsetExtensionId = 1;
constexpr uint8_t kAudioLevelExtensionId = 9;
constexpr int32_t kTimeOffset = 0x56ce;
constexpr bool kVoiceActive = true;
constexpr uint8_t kAudioLevel = 0x5a;
constexpr size_t kMaxPaddingSize = 224u;
// clang-format off
constexpr uint8_t kMinimumPacket[] = {
    0x80, kPayloadType, 0x00, kSeqNum,
    0x65, 0x43, 0x12, 0x78,
    0x12, 0x34, 0x56, 0x78};
constexpr uint8_t kPacketWithTO[] = {
    0x90, kPayloadType, 0x00, kSeqNum,
    0x65, 0x43, 0x12, 0x78,
    0x12, 0x34, 0x56, 0x78,
    0xbe, 0xde, 0x00, 0x01,
    0x12, 0x00, 0x56, 0xce};

constexpr uint8_t kPacketWithTOAndAL[] = {
    0x90, kPayloadType, 0x00, kSeqNum,
    0x65, 0x43, 0x12, 0x78,
    0x12, 0x34, 0x56, 0x78,
    0xbe, 0xde, 0x00, 0x02,
    0x12, 0x00, 0x56, 0xce,
    0x90, 0x80|kAudioLevel, 0x00, 0x00};

constexpr uint32_t kCsrcs[] = {0x34567890, 0x32435465};
constexpr uint8_t kPayload[] = {'p', 'a', 'y', 'l', 'o', 'a', 'd'};
constexpr uint8_t kPacketPaddingSize = 8;
constexpr uint8_t kPacket[] = {
    0xb2, kPayloadType, 0x00, kSeqNum,
    0x65, 0x43, 0x12, 0x78,
    0x12, 0x34, 0x56, 0x78,
    0x34, 0x56, 0x78, 0x90,
    0x32, 0x43, 0x54, 0x65,
    0xbe, 0xde, 0x00, 0x01,
    0x12, 0x00, 0x56, 0xce,
    'p', 'a', 'y', 'l', 'o', 'a', 'd',
    'p', 'a', 'd', 'd', 'i', 'n', 'g', kPacketPaddingSize};

constexpr uint8_t kPacketWithInvalidExtension[] = {
    0x90, kPayloadType, 0x00, kSeqNum,
    0x65, 0x43, 0x12, 0x78,  // kTimestamp.
    0x12, 0x34, 0x56, 0x78,  // kSSrc.
    0xbe, 0xde, 0x00, 0x02,  // Extension block of size 2 x 32bit words.
    (kTransmissionOffsetExtensionId << 4) | 6,  // (6+1)-byte extension, but
           'e',  'x',  't',                     // Transmission Offset
     'd',  'a',  't',  'a',                     // expected to be 3-bytes.
     'p',  'a',  'y',  'l',  'o',  'a',  'd'
};
// clang-format on
}  // namespace

TEST(RtpPacketTest, CreateMinimum) {
  RtpPacketToSend packet(nullptr);
  packet.SetPayloadType(kPayloadType);
  packet.SetSequenceNumber(kSeqNum);
  packet.SetTimestamp(kTimestamp);
  packet.SetSsrc(kSsrc);
  EXPECT_THAT(kMinimumPacket, ElementsAreArray(packet.data(), packet.size()));
}

TEST(RtpPacketTest, CreateWithExtension) {
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register(kRtpExtensionTransmissionTimeOffset,
                      kTransmissionOffsetExtensionId);
  RtpPacketToSend packet(&extensions);
  packet.SetPayloadType(kPayloadType);
  packet.SetSequenceNumber(kSeqNum);
  packet.SetTimestamp(kTimestamp);
  packet.SetSsrc(kSsrc);
  packet.SetExtension<TransmissionOffset>(kTimeOffset);
  EXPECT_THAT(kPacketWithTO, ElementsAreArray(packet.data(), packet.size()));
}

TEST(RtpPacketTest, CreateWith2Extensions) {
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register(kRtpExtensionTransmissionTimeOffset,
                      kTransmissionOffsetExtensionId);
  extensions.Register(kRtpExtensionAudioLevel, kAudioLevelExtensionId);
  RtpPacketToSend packet(&extensions);
  packet.SetPayloadType(kPayloadType);
  packet.SetSequenceNumber(kSeqNum);
  packet.SetTimestamp(kTimestamp);
  packet.SetSsrc(kSsrc);
  packet.SetExtension<TransmissionOffset>(kTimeOffset);
  packet.SetExtension<AudioLevel>(kVoiceActive, kAudioLevel);
  EXPECT_THAT(kPacketWithTOAndAL,
              ElementsAreArray(packet.data(), packet.size()));
}

TEST(RtpPacketTest, SetReservedExtensionsAfterPayload) {
  const size_t kPayloadSize = 4;
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register(kRtpExtensionTransmissionTimeOffset,
                      kTransmissionOffsetExtensionId);
  extensions.Register(kRtpExtensionAudioLevel, kAudioLevelExtensionId);
  RtpPacketToSend packet(&extensions);

  EXPECT_TRUE(packet.ReserveExtension<TransmissionOffset>());
  packet.AllocatePayload(kPayloadSize);
  // Can't set extension after payload.
  EXPECT_FALSE(packet.SetExtension<AudioLevel>(kVoiceActive, kAudioLevel));
  // Unless reserved.
  EXPECT_TRUE(packet.SetExtension<TransmissionOffset>(kTimeOffset));
}

TEST(RtpPacketTest, CreatePurePadding) {
  const size_t kPaddingSize = kMaxPaddingSize - 1;
  RtpPacketToSend packet(nullptr, 12 + kPaddingSize);
  packet.SetPayloadType(kPayloadType);
  packet.SetSequenceNumber(kSeqNum);
  packet.SetTimestamp(kTimestamp);
  packet.SetSsrc(kSsrc);
  Random random(0x123456789);

  EXPECT_LT(packet.size(), packet.capacity());
  EXPECT_FALSE(packet.SetPadding(kPaddingSize + 1, &random));
  EXPECT_TRUE(packet.SetPadding(kPaddingSize, &random));
  EXPECT_EQ(packet.size(), packet.capacity());
}

TEST(RtpPacketTest, CreateUnalignedPadding) {
  const size_t kPayloadSize = 3;  // Make padding start at unaligned address.
  RtpPacketToSend packet(nullptr, 12 + kPayloadSize + kMaxPaddingSize);
  packet.SetPayloadType(kPayloadType);
  packet.SetSequenceNumber(kSeqNum);
  packet.SetTimestamp(kTimestamp);
  packet.SetSsrc(kSsrc);
  packet.AllocatePayload(kPayloadSize);
  Random r(0x123456789);

  EXPECT_LT(packet.size(), packet.capacity());
  EXPECT_TRUE(packet.SetPadding(kMaxPaddingSize, &r));
  EXPECT_EQ(packet.size(), packet.capacity());
}

TEST(RtpPacketTest, ParseMinimum) {
  RtpPacketReceived packet;
  EXPECT_TRUE(packet.Parse(kMinimumPacket, sizeof(kMinimumPacket)));
  EXPECT_EQ(kPayloadType, packet.PayloadType());
  EXPECT_EQ(kSeqNum, packet.SequenceNumber());
  EXPECT_EQ(kTimestamp, packet.Timestamp());
  EXPECT_EQ(kSsrc, packet.Ssrc());
  EXPECT_EQ(0u, packet.padding_size());
  EXPECT_EQ(0u, packet.payload_size());
}

TEST(RtpPacketTest, ParseBuffer) {
  rtc::CopyOnWriteBuffer unparsed(kMinimumPacket);
  const uint8_t* raw = unparsed.data();

  RtpPacketReceived packet;
  EXPECT_TRUE(packet.Parse(std::move(unparsed)));
  EXPECT_EQ(raw, packet.data());  // Expect packet take the buffer without copy.
  EXPECT_EQ(kSeqNum, packet.SequenceNumber());
  EXPECT_EQ(kTimestamp, packet.Timestamp());
  EXPECT_EQ(kSsrc, packet.Ssrc());
  EXPECT_EQ(0u, packet.padding_size());
  EXPECT_EQ(0u, packet.payload_size());
}

TEST(RtpPacketTest, ParseWithExtension) {
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register(kRtpExtensionTransmissionTimeOffset,
                      kTransmissionOffsetExtensionId);

  RtpPacketReceived packet(&extensions);
  EXPECT_TRUE(packet.Parse(kPacketWithTO, sizeof(kPacketWithTO)));
  EXPECT_EQ(kPayloadType, packet.PayloadType());
  EXPECT_EQ(kSeqNum, packet.SequenceNumber());
  EXPECT_EQ(kTimestamp, packet.Timestamp());
  EXPECT_EQ(kSsrc, packet.Ssrc());
  int32_t time_offset;
  EXPECT_TRUE(packet.GetExtension<TransmissionOffset>(&time_offset));
  EXPECT_EQ(kTimeOffset, time_offset);
  EXPECT_EQ(0u, packet.payload_size());
  EXPECT_EQ(0u, packet.padding_size());
}

TEST(RtpPacketTest, ParseWithInvalidSizedExtension) {
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register(kRtpExtensionTransmissionTimeOffset,
                      kTransmissionOffsetExtensionId);

  RtpPacketReceived packet(&extensions);
  EXPECT_TRUE(packet.Parse(kPacketWithInvalidExtension,
                           sizeof(kPacketWithInvalidExtension)));

  // Extension should be ignored.
  int32_t time_offset;
  EXPECT_FALSE(packet.GetExtension<TransmissionOffset>(&time_offset));

  // But shouldn't prevent reading payload.
  EXPECT_THAT(packet.payload(), ElementsAreArray(kPayload));
}

TEST(RtpPacketTest, ParseWithOverSizedExtension) {
  // clang-format off
  const uint8_t bad_packet[] = {
      0x90, kPayloadType, 0x00, kSeqNum,
      0x65, 0x43, 0x12, 0x78,  // kTimestamp.
      0x12, 0x34, 0x56, 0x78,  // kSsrc.
      0xbe, 0xde, 0x00, 0x01,  // Extension of size 1x32bit word.
      0x00,  // Add a byte of padding.
            0x12,  // Extension id 1 size (2+1).
                  0xda, 0x1a  // Only 2 bytes of extension payload.
  };
  // clang-format on
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register(TransmissionOffset::kId, 1);
  RtpPacketReceived packet(&extensions);

  // Parse should ignore bad extension and proceed.
  EXPECT_TRUE(packet.Parse(bad_packet, sizeof(bad_packet)));
  int32_t time_offset;
  // But extracting extension should fail.
  EXPECT_FALSE(packet.GetExtension<TransmissionOffset>(&time_offset));
}

TEST(RtpPacketTest, ParseWith2Extensions) {
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register(kRtpExtensionTransmissionTimeOffset,
                      kTransmissionOffsetExtensionId);
  extensions.Register(kRtpExtensionAudioLevel, kAudioLevelExtensionId);
  RtpPacketReceived packet(&extensions);
  EXPECT_TRUE(packet.Parse(kPacketWithTOAndAL, sizeof(kPacketWithTOAndAL)));
  int32_t time_offset;
  EXPECT_TRUE(packet.GetExtension<TransmissionOffset>(&time_offset));
  EXPECT_EQ(kTimeOffset, time_offset);
  bool voice_active;
  uint8_t audio_level;
  EXPECT_TRUE(packet.GetExtension<AudioLevel>(&voice_active, &audio_level));
  EXPECT_EQ(kVoiceActive, voice_active);
  EXPECT_EQ(kAudioLevel, audio_level);
}

TEST(RtpPacketTest, ParseWithAllFeatures) {
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register(kRtpExtensionTransmissionTimeOffset,
                      kTransmissionOffsetExtensionId);
  RtpPacketReceived packet(&extensions);
  EXPECT_TRUE(packet.Parse(kPacket, sizeof(kPacket)));
  EXPECT_EQ(kPayloadType, packet.PayloadType());
  EXPECT_EQ(kSeqNum, packet.SequenceNumber());
  EXPECT_EQ(kTimestamp, packet.Timestamp());
  EXPECT_EQ(kSsrc, packet.Ssrc());
  EXPECT_THAT(packet.Csrcs(), ElementsAreArray(kCsrcs));
  EXPECT_THAT(packet.payload(), ElementsAreArray(kPayload));
  EXPECT_EQ(kPacketPaddingSize, packet.padding_size());
  int32_t time_offset;
  EXPECT_TRUE(packet.GetExtension<TransmissionOffset>(&time_offset));
}

TEST(RtpPacketTest, ParseWithExtensionDelayed) {
  RtpPacketReceived packet;
  EXPECT_TRUE(packet.Parse(kPacketWithTO, sizeof(kPacketWithTO)));
  EXPECT_EQ(kPayloadType, packet.PayloadType());
  EXPECT_EQ(kSeqNum, packet.SequenceNumber());
  EXPECT_EQ(kTimestamp, packet.Timestamp());
  EXPECT_EQ(kSsrc, packet.Ssrc());

  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register(kRtpExtensionTransmissionTimeOffset,
                      kTransmissionOffsetExtensionId);

  int32_t time_offset;
  EXPECT_FALSE(packet.GetExtension<TransmissionOffset>(&time_offset));
  packet.IdentifyExtensions(extensions);
  EXPECT_TRUE(packet.GetExtension<TransmissionOffset>(&time_offset));
  EXPECT_EQ(kTimeOffset, time_offset);
  EXPECT_EQ(0u, packet.payload_size());
  EXPECT_EQ(0u, packet.padding_size());
}

}  // namespace webrtc
