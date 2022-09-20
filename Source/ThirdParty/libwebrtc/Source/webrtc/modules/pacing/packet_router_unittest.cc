/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/packet_router.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "api/units/time_delta.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "rtc_base/checks.h"
#include "rtc_base/fake_clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

// TODO(eladalon): Restructure and/or replace the existing monolithic tests
// (only some of the test are monolithic) according to the new
// guidelines - small tests for one thing at a time.
// (I'm not removing any tests during CL, so as to demonstrate no regressions.)

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Field;
using ::testing::Gt;
using ::testing::Le;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;

constexpr int kProbeMinProbes = 5;
constexpr int kProbeMinBytes = 1000;

}  // namespace

class PacketRouterTest : public ::testing::Test {
 public:
  PacketRouterTest() {
    extension_manager.Register<TransportSequenceNumber>(/*id=*/1);
  }

 protected:
  std::unique_ptr<RtpPacketToSend> BuildRtpPacket(uint32_t ssrc) {
    std::unique_ptr<RtpPacketToSend> packet =
        std::make_unique<RtpPacketToSend>(&extension_manager);
    packet->SetSsrc(ssrc);
    return packet;
  }

  PacketRouter packet_router_;
  RtpHeaderExtensionMap extension_manager;
};

TEST_F(PacketRouterTest, Sanity_NoModuleRegistered_GeneratePadding) {
  constexpr DataSize bytes = DataSize::Bytes(300);
  const PacedPacketInfo paced_info(1, kProbeMinProbes, kProbeMinBytes);

  EXPECT_TRUE(packet_router_.GeneratePadding(bytes).empty());
}


TEST_F(PacketRouterTest, Sanity_NoModuleRegistered_SendRemb) {
  const std::vector<uint32_t> ssrcs = {1, 2, 3};
  constexpr uint32_t bitrate_bps = 10000;
  // Expect not to crash
  packet_router_.SendRemb(bitrate_bps, ssrcs);
}

TEST_F(PacketRouterTest, Sanity_NoModuleRegistered_SendTransportFeedback) {
  std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback;
  feedback.push_back(std::make_unique<rtcp::TransportFeedback>());
  // Expect not to crash
  packet_router_.SendCombinedRtcpPacket(std::move(feedback));
}

TEST_F(PacketRouterTest, GeneratePaddingPrioritizesRtx) {
  // Two RTP modules. The first (prioritized due to rtx) isn't sending media so
  // should not be called.
  const uint16_t kSsrc1 = 1234;
  const uint16_t kSsrc2 = 4567;

  NiceMock<MockRtpRtcpInterface> rtp_1;
  ON_CALL(rtp_1, RtxSendStatus()).WillByDefault(Return(kRtxRedundantPayloads));
  ON_CALL(rtp_1, SSRC()).WillByDefault(Return(kSsrc1));
  ON_CALL(rtp_1, SupportsPadding).WillByDefault(Return(false));

  NiceMock<MockRtpRtcpInterface> rtp_2;
  ON_CALL(rtp_2, RtxSendStatus()).WillByDefault(Return(kRtxOff));
  ON_CALL(rtp_2, SSRC()).WillByDefault(Return(kSsrc2));
  ON_CALL(rtp_2, SupportsPadding).WillByDefault(Return(true));

  packet_router_.AddSendRtpModule(&rtp_1, false);
  packet_router_.AddSendRtpModule(&rtp_2, false);

  const size_t kPaddingSize = 123;
  const size_t kExpectedPaddingPackets = 1;
  EXPECT_CALL(rtp_1, GeneratePadding(_)).Times(0);
  EXPECT_CALL(rtp_2, GeneratePadding(kPaddingSize))
      .WillOnce([&](size_t padding_size) {
        return std::vector<std::unique_ptr<RtpPacketToSend>>(
            kExpectedPaddingPackets);
      });
  auto generated_padding =
      packet_router_.GeneratePadding(DataSize::Bytes(kPaddingSize));
  EXPECT_EQ(generated_padding.size(), kExpectedPaddingPackets);

  packet_router_.RemoveSendRtpModule(&rtp_1);
  packet_router_.RemoveSendRtpModule(&rtp_2);
}

