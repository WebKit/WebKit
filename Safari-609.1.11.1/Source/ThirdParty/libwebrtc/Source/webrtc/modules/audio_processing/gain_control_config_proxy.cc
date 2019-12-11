/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/gain_control_config_proxy.h"

namespace webrtc {
namespace {

AudioProcessing::Config::GainController1::Mode InterfaceModeToConfigMode(
    GainControl::Mode agc_mode) {
  using AgcConfig = AudioProcessing::Config::GainController1;
  switch (agc_mode) {
    case GainControl::kAdaptiveAnalog:
      return AgcConfig::kAdaptiveAnalog;
    case GainControl::kAdaptiveDigital:
      return AgcConfig::kAdaptiveDigital;
    case GainControl::kFixedDigital:
      return AgcConfig::kFixedDigital;
  }
}
}  // namespace

GainControlConfigProxy::GainControlConfigProxy(
    rtc::CriticalSection* crit_capture,
    AudioProcessing* apm,
    GainControl* agc)
    : crit_capture_(crit_capture), apm_(apm), agc_(agc) {
  RTC_DCHECK(apm);
  RTC_DCHECK(agc);
  RTC_DCHECK(crit_capture);
}

GainControlConfigProxy::~GainControlConfigProxy() = default;

int GainControlConfigProxy::set_stream_analog_level(int level) {
  apm_->set_stream_analog_level(level);
  return AudioProcessing::kNoError;
}

int GainControlConfigProxy::stream_analog_level() const {
  return apm_->recommended_stream_analog_level();
}

int GainControlConfigProxy::Enable(bool enable) {
  auto apm_config = apm_->GetConfig();
  apm_config.gain_controller1.enabled = enable;
  apm_->ApplyConfig(apm_config);
  return AudioProcessing::kNoError;
}

int GainControlConfigProxy::set_mode(Mode mode) {
  auto config = apm_->GetConfig();
  config.gain_controller1.mode = InterfaceModeToConfigMode(mode);
  apm_->ApplyConfig(config);
  return AudioProcessing::kNoError;
}

int GainControlConfigProxy::set_target_level_dbfs(int level) {
  auto config = apm_->GetConfig();
  config.gain_controller1.target_level_dbfs = level;
  apm_->ApplyConfig(config);
  return AudioProcessing::kNoError;
}

int GainControlConfigProxy::set_compression_gain_db(int gain) {
  apm_->SetRuntimeSetting(
      AudioProcessing::RuntimeSetting::CreateCompressionGainDb(gain));
  return AudioProcessing::kNoError;
}

int GainControlConfigProxy::enable_limiter(bool enable) {
  auto config = apm_->GetConfig();
  config.gain_controller1.enable_limiter = enable;
  apm_->ApplyConfig(config);
  return AudioProcessing::kNoError;
}

int GainControlConfigProxy::set_analog_level_limits(int minimum, int maximum) {
  auto config = apm_->GetConfig();
  config.gain_controller1.analog_level_minimum = minimum;
  config.gain_controller1.analog_level_maximum = maximum;
  apm_->ApplyConfig(config);
  return AudioProcessing::kNoError;
}

bool GainControlConfigProxy::is_limiter_enabled() const {
  rtc::CritScope cs_capture(crit_capture_);
  return agc_->is_limiter_enabled();
}

int GainControlConfigProxy::compression_gain_db() const {
  rtc::CritScope cs_capture(crit_capture_);
  return agc_->compression_gain_db();
}

bool GainControlConfigProxy::is_enabled() const {
  rtc::CritScope cs_capture(crit_capture_);
  return agc_->is_enabled();
}

GainControl::Mode GainControlConfigProxy::mode() const {
  rtc::CritScope cs_capture(crit_capture_);
  return agc_->mode();
}

int GainControlConfigProxy::target_level_dbfs() const {
  rtc::CritScope cs_capture(crit_capture_);
  return agc_->target_level_dbfs();
}

int GainControlConfigProxy::analog_level_minimum() const {
  rtc::CritScope cs_capture(crit_capture_);
  return agc_->analog_level_minimum();
}

int GainControlConfigProxy::analog_level_maximum() const {
  rtc::CritScope cs_capture(crit_capture_);
  return agc_->analog_level_maximum();
}

bool GainControlConfigProxy::stream_is_saturated() const {
  rtc::CritScope cs_capture(crit_capture_);
  return agc_->stream_is_saturated();
}
}  // namespace webrtc
