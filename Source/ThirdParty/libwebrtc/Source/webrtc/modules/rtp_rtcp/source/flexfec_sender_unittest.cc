/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "webrtc/config.h"
#include "webrtc/modules/rtp_rtcp/include/flexfec_sender.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/fec_test_helper.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

namespace {

using test::fec::AugmentedPacket;
using test::fec::AugmentedPacketGenerator;

constexpr int kFlexfecPayloadType = 123;
constexpr uint32_t kMediaSsrc = 1234;
constexpr uint32_t kFlexfecSsrc = 5678;
const std::vector<RtpExtension> kNoRtpHeaderExtensions;
// Assume a single protected media SSRC.
constexpr size_t kFlexfecMaxHeaderSize = 32;
constexpr size_t kPayloadLength = 50;

constexpr int64_t kInitialSimulatedClockTime = 1;
// These values are deterministically given by the PRNG, due to our fixed seed.
// They should be updated if the PRNG implementation changes.
constexpr uint16_t kDeterministicSequenceNumber = 28732;
constexpr uint32_t kDeterministicTimestamp = 2305613085;

std::unique_ptr<RtpPacketToSend> GenerateSingleFlexfecPacket(
    FlexfecSender* sender) {
  // Parameters selected to generate a single FEC packet.
  FecProtectionParams params;
  params.fec_rate = 15;
  params.max_fec_frames = 1;
  params.fec_mask_type = kFecMaskRandom;
  constexpr size_t kNumPackets = 4;

  sender->SetFecParameters(params);
  AugmentedPacketGenerator packet_generator(kMediaSsrc);
  packet_generator.NewFrame(kNumPackets);
  for (size_t i = 0; i < kNumPackets; ++i) {
    std::unique_ptr<AugmentedPacket> packet =
        packet_generator.NextPacket(i, kPayloadLength);
    RtpPacketToSend rtp_packet(nullptr);  // No header extensions.
    rtp_packet.Parse(packet->data, packet->length);
    EXPECT_TRUE(sender->AddRtpPacketAndGenerateFec(rtp_packet));
  }
  EXPECT_TRUE(sender->FecAvailable());
  std::vector<std::unique_ptr<RtpPacketToSend>> fec_packets =
      sender->GetFecPackets();
  EXPECT_FALSE(sender->FecAvailable());
  EXPECT_EQ(1U, fec_packets.size());

  return std::move(fec_packets.front());
}

}  // namespace

TEST(FlexfecSenderTest, Ssrc) {
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                       kNoRtpHeaderExtensions, &clock);

  EXPECT_EQ(kFlexfecSsrc, sender.ssrc());
}

TEST(FlexfecSenderTest, NoFecAvailableBeforeMediaAdded) {
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                       kNoRtpHeaderExtensions, &clock);

  EXPECT_FALSE(sender.FecAvailable());
  auto fec_packets = sender.GetFecPackets();
  EXPECT_EQ(0U, fec_packets.size());
}

TEST(FlexfecSenderTest, ProtectOneFrameWithOneFecPacket) {
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                       kNoRtpHeaderExtensions, &clock);
  auto fec_packet = GenerateSingleFlexfecPacket(&sender);

  EXPECT_EQ(kRtpHeaderSize, fec_packet->headers_size());
  EXPECT_FALSE(fec_packet->Marker());
  EXPECT_EQ(kFlexfecPayloadType, fec_packet->PayloadType());
  EXPECT_EQ(kDeterministicSequenceNumber, fec_packet->SequenceNumber());
  EXPECT_EQ(kDeterministicTimestamp, fec_packet->Timestamp());
  EXPECT_EQ(kFlexfecSsrc, fec_packet->Ssrc());
  EXPECT_LE(kPayloadLength, fec_packet->payload_size());
}

TEST(FlexfecSenderTest, ProtectTwoFramesWithOneFecPacket) {
  // FEC parameters selected to generate a single FEC packet per frame.
  FecProtectionParams params;
  params.fec_rate = 15;
  params.max_fec_frames = 2;
  params.fec_mask_type = kFecMaskRandom;
  constexpr size_t kNumFrames = 2;
  constexpr size_t kNumPacketsPerFrame = 2;
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                       kNoRtpHeaderExtensions, &clock);
  sender.SetFecParameters(params);

  AugmentedPacketGenerator packet_generator(kMediaSsrc);
  for (size_t i = 0; i < kNumFrames; ++i) {
    packet_generator.NewFrame(kNumPacketsPerFrame);
    for (size_t j = 0; j < kNumPacketsPerFrame; ++j) {
      std::unique_ptr<AugmentedPacket> packet =
          packet_generator.NextPacket(i, kPayloadLength);
      RtpPacketToSend rtp_packet(nullptr);
      rtp_packet.Parse(packet->data, packet->length);
      EXPECT_TRUE(sender.AddRtpPacketAndGenerateFec(rtp_packet));
    }
  }
  EXPECT_TRUE(sender.FecAvailable());
  std::vector<std::unique_ptr<RtpPacketToSend>> fec_packets =
      sender.GetFecPackets();
  EXPECT_FALSE(sender.FecAvailable());
  ASSERT_EQ(1U, fec_packets.size());

  RtpPacketToSend* fec_packet = fec_packets.front().get();
  EXPECT_EQ(kRtpHeaderSize, fec_packet->headers_size());
  EXPECT_FALSE(fec_packet->Marker());
  EXPECT_EQ(kFlexfecPayloadType, fec_packet->PayloadType());
  EXPECT_EQ(kDeterministicSequenceNumber, fec_packet->SequenceNumber());
  EXPECT_EQ(kDeterministicTimestamp, fec_packet->Timestamp());
  EXPECT_EQ(kFlexfecSsrc, fec_packet->Ssrc());
}

