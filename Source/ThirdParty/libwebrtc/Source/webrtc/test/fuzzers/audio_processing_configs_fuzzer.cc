/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <bitset>
#include <string>

#include "absl/memory/memory.h"
#include "api/audio/echo_canceller3_factory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "modules/audio_processing/aec_dump/aec_dump_factory.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/test/audio_processing_builder_for_testing.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/task_queue.h"
#include "system_wrappers/include/field_trial.h"
#include "test/fuzzers/audio_processing_fuzzer_helper.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
namespace {

const std::string kFieldTrialNames[] = {
    "WebRTC-Audio-Agc2ForceExtraSaturationMargin",
    "WebRTC-Audio-Agc2ForceInitialSaturationMargin",
    "WebRTC-Aec3MinErleDuringOnsetsKillSwitch",
    "WebRTC-Aec3ShortHeadroomKillSwitch",
};

std::unique_ptr<AudioProcessing> CreateApm(test::FuzzDataHelper* fuzz_data,
                                           std::string* field_trial_string,
                                           rtc::TaskQueue* worker_queue) {
  // Parse boolean values for optionally enabling different
  // configurable public components of APM.
  bool exp_agc = fuzz_data->ReadOrDefaultValue(true);
  bool exp_ns = fuzz_data->ReadOrDefaultValue(true);
  static_cast<void>(fuzz_data->ReadOrDefaultValue(true));
  static_cast<void>(fuzz_data->ReadOrDefaultValue(true));
  static_cast<void>(fuzz_data->ReadOrDefaultValue(true));
  bool red = fuzz_data->ReadOrDefaultValue(true);
  bool hpf = fuzz_data->ReadOrDefaultValue(true);
  bool aec3 = fuzz_data->ReadOrDefaultValue(true);

  bool use_aec = fuzz_data->ReadOrDefaultValue(true);
  bool use_aecm = fuzz_data->ReadOrDefaultValue(true);
  bool use_agc = fuzz_data->ReadOrDefaultValue(true);
  bool use_ns = fuzz_data->ReadOrDefaultValue(true);
  bool use_le = fuzz_data->ReadOrDefaultValue(true);
  bool use_vad = fuzz_data->ReadOrDefaultValue(true);
  bool use_agc_limiter = fuzz_data->ReadOrDefaultValue(true);
  bool use_agc2 = fuzz_data->ReadOrDefaultValue(true);

  // Read an int8 value, but don't let it be too large or small.
  const float gain_controller2_gain_db =
      rtc::SafeClamp<int>(fuzz_data->ReadOrDefaultValue<int8_t>(0), -40, 40);

  constexpr size_t kNumFieldTrials = arraysize(kFieldTrialNames);
  // Verify that the read data type has enough bits to fuzz the field trials.
  using FieldTrialBitmaskType = uint64_t;
  static_assert(kNumFieldTrials <= sizeof(FieldTrialBitmaskType) * 8,
                "FieldTrialBitmaskType is not large enough.");
  std::bitset<kNumFieldTrials> field_trial_bitmask(
      fuzz_data->ReadOrDefaultValue<FieldTrialBitmaskType>(0));
  for (size_t i = 0; i < kNumFieldTrials; ++i) {
    if (field_trial_bitmask[i]) {
      *field_trial_string += kFieldTrialNames[i] + "/Enabled/";
    }
  }
  field_trial::InitFieldTrialsFromString(field_trial_string->c_str());

  bool use_agc2_adaptive_digital = fuzz_data->ReadOrDefaultValue(true);
  bool use_agc2_adaptive_digital_rms_estimator =
      fuzz_data->ReadOrDefaultValue(true);
  bool use_agc2_adaptive_digital_saturation_protector =
      fuzz_data->ReadOrDefaultValue(true);

  // Ignore a few bytes. Bytes from this segment will be used for
  // future config flag changes. We assume 40 bytes is enough for
  // configuring the APM.
  constexpr size_t kSizeOfConfigSegment = 40;
  RTC_DCHECK(kSizeOfConfigSegment >= fuzz_data->BytesRead());
  static_cast<void>(
      fuzz_data->ReadByteArray(kSizeOfConfigSegment - fuzz_data->BytesRead()));

  // Filter out incompatible settings that lead to CHECK failures.
  if ((use_aecm && use_aec) ||      // These settings cause CHECK failure.
      (use_aecm && aec3 && use_ns)  // These settings trigger webrtc:9489.
  ) {
    return nullptr;
  }

  // Components can be enabled through webrtc::Config and
  // webrtc::AudioProcessingConfig.
  Config config;

  std::unique_ptr<EchoControlFactory> echo_control_factory;
  if (aec3) {
    echo_control_factory.reset(new EchoCanceller3Factory());
  }

  config.Set<ExperimentalAgc>(new ExperimentalAgc(exp_agc));
  config.Set<ExperimentalNs>(new ExperimentalNs(exp_ns));

  std::unique_ptr<AudioProcessing> apm(
      AudioProcessingBuilderForTesting()
          .SetEchoControlFactory(std::move(echo_control_factory))
          .Create(config));

#ifdef WEBRTC_LINUX
  apm->AttachAecDump(AecDumpFactory::Create("/dev/null", -1, worker_queue));
#endif

  webrtc::AudioProcessing::Config apm_config;
  apm_config.pipeline.multi_channel_render = true;
  apm_config.pipeline.multi_channel_capture = true;
  apm_config.echo_canceller.enabled = use_aec || use_aecm;
  apm_config.echo_canceller.mobile_mode = use_aecm;
  apm_config.residual_echo_detector.enabled = red;
  apm_config.high_pass_filter.enabled = hpf;
  apm_config.gain_controller1.enabled = use_agc;
  apm_config.gain_controller1.enable_limiter = use_agc_limiter;
  apm_config.gain_controller2.enabled = use_agc2;
  apm_config.gain_controller2.fixed_digital.gain_db = gain_controller2_gain_db;
  apm_config.gain_controller2.adaptive_digital.enabled =
      use_agc2_adaptive_digital;
  apm_config.gain_controller2.adaptive_digital.level_estimator =
      use_agc2_adaptive_digital_rms_estimator
          ? webrtc::AudioProcessing::Config::GainController2::LevelEstimator::
                kRms
          : webrtc::AudioProcessing::Config::GainController2::LevelEstimator::
                kPeak;
  apm_config.gain_controller2.adaptive_digital.use_saturation_protector =
      use_agc2_adaptive_digital_saturation_protector;
  apm_config.noise_suppression.enabled = use_ns;
  apm_config.voice_detection.enabled = use_vad;
  apm_config.level_estimation.enabled = use_le;
  apm->ApplyConfig(apm_config);

  return apm;
}

TaskQueueFactory* GetTaskQueueFactory() {
  static TaskQueueFactory* const factory =
      CreateDefaultTaskQueueFactory().release();
  return factory;
}

}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > 400000) {
    return;
  }
  test::FuzzDataHelper fuzz_data(rtc::ArrayView<const uint8_t>(data, size));
  // This string must be in scope during execution, according to documentation
  // for field_trial.h. Hence it's created here and not in CreateApm.
  std::string field_trial_string = "";

  rtc::TaskQueue worker_queue(GetTaskQueueFactory()->CreateTaskQueue(
      "rtc-low-prio", rtc::TaskQueue::Priority::LOW));
  auto apm = CreateApm(&fuzz_data, &field_trial_string, &worker_queue);

  if (apm) {
    FuzzAudioProcessing(&fuzz_data, std::move(apm));
  }
}
}  // namespace webrtc
