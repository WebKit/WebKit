/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <string>
#include <vector>

#include "api/test/mock_audio_mixer.h"
#include "api/test/mock_frame_decryptor.h"
#include "audio/audio_receive_stream.h"
#include "audio/conversion.h"
#include "audio/mock_voe_channel_proxy.h"
#include "call/rtp_stream_receiver_controller.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "modules/audio_device/include/mock_audio_device.h"
#include "modules/audio_processing/include/mock_audio_processing.h"
#include "modules/bitrate_controller/include/mock/mock_bitrate_controller.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "test/gtest.h"
#include "test/mock_audio_decoder_factory.h"
#include "test/mock_transport.h"

namespace webrtc {
namespace test {
namespace {

using testing::_;
using testing::FloatEq;
using testing::Return;
using testing::ReturnRef;

AudioDecodingCallStats MakeAudioDecodeStatsForTest() {
  AudioDecodingCallStats audio_decode_stats;
  audio_decode_stats.calls_to_silence_generator = 234;
  audio_decode_stats.calls_to_neteq = 567;
  audio_decode_stats.decoded_normal = 890;
  audio_decode_stats.decoded_plc = 123;
  audio_decode_stats.decoded_cng = 456;
  audio_decode_stats.decoded_plc_cng = 789;
  audio_decode_stats.decoded_muted_output = 987;
  return audio_decode_stats;
}

const uint32_t kRemoteSsrc = 1234;
const uint32_t kLocalSsrc = 5678;
const size_t kOneByteExtensionHeaderLength = 4;
const size_t kOneByteExtensionLength = 4;
const int kAudioLevelId = 3;
const int kTransportSequenceNumberId = 4;
const int kJitterBufferDelay = -7;
const int kPlayoutBufferDelay = 302;
const unsigned int kSpeechOutputLevel = 99;
const double kTotalOutputEnergy = 0.25;
const double kTotalOutputDuration = 0.5;

const CallReceiveStatistics kCallStats = {345, 678, 901, 234,
                                          -12, 567, 890, 123};
const CodecInst kCodecInst = {123, "codec_name_recv", 96000, -187, 0, -103};
const NetworkStatistics kNetworkStats = {
    123, 456, false, 789012, 3456, 123, 456, 0,  {}, 789, 12,
    345, 678, 901,   0,      -1,   -1,  -1,  -1, -1, 0};
const AudioDecodingCallStats kAudioDecodeStats = MakeAudioDecodeStatsForTest();

struct ConfigHelper {
  ConfigHelper() : ConfigHelper(new rtc::RefCountedObject<MockAudioMixer>()) {}

  explicit ConfigHelper(rtc::scoped_refptr<MockAudioMixer> audio_mixer)
      : audio_mixer_(audio_mixer) {
    using testing::Invoke;

    AudioState::Config config;
    config.audio_mixer = audio_mixer_;
    config.audio_processing = new rtc::RefCountedObject<MockAudioProcessing>();
    config.audio_device_module =
        new rtc::RefCountedObject<testing::NiceMock<MockAudioDeviceModule>>();
    audio_state_ = AudioState::Create(config);

    channel_receive_ = new testing::StrictMock<MockChannelReceive>();
    EXPECT_CALL(*channel_receive_, SetLocalSSRC(kLocalSsrc)).Times(1);
    EXPECT_CALL(*channel_receive_, SetNACKStatus(true, 15)).Times(1);
    EXPECT_CALL(*channel_receive_,
                RegisterReceiverCongestionControlObjects(&packet_router_))
        .Times(1);
    EXPECT_CALL(*channel_receive_, ResetReceiverCongestionControlObjects())
        .Times(1);
    EXPECT_CALL(*channel_receive_, SetAssociatedSendChannel(nullptr)).Times(1);
    EXPECT_CALL(*channel_receive_, SetReceiveCodecs(_))
        .WillRepeatedly(Invoke([](const std::map<int, SdpAudioFormat>& codecs) {
          EXPECT_THAT(codecs, testing::IsEmpty());
        }));

    stream_config_.rtp.local_ssrc = kLocalSsrc;
    stream_config_.rtp.remote_ssrc = kRemoteSsrc;
    stream_config_.rtp.nack.rtp_history_ms = 300;
    stream_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAudioLevelUri, kAudioLevelId));
    stream_config_.rtp.extensions.push_back(RtpExtension(
        RtpExtension::kTransportSequenceNumberUri, kTransportSequenceNumberId));
    stream_config_.rtcp_send_transport = &rtcp_send_transport_;
    stream_config_.decoder_factory =
        new rtc::RefCountedObject<MockAudioDecoderFactory>;
  }