TEST(FlexfecSenderTest, ProtectTwoFramesWithTwoFecPackets) {
  // FEC parameters selected to generate a single FEC packet per frame.
  FecProtectionParams params;
  params.fec_rate = 30;
  params.max_fec_frames = 1;
  params.fec_mask_type = kFecMaskRandom;
  constexpr size_t kNumFrames = 2;
  constexpr size_t kNumPacketsPerFrame = 2;
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                       kNoRtpHeaderExtensions, &clock);
  sender.SetFecParameters(params);

  AugmentedPacketGenerator packet_generator(kMediaSsrc);
  for (size_t i = 0; i < kNumFrames; ++i) {
    packet_generator.NewFrame(kNumPacketsPerFrame);
    for (size_t j = 0; j < kNumPacketsPerFrame; ++j) {
      std::unique_ptr<AugmentedPacket> packet =
          packet_generator.NextPacket(i, kPayloadLength);
      RtpPacketToSend rtp_packet(nullptr);
      rtp_packet.Parse(packet->data, packet->length);
      EXPECT_TRUE(sender.AddRtpPacketAndGenerateFec(rtp_packet));
    }
    EXPECT_TRUE(sender.FecAvailable());
    std::vector<std::unique_ptr<RtpPacketToSend>> fec_packets =
        sender.GetFecPackets();
    EXPECT_FALSE(sender.FecAvailable());
    ASSERT_EQ(1U, fec_packets.size());

    RtpPacketToSend* fec_packet = fec_packets.front().get();
    EXPECT_EQ(kRtpHeaderSize, fec_packet->headers_size());
    EXPECT_FALSE(fec_packet->Marker());
    EXPECT_EQ(kFlexfecPayloadType, fec_packet->PayloadType());
    EXPECT_EQ(static_cast<uint16_t>(kDeterministicSequenceNumber + i),
              fec_packet->SequenceNumber());
    EXPECT_EQ(kDeterministicTimestamp, fec_packet->Timestamp());
    EXPECT_EQ(kFlexfecSsrc, fec_packet->Ssrc());
  }
}

// In the tests, we only consider RTP header extensions that are useful for BWE.
TEST(FlexfecSenderTest, NoRtpHeaderExtensionsForBweByDefault) {
  const std::vector<RtpExtension> kRtpHeaderExtensions{};
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                       kRtpHeaderExtensions, &clock);
  auto fec_packet = GenerateSingleFlexfecPacket(&sender);

  EXPECT_FALSE(fec_packet->HasExtension<AbsoluteSendTime>());
  EXPECT_FALSE(fec_packet->HasExtension<TransmissionOffset>());
  EXPECT_FALSE(fec_packet->HasExtension<TransportSequenceNumber>());
}

TEST(FlexfecSenderTest, RegisterAbsoluteSendTimeRtpHeaderExtension) {
  const std::vector<RtpExtension> kRtpHeaderExtensions{
      {RtpExtension::kAbsSendTimeUri, 1}};
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                       kRtpHeaderExtensions, &clock);
  auto fec_packet = GenerateSingleFlexfecPacket(&sender);

  EXPECT_TRUE(fec_packet->HasExtension<AbsoluteSendTime>());
  EXPECT_FALSE(fec_packet->HasExtension<TransmissionOffset>());
  EXPECT_FALSE(fec_packet->HasExtension<TransportSequenceNumber>());
}

TEST(FlexfecSenderTest, RegisterTransmissionOffsetRtpHeaderExtension) {
  const std::vector<RtpExtension> kRtpHeaderExtensions{
      {RtpExtension::kTimestampOffsetUri, 1}};
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                       kRtpHeaderExtensions, &clock);
  auto fec_packet = GenerateSingleFlexfecPacket(&sender);

  EXPECT_FALSE(fec_packet->HasExtension<AbsoluteSendTime>());
  EXPECT_TRUE(fec_packet->HasExtension<TransmissionOffset>());
  EXPECT_FALSE(fec_packet->HasExtension<TransportSequenceNumber>());
}

TEST(FlexfecSenderTest, RegisterTransportSequenceNumberRtpHeaderExtension) {
  const std::vector<RtpExtension> kRtpHeaderExtensions{
      {RtpExtension::kTransportSequenceNumberUri, 1}};
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                       kRtpHeaderExtensions, &clock);
  auto fec_packet = GenerateSingleFlexfecPacket(&sender);

  EXPECT_FALSE(fec_packet->HasExtension<AbsoluteSendTime>());
  EXPECT_FALSE(fec_packet->HasExtension<TransmissionOffset>());
  EXPECT_TRUE(fec_packet->HasExtension<TransportSequenceNumber>());
}

TEST(FlexfecSenderTest, RegisterAllRtpHeaderExtensionsForBwe) {
  const std::vector<RtpExtension> kRtpHeaderExtensions{
      {RtpExtension::kAbsSendTimeUri, 1},
      {RtpExtension::kTimestampOffsetUri, 2},
      {RtpExtension::kTransportSequenceNumberUri, 3}};
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                       kRtpHeaderExtensions, &clock);
  auto fec_packet = GenerateSingleFlexfecPacket(&sender);

  EXPECT_TRUE(fec_packet->HasExtension<AbsoluteSendTime>());
  EXPECT_TRUE(fec_packet->HasExtension<TransmissionOffset>());
  EXPECT_TRUE(fec_packet->HasExtension<TransportSequenceNumber>());
}

TEST(FlexfecSenderTest, MaxPacketOverhead) {
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc,
                       kNoRtpHeaderExtensions, &clock);

  EXPECT_EQ(kFlexfecMaxHeaderSize, sender.MaxPacketOverhead());
}

}  // namespace webrtc
