/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_GAIN_CONTROLLER2_H_
#define MODULES_AUDIO_PROCESSING_GAIN_CONTROLLER2_H_

#include <memory>
#include <string>

#include "modules/audio_processing/agc2/adaptive_agc.h"
#include "modules/audio_processing/agc2/gain_applier.h"
#include "modules/audio_processing/agc2/limiter.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/constructor_magic.h"

namespace webrtc {

class AudioBuffer;

// Gain Controller 2 aims to automatically adjust levels by acting on the
// microphone gain and/or applying digital gain.
class GainController2 {
 public:
  GainController2();
  GainController2(const GainController2&) = delete;
  GainController2& operator=(const GainController2&) = delete;
  ~GainController2();

  void Initialize(int sample_rate_hz, int num_channels);
  void Process(AudioBuffer* audio);
  void NotifyAnalogLevel(int level);

  void ApplyConfig(const AudioProcessing::Config::GainController2& config);
  static bool Validate(const AudioProcessing::Config::GainController2& config);

 private:
  static int instance_count_;
  ApmDataDumper data_dumper_;
  AudioProcessing::Config::GainController2 config_;
  GainApplier fixed_gain_applier_;
  std::unique_ptr<AdaptiveAgc> adaptive_digital_controller_;
  Limiter limiter_;
  int calls_since_last_limiter_log_;
  int analog_level_ = -1;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_GAIN_CONTROLLER2_H_
