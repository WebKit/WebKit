/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_depacketizer_sframe.h"

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/sframe_descriptor.h"
#include "modules/rtp_rtcp/source/sframe_rtp_packet_received.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::ElementsAreArray;

// Compose a wire-format SFrame descriptor byte from S/E/T flags using the
// bit indices declared on `SframeDescriptor`.
uint8_t MakeDescriptorByte(bool s, bool e, bool t) {
  std::bitset<8> bits;
  bits.set(SframeDescriptor::kSBit, s);
  bits.set(SframeDescriptor::kEBit, e);
  bits.set(SframeDescriptor::kTBit, t);
  return static_cast<uint8_t>(bits.to_ulong());
}

RtpPacketReceived MakeReceivedPacket(uint8_t descriptor,
                                     std::vector<uint8_t> inner_payload) {
  RtpPacketReceived packet;
  packet.SetSequenceNumber(0x4242);
  packet.SetTimestamp(0xDEADBEEF);
  packet.SetPayloadType(96);
  std::vector<uint8_t> payload;
  payload.reserve(1 + inner_payload.size());
  payload.push_back(descriptor);
  payload.insert(payload.end(), inner_payload.begin(), inner_payload.end());
  packet.SetPayload(payload);
  return packet;
}

TEST(ParseSframeRtpPacketOrErrorTest, FailsOnEmptyPayload) {
  RtpPacketReceived packet;
  packet.SetSequenceNumber(1);
  packet.SetTimestamp(2);
  packet.SetPayloadType(96);
  packet.SetPayload({});
  EXPECT_FALSE(ParseSframeRtpPacketOrError(packet).ok());
}

TEST(ParseSframeRtpPacketOrErrorTest, ParsesTBitAsPacketEncryptionLevel) {
  RtpPacketReceived packet = MakeReceivedPacket(
      MakeDescriptorByte(/*s=*/true, /*e=*/true, /*t=*/true), {0xAA});
  auto result = ParseSframeRtpPacketOrError(packet);
  ASSERT_TRUE(result.ok());
  auto parsed = result.MoveValue();
  EXPECT_TRUE(parsed->descriptor().start);
  EXPECT_TRUE(parsed->descriptor().end);
  EXPECT_EQ(parsed->descriptor().encryption_level,
            SframeEncryptionLevel::kPacket);
}

TEST(ParseSframeRtpPacketOrErrorTest, StripsDescriptorByteFromPayload) {
  const std::vector<uint8_t> inner = {0x01, 0x02, 0x03, 0x04, 0x05};
  RtpPacketReceived packet = MakeReceivedPacket(
      MakeDescriptorByte(/*s=*/true, /*e=*/true, /*t=*/false), inner);
  auto result = ParseSframeRtpPacketOrError(packet);
  ASSERT_TRUE(result.ok());
  auto parsed = result.MoveValue();
  EXPECT_THAT(parsed->packet().payload(), ElementsAreArray(inner));
}

TEST(ParseSframeRtpPacketOrErrorTest, AcceptsDescriptorOnlyPayload) {
  RtpPacketReceived packet = MakeReceivedPacket(
      MakeDescriptorByte(/*s=*/true, /*e=*/true, /*t=*/false), {});
  auto result = ParseSframeRtpPacketOrError(packet);
  ASSERT_TRUE(result.ok());
  auto parsed = result.MoveValue();
  EXPECT_EQ(parsed->packet().payload().size(), 0u);
  EXPECT_TRUE(parsed->descriptor().start);
  EXPECT_TRUE(parsed->descriptor().end);
}

TEST(ParseSframeRtpPacketOrErrorTest, IgnoresReservedBits) {
  // Set all five reserved low bits in addition to S=1, E=1, T=0.
  const uint8_t descriptor =
      MakeDescriptorByte(/*s=*/true, /*e=*/true, /*t=*/false) | 0x1F;
  RtpPacketReceived packet = MakeReceivedPacket(descriptor, {0xFF});
  auto result = ParseSframeRtpPacketOrError(packet);
  ASSERT_TRUE(result.ok());
  auto parsed = result.MoveValue();
  EXPECT_TRUE(parsed->descriptor().start);
  EXPECT_TRUE(parsed->descriptor().end);
  EXPECT_EQ(parsed->descriptor().encryption_level,
            SframeEncryptionLevel::kFrame);
}

TEST(ParseSframeRtpPacketOrErrorTest, PreservesRtpHeaderFields) {
  RtpPacketReceived packet = MakeReceivedPacket(
      MakeDescriptorByte(/*s=*/true, /*e=*/true, /*t=*/false), {0xAB, 0xCD});
  auto result = ParseSframeRtpPacketOrError(packet);
  ASSERT_TRUE(result.ok());
  auto parsed = result.MoveValue();
  EXPECT_EQ(parsed->SequenceNumber(), 0x4242);
  EXPECT_EQ(parsed->Timestamp(), 0xDEADBEEFu);
  EXPECT_EQ(parsed->PayloadType(), 96);
}

