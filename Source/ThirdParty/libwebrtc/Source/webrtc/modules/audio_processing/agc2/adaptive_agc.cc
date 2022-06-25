/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/adaptive_agc.h"

#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/cpu_features.h"
#include "modules/audio_processing/agc2/vad_with_level.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {

// Detects the available CPU features and applies any kill-switches.
AvailableCpuFeatures GetAllowedCpuFeatures() {
  AvailableCpuFeatures features = GetAvailableCpuFeatures();
  if (field_trial::IsEnabled("WebRTC-Agc2SimdSse2KillSwitch")) {
    features.sse2 = false;
  }
  if (field_trial::IsEnabled("WebRTC-Agc2SimdAvx2KillSwitch")) {
    features.avx2 = false;
  }
  if (field_trial::IsEnabled("WebRTC-Agc2SimdNeonKillSwitch")) {
    features.neon = false;
  }
  return features;
}

}  // namespace

AdaptiveAgc::AdaptiveAgc(
    ApmDataDumper* apm_data_dumper,
    const AudioProcessing::Config::GainController2::AdaptiveDigital& config)
    : speech_level_estimator_(apm_data_dumper, config),
      vad_(config.vad_reset_period_ms, GetAllowedCpuFeatures()),
      gain_controller_(apm_data_dumper, config),
      apm_data_dumper_(apm_data_dumper),
      noise_level_estimator_(CreateNoiseFloorEstimator(apm_data_dumper)),
      saturation_protector_(
          CreateSaturationProtector(kSaturationProtectorInitialHeadroomDb,
                                    config.adjacent_speech_frames_threshold,
                                    apm_data_dumper)) {
  RTC_DCHECK(apm_data_dumper);
  RTC_DCHECK(noise_level_estimator_);
  RTC_DCHECK(saturation_protector_);
}

AdaptiveAgc::~AdaptiveAgc() = default;

void AdaptiveAgc::Initialize(int sample_rate_hz, int num_channels) {
  gain_controller_.Initialize(sample_rate_hz, num_channels);
}

void AdaptiveAgc::Process(AudioFrameView<float> frame, float limiter_envelope) {
  AdaptiveDigitalGainApplier::FrameInfo info;

  VadLevelAnalyzer::Result vad_result = vad_.AnalyzeFrame(frame);
  info.speech_probability = vad_result.speech_probability;
  apm_data_dumper_->DumpRaw("agc2_speech_probability",
                            vad_result.speech_probability);
  apm_data_dumper_->DumpRaw("agc2_input_rms_dbfs", vad_result.rms_dbfs);
  apm_data_dumper_->DumpRaw("agc2_input_peak_dbfs", vad_result.peak_dbfs);

  speech_level_estimator_.Update(vad_result);
  info.speech_level_dbfs = speech_level_estimator_.level_dbfs();
  info.speech_level_reliable = speech_level_estimator_.IsConfident();
  apm_data_dumper_->DumpRaw("agc2_speech_level_dbfs", info.speech_level_dbfs);
  apm_data_dumper_->DumpRaw("agc2_speech_level_reliable",
                            info.speech_level_reliable);

  info.noise_rms_dbfs = noise_level_estimator_->Analyze(frame);
  apm_data_dumper_->DumpRaw("agc2_noise_rms_dbfs", info.noise_rms_dbfs);

  saturation_protector_->Analyze(info.speech_probability, vad_result.peak_dbfs,
                                 info.speech_level_dbfs);
  info.headroom_db = saturation_protector_->HeadroomDb();
  apm_data_dumper_->DumpRaw("agc2_headroom_db", info.headroom_db);

  info.limiter_envelope_dbfs = FloatS16ToDbfs(limiter_envelope);
  apm_data_dumper_->DumpRaw("agc2_limiter_envelope_dbfs",
                            info.limiter_envelope_dbfs);

  gain_controller_.Process(info, frame);
}

void AdaptiveAgc::HandleInputGainChange() {
  speech_level_estimator_.Reset();
  saturation_protector_->Reset();
}

}  // namespace webrtc
