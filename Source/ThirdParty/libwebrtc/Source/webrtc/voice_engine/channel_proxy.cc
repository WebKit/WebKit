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
#include "webrtc/base/logging.h"
#include "webrtc/voice_engine/channel.h"

namespace webrtc {
namespace voe {
ChannelProxy::ChannelProxy() : channel_owner_(nullptr) {}

ChannelProxy::ChannelProxy(const ChannelOwner& channel_owner) :
    channel_owner_(channel_owner) {
  RTC_CHECK(channel_owner_.channel());
  module_process_thread_checker_.DetachFromThread();
}

ChannelProxy::~ChannelProxy() {}

void ChannelProxy::SetRTCPStatus(bool enable) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel()->SetRTCPStatus(enable);
}

void ChannelProxy::SetLocalSSRC(uint32_t ssrc) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  int error = channel()->SetLocalSSRC(ssrc);
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::SetRTCP_CNAME(const std::string& c_name) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  // Note: VoERTP_RTCP::SetRTCP_CNAME() accepts a char[256] array.
  std::string c_name_limited = c_name.substr(0, 255);
  int error = channel()->SetRTCP_CNAME(c_name_limited.c_str());
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::SetNACKStatus(bool enable, int max_packets) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel()->SetNACKStatus(enable, max_packets);
}

void ChannelProxy::SetSendAudioLevelIndicationStatus(bool enable, int id) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  int error = channel()->SetSendAudioLevelIndicationStatus(enable, id);
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::SetReceiveAudioLevelIndicationStatus(bool enable, int id) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  int error = channel()->SetReceiveAudioLevelIndicationStatus(enable, id);
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::EnableSendTransportSequenceNumber(int id) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel()->EnableSendTransportSequenceNumber(id);
}

void ChannelProxy::EnableReceiveTransportSequenceNumber(int id) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel()->EnableReceiveTransportSequenceNumber(id);
}

void ChannelProxy::RegisterSenderCongestionControlObjects(
    RtpPacketSender* rtp_packet_sender,
    TransportFeedbackObserver* transport_feedback_observer,
    PacketRouter* packet_router,
    RtcpBandwidthObserver* bandwidth_observer) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel()->RegisterSenderCongestionControlObjects(
      rtp_packet_sender, transport_feedback_observer, packet_router,
      bandwidth_observer);
}

void ChannelProxy::RegisterReceiverCongestionControlObjects(
    PacketRouter* packet_router) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel()->RegisterReceiverCongestionControlObjects(packet_router);
}

void ChannelProxy::ResetCongestionControlObjects() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel()->ResetCongestionControlObjects();
}

CallStatistics ChannelProxy::GetRTCPStatistics() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  CallStatistics stats = {0};
  int error = channel()->GetRTPStatistics(stats);
  RTC_DCHECK_EQ(0, error);
  return stats;
}

std::vector<ReportBlock> ChannelProxy::GetRemoteRTCPReportBlocks() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  std::vector<webrtc::ReportBlock> blocks;
  int error = channel()->GetRemoteRTCPReportBlocks(&blocks);
  RTC_DCHECK_EQ(0, error);
  return blocks;
}

NetworkStatistics ChannelProxy::GetNetworkStatistics() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  NetworkStatistics stats = {0};
  int error = channel()->GetNetworkStatistics(stats);
  RTC_DCHECK_EQ(0, error);
  return stats;
}

AudioDecodingCallStats ChannelProxy::GetDecodingCallStatistics() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  AudioDecodingCallStats stats;
  channel()->GetDecodingCallStatistics(&stats);
  return stats;
}

int32_t ChannelProxy::GetSpeechOutputLevel() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  uint32_t level = 0;
  int error = channel()->GetSpeechOutputLevel(level);
  RTC_DCHECK_EQ(0, error);
  return static_cast<int32_t>(level);
}

int32_t ChannelProxy::GetSpeechOutputLevelFullRange() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  uint32_t level = 0;
  int error = channel()->GetSpeechOutputLevelFullRange(level);
  RTC_DCHECK_EQ(0, error);
  return static_cast<int32_t>(level);
}