  std::unique_ptr<internal::AudioReceiveStream> CreateAudioReceiveStream() {
    return std::unique_ptr<internal::AudioReceiveStream>(
        new internal::AudioReceiveStream(
            &rtp_stream_receiver_controller_, &packet_router_, stream_config_,
            audio_state_, &event_log_,
            std::unique_ptr<voe::ChannelReceiveInterface>(channel_receive_)));
  }

  AudioReceiveStream::Config& config() { return stream_config_; }
  rtc::scoped_refptr<MockAudioMixer> audio_mixer() { return audio_mixer_; }
  MockChannelReceive* channel_receive() { return channel_receive_; }

  void SetupMockForGetStats() {
    using testing::DoAll;
    using testing::SetArgPointee;

    ASSERT_TRUE(channel_receive_);
    EXPECT_CALL(*channel_receive_, GetRTCPStatistics())
        .WillOnce(Return(kCallStats));
    EXPECT_CALL(*channel_receive_, GetDelayEstimate())
        .WillOnce(Return(kJitterBufferDelay + kPlayoutBufferDelay));
    EXPECT_CALL(*channel_receive_, GetSpeechOutputLevelFullRange())
        .WillOnce(Return(kSpeechOutputLevel));
    EXPECT_CALL(*channel_receive_, GetTotalOutputEnergy())
        .WillOnce(Return(kTotalOutputEnergy));
    EXPECT_CALL(*channel_receive_, GetTotalOutputDuration())
        .WillOnce(Return(kTotalOutputDuration));
    EXPECT_CALL(*channel_receive_, GetNetworkStatistics())
        .WillOnce(Return(kNetworkStats));
    EXPECT_CALL(*channel_receive_, GetDecodingCallStatistics())
        .WillOnce(Return(kAudioDecodeStats));
    EXPECT_CALL(*channel_receive_, GetRecCodec(_))
        .WillOnce(DoAll(SetArgPointee<0>(kCodecInst), Return(true)));
  }

