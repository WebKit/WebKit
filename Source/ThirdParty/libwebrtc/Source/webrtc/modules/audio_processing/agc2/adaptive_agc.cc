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

#include <algorithm>
#include <numeric>

#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/vad_with_level.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

AdaptiveAgc::AdaptiveAgc(ApmDataDumper* apm_data_dumper)
    : speech_level_estimator_(apm_data_dumper),
      gain_applier_(apm_data_dumper),
      apm_data_dumper_(apm_data_dumper),
      noise_level_estimator_(apm_data_dumper) {
  RTC_DCHECK(apm_data_dumper);
}

AdaptiveAgc::~AdaptiveAgc() = default;

void AdaptiveAgc::Process(AudioFrameView<float> float_frame) {
  const VadWithLevel::LevelAndProbability vad_result =
      vad_.AnalyzeFrame(float_frame);
  apm_data_dumper_->DumpRaw("agc2_vad_probability",
                            vad_result.speech_probability);
  apm_data_dumper_->DumpRaw("agc2_vad_rms_dbfs", vad_result.speech_rms_dbfs);

  apm_data_dumper_->DumpRaw("agc2_vad_peak_dbfs", vad_result.speech_peak_dbfs);
  speech_level_estimator_.UpdateEstimation(vad_result);

  const float speech_level_dbfs = speech_level_estimator_.LatestLevelEstimate();

  const float noise_level_dbfs = noise_level_estimator_.Analyze(float_frame);

  apm_data_dumper_->DumpRaw("agc2_noise_estimate_dbfs", noise_level_dbfs);

  // The gain applier applies the gain.
  gain_applier_.Process(speech_level_dbfs, noise_level_dbfs, vad_result,
                        float_frame);
}

void AdaptiveAgc::Reset() {
  speech_level_estimator_.Reset();
}

}  // namespace webrtc
