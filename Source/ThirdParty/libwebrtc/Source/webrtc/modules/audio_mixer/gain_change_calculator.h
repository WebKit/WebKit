/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_MIXER_GAIN_CHANGE_CALCULATOR_H_
#define MODULES_AUDIO_MIXER_GAIN_CHANGE_CALCULATOR_H_

#include <stdint.h>

#include <span>

namespace webrtc {

class GainChangeCalculator {
 public:
  // The 'out' signal is assumed to be produced from 'in' by applying
  // a smoothly varying gain. This method computes variations of the
  // gain and handles special cases when the samples are small.
  float CalculateGainChange(std::span<const int16_t> in,
                            std::span<const int16_t> out);

  float LatestGain() const;

 private:
  void CalculateGain(std::span<const int16_t> in,
                     std::span<const int16_t> out,
                     std::span<float> gain);

  float CalculateDifferences(std::span<const float> values);
  float last_value_ = 0.f;
  float last_reliable_gain_ = 1.0f;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_MIXER_GAIN_CHANGE_CALCULATOR_H_
