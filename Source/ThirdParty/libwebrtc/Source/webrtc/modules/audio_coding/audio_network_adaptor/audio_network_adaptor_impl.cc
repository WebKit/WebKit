/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/audio_network_adaptor/audio_network_adaptor_impl.h"

#include <utility>

namespace webrtc {

AudioNetworkAdaptorImpl::Config::Config() = default;

AudioNetworkAdaptorImpl::Config::~Config() = default;

AudioNetworkAdaptorImpl::AudioNetworkAdaptorImpl(
    const Config& config,
    std::unique_ptr<ControllerManager> controller_manager,
    std::unique_ptr<DebugDumpWriter> debug_dump_writer)
    : config_(config),
      controller_manager_(std::move(controller_manager)),
      debug_dump_writer_(std::move(debug_dump_writer)) {
  RTC_DCHECK(controller_manager_);
}

AudioNetworkAdaptorImpl::~AudioNetworkAdaptorImpl() = default;

void AudioNetworkAdaptorImpl::SetUplinkBandwidth(int uplink_bandwidth_bps) {
  last_metrics_.uplink_bandwidth_bps = rtc::Optional<int>(uplink_bandwidth_bps);
  DumpNetworkMetrics();
}

void AudioNetworkAdaptorImpl::SetUplinkPacketLossFraction(
    float uplink_packet_loss_fraction) {
  last_metrics_.uplink_packet_loss_fraction =
      rtc::Optional<float>(uplink_packet_loss_fraction);
  DumpNetworkMetrics();
}

void AudioNetworkAdaptorImpl::SetTargetAudioBitrate(
    int target_audio_bitrate_bps) {
  last_metrics_.target_audio_bitrate_bps =
      rtc::Optional<int>(target_audio_bitrate_bps);
  DumpNetworkMetrics();
}

void AudioNetworkAdaptorImpl::SetRtt(int rtt_ms) {
  last_metrics_.rtt_ms = rtc::Optional<int>(rtt_ms);
  DumpNetworkMetrics();
}

AudioNetworkAdaptor::EncoderRuntimeConfig
AudioNetworkAdaptorImpl::GetEncoderRuntimeConfig() {
  EncoderRuntimeConfig config;
  for (auto& controller :
       controller_manager_->GetSortedControllers(last_metrics_))
    controller->MakeDecision(last_metrics_, &config);

  // TODO(minyue): Add debug dumping.
  if (debug_dump_writer_)
    debug_dump_writer_->DumpEncoderRuntimeConfig(
        config, config_.clock->TimeInMilliseconds());

  return config;
}

void AudioNetworkAdaptorImpl::StartDebugDump(FILE* file_handle) {
  debug_dump_writer_ = DebugDumpWriter::Create(file_handle);
}

void AudioNetworkAdaptorImpl::StopDebugDump() {
  debug_dump_writer_.reset(nullptr);
}

void AudioNetworkAdaptorImpl::DumpNetworkMetrics() {
  if (debug_dump_writer_)
    debug_dump_writer_->DumpNetworkMetrics(last_metrics_,
                                           config_.clock->TimeInMilliseconds());
}

}  // namespace webrtc