TEST_F(PacketRouterTest, GeneratePaddingPrioritizesVideo) {
  // Two RTP modules. Neither support RTX, both support padding,
  // but the first one is for audio and second for video.
  const uint16_t kSsrc1 = 1234;
  const uint16_t kSsrc2 = 4567;
  const size_t kPaddingSize = 123;
  const size_t kExpectedPaddingPackets = 1;

  auto generate_padding = [&](size_t padding_size) {
    return std::vector<std::unique_ptr<RtpPacketToSend>>(
        kExpectedPaddingPackets);
  };

  NiceMock<MockRtpRtcpInterface> audio_module;
  ON_CALL(audio_module, RtxSendStatus()).WillByDefault(Return(kRtxOff));
  ON_CALL(audio_module, SSRC()).WillByDefault(Return(kSsrc1));
  ON_CALL(audio_module, SupportsPadding).WillByDefault(Return(true));
  ON_CALL(audio_module, IsAudioConfigured).WillByDefault(Return(true));

  NiceMock<MockRtpRtcpInterface> video_module;
  ON_CALL(video_module, RtxSendStatus()).WillByDefault(Return(kRtxOff));
  ON_CALL(video_module, SSRC()).WillByDefault(Return(kSsrc2));
  ON_CALL(video_module, SupportsPadding).WillByDefault(Return(true));
  ON_CALL(video_module, IsAudioConfigured).WillByDefault(Return(false));

  // First add only the audio module. Since this is the only choice we have,
  // padding should be sent on the audio ssrc.
  packet_router_.AddSendRtpModule(&audio_module, false);
  EXPECT_CALL(audio_module, GeneratePadding(kPaddingSize))
      .WillOnce(generate_padding);
  packet_router_.GeneratePadding(DataSize::Bytes(kPaddingSize));

  // Add the video module, this should now be prioritized since we cannot
  // guarantee that audio packets will be included in the BWE.
  packet_router_.AddSendRtpModule(&video_module, false);
  EXPECT_CALL(audio_module, GeneratePadding).Times(0);
  EXPECT_CALL(video_module, GeneratePadding(kPaddingSize))
      .WillOnce(generate_padding);
  packet_router_.GeneratePadding(DataSize::Bytes(kPaddingSize));

  // Remove and the add audio module again. Module order shouldn't matter;
  // video should still be prioritized.
  packet_router_.RemoveSendRtpModule(&audio_module);
  packet_router_.AddSendRtpModule(&audio_module, false);
  EXPECT_CALL(audio_module, GeneratePadding).Times(0);
  EXPECT_CALL(video_module, GeneratePadding(kPaddingSize))
      .WillOnce(generate_padding);
  packet_router_.GeneratePadding(DataSize::Bytes(kPaddingSize));

  // Remove and the video module, we should fall back to padding on the
  // audio module again.
  packet_router_.RemoveSendRtpModule(&video_module);
  EXPECT_CALL(audio_module, GeneratePadding(kPaddingSize))
      .WillOnce(generate_padding);
  packet_router_.GeneratePadding(DataSize::Bytes(kPaddingSize));

  packet_router_.RemoveSendRtpModule(&audio_module);
}

