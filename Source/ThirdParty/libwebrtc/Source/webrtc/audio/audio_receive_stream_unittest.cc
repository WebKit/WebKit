/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>
#include <vector>

#include "webrtc/api/test/mock_audio_mixer.h"
#include "webrtc/audio/audio_receive_stream.h"
#include "webrtc/audio/conversion.h"
#include "webrtc/logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "webrtc/modules/audio_coding/codecs/mock/mock_audio_decoder_factory.h"
#include "webrtc/modules/bitrate_controller/include/mock/mock_bitrate_controller.h"
#include "webrtc/modules/pacing/packet_router.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/mock_voe_channel_proxy.h"
#include "webrtc/test/mock_voice_engine.h"

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

const int kChannelId = 2;
const uint32_t kRemoteSsrc = 1234;
const uint32_t kLocalSsrc = 5678;
const size_t kOneByteExtensionHeaderLength = 4;
const size_t kOneByteExtensionLength = 4;
const int kAudioLevelId = 3;
const int kTransportSequenceNumberId = 4;
const int kJitterBufferDelay = -7;
const int kPlayoutBufferDelay = 302;
const unsigned int kSpeechOutputLevel = 99;
const CallStatistics kCallStats = {
    345,  678,  901, 234, -12, 3456, 7890, 567, 890, 123};
const CodecInst kCodecInst = {
    123, "codec_name_recv", 96000, -187, 0, -103};
const NetworkStatistics kNetworkStats = {
    123, 456, false, 0, 0, 789, 12, 345, 678, 901, -1, -1, -1, -1, -1, 0};
const AudioDecodingCallStats kAudioDecodeStats = MakeAudioDecodeStatsForTest();

