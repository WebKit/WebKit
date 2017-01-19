/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/channel_proxy.h"

#include <utility>

#include "webrtc/api/call/audio_sink.h"
#include "webrtc/base/checks.h"
#include "webrtc/voice_engine/channel.h"

namespace webrtc {
namespace voe {
ChannelProxy::ChannelProxy() : channel_owner_(nullptr) {}

ChannelProxy::ChannelProxy(const ChannelOwner& channel_owner) :
    channel_owner_(channel_owner) {
  RTC_CHECK(channel_owner_.channel());
}

ChannelProxy::~ChannelProxy() {}

void ChannelProxy::SetRTCPStatus(bool enable) {
  channel()->SetRTCPStatus(enable);
}

void ChannelProxy::SetLocalSSRC(uint32_t ssrc) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  int error = channel()->SetLocalSSRC(ssrc);
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::SetRTCP_CNAME(const std::string& c_name) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  // Note: VoERTP_RTCP::SetRTCP_CNAME() accepts a char[256] array.
  std::string c_name_limited = c_name.substr(0, 255);
  int error = channel()->SetRTCP_CNAME(c_name_limited.c_str());
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::SetNACKStatus(bool enable, int max_packets) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel()->SetNACKStatus(enable, max_packets);
}

void ChannelProxy::SetSendAudioLevelIndicationStatus(bool enable, int id) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  int error = channel()->SetSendAudioLevelIndicationStatus(enable, id);
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::SetReceiveAudioLevelIndicationStatus(bool enable, int id) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  int error = channel()->SetReceiveAudioLevelIndicationStatus(enable, id);
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::EnableSendTransportSequenceNumber(int id) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel()->EnableSendTransportSequenceNumber(id);
}

void ChannelProxy::EnableReceiveTransportSequenceNumber(int id) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel()->EnableReceiveTransportSequenceNumber(id);
}

void ChannelProxy::RegisterSenderCongestionControlObjects(
    RtpPacketSender* rtp_packet_sender,
    TransportFeedbackObserver* transport_feedback_observer,
    PacketRouter* packet_router) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel()->RegisterSenderCongestionControlObjects(
      rtp_packet_sender, transport_feedback_observer, packet_router);
}

void ChannelProxy::RegisterReceiverCongestionControlObjects(
    PacketRouter* packet_router) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel()->RegisterReceiverCongestionControlObjects(packet_router);
}

void ChannelProxy::ResetCongestionControlObjects() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel()->ResetCongestionControlObjects();
}

CallStatistics ChannelProxy::GetRTCPStatistics() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  CallStatistics stats = {0};
  int error = channel()->GetRTPStatistics(stats);
  RTC_DCHECK_EQ(0, error);
  return stats;
}

std::vector<ReportBlock> ChannelProxy::GetRemoteRTCPReportBlocks() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<webrtc::ReportBlock> blocks;
  int error = channel()->GetRemoteRTCPReportBlocks(&blocks);
  RTC_DCHECK_EQ(0, error);
  return blocks;
}

NetworkStatistics ChannelProxy::GetNetworkStatistics() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  NetworkStatistics stats = {0};
  int error = channel()->GetNetworkStatistics(stats);
  RTC_DCHECK_EQ(0, error);
  return stats;
}

AudioDecodingCallStats ChannelProxy::GetDecodingCallStatistics() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  AudioDecodingCallStats stats;
  channel()->GetDecodingCallStatistics(&stats);
  return stats;
}

int32_t ChannelProxy::GetSpeechOutputLevelFullRange() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  uint32_t level = 0;
  int error = channel()->GetSpeechOutputLevelFullRange(level);
  RTC_DCHECK_EQ(0, error);
  return static_cast<int32_t>(level);
}

uint32_t ChannelProxy::GetDelayEstimate() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return channel()->GetDelayEstimate();
}

bool ChannelProxy::SetSendTelephoneEventPayloadType(int payload_type) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return channel()->SetSendTelephoneEventPayloadType(payload_type) == 0;
}

bool ChannelProxy::SendTelephoneEventOutband(int event, int duration_ms) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return channel()->SendTelephoneEventOutband(event, duration_ms) == 0;
}

void ChannelProxy::SetBitrate(int bitrate_bps) {
  // May be called on different threads and needs to be handled by the channel.
  channel()->SetBitRate(bitrate_bps);
}

void ChannelProxy::SetSink(std::unique_ptr<AudioSinkInterface> sink) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel()->SetSink(std::move(sink));
}

void ChannelProxy::SetInputMute(bool muted) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  int error = channel()->SetInputMute(muted);
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::RegisterExternalTransport(Transport* transport) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  int error = channel()->RegisterExternalTransport(transport);
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::DeRegisterExternalTransport() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel()->DeRegisterExternalTransport();
}

bool ChannelProxy::ReceivedRTPPacket(const uint8_t* packet,
                                     size_t length,
                                     const PacketTime& packet_time) {
  // May be called on either worker thread or network thread.
  return channel()->ReceivedRTPPacket(packet, length, packet_time) == 0;
}

bool ChannelProxy::ReceivedRTCPPacket(const uint8_t* packet, size_t length) {
  // May be called on either worker thread or network thread.
  return channel()->ReceivedRTCPPacket(packet, length) == 0;
}

const rtc::scoped_refptr<AudioDecoderFactory>&
    ChannelProxy::GetAudioDecoderFactory() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return channel()->GetAudioDecoderFactory();
}

void ChannelProxy::SetChannelOutputVolumeScaling(float scaling) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  int error = channel()->SetChannelOutputVolumeScaling(scaling);
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::SetRtcEventLog(RtcEventLog* event_log) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel()->SetRtcEventLog(event_log);
}

void ChannelProxy::EnableAudioNetworkAdaptor(const std::string& config_string) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  bool ret = channel()->EnableAudioNetworkAdaptor(config_string);
  RTC_DCHECK(ret);
;}

void ChannelProxy::DisableAudioNetworkAdaptor() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel()->DisableAudioNetworkAdaptor();
}

void ChannelProxy::SetReceiverFrameLengthRange(int min_frame_length_ms,
                                               int max_frame_length_ms) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel()->SetReceiverFrameLengthRange(min_frame_length_ms,
                                         max_frame_length_ms);
}

AudioMixer::Source::AudioFrameInfo ChannelProxy::GetAudioFrameWithInfo(
    int sample_rate_hz,
    AudioFrame* audio_frame) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  return channel()->GetAudioFrameWithInfo(sample_rate_hz, audio_frame);
}

int ChannelProxy::NeededFrequency() const {
  return static_cast<int>(channel()->NeededFrequency(-1));
}

void ChannelProxy::SetTransportOverhead(int transport_overhead_per_packet) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel()->SetTransportOverhead(transport_overhead_per_packet);
}

void ChannelProxy::AssociateSendChannel(
    const ChannelProxy& send_channel_proxy) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel()->set_associate_send_channel(send_channel_proxy.channel_owner_);
}

void ChannelProxy::DisassociateSendChannel() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  channel()->set_associate_send_channel(ChannelOwner(nullptr));
}

Channel* ChannelProxy::channel() const {
  RTC_DCHECK(channel_owner_.channel());
  return channel_owner_.channel();
}

}  // namespace voe
}  // namespace webrtc