TEST_F(PacketRouterTest, PadsOnLastActiveMediaStream) {
  const uint16_t kSsrc1 = 1234;
  const uint16_t kSsrc2 = 4567;
  const uint16_t kSsrc3 = 8901;

  // First two rtp modules send media and have rtx.
  NiceMock<MockRtpRtcpInterface> rtp_1;
  EXPECT_CALL(rtp_1, SSRC()).WillRepeatedly(Return(kSsrc1));
  EXPECT_CALL(rtp_1, SupportsPadding).WillRepeatedly(Return(true));
  EXPECT_CALL(rtp_1, SupportsRtxPayloadPadding).WillRepeatedly(Return(true));
  EXPECT_CALL(rtp_1, TrySendPacket).WillRepeatedly(Return(false));
  EXPECT_CALL(
      rtp_1,
      TrySendPacket(
          ::testing::Pointee(Property(&RtpPacketToSend::Ssrc, kSsrc1)), _))
      .WillRepeatedly(Return(true));

  NiceMock<MockRtpRtcpInterface> rtp_2;
  EXPECT_CALL(rtp_2, SSRC()).WillRepeatedly(Return(kSsrc2));
  EXPECT_CALL(rtp_2, SupportsPadding).WillRepeatedly(Return(true));
  EXPECT_CALL(rtp_2, SupportsRtxPayloadPadding).WillRepeatedly(Return(true));
  EXPECT_CALL(rtp_2, TrySendPacket).WillRepeatedly(Return(false));
  EXPECT_CALL(
      rtp_2,
      TrySendPacket(
          ::testing::Pointee(Property(&RtpPacketToSend::Ssrc, kSsrc2)), _))
      .WillRepeatedly(Return(true));

  // Third module is sending media, but does not support rtx.
  NiceMock<MockRtpRtcpInterface> rtp_3;
  EXPECT_CALL(rtp_3, SSRC()).WillRepeatedly(Return(kSsrc3));
  EXPECT_CALL(rtp_3, SupportsPadding).WillRepeatedly(Return(true));
  EXPECT_CALL(rtp_3, SupportsRtxPayloadPadding).WillRepeatedly(Return(false));
  EXPECT_CALL(rtp_3, TrySendPacket).WillRepeatedly(Return(false));
  EXPECT_CALL(
      rtp_3,
      TrySendPacket(
          ::testing::Pointee(Property(&RtpPacketToSend::Ssrc, kSsrc3)), _))
      .WillRepeatedly(Return(true));

  packet_router_.AddSendRtpModule(&rtp_1, false);
  packet_router_.AddSendRtpModule(&rtp_2, false);
  packet_router_.AddSendRtpModule(&rtp_3, false);

  const size_t kPaddingBytes = 100;

  // Initially, padding will be sent on last added rtp module that sends media
  // and supports rtx.
  EXPECT_CALL(rtp_2, GeneratePadding(kPaddingBytes))
      .Times(1)
      .WillOnce([&](size_t target_size_bytes) {
        std::vector<std::unique_ptr<RtpPacketToSend>> packets;
        packets.push_back(BuildRtpPacket(kSsrc2));
        return packets;
      });
  packet_router_.GeneratePadding(DataSize::Bytes(kPaddingBytes));

  // Send media on first module. Padding should be sent on that module.
  packet_router_.SendPacket(BuildRtpPacket(kSsrc1), PacedPacketInfo());

  EXPECT_CALL(rtp_1, GeneratePadding(kPaddingBytes))
      .Times(1)
      .WillOnce([&](size_t target_size_bytes) {
        std::vector<std::unique_ptr<RtpPacketToSend>> packets;
        packets.push_back(BuildRtpPacket(kSsrc1));
        return packets;
      });
  packet_router_.GeneratePadding(DataSize::Bytes(kPaddingBytes));

  // Send media on second module. Padding should be sent there.
  packet_router_.SendPacket(BuildRtpPacket(kSsrc2), PacedPacketInfo());

  // If the last active module is removed, and no module sends media before
  // the next padding request, and arbitrary module will be selected.
  packet_router_.RemoveSendRtpModule(&rtp_2);

  // Send on and then remove all remaining modules.
  RtpRtcpInterface* last_send_module;
  EXPECT_CALL(rtp_1, GeneratePadding(kPaddingBytes))
      .Times(1)
      .WillOnce([&](size_t target_size_bytes) {
        last_send_module = &rtp_1;
        std::vector<std::unique_ptr<RtpPacketToSend>> packets;
        packets.push_back(BuildRtpPacket(kSsrc1));
        return packets;
      });
  EXPECT_CALL(rtp_3, GeneratePadding(kPaddingBytes))
      .Times(1)
      .WillOnce([&](size_t target_size_bytes) {
        last_send_module = &rtp_3;
        std::vector<std::unique_ptr<RtpPacketToSend>> packets;
        packets.push_back(BuildRtpPacket(kSsrc3));
        return packets;
      });

  for (int i = 0; i < 2; ++i) {
    last_send_module = nullptr;
    packet_router_.GeneratePadding(DataSize::Bytes(kPaddingBytes));
    EXPECT_NE(last_send_module, nullptr);
    packet_router_.RemoveSendRtpModule(last_send_module);
  }
}