TEST(ParseSframeRtpPacketOrErrorTest, ParsesAllEightSETCombinations) {
  // Exhaustively walk the 2^3 = 8 (S, E, T) descriptor combinations.
  for (uint8_t bits = 0; bits < 8; ++bits) {
    const bool expect_s = (bits & 0b100) != 0;
    const bool expect_e = (bits & 0b010) != 0;
    const bool expect_t = (bits & 0b001) != 0;
    const uint8_t descriptor = MakeDescriptorByte(expect_s, expect_e, expect_t);
    SCOPED_TRACE(static_cast<int>(descriptor));
    RtpPacketReceived packet = MakeReceivedPacket(descriptor, {0x55});
    auto result = ParseSframeRtpPacketOrError(packet);
    ASSERT_TRUE(result.ok());
    auto parsed = result.MoveValue();
    EXPECT_EQ(parsed->descriptor().start, expect_s);
    EXPECT_EQ(parsed->descriptor().end, expect_e);
    EXPECT_EQ(parsed->descriptor().encryption_level,
              expect_t ? SframeEncryptionLevel::kPacket
                       : SframeEncryptionLevel::kFrame);
  }
}

TEST(ParseSframeRtpPacketOrErrorTest, PreservesLargePayloadBytes) {
  std::vector<uint8_t> inner_payload(1000);
  for (size_t i = 0; i < inner_payload.size(); ++i) {
    inner_payload[i] = static_cast<uint8_t>((i * 17) & 0xFF);
  }
  RtpPacketReceived packet = MakeReceivedPacket(
      MakeDescriptorByte(/*s=*/true, /*e=*/true, /*t=*/false), inner_payload);
  auto result = ParseSframeRtpPacketOrError(packet);
  ASSERT_TRUE(result.ok());
  auto parsed = result.MoveValue();
  EXPECT_THAT(parsed->packet().payload(), ElementsAreArray(inner_payload));
}

TEST(ParseSframeRtpPacketOrErrorTest, PreservesMarkerBit) {
  for (bool marker : {false, true}) {
    SCOPED_TRACE(marker);
    RtpPacketReceived packet = MakeReceivedPacket(
        MakeDescriptorByte(/*s=*/true, /*e=*/true, /*t=*/false), {0xAA});
    packet.SetMarker(marker);
    auto result = ParseSframeRtpPacketOrError(packet);
    ASSERT_TRUE(result.ok());
    auto parsed = result.MoveValue();
    EXPECT_EQ(parsed->packet().Marker(), marker);
  }
}

TEST(ParseSframeRtpPacketOrErrorTest, PreservesSsrc) {
  RtpPacketReceived packet = MakeReceivedPacket(
      MakeDescriptorByte(/*s=*/true, /*e=*/true, /*t=*/false), {0xAA});
  packet.SetSsrc(0xCAFEBABE);
  auto result = ParseSframeRtpPacketOrError(packet);
  ASSERT_TRUE(result.ok());
  auto parsed = result.MoveValue();
  EXPECT_EQ(parsed->packet().Ssrc(), 0xCAFEBABEu);
}

TEST(ParseSframeRtpPacketOrErrorTest, PreservesPadding) {
  // RTP packets with padding must not crash inside the depacketizer and the
  // padding must round-trip through the rebuilt wire buffer.
  RtpPacketReceived packet = MakeReceivedPacket(
      MakeDescriptorByte(/*s=*/true, /*e=*/true, /*t=*/false), {0xAB, 0xCD});
  ASSERT_TRUE(packet.SetPadding(4));
  auto result = ParseSframeRtpPacketOrError(packet);
  ASSERT_TRUE(result.ok());
  auto parsed = result.MoveValue();
  EXPECT_EQ(parsed->packet().padding_size(), 4u);
  EXPECT_THAT(parsed->packet().payload(), ElementsAreArray({0xAB, 0xCD}));
}

TEST(ParseSframeRtpPacketOrErrorTest, ReturnedPacketIsIndependentCopy) {
  // The returned object must own a copy independent of the source packet.
  RtpPacketReceived packet = MakeReceivedPacket(
      MakeDescriptorByte(/*s=*/true, /*e=*/true, /*t=*/false), {0x11, 0x22});
  auto result = ParseSframeRtpPacketOrError(packet);
  ASSERT_TRUE(result.ok());
  auto parsed = result.MoveValue();
  const uint16_t original_seq = parsed->packet().SequenceNumber();
  const uint32_t original_ts = parsed->packet().Timestamp();
  packet.SetSequenceNumber(0xFFFF);
  packet.SetTimestamp(0xFFFFFFFF);
  packet.SetPayload(std::vector<uint8_t>{0x00});
  EXPECT_EQ(parsed->packet().SequenceNumber(), original_seq);
  EXPECT_EQ(parsed->packet().Timestamp(), original_ts);
  EXPECT_THAT(parsed->packet().payload(), ElementsAreArray({0x11, 0x22}));
}

}  // namespace
}  // namespace webrtc
