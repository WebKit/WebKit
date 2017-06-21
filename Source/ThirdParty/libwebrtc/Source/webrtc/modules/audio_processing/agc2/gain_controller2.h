/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AGC2_GAIN_CONTROLLER2_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AGC2_GAIN_CONTROLLER2_H_

#include <memory>
#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/agc2/digital_gain_applier.h"

namespace webrtc {

class ApmDataDumper;
class AudioBuffer;

// Gain Controller 2 aims to automatically adjust levels by acting on the
// microphone gain and/or applying digital gain.
//
// It temporarily implements a hard-coded gain mode only.
class GainController2 {
 public:
  explicit GainController2(int sample_rate_hz);
  ~GainController2();

  int sample_rate_hz() { return sample_rate_hz_; }

  void Process(AudioBuffer* audio);

  static bool Validate(const AudioProcessing::Config::GainController2& config);
  static std::string ToString(
      const AudioProcessing::Config::GainController2& config);

 private:
  int sample_rate_hz_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  DigitalGainApplier digital_gain_applier_;
  static int instance_count_;
  // TODO(alessiob): Remove once a meaningful gain controller mode is
  // implemented.
  const float gain_;
  RTC_DISALLOW_COPY_AND_ASSIGN(GainController2);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AGC2_GAIN_CONTROLLER2_H_