TEST_F(PacketRouterTest, AllocatesTransportSequenceNumbers) {
  const uint16_t kStartSeq = 0xFFF0;
  const size_t kNumPackets = 32;
  const uint16_t kSsrc1 = 1234;

  PacketRouter packet_router(kStartSeq - 1);
  NiceMock<MockRtpRtcpInterface> rtp_1;
  EXPECT_CALL(rtp_1, SSRC()).WillRepeatedly(Return(kSsrc1));
  EXPECT_CALL(rtp_1, TrySendPacket).WillRepeatedly(Return(true));
  packet_router.AddSendRtpModule(&rtp_1, false);

  for (size_t i = 0; i < kNumPackets; ++i) {
    auto packet = BuildRtpPacket(kSsrc1);
    EXPECT_TRUE(packet->ReserveExtension<TransportSequenceNumber>());
    packet_router.SendPacket(std::move(packet), PacedPacketInfo());
    uint32_t expected_unwrapped_seq = static_cast<uint32_t>(kStartSeq) + i;
    EXPECT_EQ(static_cast<uint16_t>(expected_unwrapped_seq & 0xFFFF),
              packet_router.CurrentTransportSequenceNumber());
  }

  packet_router.RemoveSendRtpModule(&rtp_1);
}

TEST_F(PacketRouterTest, SendTransportFeedback) {
  NiceMock<MockRtpRtcpInterface> rtp_1;
  NiceMock<MockRtpRtcpInterface> rtp_2;

  ON_CALL(rtp_1, RTCP()).WillByDefault(Return(RtcpMode::kCompound));
  ON_CALL(rtp_2, RTCP()).WillByDefault(Return(RtcpMode::kCompound));

  packet_router_.AddSendRtpModule(&rtp_1, false);
  packet_router_.AddReceiveRtpModule(&rtp_2, false);

  std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback;
  feedback.push_back(std::make_unique<rtcp::TransportFeedback>());
  EXPECT_CALL(rtp_1, SendCombinedRtcpPacket);
  packet_router_.SendCombinedRtcpPacket(std::move(feedback));
  packet_router_.RemoveSendRtpModule(&rtp_1);
  EXPECT_CALL(rtp_2, SendCombinedRtcpPacket);
  std::vector<std::unique_ptr<rtcp::RtcpPacket>> new_feedback;
  new_feedback.push_back(std::make_unique<rtcp::TransportFeedback>());
  packet_router_.SendCombinedRtcpPacket(std::move(new_feedback));
  packet_router_.RemoveReceiveRtpModule(&rtp_2);
}

TEST_F(PacketRouterTest, SendPacketWithoutTransportSequenceNumbers) {
  const uint16_t kSsrc1 = 1234;
  NiceMock<MockRtpRtcpInterface> rtp_1;
  ON_CALL(rtp_1, SendingMedia).WillByDefault(Return(true));
  ON_CALL(rtp_1, SSRC).WillByDefault(Return(kSsrc1));
  packet_router_.AddSendRtpModule(&rtp_1, false);

  // Send a packet without TransportSequenceNumber extension registered,
  // packets sent should not have the extension set.
  RtpHeaderExtensionMap extension_manager;
  auto packet = std::make_unique<RtpPacketToSend>(&extension_manager);
  packet->SetSsrc(kSsrc1);
  EXPECT_CALL(
      rtp_1,
      TrySendPacket(
          Property(&RtpPacketToSend::HasExtension<TransportSequenceNumber>,
                   false),
          _))
      .WillOnce(Return(true));
  packet_router_.SendPacket(std::move(packet), PacedPacketInfo());

  packet_router_.RemoveSendRtpModule(&rtp_1);
}

