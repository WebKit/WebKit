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
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/audio/audio_processing.h"
#include "api/audio/builtin_audio_processing_builder.h"
#include "api/audio/echo_canceller3_factory.h"
#include "api/audio/echo_control.h"
#include "api/audio/echo_detector_creator.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/field_trials.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "modules/audio_processing/aec_dump/aec_dump_factory.h"
#include "rtc_base/checks.h"
#include "test/fuzzers/audio_processing_fuzzer_helper.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
namespace {

constexpr absl::string_view kFieldTrialNames[] = {
    "WebRTC-Aec3MinErleDuringOnsetsKillSwitch",
    "WebRTC-Aec3ShortHeadroomKillSwitch",
};

const Environment& GetEnvironment() {
  static const Environment* const env = new Environment(CreateEnvironment());
  return *env;
}

webrtc::scoped_refptr<AudioProcessing> CreateApm(
    test::FuzzDataHelper* fuzz_data,
    TaskQueueBase* absl_nonnull worker_queue) {
  // Parse boolean values for optionally enabling different
  // configurable public components of APM.
  bool use_ts = fuzz_data->ReadOrDefaultValue(true);
  bool use_red = fuzz_data->ReadOrDefaultValue(true);
  bool use_hpf = fuzz_data->ReadOrDefaultValue(true);
  bool use_aec3 = fuzz_data->ReadOrDefaultValue(true);
  bool use_aec = fuzz_data->ReadOrDefaultValue(true);
  bool use_aecm = fuzz_data->ReadOrDefaultValue(true);
  bool use_agc = fuzz_data->ReadOrDefaultValue(true);
  bool use_ns = fuzz_data->ReadOrDefaultValue(true);
  bool use_agc_limiter = fuzz_data->ReadOrDefaultValue(true);
  bool use_agc2 = fuzz_data->ReadOrDefaultValue(true);
  bool use_agc2_adaptive_digital = fuzz_data->ReadOrDefaultValue(true);

  // Read a gain value supported by GainController2::Validate().
  const float gain_controller2_gain_db =
      fuzz_data->ReadOrDefaultValue<uint8_t>(0) % 50;

  constexpr size_t kNumFieldTrials = std::size(kFieldTrialNames);
  // Verify that the read data type has enough bits to fuzz the field trials.
  using FieldTrialBitmaskType = uint64_t;
  static_assert(kNumFieldTrials <= sizeof(FieldTrialBitmaskType) * 8,
                "FieldTrialBitmaskType is not large enough.");
  std::bitset<kNumFieldTrials> field_trial_bitmask(
      fuzz_data->ReadOrDefaultValue<FieldTrialBitmaskType>(0));
  auto field_trials = std::make_unique<FieldTrials>("");
  for (size_t i = 0; i < kNumFieldTrials; ++i) {
    if (field_trial_bitmask[i]) {
      field_trials->Set(kFieldTrialNames[i], "Enabled");
    }
  }
  EnvironmentFactory env_factory(GetEnvironment());
  env_factory.Set(std::move(field_trials));
  const Environment env = env_factory.Create();

  // Ignore a few bytes. Bytes from this segment will be used for
  // future config flag changes. We assume 40 bytes is enough for
  // configuring the APM.
  constexpr size_t kSizeOfConfigSegment = 40;
  RTC_DCHECK(kSizeOfConfigSegment >= fuzz_data->BytesRead());
  static_cast<void>(
      fuzz_data->ReadByteArray(kSizeOfConfigSegment - fuzz_data->BytesRead()));

  // Filter out incompatible settings that lead to CHECK failures.
  if ((use_aecm && use_aec) ||          // These settings cause CHECK failure.
      (use_aecm && use_aec3 && use_ns)  // These settings trigger webrtc:9489.
  ) {
    return nullptr;
  }

  std::unique_ptr<EchoControlFactory> echo_control_factory;
  if (use_aec3) {
    echo_control_factory.reset(new EchoCanceller3Factory());
  }

  webrtc::AudioProcessing::Config apm_config;
  apm_config.pipeline.multi_channel_render = true;
  apm_config.pipeline.multi_channel_capture = true;
  apm_config.echo_canceller.enabled = use_aec || use_aecm;
  apm_config.high_pass_filter.enabled = use_hpf;
  apm_config.gain_controller1.enabled = use_agc;
  apm_config.gain_controller1.enable_limiter = use_agc_limiter;
  apm_config.gain_controller2.enabled = use_agc2;
  apm_config.gain_controller2.fixed_digital.gain_db = gain_controller2_gain_db;
  apm_config.gain_controller2.adaptive_digital.enabled =
      use_agc2_adaptive_digital;
  apm_config.noise_suppression.enabled = use_ns;
  apm_config.transient_suppression.enabled = use_ts;

  scoped_refptr<AudioProcessing> apm =
      BuiltinAudioProcessingBuilder()
          .SetEchoControlFactory(std::move(echo_control_factory))
          .SetEchoDetector(use_red ? CreateEchoDetector() : nullptr)
          .SetConfig(apm_config)
          .Build(env);

#ifdef WEBRTC_LINUX
  apm->AttachAecDump(AecDumpFactory::Create("/dev/null", -1, worker_queue));
#endif

  return apm;
}

}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > 400000) {
    return;
  }
  test::FuzzDataHelper fuzz_data(webrtc::ArrayView<const uint8_t>(data, size));

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> worker_queue =
      GetEnvironment().task_queue_factory().CreateTaskQueue(
          "rtc-low-prio", TaskQueueFactory::Priority::kLow);
  auto apm = CreateApm(&fuzz_data, worker_queue.get());

  if (apm) {
    FuzzAudioProcessing(&fuzz_data, std::move(apm));
  }
}
}  // namespace webrtc