uint32_t ChannelProxy::GetDelayEstimate() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread() ||
             module_process_thread_checker_.CalledOnValidThread());
  return channel()->GetDelayEstimate();
}

bool ChannelProxy::SetSendTelephoneEventPayloadType(int payload_type,
                                                    int payload_frequency) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel()->SetSendTelephoneEventPayloadType(payload_type,
                                                     payload_frequency) == 0;
}

bool ChannelProxy::SendTelephoneEventOutband(int event, int duration_ms) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel()->SendTelephoneEventOutband(event, duration_ms) == 0;
}

void ChannelProxy::SetBitrate(int bitrate_bps, int64_t probing_interval_ms) {
  // This method can be called on the worker thread, module process thread
  // or on a TaskQueue via VideoSendStreamImpl::OnEncoderConfigurationChanged.
  // TODO(solenberg): Figure out a good way to check this or enforce calling
  // rules.
  // RTC_DCHECK(worker_thread_checker_.CalledOnValidThread() ||
  //            module_process_thread_checker_.CalledOnValidThread());
  channel()->SetBitRate(bitrate_bps, probing_interval_ms);
}

void ChannelProxy::SetRecPayloadType(int payload_type,
                                     const SdpAudioFormat& format) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  const int result = channel()->SetRecPayloadType(payload_type, format);
  RTC_DCHECK_EQ(0, result);
}

void ChannelProxy::SetSink(std::unique_ptr<AudioSinkInterface> sink) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel()->SetSink(std::move(sink));
}

void ChannelProxy::SetInputMute(bool muted) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  int error = channel()->SetInputMute(muted);
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::RegisterExternalTransport(Transport* transport) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  int error = channel()->RegisterExternalTransport(transport);
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::DeRegisterExternalTransport() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel()->DeRegisterExternalTransport();
}

void ChannelProxy::OnRtpPacket(const RtpPacketReceived& packet) {
  // May be called on either worker thread or network thread.
  channel()->OnRtpPacket(packet);
}

bool ChannelProxy::ReceivedRTCPPacket(const uint8_t* packet, size_t length) {
  // May be called on either worker thread or network thread.
  return channel()->ReceivedRTCPPacket(packet, length) == 0;
}

const rtc::scoped_refptr<AudioDecoderFactory>&
    ChannelProxy::GetAudioDecoderFactory() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel()->GetAudioDecoderFactory();
}

void ChannelProxy::SetChannelOutputVolumeScaling(float scaling) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  int error = channel()->SetChannelOutputVolumeScaling(scaling);
  RTC_DCHECK_EQ(0, error);
}

void ChannelProxy::SetRtcEventLog(RtcEventLog* event_log) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel()->SetRtcEventLog(event_log);
}

void ChannelProxy::EnableAudioNetworkAdaptor(const std::string& config_string) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  bool ret = channel()->EnableAudioNetworkAdaptor(config_string);
  RTC_DCHECK(ret);
;}

void ChannelProxy::DisableAudioNetworkAdaptor() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel()->DisableAudioNetworkAdaptor();
}

void ChannelProxy::SetReceiverFrameLengthRange(int min_frame_length_ms,
                                               int max_frame_length_ms) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel()->SetReceiverFrameLengthRange(min_frame_length_ms,
                                         max_frame_length_ms);
}

AudioMixer::Source::AudioFrameInfo ChannelProxy::GetAudioFrameWithInfo(
    int sample_rate_hz,
    AudioFrame* audio_frame) {
  RTC_DCHECK_RUNS_SERIALIZED(&audio_thread_race_checker_);
  return channel()->GetAudioFrameWithInfo(sample_rate_hz, audio_frame);
}

int ChannelProxy::NeededFrequency() const {
  RTC_DCHECK_RUNS_SERIALIZED(&audio_thread_race_checker_);
  return static_cast<int>(channel()->NeededFrequency(-1));
}

void ChannelProxy::SetTransportOverhead(int transport_overhead_per_packet) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel()->SetTransportOverhead(transport_overhead_per_packet);
}

void ChannelProxy::AssociateSendChannel(
    const ChannelProxy& send_channel_proxy) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel()->set_associate_send_channel(send_channel_proxy.channel_owner_);
}