TEST_F(PacketRouterTest, SendPacketAssignsTransportSequenceNumbers) {
  NiceMock<MockRtpRtcpInterface> rtp_1;
  NiceMock<MockRtpRtcpInterface> rtp_2;

  const uint16_t kSsrc1 = 1234;
  const uint16_t kSsrc2 = 2345;

  ON_CALL(rtp_1, SSRC).WillByDefault(Return(kSsrc1));
  ON_CALL(rtp_2, SSRC).WillByDefault(Return(kSsrc2));

  packet_router_.AddSendRtpModule(&rtp_1, false);
  packet_router_.AddSendRtpModule(&rtp_2, false);

  // Transport sequence numbers start at 1, for historical reasons.
  uint16_t transport_sequence_number = 1;

  auto packet = BuildRtpPacket(kSsrc1);
  EXPECT_TRUE(packet->ReserveExtension<TransportSequenceNumber>());
  EXPECT_CALL(
      rtp_1,
      TrySendPacket(
          Property(&RtpPacketToSend::GetExtension<TransportSequenceNumber>,
                   transport_sequence_number),
          _))
      .WillOnce(Return(true));
  packet_router_.SendPacket(std::move(packet), PacedPacketInfo());

  ++transport_sequence_number;
  packet = BuildRtpPacket(kSsrc2);
  EXPECT_TRUE(packet->ReserveExtension<TransportSequenceNumber>());

  EXPECT_CALL(
      rtp_2,
      TrySendPacket(
          Property(&RtpPacketToSend::GetExtension<TransportSequenceNumber>,
                   transport_sequence_number),
          _))
      .WillOnce(Return(true));
  packet_router_.SendPacket(std::move(packet), PacedPacketInfo());

  packet_router_.RemoveSendRtpModule(&rtp_1);
  packet_router_.RemoveSendRtpModule(&rtp_2);
}

TEST_F(PacketRouterTest, DoesNotIncrementTransportSequenceNumberOnSendFailure) {
  NiceMock<MockRtpRtcpInterface> rtp;
  constexpr uint32_t kSsrc = 1234;
  ON_CALL(rtp, SSRC).WillByDefault(Return(kSsrc));
  packet_router_.AddSendRtpModule(&rtp, false);

  // Transport sequence numbers start at 1, for historical reasons.
  const uint16_t kStartTransportSequenceNumber = 1;

  // Build and send a packet - it should be assigned the start sequence number.
  // Return failure status code to make sure sequence number is not incremented.
  auto packet = BuildRtpPacket(kSsrc);
  EXPECT_TRUE(packet->ReserveExtension<TransportSequenceNumber>());
  EXPECT_CALL(
      rtp, TrySendPacket(
               Property(&RtpPacketToSend::GetExtension<TransportSequenceNumber>,
                        kStartTransportSequenceNumber),
               _))
      .WillOnce(Return(false));
  packet_router_.SendPacket(std::move(packet), PacedPacketInfo());

  // Send another packet, verify transport sequence number is still at the
  // start state.
  packet = BuildRtpPacket(kSsrc);
  EXPECT_TRUE(packet->ReserveExtension<TransportSequenceNumber>());

  EXPECT_CALL(
      rtp, TrySendPacket(
               Property(&RtpPacketToSend::GetExtension<TransportSequenceNumber>,
                        kStartTransportSequenceNumber),
               _))
      .WillOnce(Return(true));
  packet_router_.SendPacket(std::move(packet), PacedPacketInfo());

  packet_router_.RemoveSendRtpModule(&rtp);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
using PacketRouterDeathTest = PacketRouterTest;
TEST_F(PacketRouterDeathTest, DoubleRegistrationOfSendModuleDisallowed) {
  NiceMock<MockRtpRtcpInterface> module;

  constexpr bool remb_candidate = false;  // Value irrelevant.
  packet_router_.AddSendRtpModule(&module, remb_candidate);
  EXPECT_DEATH(packet_router_.AddSendRtpModule(&module, remb_candidate), "");

  // Test tear-down
  packet_router_.RemoveSendRtpModule(&module);
}

TEST_F(PacketRouterDeathTest, DoubleRegistrationOfReceiveModuleDisallowed) {
  NiceMock<MockRtpRtcpInterface> module;

  constexpr bool remb_candidate = false;  // Value irrelevant.
  packet_router_.AddReceiveRtpModule(&module, remb_candidate);
  EXPECT_DEATH(packet_router_.AddReceiveRtpModule(&module, remb_candidate), "");

  // Test tear-down
  packet_router_.RemoveReceiveRtpModule(&module);
}

TEST_F(PacketRouterDeathTest, RemovalOfNeverAddedSendModuleDisallowed) {
  NiceMock<MockRtpRtcpInterface> module;

  EXPECT_DEATH(packet_router_.RemoveSendRtpModule(&module), "");
}

TEST_F(PacketRouterDeathTest, RemovalOfNeverAddedReceiveModuleDisallowed) {
  NiceMock<MockRtpRtcpInterface> module;

  EXPECT_DEATH(packet_router_.RemoveReceiveRtpModule(&module), "");
}
#endif  // RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST(PacketRouterRembTest, ChangeSendRtpModuleChangeRembSender) {
  rtc::ScopedFakeClock clock;
  NiceMock<MockRtpRtcpInterface> rtp_send;
  NiceMock<MockRtpRtcpInterface> rtp_recv;
  PacketRouter packet_router;
  packet_router.AddSendRtpModule(&rtp_send, true);
  packet_router.AddReceiveRtpModule(&rtp_recv, true);

  uint32_t bitrate_estimate = 456;
  std::vector<uint32_t> ssrcs = {1234, 5678};

  EXPECT_CALL(rtp_send, SetRemb(bitrate_estimate, ssrcs));
  packet_router.SendRemb(bitrate_estimate, ssrcs);

  // Remove the sending module -> should get remb on the second module.
  packet_router.RemoveSendRtpModule(&rtp_send);

  EXPECT_CALL(rtp_recv, SetRemb(bitrate_estimate, ssrcs));
  packet_router.SendRemb(bitrate_estimate, ssrcs);

  packet_router.RemoveReceiveRtpModule(&rtp_recv);
}

