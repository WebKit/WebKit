/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_GAIN_APPLIER_H_
#define MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_GAIN_APPLIER_H_

#include "rtc_base/constructormagic.h"

namespace webrtc {

class ApmDataDumper;
class AudioBuffer;

class GainApplier {
 public:
  explicit GainApplier(ApmDataDumper* data_dumper);
  void Initialize(int sample_rate_hz);

  // Applies the specified gain to the audio frame and returns the resulting
  // number of saturated sample values.
  int Process(float new_gain, AudioBuffer* audio);

 private:
  ApmDataDumper* const data_dumper_;
  float old_gain_ = 1.f;
  float gain_increase_step_size_ = 0.f;
  float gain_normal_decrease_step_size_ = 0.f;
  float gain_saturated_decrease_step_size_ = 0.f;
  bool last_frame_was_saturated_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(GainApplier);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_LEVEL_CONTROLLER_GAIN_APPLIER_H_
