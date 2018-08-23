/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/echo_control_mobile_proxy.h"

namespace webrtc {

EchoControlMobileProxy::EchoControlMobileProxy(
    AudioProcessing* audio_processing,
    EchoControlMobile* echo_control_mobile)
    : audio_processing_(audio_processing),
      echo_control_mobile_(echo_control_mobile) {}

EchoControlMobileProxy::~EchoControlMobileProxy() = default;

int EchoControlMobileProxy::Enable(bool enable) {
  // Change the config in APM to mirror the applied settings.
  // TODO(bugs.webrtc.org/9535): Remove the call to EchoControlMobile::Enable
  // when APM starts taking the config into account.
  AudioProcessing::Config apm_config = audio_processing_->GetConfig();
  bool aecm_enabled = apm_config.echo_canceller.enabled &&
                      apm_config.echo_canceller.mobile_mode;
  if ((aecm_enabled && !enable) || (!aecm_enabled && enable)) {
    apm_config.echo_canceller.enabled = enable;
    apm_config.echo_canceller.mobile_mode = true;
    audio_processing_->ApplyConfig(apm_config);
  }
  echo_control_mobile_->Enable(enable);
  return AudioProcessing::kNoError;
}

bool EchoControlMobileProxy::is_enabled() const {
  return echo_control_mobile_->is_enabled();
}

int EchoControlMobileProxy::set_routing_mode(RoutingMode mode) {
  return echo_control_mobile_->set_routing_mode(mode);
}

EchoControlMobile::RoutingMode EchoControlMobileProxy::routing_mode() const {
  return echo_control_mobile_->routing_mode();
}

int EchoControlMobileProxy::enable_comfort_noise(bool enable) {
  return echo_control_mobile_->enable_comfort_noise(enable);
}

bool EchoControlMobileProxy::is_comfort_noise_enabled() const {
  return echo_control_mobile_->is_comfort_noise_enabled();
}

int EchoControlMobileProxy::SetEchoPath(const void* echo_path,
                                        size_t size_bytes) {
  return echo_control_mobile_->SetEchoPath(echo_path, size_bytes);
}

int EchoControlMobileProxy::GetEchoPath(void* echo_path,
                                        size_t size_bytes) const {
  return echo_control_mobile_->GetEchoPath(echo_path, size_bytes);
}

}  // namespace webrtc
