/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_GAIN_SELECTOR_H_
#define MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_GAIN_SELECTOR_H_

#include "rtc_base/constructormagic.h"

#include "modules/audio_processing/level_controller/signal_classifier.h"

namespace webrtc {

class GainSelector {
 public:
  GainSelector();
  void Initialize(int sample_rate_hz);
  float GetNewGain(float peak_level,
                   float noise_energy,
                   float saturating_gain,
                   bool gain_jumpstart,
                   SignalClassifier::SignalType signal_type);

 private:
  float gain_;
  size_t frame_length_;
  int highly_nonstationary_signal_hold_counter_;

  RTC_DISALLOW_COPY_AND_ASSIGN(GainSelector);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_GAIN_SELECTOR_H_
