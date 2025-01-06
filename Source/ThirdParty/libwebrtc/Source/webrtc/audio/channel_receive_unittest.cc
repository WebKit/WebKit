/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/channel_receive.h"

#include "absl/strings/escaping.h"
#include "api/audio/audio_device.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/crypto/frame_decryptor_interface.h"
#include "api/environment/environment_factory.h"
#include "api/test/mock_frame_transformer.h"
#include "modules/audio_device/include/mock_audio_device.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/ntp_time_util.h"
#include "modules/rtp_rtcp/source/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sdes.h"
#include "modules/rtp_rtcp/source/rtcp_packet/sender_report.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_audio_decoder_factory.h"
#include "test/mock_transport.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace voe {
namespace {

using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::Test;

constexpr uint32_t kLocalSsrc = 1111;
constexpr uint32_t kRemoteSsrc = 2222;
// We run RTP data with 8 kHz PCMA (fixed payload type 8).
constexpr char kPayloadName[] = "PCMA";
constexpr int kPayloadType = 8;
constexpr int kSampleRateHz = 8000;

class ChannelReceiveTest : public Test {
 public:
  ChannelReceiveTest()
      : time_controller_(Timestamp::Seconds(5555)),
        audio_device_module_(test::MockAudioDeviceModule::CreateNice()),
        audio_decoder_factory_(CreateBuiltinAudioDecoderFactory()) {
    ON_CALL(*audio_device_module_, PlayoutDelay).WillByDefault(Return(0));
  }

  std::unique_ptr<ChannelReceiveInterface> CreateTestChannelReceive() {
    CryptoOptions crypto_options;
    auto channel = CreateChannelReceive(
        CreateEnvironment(time_controller_.GetClock()),
        /* neteq_factory= */ nullptr, audio_device_module_.get(), &transport_,
        kLocalSsrc, kRemoteSsrc,
        /* jitter_buffer_max_packets= */ 0,
        /* jitter_buffer_fast_playout= */ false,
        /* jitter_buffer_min_delay_ms= */ 0,
        /* enable_non_sender_rtt= */ false, audio_decoder_factory_,
        /* codec_pair_id= */ std::nullopt,
        /* frame_decryptor_interface= */ nullptr, crypto_options,
        /* frame_transformer= */ nullptr);
    channel->SetReceiveCodecs(
        {{kPayloadType, {kPayloadName, kSampleRateHz, 1}}});
    return channel;
  }

  NtpTime NtpNow() { return time_controller_.GetClock()->CurrentNtpTime(); }

  uint32_t RtpNow() {
    // Note - the "random" offset of this timestamp is zero.
    return rtc::TimeMillis() * 1000 / kSampleRateHz;
  }

  RtpPacketReceived CreateRtpPacket() {
    RtpPacketReceived packet;
    packet.set_arrival_time(time_controller_.GetClock()->CurrentTime());
    packet.SetTimestamp(RtpNow());
    packet.SetSsrc(kLocalSsrc);
    packet.SetPayloadType(kPayloadType);
    // Packet size should be enough to give at least 10 ms of data.
    // For PCMA, that's 80 bytes; this should be enough.
    uint8_t* datapos = packet.SetPayloadSize(100);
    memset(datapos, 0, 100);
    return packet;
  }

  std::vector<uint8_t> CreateRtcpSenderReport() {
    std::vector<uint8_t> packet(1024);
    size_t pos = 0;
    rtcp::SenderReport report;
    report.SetSenderSsrc(kRemoteSsrc);
    report.SetNtp(NtpNow());
    report.SetRtpTimestamp(RtpNow());
    report.SetPacketCount(0);
    report.SetOctetCount(0);
    report.Create(&packet[0], &pos, packet.size(), nullptr);
    // No report blocks.
    packet.resize(pos);
    return packet;
  }

  std::vector<uint8_t> CreateRtcpReceiverReport() {
    rtcp::ReportBlock block;
    block.SetMediaSsrc(kLocalSsrc);
    // Middle 32 bits of the NTP timestamp from received SR
    block.SetLastSr(CompactNtp(NtpNow()));
    block.SetDelayLastSr(0);

    rtcp::ReceiverReport report;
    report.SetSenderSsrc(kRemoteSsrc);
    report.AddReportBlock(block);

    std::vector<uint8_t> packet(1024);
    size_t pos = 0;
    report.Create(&packet[0], &pos, packet.size(), nullptr);
    packet.resize(pos);
    return packet;
  }

  void HandleGeneratedRtcp(ChannelReceiveInterface& /* channel */,
                           rtc::ArrayView<const uint8_t> packet) {
    if (packet[1] == rtcp::ReceiverReport::kPacketType) {
      // Ignore RR, it requires no response
    } else {
      RTC_LOG(LS_ERROR) << "Unexpected RTCP packet generated";
      RTC_LOG(LS_ERROR) << "Packet content "
                        << rtc::hex_encode_with_delimiter(
                               absl::string_view(
                                   reinterpret_cast<char*>(packet.data()[0]),
                                   packet.size()),
                               ' ');
    }
  }