struct ConfigHelper {
  ConfigHelper()
      : decoder_factory_(new rtc::RefCountedObject<MockAudioDecoderFactory>),
        audio_mixer_(new rtc::RefCountedObject<MockAudioMixer>()) {
    using testing::Invoke;

    EXPECT_CALL(voice_engine_,
        RegisterVoiceEngineObserver(_)).WillOnce(Return(0));
    EXPECT_CALL(voice_engine_,
        DeRegisterVoiceEngineObserver()).WillOnce(Return(0));
    EXPECT_CALL(voice_engine_, audio_processing());
    EXPECT_CALL(voice_engine_, audio_device_module());
    EXPECT_CALL(voice_engine_, audio_transport());

    AudioState::Config config;
    config.voice_engine = &voice_engine_;
    config.audio_mixer = audio_mixer_;
    audio_state_ = AudioState::Create(config);

    EXPECT_CALL(voice_engine_, ChannelProxyFactory(kChannelId))
        .WillOnce(Invoke([this](int channel_id) {
          EXPECT_FALSE(channel_proxy_);
          channel_proxy_ = new testing::StrictMock<MockVoEChannelProxy>();
          EXPECT_CALL(*channel_proxy_, SetLocalSSRC(kLocalSsrc)).Times(1);
          EXPECT_CALL(*channel_proxy_, SetNACKStatus(true, 15)).Times(1);
          EXPECT_CALL(*channel_proxy_,
              SetReceiveAudioLevelIndicationStatus(true, kAudioLevelId))
                  .Times(1);
          EXPECT_CALL(*channel_proxy_,
              EnableReceiveTransportSequenceNumber(kTransportSequenceNumberId))
                  .Times(1);
          EXPECT_CALL(*channel_proxy_,
              RegisterReceiverCongestionControlObjects(&packet_router_))
                  .Times(1);
          EXPECT_CALL(*channel_proxy_, ResetCongestionControlObjects())
              .Times(1);
          EXPECT_CALL(*channel_proxy_, RegisterExternalTransport(nullptr))
              .Times(1);
          EXPECT_CALL(*channel_proxy_, DeRegisterExternalTransport())
              .Times(1);
          EXPECT_CALL(*channel_proxy_, GetAudioDecoderFactory())
              .WillOnce(ReturnRef(decoder_factory_));
          testing::Expectation expect_set =
              EXPECT_CALL(*channel_proxy_, SetRtcEventLog(&event_log_))
                  .Times(1);
          EXPECT_CALL(*channel_proxy_, SetRtcEventLog(testing::IsNull()))
              .Times(1)
              .After(expect_set);
          EXPECT_CALL(*channel_proxy_, DisassociateSendChannel()).Times(1);
          return channel_proxy_;
        }));
    stream_config_.voe_channel_id = kChannelId;
    stream_config_.rtp.local_ssrc = kLocalSsrc;
    stream_config_.rtp.remote_ssrc = kRemoteSsrc;
    stream_config_.rtp.nack.rtp_history_ms = 300;
    stream_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAudioLevelUri, kAudioLevelId));
    stream_config_.rtp.extensions.push_back(RtpExtension(
        RtpExtension::kTransportSequenceNumberUri, kTransportSequenceNumberId));
    stream_config_.decoder_factory = decoder_factory_;
  }

  PacketRouter* packet_router() { return &packet_router_; }
  MockRtcEventLog* event_log() { return &event_log_; }
  AudioReceiveStream::Config& config() { return stream_config_; }
  rtc::scoped_refptr<AudioState> audio_state() { return audio_state_; }
  rtc::scoped_refptr<MockAudioMixer> audio_mixer() { return audio_mixer_; }
  MockVoiceEngine& voice_engine() { return voice_engine_; }
  MockVoEChannelProxy* channel_proxy() { return channel_proxy_; }

  void SetupMockForGetStats() {
    using testing::DoAll;
    using testing::SetArgPointee;

    ASSERT_TRUE(channel_proxy_);
    EXPECT_CALL(*channel_proxy_, GetRTCPStatistics())
        .WillOnce(Return(kCallStats));
    EXPECT_CALL(*channel_proxy_, GetDelayEstimate())
        .WillOnce(Return(kJitterBufferDelay + kPlayoutBufferDelay));
    EXPECT_CALL(*channel_proxy_, GetSpeechOutputLevelFullRange())
        .WillOnce(Return(kSpeechOutputLevel));
    EXPECT_CALL(*channel_proxy_, GetNetworkStatistics())
        .WillOnce(Return(kNetworkStats));
    EXPECT_CALL(*channel_proxy_, GetDecodingCallStatistics())
        .WillOnce(Return(kAudioDecodeStats));
    EXPECT_CALL(*channel_proxy_, GetRecCodec(_))
        .WillOnce(DoAll(SetArgPointee<0>(kCodecInst), Return(true)));
  }

 private:
  PacketRouter packet_router_;
  rtc::scoped_refptr<AudioDecoderFactory> decoder_factory_;
  MockRtcEventLog event_log_;
  testing::StrictMock<MockVoiceEngine> voice_engine_;
  rtc::scoped_refptr<AudioState> audio_state_;
  rtc::scoped_refptr<MockAudioMixer> audio_mixer_;
  AudioReceiveStream::Config stream_config_;
  testing::StrictMock<MockVoEChannelProxy>* channel_proxy_ = nullptr;
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
  config.voe_channel_id = kChannelId;
  config.rtp.extensions.push_back(
      RtpExtension(RtpExtension::kAudioLevelUri, kAudioLevelId));
  EXPECT_EQ(
      "{rtp: {remote_ssrc: 1234, local_ssrc: 5678, transport_cc: off, nack: "
      "{rtp_history_ms: 0}, extensions: [{uri: "
      "urn:ietf:params:rtp-hdrext:ssrc-audio-level, id: 3}]}, "
      "rtcp_send_transport: null, voe_channel_id: 2}",
      config.ToString());
}

TEST(AudioReceiveStreamTest, ConstructDestruct) {
  ConfigHelper helper;
  internal::AudioReceiveStream recv_stream(
      helper.packet_router(),
      helper.config(), helper.audio_state(), helper.event_log());
}

