/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/audio_network_adaptor/audio_network_adaptor_impl.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <utility>

#include "api/audio_codecs/audio_encoder.h"
#include "api/environment/environment.h"
#include "modules/audio_coding/audio_network_adaptor/controller.h"
#include "modules/audio_coding/audio_network_adaptor/controller_manager.h"
#include "modules/audio_coding/audio_network_adaptor/debug_dump_writer.h"
#include "modules/audio_coding/audio_network_adaptor/event_log_writer.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor_config.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {
constexpr int kEventLogMinBitrateChangeBps = 5000;
constexpr float kEventLogMinBitrateChangeFraction = 0.25;
constexpr float kEventLogMinPacketLossChangeFraction = 0.5;
}  // namespace

AudioNetworkAdaptorImpl::AudioNetworkAdaptorImpl(
    const Environment& env,
    std::unique_ptr<ControllerManager> controller_manager,
    std::unique_ptr<DebugDumpWriter> debug_dump_writer)
    : env_(env),
      controller_manager_(std::move(controller_manager)),
      debug_dump_writer_(std::move(debug_dump_writer)),
      event_log_writer_(std::make_unique<EventLogWriter>(
          &env_.event_log(),
          kEventLogMinBitrateChangeBps,
          kEventLogMinBitrateChangeFraction,
          kEventLogMinPacketLossChangeFraction)) {
  RTC_DCHECK(controller_manager_);
}

AudioNetworkAdaptorImpl::~AudioNetworkAdaptorImpl() = default;

void AudioNetworkAdaptorImpl::SetUplinkBandwidth(int uplink_bandwidth_bps) {
  last_metrics_.uplink_bandwidth_bps = uplink_bandwidth_bps;
  DumpNetworkMetrics();

  Controller::NetworkMetrics network_metrics;
  network_metrics.uplink_bandwidth_bps = uplink_bandwidth_bps;
  UpdateNetworkMetrics(network_metrics);
}

void AudioNetworkAdaptorImpl::SetUplinkPacketLossFraction(
    float uplink_packet_loss_fraction) {
  last_metrics_.uplink_packet_loss_fraction = uplink_packet_loss_fraction;
  DumpNetworkMetrics();

  Controller::NetworkMetrics network_metrics;
  network_metrics.uplink_packet_loss_fraction = uplink_packet_loss_fraction;
  UpdateNetworkMetrics(network_metrics);
}

void AudioNetworkAdaptorImpl::SetRtt(int rtt_ms) {
  last_metrics_.rtt_ms = rtt_ms;
  DumpNetworkMetrics();

  Controller::NetworkMetrics network_metrics;
  network_metrics.rtt_ms = rtt_ms;
  UpdateNetworkMetrics(network_metrics);
}

void AudioNetworkAdaptorImpl::SetTargetAudioBitrate(
    int target_audio_bitrate_bps) {
  last_metrics_.target_audio_bitrate_bps = target_audio_bitrate_bps;
  DumpNetworkMetrics();

  Controller::NetworkMetrics network_metrics;
  network_metrics.target_audio_bitrate_bps = target_audio_bitrate_bps;
  UpdateNetworkMetrics(network_metrics);
}

void AudioNetworkAdaptorImpl::SetOverhead(size_t overhead_bytes_per_packet) {
  last_metrics_.overhead_bytes_per_packet = overhead_bytes_per_packet;
  DumpNetworkMetrics();

  Controller::NetworkMetrics network_metrics;
  network_metrics.overhead_bytes_per_packet = overhead_bytes_per_packet;
  UpdateNetworkMetrics(network_metrics);
}

AudioEncoderRuntimeConfig AudioNetworkAdaptorImpl::GetEncoderRuntimeConfig() {
  AudioEncoderRuntimeConfig config;
  for (auto& controller :
       controller_manager_->GetSortedControllers(last_metrics_))
    controller->MakeDecision(&config);

  // Update ANA stats.
  auto increment_opt = [](std::optional<uint32_t>& a) {
    a = a.value_or(0) + 1;
  };
  if (prev_config_) {
    if (config.bitrate_bps != prev_config_->bitrate_bps) {
      increment_opt(stats_.bitrate_action_counter);
    }
    if (config.enable_dtx != prev_config_->enable_dtx) {
      increment_opt(stats_.dtx_action_counter);
    }
    if (config.enable_fec != prev_config_->enable_fec) {
      increment_opt(stats_.fec_action_counter);
    }
    if (config.frame_length_ms && prev_config_->frame_length_ms) {
      if (*config.frame_length_ms > *prev_config_->frame_length_ms) {
        increment_opt(stats_.frame_length_increase_counter);
      } else if (*config.frame_length_ms < *prev_config_->frame_length_ms) {
        increment_opt(stats_.frame_length_decrease_counter);
      }
    }
    if (config.num_channels != prev_config_->num_channels) {
      increment_opt(stats_.channel_action_counter);
    }
    if (config.uplink_packet_loss_fraction) {
      stats_.uplink_packet_loss_fraction = *config.uplink_packet_loss_fraction;
    }
  }
  prev_config_ = config;

  if (debug_dump_writer_) {
    debug_dump_writer_->DumpEncoderRuntimeConfig(
        config, env_.clock().TimeInMilliseconds());
  }

  event_log_writer_->MaybeLogEncoderConfig(config);

  return config;
}

void AudioNetworkAdaptorImpl::StartDebugDump(FILE* file_handle) {
  debug_dump_writer_ = DebugDumpWriter::Create(file_handle);
}

void AudioNetworkAdaptorImpl::StopDebugDump() {
  debug_dump_writer_.reset(nullptr);
}

ANAStats AudioNetworkAdaptorImpl::GetStats() const {
  return stats_;
}

void AudioNetworkAdaptorImpl::DumpNetworkMetrics() {
  if (debug_dump_writer_) {
    debug_dump_writer_->DumpNetworkMetrics(last_metrics_,
                                           env_.clock().TimeInMilliseconds());
  }
}

void AudioNetworkAdaptorImpl::UpdateNetworkMetrics(
    const Controller::NetworkMetrics& network_metrics) {
  for (auto& controller : controller_manager_->GetControllers())
    controller->UpdateNetworkMetrics(network_metrics);
}

}  // namespace webrtc
