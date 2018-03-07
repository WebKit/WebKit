/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/include/audio_processing.h"
#include "test/fuzzers/audio_processing_fuzzer.h"

#include "api/optional.h"

namespace webrtc {

std::unique_ptr<AudioProcessing> CreateAPM(const uint8_t** data,
                                           size_t* remaining_size) {
  // Parse boolean values for optionally enabling different
  // configurable public components of APM.
  auto exp_agc = ParseBool(data, remaining_size);
  auto exp_ns = ParseBool(data, remaining_size);
  auto bf = ParseBool(data, remaining_size);
  auto ef = ParseBool(data, remaining_size);
  auto raf = ParseBool(data, remaining_size);
  auto da = ParseBool(data, remaining_size);
  auto ie = ParseBool(data, remaining_size);
  auto red = ParseBool(data, remaining_size);
  auto lc = ParseBool(data, remaining_size);
  auto hpf = ParseBool(data, remaining_size);
  auto aec3 = ParseBool(data, remaining_size);

  auto use_aec = ParseBool(data, remaining_size);
  auto use_aecm = ParseBool(data, remaining_size);
  auto use_agc = ParseBool(data, remaining_size);
  auto use_ns = ParseBool(data, remaining_size);
  auto use_le = ParseBool(data, remaining_size);
  auto use_vad = ParseBool(data, remaining_size);
  auto use_agc_limiter = ParseBool(data, remaining_size);

  if (!(exp_agc && exp_ns && bf && ef && raf && da && ie && red && lc && hpf &&
        aec3 && use_aec && use_aecm && use_agc && use_ns && use_le && use_vad &&
        use_agc_limiter)) {
    return nullptr;
  }

  // Filter out incompatible settings that lead to CHECK failures.
  if (*use_aecm && *use_aec) {
    return nullptr;
  }

  // Components can be enabled through webrtc::Config and
  // webrtc::AudioProcessingConfig.
  Config config;

  std::unique_ptr<EchoControlFactory> echo_control_factory;
  if (*aec3) {
    echo_control_factory.reset(new EchoCanceller3Factory());
  }

  config.Set<ExperimentalAgc>(new ExperimentalAgc(*exp_agc));
  config.Set<ExperimentalNs>(new ExperimentalNs(*exp_ns));
  if (*bf) {
    config.Set<Beamforming>(new Beamforming());
  }
  config.Set<ExtendedFilter>(new ExtendedFilter(*ef));
  config.Set<RefinedAdaptiveFilter>(new RefinedAdaptiveFilter(*raf));
  config.Set<DelayAgnostic>(new DelayAgnostic(*da));
  config.Set<Intelligibility>(new Intelligibility(*ie));

  std::unique_ptr<AudioProcessing> apm(AudioProcessing::Create(
      config, nullptr, std::move(echo_control_factory), nullptr));

  webrtc::AudioProcessing::Config apm_config;
  apm_config.residual_echo_detector.enabled = *red;
  apm_config.level_controller.enabled = *lc;
  apm_config.high_pass_filter.enabled = *hpf;

  apm->ApplyConfig(apm_config);

  apm->echo_cancellation()->Enable(*use_aec);
  apm->echo_control_mobile()->Enable(*use_aecm);
  apm->gain_control()->Enable(*use_agc);
  apm->noise_suppression()->Enable(*use_ns);
  apm->level_estimator()->Enable(*use_le);
  apm->voice_detection()->Enable(*use_vad);
  apm->gain_control()->enable_limiter(*use_agc_limiter);

  return apm;
}

void FuzzOneInput(const uint8_t* data, size_t size) {
  auto apm = CreateAPM(&data, &size);
  if (apm) {
    FuzzAudioProcessing(data, size, std::move(apm));
  }
}
}  // namespace webrtc
