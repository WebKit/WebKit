/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/apm_helpers.h"

#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace apm_helpers {

void Init(AudioProcessing* apm) {
  RTC_DCHECK(apm);

  constexpr int kMinVolumeLevel = 0;
  constexpr int kMaxVolumeLevel = 255;

  // This is the initialization which used to happen in VoEBase::Init(), but
  // which is not covered by the WVoE::ApplyOptions().
  if (apm->echo_cancellation()->enable_drift_compensation(false) != 0) {
    RTC_DLOG(LS_ERROR) << "Failed to disable drift compensation.";
  }
  GainControl* gc = apm->gain_control();
  if (gc->set_analog_level_limits(kMinVolumeLevel, kMaxVolumeLevel) != 0) {
    RTC_DLOG(LS_ERROR) << "Failed to set analog level limits with minimum: "
                       << kMinVolumeLevel
                       << " and maximum: " << kMaxVolumeLevel;
  }
}

AgcConfig GetAgcConfig(AudioProcessing* apm) {
  RTC_DCHECK(apm);
  AgcConfig result;
  result.targetLeveldBOv = apm->gain_control()->target_level_dbfs();
  result.digitalCompressionGaindB = apm->gain_control()->compression_gain_db();
  result.limiterEnable = apm->gain_control()->is_limiter_enabled();
  return result;
}

void SetAgcConfig(AudioProcessing* apm, const AgcConfig& config) {
  RTC_DCHECK(apm);
  GainControl* gc = apm->gain_control();
  if (gc->set_target_level_dbfs(config.targetLeveldBOv) != 0) {
    RTC_LOG(LS_ERROR) << "Failed to set target level: "
                      << config.targetLeveldBOv;
  }
  if (gc->set_compression_gain_db(config.digitalCompressionGaindB) != 0) {
    RTC_LOG(LS_ERROR) << "Failed to set compression gain: "
                      << config.digitalCompressionGaindB;
  }
  if (gc->enable_limiter(config.limiterEnable) != 0) {
    RTC_LOG(LS_ERROR) << "Failed to set limiter on/off: "
                      << config.limiterEnable;
  }
}

void SetAgcStatus(AudioProcessing* apm, bool enable) {
  RTC_DCHECK(apm);
#if defined(WEBRTC_IOS) || defined(WEBRTC_ANDROID)
  GainControl::Mode agc_mode = GainControl::kFixedDigital;
#else
  GainControl::Mode agc_mode = GainControl::kAdaptiveAnalog;
#endif
  GainControl* gc = apm->gain_control();
  if (gc->set_mode(agc_mode) != 0) {
    RTC_LOG(LS_ERROR) << "Failed to set AGC mode: " << agc_mode;
    return;
  }
  if (gc->Enable(enable) != 0) {
    RTC_LOG(LS_ERROR) << "Failed to enable/disable AGC: " << enable;
    return;
  }
  RTC_LOG(LS_INFO) << "AGC set to " << enable << " with mode " << agc_mode;
}

void SetEcStatus(AudioProcessing* apm, bool enable, EcModes mode) {
  RTC_DCHECK(apm);
  RTC_DCHECK(mode == kEcConference || mode == kEcAecm) << "mode: " << mode;
  EchoCancellation* ec = apm->echo_cancellation();
  EchoControlMobile* ecm = apm->echo_control_mobile();
  if (mode == kEcConference) {
    // Disable the AECM before enabling the AEC.
    if (enable && ecm->is_enabled() && ecm->Enable(false) != 0) {
      RTC_LOG(LS_ERROR) << "Failed to disable AECM.";
      return;
    }
    if (ec->Enable(enable) != 0) {
      RTC_LOG(LS_ERROR) << "Failed to enable/disable AEC: " << enable;
      return;
    }
    if (ec->set_suppression_level(EchoCancellation::kHighSuppression) != 0) {
      RTC_LOG(LS_ERROR) << "Failed to set high AEC aggressiveness.";
      return;
    }
  } else {
    // Disable the AEC before enabling the AECM.
    if (enable && ec->is_enabled() && ec->Enable(false) != 0) {
      RTC_LOG(LS_ERROR) << "Failed to disable AEC.";
      return;
    }
    if (ecm->Enable(enable) != 0) {
      RTC_LOG(LS_ERROR) << "Failed to enable/disable AECM: " << enable;
      return;
    }
  }
  RTC_LOG(LS_INFO) << "Echo control set to " << enable << " with mode " << mode;
}

void SetEcMetricsStatus(AudioProcessing* apm, bool enable) {
  RTC_DCHECK(apm);
  if ((apm->echo_cancellation()->enable_metrics(enable) != 0) ||
      (apm->echo_cancellation()->enable_delay_logging(enable) != 0)) {
    RTC_LOG(LS_ERROR) << "Failed to enable/disable EC metrics: " << enable;
    return;
  }
  RTC_LOG(LS_INFO) << "EC metrics set to " << enable;
}

void SetAecmMode(AudioProcessing* apm, bool enable) {
  RTC_DCHECK(apm);
  EchoControlMobile* ecm = apm->echo_control_mobile();
  RTC_DCHECK_EQ(EchoControlMobile::kSpeakerphone, ecm->routing_mode());
  if (ecm->enable_comfort_noise(enable) != 0) {
    RTC_LOG(LS_ERROR) << "Failed to enable/disable CNG: " << enable;
    return;
  }
  RTC_LOG(LS_INFO) << "CNG set to " << enable;
}

void SetNsStatus(AudioProcessing* apm, bool enable) {
  RTC_DCHECK(apm);
  NoiseSuppression* ns = apm->noise_suppression();
  if (ns->set_level(NoiseSuppression::kHigh) != 0) {
    RTC_LOG(LS_ERROR) << "Failed to set high NS level.";
    return;
  }
  if (ns->Enable(enable) != 0) {
    RTC_LOG(LS_ERROR) << "Failed to enable/disable NS: " << enable;
    return;
  }
  RTC_LOG(LS_INFO) << "NS set to " << enable;
}

void SetTypingDetectionStatus(AudioProcessing* apm, bool enable) {
  RTC_DCHECK(apm);
  VoiceDetection* vd = apm->voice_detection();
  if (vd->Enable(enable)) {
    RTC_LOG(LS_ERROR) << "Failed to enable/disable VAD: " << enable;
    return;
  }
  if (vd->set_likelihood(VoiceDetection::kVeryLowLikelihood)) {
    RTC_LOG(LS_ERROR) << "Failed to set low VAD likelihood.";
    return;
  }
  RTC_LOG(LS_INFO) << "VAD set to " << enable << " for typing detection.";
}
}  // namespace apm_helpers
}  // namespace webrtc
