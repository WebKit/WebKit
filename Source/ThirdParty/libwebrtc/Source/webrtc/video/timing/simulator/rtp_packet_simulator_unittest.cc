/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/rtp_packet_simulator.h"

#include <optional>

#include "api/environment/environment.h"
#include "api/rtp_headers.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/logged_rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "system_wrappers/include/clock.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc::video_timing_simulator {
namespace {

constexpr int kBaseRtpHeaderSize = 12;

using ::testing::Eq;

TEST(RtpPacketSimulatorTest, SimulatesArrivalTimeFromEnvironmentClock) {
  SimulatedClock clock(/*initial_time=*/Timestamp::Seconds(10000));
  Environment env =
      CreateTestEnvironment(CreateTestEnvironmentOptions{.time = &clock});
  RtpPacketSimulator simulator(env);

  LoggedRtpPacket logged_packet(
      /*timestamp=*/Timestamp::Millis(123456789), RTPHeader(),
      /*header_length=*/kBaseRtpHeaderSize,
      /*total_length=*/kBaseRtpHeaderSize);
  RtpPacketSimulator::SimulatedPacket simulated_packet =
      simulator.SimulateRtpPacketReceived(logged_packet);

  EXPECT_EQ(simulated_packet.rtp_packet.arrival_time(),
            Timestamp::Seconds(10000));
}

TEST(RtpPacketSimulatorTest, SimulatesRtpHeader) {
  SimulatedClock clock(/*initial_time=*/Timestamp::Seconds(10000));
  Environment env =
      CreateTestEnvironment(CreateTestEnvironmentOptions{.time = &clock});
  RtpPacketSimulator simulator(env);

  RTPHeader logged_rtp_header;
  logged_rtp_header.markerBit = 1;
  logged_rtp_header.payloadType = 98;
  logged_rtp_header.sequenceNumber = 123;
  logged_rtp_header.timestamp = 5235235;
  logged_rtp_header.ssrc = 83653358;
  LoggedRtpPacket logged_packet(
      /*timestamp=*/Timestamp::Millis(123456789), logged_rtp_header,
      /*header_length=*/kBaseRtpHeaderSize,
      /*total_length=*/kBaseRtpHeaderSize);
  RtpPacketSimulator::SimulatedPacket simulated_packet =
      simulator.SimulateRtpPacketReceived(logged_packet);

  const RtpPacketReceived& rtp_packet = simulated_packet.rtp_packet;
  EXPECT_TRUE(rtp_packet.Marker());
  EXPECT_THAT(rtp_packet.PayloadType(), Eq(98));
  EXPECT_THAT(rtp_packet.SequenceNumber(), Eq(123));
  EXPECT_THAT(rtp_packet.Timestamp(), Eq(5235235));
  EXPECT_THAT(rtp_packet.Ssrc(), Eq(83653358));
}

TEST(RtpPacketSimulatorTest, SimulatesBasicRtpHeaderExtensions) {
  SimulatedClock clock(/*initial_time=*/Timestamp::Seconds(10000));
  Environment env =
      CreateTestEnvironment(CreateTestEnvironmentOptions{.time = &clock});
  RtpPacketSimulator simulator(env);

  RTPHeader logged_rtp_header;
  auto& logged_extension = logged_rtp_header.extension;
  logged_extension.transportSequenceNumber = 99;
  logged_extension.hasTransportSequenceNumber = true;
  logged_extension.transmissionTimeOffset = 887766;
  logged_extension.hasTransmissionTimeOffset = true;
  logged_extension.absoluteSendTime = 11223344;
  logged_extension.hasAbsoluteSendTime = true;
  LoggedRtpPacket logged_packet(
      /*timestamp=*/Timestamp::Millis(123456789), logged_rtp_header,
      /*header_length=*/kBaseRtpHeaderSize,
      /*total_length=*/kBaseRtpHeaderSize);
  RtpPacketSimulator::SimulatedPacket simulated_packet =
      simulator.SimulateRtpPacketReceived(logged_packet);

  const RtpPacketReceived& rtp_packet = simulated_packet.rtp_packet;
  EXPECT_THAT(rtp_packet.GetExtension<TransportSequenceNumber>(), Eq(99));
  EXPECT_THAT(rtp_packet.GetExtension<TransmissionOffset>(), Eq(887766));
  EXPECT_THAT(rtp_packet.GetExtension<AbsoluteSendTime>(), Eq(11223344));
}

// TODO(b/423646186): Add test for dependency descriptor.

TEST(RtpPacketSimulatorTest, SimulatesSizes) {
  SimulatedClock clock(/*initial_time=*/Timestamp::Seconds(10000));
  Environment env =
      CreateTestEnvironment(CreateTestEnvironmentOptions{.time = &clock});
  RtpPacketSimulator simulator(env);

  RTPHeader logged_header;
  logged_header.paddingLength = 100;
  LoggedRtpPacket logged_packet(
      /*timestamp=*/Timestamp::Millis(123456789), logged_header,
      /*header_length=*/kBaseRtpHeaderSize,
      /*total_length=*/1200);
  RtpPacketSimulator::SimulatedPacket simulated_packet =
      simulator.SimulateRtpPacketReceived(logged_packet);

  const RtpPacketReceived& rtp_packet = simulated_packet.rtp_packet;
  EXPECT_THAT(rtp_packet.size(), Eq(1200));
  EXPECT_THAT(rtp_packet.payload_size(), Eq(1088));
  EXPECT_THAT(rtp_packet.padding_size(), Eq(100));
}

TEST(RtpPacketSimulatorTest, DoesNotSetRtxOsnWhenNotLogged) {
  SimulatedClock clock(/*initial_time=*/Timestamp::Seconds(10000));
  Environment env =
      CreateTestEnvironment(CreateTestEnvironmentOptions{.time = &clock});
  RtpPacketSimulator simulator(env);

  RTPHeader logged_rtp_header;
  LoggedRtpPacket logged_packet(
      /*timestamp=*/Timestamp::Millis(123456789), logged_rtp_header,
      /*header_length=*/kBaseRtpHeaderSize,
      /*total_length=*/kBaseRtpHeaderSize);
  logged_packet.rtx_original_sequence_number = std::nullopt;

  RtpPacketSimulator::SimulatedPacket simulated_packet =
      simulator.SimulateRtpPacketReceived(logged_packet);
  EXPECT_FALSE(simulated_packet.has_rtx_osn);
}

TEST(RtpPacketSimulatorTest, SimulatesRtxOsnWhenLogged) {
  SimulatedClock clock(/*initial_time=*/Timestamp::Seconds(10000));
  Environment env =
      CreateTestEnvironment(CreateTestEnvironmentOptions{.time = &clock});
  RtpPacketSimulator simulator(env);

  RTPHeader logged_rtp_header;
  LoggedRtpPacket logged_packet(
      /*timestamp=*/Timestamp::Millis(123456789), logged_rtp_header,
      /*header_length=*/kBaseRtpHeaderSize,
      /*total_length=*/kBaseRtpHeaderSize + kRtpHeaderSize);
  logged_packet.rtx_original_sequence_number = 0xabcd;

  RtpPacketSimulator::SimulatedPacket simulated_packet =
      simulator.SimulateRtpPacketReceived(logged_packet);
  EXPECT_TRUE(simulated_packet.has_rtx_osn);
  ASSERT_EQ(simulated_packet.rtp_packet.payload_size(), kRtpHeaderSize);
  EXPECT_EQ(simulated_packet.rtp_packet.payload()[0], 0xab);
  EXPECT_EQ(simulated_packet.rtp_packet.payload()[1], 0xcd);
}

}  // namespace

}  // namespace webrtc::video_timing_simulator