 private:
  PacketRouter packet_router_;
  MockRtcEventLog event_log_;
  rtc::scoped_refptr<AudioState> audio_state_;
  rtc::scoped_refptr<MockAudioMixer> audio_mixer_;
  AudioReceiveStream::Config stream_config_;
  testing::StrictMock<MockChannelReceive>* channel_receive_ = nullptr;
  RtpStreamReceiverController rtp_stream_receiver_controller_;
  MockTransport rtcp_send_transport_;
};

void BuildOneByteExtension(std::vector<uint8_t>::iterator it,
                           int id,
                           uint32_t extension_value,
                           size_t value_length) {
  const uint16_t kRtpOneByteHeaderExtensionId = 0xBEDE;
  ByteWriter<uint16_t>::WriteBigEndian(&(*it), kRtpOneByteHeaderExtensionId);
  it += 2;

  ByteWriter<uint16_t>::WriteBigEndian(&(*it), kOneByteExtensionLength / 4);
  it += 2;
  const size_t kExtensionDataLength = kOneByteExtensionLength - 1;
  uint32_t shifted_value = extension_value
                           << (8 * (kExtensionDataLength - value_length));
  *it = (id << 4) + (static_cast<uint8_t>(value_length) - 1);
  ++it;
  ByteWriter<uint32_t, kExtensionDataLength>::WriteBigEndian(&(*it),
                                                             shifted_value);
}

const std::vector<uint8_t> CreateRtpHeaderWithOneByteExtension(
    int extension_id,
    uint32_t extension_value,
    size_t value_length) {
  std::vector<uint8_t> header;
  header.resize(webrtc::kRtpHeaderSize + kOneByteExtensionHeaderLength +
                kOneByteExtensionLength);
  header[0] = 0x80;   // Version 2.
  header[0] |= 0x10;  // Set extension bit.
  header[1] = 100;    // Payload type.
  header[1] |= 0x80;  // Marker bit is set.
  ByteWriter<uint16_t>::WriteBigEndian(&header[2], 0x1234);  // Sequence number.
  ByteWriter<uint32_t>::WriteBigEndian(&header[4], 0x5678);  // Timestamp.
  ByteWriter<uint32_t>::WriteBigEndian(&header[8], 0x4321);  // SSRC.

  BuildOneByteExtension(header.begin() + webrtc::kRtpHeaderSize, extension_id,
                        extension_value, value_length);
  return header;
}

const std::vector<uint8_t> CreateRtcpSenderReport() {
  std::vector<uint8_t> packet;
  const size_t kRtcpSrLength = 28;  // In bytes.
  packet.resize(kRtcpSrLength);
  packet[0] = 0x80;  // Version 2.
  packet[1] = 0xc8;  // PT = 200, SR.
  // Length in number of 32-bit words - 1.
  ByteWriter<uint16_t>::WriteBigEndian(&packet[2], 6);
  ByteWriter<uint32_t>::WriteBigEndian(&packet[4], kLocalSsrc);
  return packet;
}
}  // namespace

TEST(AudioReceiveStreamTest, ConfigToString) {
  AudioReceiveStream::Config config;
  config.rtp.remote_ssrc = kRemoteSsrc;
  config.rtp.local_ssrc = kLocalSsrc;
  config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAudioLevelUri, kAudioLevelId));
  EXPECT_EQ(
      "{rtp: {remote_ssrc: 1234, local_ssrc: 5678, transport_cc: off, nack: "
      "{rtp_history_ms: 0}, extensions: [{uri: "
      "urn:ietf:params:rtp-hdrext:ssrc-audio-level, id: 3}]}, "
      "rtcp_send_transport: null, media_transport: null}",
      config.ToString());
}

TEST(AudioReceiveStreamTest, ConstructDestruct) {
  ConfigHelper helper;
  auto recv_stream = helper.CreateAudioReceiveStream();
}

TEST(AudioReceiveStreamTest, ReceiveRtpPacket) {
  ConfigHelper helper;
  helper.config().rtp.transport_cc = true;
  auto recv_stream = helper.CreateAudioReceiveStream();
  const int kTransportSequenceNumberValue = 1234;
  std::vector<uint8_t> rtp_packet = CreateRtpHeaderWithOneByteExtension(
      kTransportSequenceNumberId, kTransportSequenceNumberValue, 2);
  constexpr int64_t packet_time_us = 5678000;

  RtpPacketReceived parsed_packet;
  ASSERT_TRUE(parsed_packet.Parse(&rtp_packet[0], rtp_packet.size()));
  parsed_packet.set_arrival_time_ms((packet_time_us + 500) / 1000);

  EXPECT_CALL(*helper.channel_receive(),
              OnRtpPacket(testing::Ref(parsed_packet)));

  recv_stream->OnRtpPacket(parsed_packet);
}

TEST(AudioReceiveStreamTest, ReceiveRtcpPacket) {
  ConfigHelper helper;
  helper.config().rtp.transport_cc = true;
  auto recv_stream = helper.CreateAudioReceiveStream();
  std::vector<uint8_t> rtcp_packet = CreateRtcpSenderReport();
  EXPECT_CALL(*helper.channel_receive(),
              ReceivedRTCPPacket(&rtcp_packet[0], rtcp_packet.size()))
      .WillOnce(Return(true));
  EXPECT_TRUE(recv_stream->DeliverRtcp(&rtcp_packet[0], rtcp_packet.size()));
}

