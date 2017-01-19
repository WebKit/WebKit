/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_PEAK_LEVEL_ESTIMATOR_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_PEAK_LEVEL_ESTIMATOR_H_

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_processing/level_controller/level_controller_constants.h"
#include "webrtc/modules/audio_processing/level_controller/signal_classifier.h"

namespace webrtc {

class PeakLevelEstimator {
 public:
  explicit PeakLevelEstimator(float initial_peak_level_dbfs);
  ~PeakLevelEstimator();
  void Initialize(float initial_peak_level_dbfs);
  float Analyze(SignalClassifier::SignalType signal_type,
                float frame_peak_level);
 private:
  float peak_level_;
  int hold_counter_;
  bool initialization_phase_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(PeakLevelEstimator);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_PEAK_LEVEL_ESTIMATOR_H_