TEST(AudioReceiveStreamTest, ReceiveRtpPacket) {
  ConfigHelper helper;
  helper.config().rtp.transport_cc = true;
  internal::AudioReceiveStream recv_stream(
      helper.packet_router(),
      helper.config(), helper.audio_state(), helper.event_log());
  const int kTransportSequenceNumberValue = 1234;
  std::vector<uint8_t> rtp_packet = CreateRtpHeaderWithOneByteExtension(
      kTransportSequenceNumberId, kTransportSequenceNumberValue, 2);
  PacketTime packet_time(5678000, 0);

  RtpPacketReceived parsed_packet;
  ASSERT_TRUE(parsed_packet.Parse(&rtp_packet[0], rtp_packet.size()));
  parsed_packet.set_arrival_time_ms((packet_time.timestamp + 500) / 1000);

  EXPECT_CALL(*helper.channel_proxy(),
              OnRtpPacket(testing::Ref(parsed_packet)));

  recv_stream.OnRtpPacket(parsed_packet);
}

TEST(AudioReceiveStreamTest, ReceiveRtcpPacket) {
  ConfigHelper helper;
  helper.config().rtp.transport_cc = true;
  internal::AudioReceiveStream recv_stream(
      helper.packet_router(),
      helper.config(), helper.audio_state(), helper.event_log());

  std::vector<uint8_t> rtcp_packet = CreateRtcpSenderReport();
  EXPECT_CALL(*helper.channel_proxy(),
              ReceivedRTCPPacket(&rtcp_packet[0], rtcp_packet.size()))
      .WillOnce(Return(true));
  EXPECT_TRUE(recv_stream.DeliverRtcp(&rtcp_packet[0], rtcp_packet.size()));
}

TEST(AudioReceiveStreamTest, GetStats) {
  ConfigHelper helper;
  internal::AudioReceiveStream recv_stream(
      helper.packet_router(),
      helper.config(), helper.audio_state(), helper.event_log());
  helper.SetupMockForGetStats();
  AudioReceiveStream::Stats stats = recv_stream.GetStats();
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
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentExpandRate), stats.expand_rate);
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentSpeechExpandRate),
            stats.speech_expand_rate);
  EXPECT_EQ(Q14ToFloat(kNetworkStats.currentSecondaryDecodedRate),
            stats.secondary_decoded_rate);
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
  internal::AudioReceiveStream recv_stream(
      helper.packet_router(),
      helper.config(), helper.audio_state(), helper.event_log());
  EXPECT_CALL(*helper.channel_proxy(),
      SetChannelOutputVolumeScaling(FloatEq(0.765f)));
  recv_stream.SetGain(0.765f);
}

TEST(AudioReceiveStreamTest, StreamShouldNotBeAddedToMixerWhenVoEReturnsError) {
  ConfigHelper helper;
  internal::AudioReceiveStream recv_stream(
      helper.packet_router(),
      helper.config(), helper.audio_state(), helper.event_log());

  EXPECT_CALL(helper.voice_engine(), StartPlayout(_)).WillOnce(Return(-1));
  EXPECT_CALL(*helper.audio_mixer(), AddSource(_)).Times(0);

  recv_stream.Start();
}

TEST(AudioReceiveStreamTest, StreamShouldBeAddedToMixerOnStart) {
  ConfigHelper helper;
  internal::AudioReceiveStream recv_stream(
      helper.packet_router(),
      helper.config(), helper.audio_state(), helper.event_log());

  EXPECT_CALL(helper.voice_engine(), StartPlayout(_)).WillOnce(Return(0));
  EXPECT_CALL(helper.voice_engine(), StopPlayout(_));
  EXPECT_CALL(*helper.audio_mixer(), AddSource(&recv_stream))
      .WillOnce(Return(true));

  recv_stream.Start();
}
}  // namespace test
}  // namespace webrtc
