/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/flexfec_sender.h"

#include <vector>

#include "api/rtp_parameters.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/fec_test_helper.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/source/rtp_sender.h"
#include "modules/rtp_rtcp/source/rtp_utility.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

using RtpUtility::Word32Align;
using test::fec::AugmentedPacket;
using test::fec::AugmentedPacketGenerator;

constexpr int kFlexfecPayloadType = 123;
constexpr uint32_t kMediaSsrc = 1234;
constexpr uint32_t kFlexfecSsrc = 5678;
const char kNoMid[] = "";
const std::vector<RtpExtension> kNoRtpHeaderExtensions;
const std::vector<RtpExtensionSize> kNoRtpHeaderExtensionSizes;
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

  sender->SetProtectionParameters(params, params);
  AugmentedPacketGenerator packet_generator(kMediaSsrc);
  packet_generator.NewFrame(kNumPackets);
  for (size_t i = 0; i < kNumPackets; ++i) {
    std::unique_ptr<AugmentedPacket> packet =
        packet_generator.NextPacket(i, kPayloadLength);
    RtpPacketToSend rtp_packet(nullptr);  // No header extensions.
    rtp_packet.Parse(packet->data);
    sender->AddPacketAndGenerateFec(rtp_packet);
  }
  std::vector<std::unique_ptr<RtpPacketToSend>> fec_packets =
      sender->GetFecPackets();
  EXPECT_EQ(1U, fec_packets.size());
  EXPECT_TRUE(sender->GetFecPackets().empty());

  return std::move(fec_packets.front());
}

}  // namespace

TEST(FlexfecSenderTest, Ssrc) {
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc, kNoMid,
                       kNoRtpHeaderExtensions, kNoRtpHeaderExtensionSizes,
                       nullptr /* rtp_state */, &clock);

  EXPECT_EQ(kFlexfecSsrc, sender.FecSsrc());
}

TEST(FlexfecSenderTest, NoFecAvailableBeforeMediaAdded) {
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc, kNoMid,
                       kNoRtpHeaderExtensions, kNoRtpHeaderExtensionSizes,
                       nullptr /* rtp_state */, &clock);

  EXPECT_TRUE(sender.GetFecPackets().empty());
}

TEST(FlexfecSenderTest, ProtectOneFrameWithOneFecPacket) {
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc, kNoMid,
                       kNoRtpHeaderExtensions, kNoRtpHeaderExtensionSizes,
                       nullptr /* rtp_state */, &clock);
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
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc, kNoMid,
                       kNoRtpHeaderExtensions, kNoRtpHeaderExtensionSizes,
                       nullptr /* rtp_state */, &clock);
  sender.SetProtectionParameters(params, params);

  AugmentedPacketGenerator packet_generator(kMediaSsrc);
  for (size_t i = 0; i < kNumFrames; ++i) {
    packet_generator.NewFrame(kNumPacketsPerFrame);
    for (size_t j = 0; j < kNumPacketsPerFrame; ++j) {
      std::unique_ptr<AugmentedPacket> packet =
          packet_generator.NextPacket(i, kPayloadLength);
      RtpPacketToSend rtp_packet(nullptr);
      rtp_packet.Parse(packet->data);
      sender.AddPacketAndGenerateFec(rtp_packet);
    }
  }
  std::vector<std::unique_ptr<RtpPacketToSend>> fec_packets =
      sender.GetFecPackets();
  ASSERT_EQ(1U, fec_packets.size());
  EXPECT_TRUE(sender.GetFecPackets().empty());

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
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc, kNoMid,
                       kNoRtpHeaderExtensions, kNoRtpHeaderExtensionSizes,
                       nullptr /* rtp_state */, &clock);
  sender.SetProtectionParameters(params, params);

  AugmentedPacketGenerator packet_generator(kMediaSsrc);
  for (size_t i = 0; i < kNumFrames; ++i) {
    packet_generator.NewFrame(kNumPacketsPerFrame);
    for (size_t j = 0; j < kNumPacketsPerFrame; ++j) {
      std::unique_ptr<AugmentedPacket> packet =
          packet_generator.NextPacket(i, kPayloadLength);
      RtpPacketToSend rtp_packet(nullptr);
      rtp_packet.Parse(packet->data);
      sender.AddPacketAndGenerateFec(rtp_packet);
    }
    std::vector<std::unique_ptr<RtpPacketToSend>> fec_packets =
        sender.GetFecPackets();
    ASSERT_EQ(1U, fec_packets.size());
    EXPECT_TRUE(sender.GetFecPackets().empty());

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
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc, kNoMid,
                       kRtpHeaderExtensions, kNoRtpHeaderExtensionSizes,
                       nullptr /* rtp_state */, &clock);
  auto fec_packet = GenerateSingleFlexfecPacket(&sender);

  EXPECT_FALSE(fec_packet->HasExtension<AbsoluteSendTime>());
  EXPECT_FALSE(fec_packet->HasExtension<TransmissionOffset>());
  EXPECT_FALSE(fec_packet->HasExtension<TransportSequenceNumber>());
}

TEST(FlexfecSenderTest, RegisterAbsoluteSendTimeRtpHeaderExtension) {
  const std::vector<RtpExtension> kRtpHeaderExtensions{
      {RtpExtension::kAbsSendTimeUri, 1}};
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc, kNoMid,
                       kRtpHeaderExtensions, kNoRtpHeaderExtensionSizes,
                       nullptr /* rtp_state */, &clock);
  auto fec_packet = GenerateSingleFlexfecPacket(&sender);

  EXPECT_TRUE(fec_packet->HasExtension<AbsoluteSendTime>());
  EXPECT_FALSE(fec_packet->HasExtension<TransmissionOffset>());
  EXPECT_FALSE(fec_packet->HasExtension<TransportSequenceNumber>());
}

