/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_MOCK_VOE_CHANNEL_PROXY_H_
#define AUDIO_MOCK_VOE_CHANNEL_PROXY_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/test/mock_frame_encryptor.h"
#include "audio/channel_receive.h"
#include "audio/channel_send.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "test/gmock.h"

namespace webrtc {
namespace test {

class MockChannelReceive : public voe::ChannelReceiveInterface {
 public:
  MOCK_METHOD1(SetLocalSSRC, void(uint32_t ssrc));
  MOCK_METHOD2(SetNACKStatus, void(bool enable, int max_packets));
  MOCK_METHOD1(RegisterReceiverCongestionControlObjects,
               void(PacketRouter* packet_router));
  MOCK_METHOD0(ResetReceiverCongestionControlObjects, void());
  MOCK_CONST_METHOD0(GetRTCPStatistics, CallReceiveStatistics());
  MOCK_CONST_METHOD0(GetNetworkStatistics, NetworkStatistics());
  MOCK_CONST_METHOD0(GetDecodingCallStatistics, AudioDecodingCallStats());
  MOCK_CONST_METHOD0(GetSpeechOutputLevelFullRange, int());
  MOCK_CONST_METHOD0(GetTotalOutputEnergy, double());
  MOCK_CONST_METHOD0(GetTotalOutputDuration, double());
  MOCK_CONST_METHOD0(GetDelayEstimate, uint32_t());
  MOCK_METHOD1(SetSink, void(AudioSinkInterface* sink));
  MOCK_METHOD1(OnRtpPacket, void(const RtpPacketReceived& packet));
  MOCK_METHOD2(ReceivedRTCPPacket, bool(const uint8_t* packet, size_t length));
  MOCK_METHOD1(SetChannelOutputVolumeScaling, void(float scaling));
  MOCK_METHOD2(GetAudioFrameWithInfo,
               AudioMixer::Source::AudioFrameInfo(int sample_rate_hz,
                                                  AudioFrame* audio_frame));
  MOCK_CONST_METHOD0(PreferredSampleRate, int());
  MOCK_METHOD1(SetAssociatedSendChannel,
               void(const voe::ChannelSendInterface* send_channel));
  MOCK_CONST_METHOD0(GetPlayoutTimestamp, uint32_t());
  MOCK_CONST_METHOD0(GetSyncInfo, absl::optional<Syncable::Info>());
  MOCK_METHOD1(SetMinimumPlayoutDelay, void(int delay_ms));
  MOCK_CONST_METHOD1(GetRecCodec, bool(CodecInst* codec_inst));
  MOCK_METHOD1(SetReceiveCodecs,
               void(const std::map<int, SdpAudioFormat>& codecs));
  MOCK_CONST_METHOD0(GetSources, std::vector<RtpSource>());
  MOCK_METHOD0(StartPlayout, void());
  MOCK_METHOD0(StopPlayout, void());
};

class MockChannelSend : public voe::ChannelSendInterface {
 public:
  // GMock doesn't like move-only types, like std::unique_ptr.
  virtual bool SetEncoder(int payload_type,
                          std::unique_ptr<AudioEncoder> encoder) {
    return SetEncoderForMock(payload_type, &encoder);
  }
  MOCK_METHOD2(SetEncoderForMock,
               bool(int payload_type, std::unique_ptr<AudioEncoder>* encoder));
  MOCK_METHOD1(
      ModifyEncoder,
      void(rtc::FunctionView<void(std::unique_ptr<AudioEncoder>*)> modifier));
  MOCK_METHOD2(SetMid, void(const std::string& mid, int extension_id));
  MOCK_METHOD1(SetLocalSSRC, void(uint32_t ssrc));
  MOCK_METHOD1(SetRTCP_CNAME, void(absl::string_view c_name));
  MOCK_METHOD1(SetExtmapAllowMixed, void(bool extmap_allow_mixed));
  MOCK_METHOD2(SetSendAudioLevelIndicationStatus, void(bool enable, int id));
  MOCK_METHOD1(EnableSendTransportSequenceNumber, void(int id));
  MOCK_METHOD2(RegisterSenderCongestionControlObjects,
               void(RtpTransportControllerSendInterface* transport,
                    RtcpBandwidthObserver* bandwidth_observer));
  MOCK_METHOD0(ResetSenderCongestionControlObjects, void());
  MOCK_CONST_METHOD0(GetRTCPStatistics, CallSendStatistics());
  MOCK_CONST_METHOD0(GetRemoteRTCPReportBlocks, std::vector<ReportBlock>());
  MOCK_CONST_METHOD0(GetANAStatistics, ANAStats());
  MOCK_METHOD2(SetSendTelephoneEventPayloadType,
               bool(int payload_type, int payload_frequency));
  MOCK_METHOD2(SendTelephoneEventOutband, bool(int event, int duration_ms));
  MOCK_METHOD1(OnBitrateAllocation, void(BitrateAllocationUpdate update));
  MOCK_METHOD1(SetInputMute, void(bool muted));
  MOCK_METHOD2(ReceivedRTCPPacket, bool(const uint8_t* packet, size_t length));
  // GMock doesn't like move-only types, like std::unique_ptr.
  virtual void ProcessAndEncodeAudio(std::unique_ptr<AudioFrame> audio_frame) {
    ProcessAndEncodeAudioForMock(&audio_frame);
  }
  MOCK_METHOD1(ProcessAndEncodeAudioForMock,
               void(std::unique_ptr<AudioFrame>* audio_frame));
  MOCK_METHOD1(SetTransportOverhead,
               void(size_t transport_overhead_per_packet));
  MOCK_CONST_METHOD0(GetRtpRtcp, RtpRtcp*());
  MOCK_CONST_METHOD0(GetBitrate, int());
  MOCK_METHOD1(OnTwccBasedUplinkPacketLossRate, void(float packet_loss_rate));
  MOCK_METHOD1(OnRecoverableUplinkPacketLossRate,
               void(float recoverable_packet_loss_rate));
  MOCK_CONST_METHOD0(GetRTT, int64_t());
  MOCK_METHOD0(StartSend, void());
  MOCK_METHOD0(StopSend, void());
  MOCK_METHOD1(
      SetFrameEncryptor,
      void(rtc::scoped_refptr<FrameEncryptorInterface> frame_encryptor));
};
}  // namespace test
}  // namespace webrtc

#endif  // AUDIO_MOCK_VOE_CHANNEL_PROXY_H_