TEST(AudioReceiveStreamTest, GetStats) {
  ConfigHelper helper;
  auto recv_stream = helper.CreateAudioReceiveStream();
  helper.SetupMockForGetStats();
  AudioReceiveStream::Stats stats = recv_stream->GetStats();
  EXPECT_EQ(kRemoteSsrc, stats.remote_ssrc);
  EXPECT_EQ(static_cast<int64_t>(kCallStats.bytesReceived), stats.bytes_rcvd);
  EXPECT_EQ(static_cast<uint32_t>(kCallStats.packetsReceived),
            stats.packets_rcvd);
  EXPECT_EQ(kCallStats.cumulativeLost, stats.packets_lost);
  EXPECT_EQ(Q8ToFloat(kCallStats.fractionLost), stats.fraction_lost);
  EXPECT_EQ(std::string(kCodecInst.plname), stats.codec_name);
  EXPECT_EQ(kCallStats.extendedMax, stats.ext_seqnum);
  EXPECT_EQ(kCallStats.jitterSamples / (kCodecInst.plfreq / 1000),
            stats.jitter_ms);
  EXPECT_EQ(kNetworkStats.currentBufferSize, stats.jitter_buffer_ms);
  EXPECT_EQ(kNetworkStats.preferredBufferSize,
            stats.jitter_buffer_preferred_ms);
  EXPECT_EQ(static_cast<uint32_t>(kJitterBufferDelay + kPlayoutBufferDelay),
            stats.delay_estimate_ms);
  EXPECT_EQ(static_cast<int32_t>(kSpeechOutputLevel), stats.audio_level);
  EXPECT_EQ(kTotalOutputEnergy, stats.total_output_energy);
  EXPECT_EQ(kNetworkStats.totalSamplesReceived, stats.total_samples_received);
  EXPECT_EQ(kTotalOutputDuration, stats.total_output_duration);
  EXPECT_EQ(kNetworkStats.concealedSamples, stats.concealed_samples);
  EXPECT_EQ(kNetworkStats.concealmentEvents, stats.concealment_events);
  EXPECT_EQ(static_cast<double>(kNetworkStats.jitterBufferDelayMs) /
                static_cast<double>(rtc::kNumMillisecsPerSec),
            stats.jitter_buffer_delay_seconds);
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentExpandRate), stats.expand_rate);
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentSpeechExpandRate),
            stats.speech_expand_rate);
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentSecondaryDecodedRate),
            stats.secondary_decoded_rate);
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentSecondaryDiscardedRate),
            stats.secondary_discarded_rate);
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentAccelerateRate),
            stats.accelerate_rate);
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentPreemptiveRate),
            stats.preemptive_expand_rate);
  EXPECT_EQ(kAudioDecodeStats.calls_to_silence_generator,
            stats.decoding_calls_to_silence_generator);
  EXPECT_EQ(kAudioDecodeStats.calls_to_neteq, stats.decoding_calls_to_neteq);
  EXPECT_EQ(kAudioDecodeStats.decoded_normal, stats.decoding_normal);
  EXPECT_EQ(kAudioDecodeStats.decoded_plc, stats.decoding_plc);
  EXPECT_EQ(kAudioDecodeStats.decoded_cng, stats.decoding_cng);
  EXPECT_EQ(kAudioDecodeStats.decoded_plc_cng, stats.decoding_plc_cng);
  EXPECT_EQ(kAudioDecodeStats.decoded_muted_output,
            stats.decoding_muted_output);
  EXPECT_EQ(kCallStats.capture_start_ntp_time_ms_,
            stats.capture_start_ntp_time_ms);
}

