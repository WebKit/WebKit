/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/echo_cancellation_proxy.h"

namespace webrtc {

EchoCancellationProxy::EchoCancellationProxy(
    AudioProcessing* audio_processing,
    EchoCancellation* echo_cancellation)
    : audio_processing_(audio_processing),
      echo_cancellation_(echo_cancellation) {}

EchoCancellationProxy::~EchoCancellationProxy() = default;

int EchoCancellationProxy::Enable(bool enable) {
  // Change the config in APM to mirror the applied settings.
  // TODO(bugs.webrtc.org/9535): Remove the call to EchoCancellation::Enable
  // when APM starts taking the config into account.
  AudioProcessing::Config apm_config = audio_processing_->GetConfig();
  bool aec2_enabled = apm_config.echo_canceller.enabled &&
                      !apm_config.echo_canceller.mobile_mode;
  if ((aec2_enabled && !enable) || (!aec2_enabled && enable)) {
    apm_config.echo_canceller.enabled = enable;
    apm_config.echo_canceller.mobile_mode = false;
    audio_processing_->ApplyConfig(apm_config);
  }
  echo_cancellation_->Enable(enable);
  return AudioProcessing::kNoError;
}

bool EchoCancellationProxy::is_enabled() const {
  return echo_cancellation_->is_enabled();
}

int EchoCancellationProxy::enable_drift_compensation(bool enable) {
  return echo_cancellation_->enable_drift_compensation(enable);
}

bool EchoCancellationProxy::is_drift_compensation_enabled() const {
  return echo_cancellation_->is_drift_compensation_enabled();
}

void EchoCancellationProxy::set_stream_drift_samples(int drift) {
  echo_cancellation_->set_stream_drift_samples(drift);
}

int EchoCancellationProxy::stream_drift_samples() const {
  return echo_cancellation_->stream_drift_samples();
}

int EchoCancellationProxy::set_suppression_level(
    EchoCancellation::SuppressionLevel level) {
  return echo_cancellation_->set_suppression_level(level);
}

EchoCancellation::SuppressionLevel EchoCancellationProxy::suppression_level()
    const {
  return echo_cancellation_->suppression_level();
}

bool EchoCancellationProxy::stream_has_echo() const {
  return echo_cancellation_->stream_has_echo();
}
int EchoCancellationProxy::enable_metrics(bool enable) {
  return echo_cancellation_->enable_metrics(enable);
}
bool EchoCancellationProxy::are_metrics_enabled() const {
  return echo_cancellation_->are_metrics_enabled();
}
int EchoCancellationProxy::GetMetrics(Metrics* metrics) {
  return echo_cancellation_->GetMetrics(metrics);
}
int EchoCancellationProxy::enable_delay_logging(bool enable) {
  return echo_cancellation_->enable_delay_logging(enable);
}
bool EchoCancellationProxy::is_delay_logging_enabled() const {
  return echo_cancellation_->is_delay_logging_enabled();
}
int EchoCancellationProxy::GetDelayMetrics(int* median, int* std) {
  return echo_cancellation_->GetDelayMetrics(median, std);
}
int EchoCancellationProxy::GetDelayMetrics(int* median,
                                           int* std,
                                           float* fraction_poor_delays) {
  return echo_cancellation_->GetDelayMetrics(median, std, fraction_poor_delays);
}

struct AecCore* EchoCancellationProxy::aec_core() const {
  return echo_cancellation_->aec_core();
}

}  // namespace webrtc