  int64_t ProbeCaptureStartNtpTime(ChannelReceiveInterface& channel) {
    // Computation of the capture_start_ntp_time_ms_ occurs when the
    // audio data is pulled, not when it is received. So we need to
    // inject an RTP packet, and then fetch its data.
    AudioFrame audio_frame;
    channel.OnRtpPacket(CreateRtpPacket());
    channel.GetAudioFrameWithInfo(kSampleRateHz, &audio_frame);
    CallReceiveStatistics stats = channel.GetRTCPStatistics();
    return stats.capture_start_ntp_time_ms_;
  }

 protected:
  GlobalSimulatedTimeController time_controller_;
  rtc::scoped_refptr<test::MockAudioDeviceModule> audio_device_module_;
  rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory_;
  MockTransport transport_;
};

TEST_F(ChannelReceiveTest, CreateAndDestroy) {
  auto channel = CreateTestChannelReceive();
  EXPECT_THAT(channel, NotNull());
}

TEST_F(ChannelReceiveTest, ReceiveReportGeneratedOnTime) {
  auto channel = CreateTestChannelReceive();

  bool receiver_report_sent = false;
  EXPECT_CALL(transport_, SendRtcp)
      .WillRepeatedly([&](rtc::ArrayView<const uint8_t> packet) {
        if (packet.size() >= 2 &&
            packet[1] == rtcp::ReceiverReport::kPacketType) {
          receiver_report_sent = true;
        }
        return true;
      });
  // RFC 3550 section 6.2 mentions 5 seconds as a reasonable expectation
  // for the interval between RTCP packets.
  time_controller_.AdvanceTime(TimeDelta::Seconds(5));

  EXPECT_TRUE(receiver_report_sent);
}

TEST_F(ChannelReceiveTest, CaptureStartTimeBecomesValid) {
  auto channel = CreateTestChannelReceive();

  EXPECT_CALL(transport_, SendRtcp)
      .WillRepeatedly([&](rtc::ArrayView<const uint8_t> packet) {
        HandleGeneratedRtcp(*channel, packet);
        return true;
      });
  // Before any packets are sent, CaptureStartTime is invalid.
  EXPECT_EQ(ProbeCaptureStartNtpTime(*channel), -1);

  // Must start playout, otherwise packet is discarded.
  channel->StartPlayout();
  // Send one RTP packet. This causes registration of the SSRC.
  channel->OnRtpPacket(CreateRtpPacket());
  EXPECT_EQ(ProbeCaptureStartNtpTime(*channel), -1);

  // Receive a sender report.
  auto rtcp_packet_1 = CreateRtcpSenderReport();
  channel->ReceivedRTCPPacket(rtcp_packet_1.data(), rtcp_packet_1.size());
  EXPECT_EQ(ProbeCaptureStartNtpTime(*channel), -1);

  time_controller_.AdvanceTime(TimeDelta::Seconds(5));

  // Receive a receiver report. This is necessary, which is odd.
  // Presumably it is because the receiver needs to know the RTT
  // before it can compute the capture start NTP time.
  // The receiver report must happen before the second sender report.
  auto rtcp_rr = CreateRtcpReceiverReport();
  channel->ReceivedRTCPPacket(rtcp_rr.data(), rtcp_rr.size());
  EXPECT_EQ(ProbeCaptureStartNtpTime(*channel), -1);

  // Receive another sender report after 5 seconds.
  // This should be enough to establish the capture start NTP time.
  auto rtcp_packet_2 = CreateRtcpSenderReport();
  channel->ReceivedRTCPPacket(rtcp_packet_2.data(), rtcp_packet_2.size());

  EXPECT_NE(ProbeCaptureStartNtpTime(*channel), -1);
}

TEST_F(ChannelReceiveTest, SettingFrameTransformer) {
  auto channel = CreateTestChannelReceive();

  rtc::scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      rtc::make_ref_counted<MockFrameTransformer>();

  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameCallback);
  channel->SetDepacketizerToDecoderFrameTransformer(mock_frame_transformer);

  // Must start playout, otherwise packet is discarded.
  channel->StartPlayout();

  RtpPacketReceived packet = CreateRtpPacket();

  // Receive one RTP packet, this should be transformed.
  EXPECT_CALL(*mock_frame_transformer, Transform);
  channel->OnRtpPacket(packet);
}

TEST_F(ChannelReceiveTest, SettingFrameTransformerMultipleTimes) {
  auto channel = CreateTestChannelReceive();

  rtc::scoped_refptr<MockFrameTransformer> mock_frame_transformer =
      rtc::make_ref_counted<MockFrameTransformer>();

  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameCallback);
  channel->SetDepacketizerToDecoderFrameTransformer(mock_frame_transformer);

  // Set the same transformer again, shouldn't cause any additional callback
  // registration calls.
  EXPECT_CALL(*mock_frame_transformer, RegisterTransformedFrameCallback)
      .Times(0);
  channel->SetDepacketizerToDecoderFrameTransformer(mock_frame_transformer);
}

}  // namespace
}  // namespace voe
}  // namespace webrtc