// Only register receiving modules and make sure we fallback to trigger a REMB
// packet on this one.
TEST(PacketRouterRembTest, NoSendingRtpModule) {
  rtc::ScopedFakeClock clock;
  NiceMock<MockRtpRtcpInterface> rtp;
  PacketRouter packet_router;

  packet_router.AddReceiveRtpModule(&rtp, true);

  uint32_t bitrate_estimate = 456;
  const std::vector<uint32_t> ssrcs = {1234};

  EXPECT_CALL(rtp, SetRemb(bitrate_estimate, ssrcs));
  packet_router.SendRemb(bitrate_estimate, ssrcs);

  // Lower the estimate to trigger a new packet REMB packet.
  EXPECT_CALL(rtp, SetRemb(bitrate_estimate, ssrcs));
  packet_router.SendRemb(bitrate_estimate, ssrcs);

  EXPECT_CALL(rtp, UnsetRemb());
  packet_router.RemoveReceiveRtpModule(&rtp);
}

TEST(PacketRouterRembTest, NonCandidateSendRtpModuleNotUsedForRemb) {
  rtc::ScopedFakeClock clock;
  PacketRouter packet_router;
  NiceMock<MockRtpRtcpInterface> module;

  constexpr bool remb_candidate = false;

  packet_router.AddSendRtpModule(&module, remb_candidate);

  constexpr uint32_t bitrate_estimate = 456;
  const std::vector<uint32_t> ssrcs = {1234};
  EXPECT_CALL(module, SetRemb(_, _)).Times(0);
  packet_router.SendRemb(bitrate_estimate, ssrcs);

  // Test tear-down
  packet_router.RemoveSendRtpModule(&module);
}

TEST(PacketRouterRembTest, CandidateSendRtpModuleUsedForRemb) {
  rtc::ScopedFakeClock clock;
  PacketRouter packet_router;
  NiceMock<MockRtpRtcpInterface> module;

  constexpr bool remb_candidate = true;

  packet_router.AddSendRtpModule(&module, remb_candidate);

  constexpr uint32_t bitrate_estimate = 456;
  const std::vector<uint32_t> ssrcs = {1234};
  EXPECT_CALL(module, SetRemb(bitrate_estimate, ssrcs));
  packet_router.SendRemb(bitrate_estimate, ssrcs);

  // Test tear-down
  packet_router.RemoveSendRtpModule(&module);
}

TEST(PacketRouterRembTest, NonCandidateReceiveRtpModuleNotUsedForRemb) {
  rtc::ScopedFakeClock clock;
  PacketRouter packet_router;
  NiceMock<MockRtpRtcpInterface> module;

  constexpr bool remb_candidate = false;

  packet_router.AddReceiveRtpModule(&module, remb_candidate);

  constexpr uint32_t bitrate_estimate = 456;
  const std::vector<uint32_t> ssrcs = {1234};
  EXPECT_CALL(module, SetRemb(_, _)).Times(0);
  packet_router.SendRemb(bitrate_estimate, ssrcs);

  // Test tear-down
  packet_router.RemoveReceiveRtpModule(&module);
}

