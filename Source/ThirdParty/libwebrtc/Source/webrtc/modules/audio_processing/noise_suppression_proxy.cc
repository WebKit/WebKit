/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/noise_suppression_proxy.h"

namespace webrtc {
NoiseSuppressionProxy::NoiseSuppressionProxy(AudioProcessing* apm,
                                             NoiseSuppression* ns)
    : apm_(apm), ns_(ns) {}

NoiseSuppressionProxy::~NoiseSuppressionProxy() {}

int NoiseSuppressionProxy::Enable(bool enable) {
  AudioProcessing::Config config = apm_->GetConfig();
  if (config.noise_suppression.enabled != enable) {
    config.noise_suppression.enabled = enable;
    apm_->ApplyConfig(config);
  }
  return AudioProcessing::kNoError;
}

bool NoiseSuppressionProxy::is_enabled() const {
  return ns_->is_enabled();
}

int NoiseSuppressionProxy::set_level(Level level) {
  AudioProcessing::Config config = apm_->GetConfig();
  using NsConfig = AudioProcessing::Config::NoiseSuppression;
  NsConfig::Level new_level;
  switch (level) {
    case NoiseSuppression::kLow:
      new_level = NsConfig::kLow;
      break;
    case NoiseSuppression::kModerate:
      new_level = NsConfig::kModerate;
      break;
    case NoiseSuppression::kHigh:
      new_level = NsConfig::kHigh;
      break;
    case NoiseSuppression::kVeryHigh:
      new_level = NsConfig::kVeryHigh;
      break;
    default:
      RTC_NOTREACHED();
  }
  if (config.noise_suppression.level != new_level) {
    config.noise_suppression.level = new_level;
    apm_->ApplyConfig(config);
  }
  return AudioProcessing::kNoError;
}

NoiseSuppression::Level NoiseSuppressionProxy::level() const {
  return ns_->level();
}

float NoiseSuppressionProxy::speech_probability() const {
  return ns_->speech_probability();
}

std::vector<float> NoiseSuppressionProxy::NoiseEstimate() {
  return ns_->NoiseEstimate();
}
}  // namespace webrtc