TEST(AudioReceiveStreamTest, SetGain) {
  ConfigHelper helper;
  auto recv_stream = helper.CreateAudioReceiveStream();
  EXPECT_CALL(*helper.channel_receive(),
              SetChannelOutputVolumeScaling(FloatEq(0.765f)));
  recv_stream->SetGain(0.765f);
}

TEST(AudioReceiveStreamTest, StreamsShouldBeAddedToMixerOnceOnStart) {
  ConfigHelper helper1;
  ConfigHelper helper2(helper1.audio_mixer());
  auto recv_stream1 = helper1.CreateAudioReceiveStream();
  auto recv_stream2 = helper2.CreateAudioReceiveStream();

  EXPECT_CALL(*helper1.channel_receive(), StartPlayout()).Times(1);
  EXPECT_CALL(*helper2.channel_receive(), StartPlayout()).Times(1);
  EXPECT_CALL(*helper1.channel_receive(), StopPlayout()).Times(1);
  EXPECT_CALL(*helper2.channel_receive(), StopPlayout()).Times(1);
  EXPECT_CALL(*helper1.audio_mixer(), AddSource(recv_stream1.get()))
      .WillOnce(Return(true));
  EXPECT_CALL(*helper1.audio_mixer(), AddSource(recv_stream2.get()))
      .WillOnce(Return(true));
  EXPECT_CALL(*helper1.audio_mixer(), RemoveSource(recv_stream1.get()))
      .Times(1);
  EXPECT_CALL(*helper1.audio_mixer(), RemoveSource(recv_stream2.get()))
      .Times(1);

  recv_stream1->Start();
  recv_stream2->Start();

  // One more should not result in any more mixer sources added.
  recv_stream1->Start();

  // Stop stream before it is being destructed.
  recv_stream2->Stop();
}

TEST(AudioReceiveStreamTest, ReconfigureWithSameConfig) {
  ConfigHelper helper;
  auto recv_stream = helper.CreateAudioReceiveStream();
  recv_stream->Reconfigure(helper.config());
}

TEST(AudioReceiveStreamTest, ReconfigureWithUpdatedConfig) {
  ConfigHelper helper;
  auto recv_stream = helper.CreateAudioReceiveStream();

  auto new_config = helper.config();
  new_config.rtp.local_ssrc = kLocalSsrc + 1;
  new_config.rtp.nack.rtp_history_ms = 300 + 20;
  new_config.rtp.extensions.clear();
  new_config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAudioLevelUri, kAudioLevelId + 1));
  new_config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kTransportSequenceNumberUri,
                   kTransportSequenceNumberId + 1));
  new_config.decoder_map.emplace(1, SdpAudioFormat("foo", 8000, 1));

  MockChannelReceive& channel_receive = *helper.channel_receive();
  EXPECT_CALL(channel_receive, SetLocalSSRC(kLocalSsrc + 1)).Times(1);
  EXPECT_CALL(channel_receive, SetNACKStatus(true, 15 + 1)).Times(1);
  EXPECT_CALL(channel_receive, SetReceiveCodecs(new_config.decoder_map));

  recv_stream->Reconfigure(new_config);
}

TEST(AudioReceiveStreamTest, ReconfigureWithFrameDecryptor) {
  ConfigHelper helper;
  auto recv_stream = helper.CreateAudioReceiveStream();

  auto new_config_0 = helper.config();
  rtc::scoped_refptr<FrameDecryptorInterface> mock_frame_decryptor_0(
      new rtc::RefCountedObject<MockFrameDecryptor>());
  new_config_0.frame_decryptor = mock_frame_decryptor_0;

  recv_stream->Reconfigure(new_config_0);

  auto new_config_1 = helper.config();
  rtc::scoped_refptr<FrameDecryptorInterface> mock_frame_decryptor_1(
      new rtc::RefCountedObject<MockFrameDecryptor>());
  new_config_1.frame_decryptor = mock_frame_decryptor_1;
  new_config_1.crypto_options.sframe.require_frame_encryption = true;
  recv_stream->Reconfigure(new_config_1);
}

}  // namespace test
}  // namespace webrtc
