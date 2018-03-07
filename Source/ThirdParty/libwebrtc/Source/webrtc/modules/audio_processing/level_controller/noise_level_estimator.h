/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_NOISE_LEVEL_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_NOISE_LEVEL_ESTIMATOR_H_

#include "modules/audio_processing/level_controller/signal_classifier.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class NoiseLevelEstimator {
 public:
  NoiseLevelEstimator();
  ~NoiseLevelEstimator();
  void Initialize(int sample_rate_hz);
  float Analyze(SignalClassifier::SignalType signal_type, float frame_energy);

 private:
  float min_noise_energy_ = 0.f;
  bool first_update_;
  float noise_energy_;
  int noise_energy_hold_counter_;

  RTC_DISALLOW_COPY_AND_ASSIGN(NoiseLevelEstimator);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_NOISE_LEVEL_ESTIMATOR_H_