TEST(PacketRouterRembTest, CandidateReceiveRtpModuleUsedForRemb) {
  rtc::ScopedFakeClock clock;
  PacketRouter packet_router;
  NiceMock<MockRtpRtcpInterface> module;

  constexpr bool remb_candidate = true;

  packet_router.AddReceiveRtpModule(&module, remb_candidate);

  constexpr uint32_t bitrate_estimate = 456;
  const std::vector<uint32_t> ssrcs = {1234};
  EXPECT_CALL(module, SetRemb(bitrate_estimate, ssrcs));
  packet_router.SendRemb(bitrate_estimate, ssrcs);

  // Test tear-down
  packet_router.RemoveReceiveRtpModule(&module);
}

TEST(PacketRouterRembTest,
     SendCandidatePreferredOverReceiveCandidate_SendModuleAddedFirst) {
  rtc::ScopedFakeClock clock;
  PacketRouter packet_router;
  NiceMock<MockRtpRtcpInterface> send_module;
  NiceMock<MockRtpRtcpInterface> receive_module;

  constexpr bool remb_candidate = true;

  // Send module added - activated.
  packet_router.AddSendRtpModule(&send_module, remb_candidate);

  // Receive module added - the send module remains the active one.
  packet_router.AddReceiveRtpModule(&receive_module, remb_candidate);

  constexpr uint32_t bitrate_estimate = 456;
  const std::vector<uint32_t> ssrcs = {1234};
  EXPECT_CALL(send_module, SetRemb(bitrate_estimate, ssrcs));
  EXPECT_CALL(receive_module, SetRemb(_, _)).Times(0);

  packet_router.SendRemb(bitrate_estimate, ssrcs);

  // Test tear-down
  packet_router.RemoveReceiveRtpModule(&receive_module);
  packet_router.RemoveSendRtpModule(&send_module);
}

TEST(PacketRouterRembTest,
     SendCandidatePreferredOverReceiveCandidate_ReceiveModuleAddedFirst) {
  rtc::ScopedFakeClock clock;
  PacketRouter packet_router;
  NiceMock<MockRtpRtcpInterface> send_module;
  NiceMock<MockRtpRtcpInterface> receive_module;

  constexpr bool remb_candidate = true;

  // Receive module added - activated.
  packet_router.AddReceiveRtpModule(&receive_module, remb_candidate);

  // Send module added - replaces receive module as active.
  packet_router.AddSendRtpModule(&send_module, remb_candidate);

  constexpr uint32_t bitrate_estimate = 456;
  const std::vector<uint32_t> ssrcs = {1234};
  EXPECT_CALL(send_module, SetRemb(bitrate_estimate, ssrcs));
  EXPECT_CALL(receive_module, SetRemb(_, _)).Times(0);

  clock.AdvanceTime(TimeDelta::Millis(1000));
  packet_router.SendRemb(bitrate_estimate, ssrcs);

  // Test tear-down
  packet_router.RemoveReceiveRtpModule(&receive_module);
  packet_router.RemoveSendRtpModule(&send_module);
}

TEST(PacketRouterRembTest, ReceiveModuleTakesOverWhenLastSendModuleRemoved) {
  rtc::ScopedFakeClock clock;
  PacketRouter packet_router;
  NiceMock<MockRtpRtcpInterface> send_module;
  NiceMock<MockRtpRtcpInterface> receive_module;

  constexpr bool remb_candidate = true;

  // Send module active, receive module inactive.
  packet_router.AddSendRtpModule(&send_module, remb_candidate);
  packet_router.AddReceiveRtpModule(&receive_module, remb_candidate);

  // Send module removed - receive module becomes active.
  packet_router.RemoveSendRtpModule(&send_module);
  constexpr uint32_t bitrate_estimate = 456;
  const std::vector<uint32_t> ssrcs = {1234};
  EXPECT_CALL(send_module, SetRemb(_, _)).Times(0);
  EXPECT_CALL(receive_module, SetRemb(bitrate_estimate, ssrcs));
  packet_router.SendRemb(bitrate_estimate, ssrcs);

  // Test tear-down
  packet_router.RemoveReceiveRtpModule(&receive_module);
}

}  // namespace webrtc
