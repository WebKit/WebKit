/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/channel_receive_proxy.h"

#include <utility>

#include "api/call/audio_sink.h"
#include "audio/channel_send_proxy.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace voe {
ChannelReceiveProxy::ChannelReceiveProxy() {}

ChannelReceiveProxy::ChannelReceiveProxy(
    std::unique_ptr<ChannelReceive> channel)
    : channel_(std::move(channel)) {
  RTC_DCHECK(channel_);
  module_process_thread_checker_.DetachFromThread();
}

ChannelReceiveProxy::~ChannelReceiveProxy() {}

void ChannelReceiveProxy::SetLocalSSRC(uint32_t ssrc) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  int error = channel_->SetLocalSSRC(ssrc);
  RTC_DCHECK_EQ(0, error);
}

void ChannelReceiveProxy::SetNACKStatus(bool enable, int max_packets) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->SetNACKStatus(enable, max_packets);
}

CallReceiveStatistics ChannelReceiveProxy::GetRTCPStatistics() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  CallReceiveStatistics stats = {0};
  int error = channel_->GetRTPStatistics(stats);
  RTC_DCHECK_EQ(0, error);
  return stats;
}

bool ChannelReceiveProxy::ReceivedRTCPPacket(const uint8_t* packet,
                                             size_t length) {
  // May be called on either worker thread or network thread.
  return channel_->ReceivedRTCPPacket(packet, length) == 0;
}

void ChannelReceiveProxy::RegisterReceiverCongestionControlObjects(
    PacketRouter* packet_router) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->RegisterReceiverCongestionControlObjects(packet_router);
}

void ChannelReceiveProxy::ResetReceiverCongestionControlObjects() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->ResetReceiverCongestionControlObjects();
}

NetworkStatistics ChannelReceiveProxy::GetNetworkStatistics() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  NetworkStatistics stats = {0};
  int error = channel_->GetNetworkStatistics(stats);
  RTC_DCHECK_EQ(0, error);
  return stats;
}

AudioDecodingCallStats ChannelReceiveProxy::GetDecodingCallStatistics() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  AudioDecodingCallStats stats;
  channel_->GetDecodingCallStatistics(&stats);
  return stats;
}

int ChannelReceiveProxy::GetSpeechOutputLevelFullRange() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel_->GetSpeechOutputLevelFullRange();
}

double ChannelReceiveProxy::GetTotalOutputEnergy() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel_->GetTotalOutputEnergy();
}

double ChannelReceiveProxy::GetTotalOutputDuration() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel_->GetTotalOutputDuration();
}

uint32_t ChannelReceiveProxy::GetDelayEstimate() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread() ||
             module_process_thread_checker_.CalledOnValidThread());
  return channel_->GetDelayEstimate();
}

void ChannelReceiveProxy::SetReceiveCodecs(
    const std::map<int, SdpAudioFormat>& codecs) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->SetReceiveCodecs(codecs);
}

void ChannelReceiveProxy::SetSink(AudioSinkInterface* sink) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->SetSink(sink);
}

void ChannelReceiveProxy::OnRtpPacket(const RtpPacketReceived& packet) {
  // May be called on either worker thread or network thread.
  channel_->OnRtpPacket(packet);
}

void ChannelReceiveProxy::SetChannelOutputVolumeScaling(float scaling) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->SetChannelOutputVolumeScaling(scaling);
}

AudioMixer::Source::AudioFrameInfo ChannelReceiveProxy::GetAudioFrameWithInfo(
    int sample_rate_hz,
    AudioFrame* audio_frame) {
  RTC_DCHECK_RUNS_SERIALIZED(&audio_thread_race_checker_);
  return channel_->GetAudioFrameWithInfo(sample_rate_hz, audio_frame);
}

int ChannelReceiveProxy::PreferredSampleRate() const {
  RTC_DCHECK_RUNS_SERIALIZED(&audio_thread_race_checker_);
  return channel_->PreferredSampleRate();
}

void ChannelReceiveProxy::AssociateSendChannel(
    const ChannelSendProxy& send_channel_proxy) {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->SetAssociatedSendChannel(send_channel_proxy.GetChannel());
}

void ChannelReceiveProxy::DisassociateSendChannel() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  channel_->SetAssociatedSendChannel(nullptr);
}

absl::optional<Syncable::Info> ChannelReceiveProxy::GetSyncInfo() const {
  RTC_DCHECK(module_process_thread_checker_.CalledOnValidThread());
  return channel_->GetSyncInfo();
}

uint32_t ChannelReceiveProxy::GetPlayoutTimestamp() const {
  RTC_DCHECK_RUNS_SERIALIZED(&video_capture_thread_race_checker_);
  unsigned int timestamp = 0;
  int error = channel_->GetPlayoutTimestamp(timestamp);
  RTC_DCHECK(!error || timestamp == 0);
  return timestamp;
}

void ChannelReceiveProxy::SetMinimumPlayoutDelay(int delay_ms) {
  RTC_DCHECK(module_process_thread_checker_.CalledOnValidThread());
  // Limit to range accepted by both VoE and ACM, so we're at least getting as
  // close as possible, instead of failing.
  delay_ms = rtc::SafeClamp(delay_ms, 0, 10000);
  int error = channel_->SetMinimumPlayoutDelay(delay_ms);
  if (0 != error) {
    RTC_LOG(LS_WARNING) << "Error setting minimum playout delay.";
  }
}

bool ChannelReceiveProxy::GetRecCodec(CodecInst* codec_inst) const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel_->GetRecCodec(*codec_inst) == 0;
}

std::vector<RtpSource> ChannelReceiveProxy::GetSources() const {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  return channel_->GetSources();
}

void ChannelReceiveProxy::StartPlayout() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  int error = channel_->StartPlayout();
  RTC_DCHECK_EQ(0, error);
}

void ChannelReceiveProxy::StopPlayout() {
  RTC_DCHECK(worker_thread_checker_.CalledOnValidThread());
  int error = channel_->StopPlayout();
  RTC_DCHECK_EQ(0, error);
}
}  // namespace voe
}  // namespace webrtc
