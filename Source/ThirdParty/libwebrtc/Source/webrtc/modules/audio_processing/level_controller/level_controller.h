/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_LEVEL_CONTROLLER_H_
#define MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_LEVEL_CONTROLLER_H_

#include <memory>
#include <vector>

#include "api/optional.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/level_controller/gain_applier.h"
#include "modules/audio_processing/level_controller/gain_selector.h"
#include "modules/audio_processing/level_controller/noise_level_estimator.h"
#include "modules/audio_processing/level_controller/peak_level_estimator.h"
#include "modules/audio_processing/level_controller/saturating_gain_estimator.h"
#include "modules/audio_processing/level_controller/signal_classifier.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class ApmDataDumper;
class AudioBuffer;

class LevelController {
 public:
  LevelController();
  ~LevelController();

  void Initialize(int sample_rate_hz);
  void Process(AudioBuffer* audio);
  float GetLastGain() { return last_gain_; }

  // TODO(peah): This method is a temporary solution as the the aim is to
  // instead apply the config inside the constructor. Therefore this is likely
  // to change.
  void ApplyConfig(const AudioProcessing::Config::LevelController& config);
  // Validates a config.
  static bool Validate(const AudioProcessing::Config::LevelController& config);
  // Dumps a config to a string.
  static std::string ToString(
      const AudioProcessing::Config::LevelController& config);

 private:
  class Metrics {
   public:
    Metrics() { Initialize(AudioProcessing::kSampleRate48kHz); }
    void Initialize(int sample_rate_hz);
    void Update(float long_term_peak_level,
                float noise_level,
                float gain,
                float frame_peak_level);

   private:
    void Reset();

    size_t metrics_frame_counter_;
    float gain_sum_;
    float peak_level_sum_;
    float noise_energy_sum_;
    float max_gain_;
    float max_peak_level_;
    float max_noise_energy_;
    float frame_length_;
  };

  std::unique_ptr<ApmDataDumper> data_dumper_;
  GainSelector gain_selector_;
  GainApplier gain_applier_;
  SignalClassifier signal_classifier_;
  NoiseLevelEstimator noise_level_estimator_;
  PeakLevelEstimator peak_level_estimator_;
  SaturatingGainEstimator saturating_gain_estimator_;
  Metrics metrics_;
  rtc::Optional<int> sample_rate_hz_;
  static int instance_count_;
  float dc_level_[2];
  float dc_forgetting_factor_;
  float last_gain_;
  bool gain_jumpstart_ = false;
  AudioProcessing::Config::LevelController config_;

  RTC_DISALLOW_COPY_AND_ASSIGN(LevelController);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_LEVEL_CONTROLLER_H_
