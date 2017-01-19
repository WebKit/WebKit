/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_MOCK_VOE_CHANNEL_PROXY_H_
#define WEBRTC_TEST_MOCK_VOE_CHANNEL_PROXY_H_

#include <string>

#include "webrtc/test/gmock.h"
#include "webrtc/voice_engine/channel_proxy.h"

namespace webrtc {
namespace test {

class MockVoEChannelProxy : public voe::ChannelProxy {
 public:
  MOCK_METHOD1(SetRTCPStatus, void(bool enable));
  MOCK_METHOD1(SetLocalSSRC, void(uint32_t ssrc));
  MOCK_METHOD1(SetRTCP_CNAME, void(const std::string& c_name));
  MOCK_METHOD2(SetNACKStatus, void(bool enable, int max_packets));
  MOCK_METHOD2(SetSendAudioLevelIndicationStatus, void(bool enable, int id));
  MOCK_METHOD2(SetReceiveAudioLevelIndicationStatus, void(bool enable, int id));
  MOCK_METHOD1(EnableSendTransportSequenceNumber, void(int id));
  MOCK_METHOD1(EnableReceiveTransportSequenceNumber, void(int id));
  MOCK_METHOD3(RegisterSenderCongestionControlObjects,
               void(RtpPacketSender* rtp_packet_sender,
                    TransportFeedbackObserver* transport_feedback_observer,
                    PacketRouter* packet_router));
  MOCK_METHOD1(RegisterReceiverCongestionControlObjects,
               void(PacketRouter* packet_router));
  MOCK_METHOD0(ResetCongestionControlObjects, void());
  MOCK_CONST_METHOD0(GetRTCPStatistics, CallStatistics());
  MOCK_CONST_METHOD0(GetRemoteRTCPReportBlocks, std::vector<ReportBlock>());
  MOCK_CONST_METHOD0(GetNetworkStatistics, NetworkStatistics());
  MOCK_CONST_METHOD0(GetDecodingCallStatistics, AudioDecodingCallStats());
  MOCK_CONST_METHOD0(GetSpeechOutputLevelFullRange, int32_t());
  MOCK_CONST_METHOD0(GetDelayEstimate, uint32_t());
  MOCK_METHOD1(SetSendTelephoneEventPayloadType, bool(int payload_type));
  MOCK_METHOD2(SendTelephoneEventOutband, bool(int event, int duration_ms));
  MOCK_METHOD1(SetBitrate, void(int bitrate_bps));
  // TODO(solenberg): Talk the compiler into accepting this mock method:
  // MOCK_METHOD1(SetSink, void(std::unique_ptr<AudioSinkInterface> sink));
  MOCK_METHOD1(SetInputMute, void(bool muted));
  MOCK_METHOD1(RegisterExternalTransport, void(Transport* transport));
  MOCK_METHOD0(DeRegisterExternalTransport, void());
  MOCK_METHOD3(ReceivedRTPPacket, bool(const uint8_t* packet,
                                       size_t length,
                                       const PacketTime& packet_time));
  MOCK_METHOD2(ReceivedRTCPPacket, bool(const uint8_t* packet, size_t length));
  MOCK_CONST_METHOD0(GetAudioDecoderFactory,
                     const rtc::scoped_refptr<AudioDecoderFactory>&());
  MOCK_METHOD1(SetChannelOutputVolumeScaling, void(float scaling));
  MOCK_METHOD1(SetRtcEventLog, void(RtcEventLog* event_log));
  MOCK_METHOD1(EnableAudioNetworkAdaptor,
               void(const std::string& config_string));
  MOCK_METHOD0(DisableAudioNetworkAdaptor, void());
  MOCK_METHOD2(SetReceiverFrameLengthRange,
               void(int min_frame_length_ms, int max_frame_length_ms));
  MOCK_METHOD2(GetAudioFrameWithInfo,
      AudioMixer::Source::AudioFrameInfo(int sample_rate_hz,
                                         AudioFrame* audio_frame));
  MOCK_CONST_METHOD0(NeededFrequency, int());
  MOCK_METHOD1(SetTransportOverhead, void(int transport_overhead_per_packet));
  MOCK_METHOD1(AssociateSendChannel,
               void(const ChannelProxy& send_channel_proxy));
  MOCK_METHOD0(DisassociateSendChannel, void());
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_MOCK_VOE_CHANNEL_PROXY_H_