TEST(FlexfecSenderTest, RegisterTransmissionOffsetRtpHeaderExtension) {
  const std::vector<RtpExtension> kRtpHeaderExtensions{
      {RtpExtension::kTimestampOffsetUri, 1}};
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc, kNoMid,
                       kRtpHeaderExtensions, kNoRtpHeaderExtensionSizes,
                       nullptr /* rtp_state */, &clock);
  auto fec_packet = GenerateSingleFlexfecPacket(&sender);

  EXPECT_FALSE(fec_packet->HasExtension<AbsoluteSendTime>());
  EXPECT_TRUE(fec_packet->HasExtension<TransmissionOffset>());
  EXPECT_FALSE(fec_packet->HasExtension<TransportSequenceNumber>());
}

TEST(FlexfecSenderTest, RegisterTransportSequenceNumberRtpHeaderExtension) {
  const std::vector<RtpExtension> kRtpHeaderExtensions{
      {RtpExtension::kTransportSequenceNumberUri, 1}};
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc, kNoMid,
                       kRtpHeaderExtensions, kNoRtpHeaderExtensionSizes,
                       nullptr /* rtp_state */, &clock);
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
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc, kNoMid,
                       kRtpHeaderExtensions, kNoRtpHeaderExtensionSizes,
                       nullptr /* rtp_state */, &clock);
  auto fec_packet = GenerateSingleFlexfecPacket(&sender);

  EXPECT_TRUE(fec_packet->HasExtension<AbsoluteSendTime>());
  EXPECT_TRUE(fec_packet->HasExtension<TransmissionOffset>());
  EXPECT_TRUE(fec_packet->HasExtension<TransportSequenceNumber>());
}

TEST(FlexfecSenderTest, MaxPacketOverhead) {
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc, kNoMid,
                       kNoRtpHeaderExtensions, kNoRtpHeaderExtensionSizes,
                       nullptr /* rtp_state */, &clock);

  EXPECT_EQ(kFlexfecMaxHeaderSize, sender.MaxPacketOverhead());
}

TEST(FlexfecSenderTest, MaxPacketOverheadWithExtensions) {
  const std::vector<RtpExtension> kRtpHeaderExtensions{
      {RtpExtension::kAbsSendTimeUri, 1},
      {RtpExtension::kTimestampOffsetUri, 2},
      {RtpExtension::kTransportSequenceNumberUri, 3}};
  SimulatedClock clock(kInitialSimulatedClockTime);
  const size_t kExtensionHeaderLength = 1;
  const size_t kRtpOneByteHeaderLength = 4;
  const size_t kExtensionsTotalSize =
      Word32Align(kRtpOneByteHeaderLength + kExtensionHeaderLength +
                  AbsoluteSendTime::kValueSizeBytes + kExtensionHeaderLength +
                  TransmissionOffset::kValueSizeBytes + kExtensionHeaderLength +
                  TransportSequenceNumber::kValueSizeBytes);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc, kNoMid,
                       kRtpHeaderExtensions, RTPSender::FecExtensionSizes(),
                       nullptr /* rtp_state */, &clock);

  EXPECT_EQ(kExtensionsTotalSize + kFlexfecMaxHeaderSize,
            sender.MaxPacketOverhead());
}

TEST(FlexfecSenderTest, MidIncludedInPacketsWhenSet) {
  const std::vector<RtpExtension> kRtpHeaderExtensions{
      {RtpExtension::kMidUri, 1}};
  const char kMid[] = "mid";
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc, kMid,
                       kRtpHeaderExtensions, RTPSender::FecExtensionSizes(),
                       nullptr /* rtp_state */, &clock);

  auto fec_packet = GenerateSingleFlexfecPacket(&sender);

  std::string mid;
  ASSERT_TRUE(fec_packet->GetExtension<RtpMid>(&mid));
  EXPECT_EQ(kMid, mid);
}

TEST(FlexfecSenderTest, SetsAndGetsRtpState) {
  RtpState initial_rtp_state;
  initial_rtp_state.sequence_number = 100;
  initial_rtp_state.start_timestamp = 200;
  SimulatedClock clock(kInitialSimulatedClockTime);
  FlexfecSender sender(kFlexfecPayloadType, kFlexfecSsrc, kMediaSsrc, kNoMid,
                       kNoRtpHeaderExtensions, kNoRtpHeaderExtensionSizes,
                       &initial_rtp_state, &clock);

  auto fec_packet = GenerateSingleFlexfecPacket(&sender);
  EXPECT_EQ(initial_rtp_state.sequence_number, fec_packet->SequenceNumber());
  EXPECT_EQ(initial_rtp_state.start_timestamp, fec_packet->Timestamp());

  clock.AdvanceTimeMilliseconds(1000);
  fec_packet = GenerateSingleFlexfecPacket(&sender);
  EXPECT_EQ(initial_rtp_state.sequence_number + 1,
            fec_packet->SequenceNumber());
  EXPECT_EQ(initial_rtp_state.start_timestamp + 1 * kVideoPayloadTypeFrequency,
            fec_packet->Timestamp());

  RtpState updated_rtp_state = sender.GetRtpState().value();
  EXPECT_EQ(initial_rtp_state.sequence_number + 2,
            updated_rtp_state.sequence_number);
  EXPECT_EQ(initial_rtp_state.start_timestamp,
            updated_rtp_state.start_timestamp);
}

}  // namespace webrtc
