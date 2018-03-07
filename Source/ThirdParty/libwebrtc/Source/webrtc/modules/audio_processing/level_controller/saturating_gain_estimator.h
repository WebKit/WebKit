/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_SATURATING_GAIN_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_SATURATING_GAIN_ESTIMATOR_H_

#include "rtc_base/constructormagic.h"

namespace webrtc {

class ApmDataDumper;

class SaturatingGainEstimator {
 public:
  SaturatingGainEstimator();
  ~SaturatingGainEstimator();
  void Initialize();
  void Update(float gain, int num_saturations);
  float GetGain() const { return saturating_gain_; }

 private:
  float saturating_gain_;
  int saturating_gain_hold_counter_;

  RTC_DISALLOW_COPY_AND_ASSIGN(SaturatingGainEstimator);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_SATURATING_GAIN_ESTIMATOR_H_