void ChannelProxy::DisassociateSendChannel() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel()->set_associate_send_channel(ChannelOwner(nullptr));
}

void ChannelProxy::GetRtpRtcp(RtpRtcp** rtp_rtcp,
                              RtpReceiver** rtp_receiver) const {
  RTC_DCHECK(module_process_thread_checker_.CalledOnValidThread());
  RTC_DCHECK(rtp_rtcp);
  RTC_DCHECK(rtp_receiver);
  int error = channel()->GetRtpRtcp(rtp_rtcp, rtp_receiver);
  RTC_DCHECK_EQ(0, error);
}

uint32_t ChannelProxy::GetPlayoutTimestamp() const {
  RTC_DCHECK_RUNS_SERIALIZED(&video_capture_thread_race_checker_);
  unsigned int timestamp = 0;
  int error = channel()->GetPlayoutTimestamp(timestamp);
  RTC_DCHECK(!error || timestamp == 0);
  return timestamp;
}

void ChannelProxy::SetMinimumPlayoutDelay(int delay_ms) {
  RTC_DCHECK(module_process_thread_checker_.CalledOnValidThread());
  // Limit to range accepted by both VoE and ACM, so we're at least getting as
  // close as possible, instead of failing.
  delay_ms = std::max(0, std::min(delay_ms, 10000));
  int error = channel()->SetMinimumPlayoutDelay(delay_ms);
  if (0 != error) {
    LOG(LS_WARNING) << "Error setting minimum playout delay.";
  }
}

void ChannelProxy::SetRtcpRttStats(RtcpRttStats* rtcp_rtt_stats) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel()->SetRtcpRttStats(rtcp_rtt_stats);
}

bool ChannelProxy::GetRecCodec(CodecInst* codec_inst) const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel()->GetRecCodec(*codec_inst) == 0;
}

bool ChannelProxy::GetSendCodec(CodecInst* codec_inst) const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel()->GetSendCodec(*codec_inst) == 0;
}

bool ChannelProxy::SetVADStatus(bool enable) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel()->SetVADStatus(enable, VADNormal, false) == 0;
}

bool ChannelProxy::SetCodecFECStatus(bool enable) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel()->SetCodecFECStatus(enable) == 0;
}

bool ChannelProxy::SetOpusDtx(bool enable) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel()->SetOpusDtx(enable) == 0;
}

bool ChannelProxy::SetOpusMaxPlaybackRate(int frequency_hz) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel()->SetOpusMaxPlaybackRate(frequency_hz) == 0;
}

bool ChannelProxy::SetSendCodec(const CodecInst& codec_inst) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  // Validation code copied from VoECodecImpl::SetSendCodec().
  if ((STR_CASE_CMP(codec_inst.plname, "L16") == 0) &&
      (codec_inst.pacsize >= 960)) {
    return false;
  }
  if (!STR_CASE_CMP(codec_inst.plname, "CN") ||
      !STR_CASE_CMP(codec_inst.plname, "TELEPHONE-EVENT") ||
      !STR_CASE_CMP(codec_inst.plname, "RED")) {
    return false;
  }
  if ((codec_inst.channels != 1) && (codec_inst.channels != 2)) {
    return false;
  }
  if (!AudioCodingModule::IsCodecValid(codec_inst)) {
    return false;
  }
  return channel()->SetSendCodec(codec_inst) == 0;
}

bool ChannelProxy::SetSendCNPayloadType(int type,
                                        PayloadFrequencies frequency) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  // Validation code copied from VoECodecImpl::SetSendCNPayloadType().
  if (type < 96 || type > 127) {
    // Only allow dynamic range: 96 to 127
    return false;
  }
  if ((frequency != kFreq16000Hz) && (frequency != kFreq32000Hz)) {
    // It is not possible to modify the payload type for CN/8000.
    // We only allow modification of the CN payload type for CN/16000
    // and CN/32000.
    return false;
  }
  return channel()->SetSendCNPayloadType(type, frequency) == 0;
}

Channel* ChannelProxy::channel() const {
  RTC_DCHECK(channel_owner_.channel());
  return channel_owner_.channel();
}

}  // namespace voe
}  // namespace webrtc
