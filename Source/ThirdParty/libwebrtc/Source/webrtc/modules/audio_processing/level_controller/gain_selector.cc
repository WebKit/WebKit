/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/level_controller/gain_selector.h"

#include <math.h>
#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/level_controller/level_controller_constants.h"

namespace webrtc {

GainSelector::GainSelector() {
  Initialize(AudioProcessing::kSampleRate48kHz);
}

void GainSelector::Initialize(int sample_rate_hz) {
  gain_ = 1.f;
  frame_length_ = rtc::CheckedDivExact(sample_rate_hz, 100);
  highly_nonstationary_signal_hold_counter_ = 0;
}

// Chooses the gain to apply by the level controller such that
// 1) The level of the stationary noise does not exceed
//    a predefined threshold.
// 2) The gain does not exceed the gain that has been found
//    to saturate the signal.
// 3) The peak level achieves the target peak level.
// 4) The gain is not below 1.
// 4) The gain is 1 if the signal has been classified as stationary
//    for a long time.
// 5) The gain is not above the maximum gain.
float GainSelector::GetNewGain(float peak_level,
                               float noise_energy,
                               float saturating_gain,
                               bool gain_jumpstart,
                               SignalClassifier::SignalType signal_type) {
  RTC_DCHECK_LT(0.f, peak_level);

  if (signal_type == SignalClassifier::SignalType::kHighlyNonStationary ||
      gain_jumpstart) {
    highly_nonstationary_signal_hold_counter_ = 100;
  } else {
    highly_nonstationary_signal_hold_counter_ =
        std::max(0, highly_nonstationary_signal_hold_counter_ - 1);
  }

  float desired_gain;
  if (highly_nonstationary_signal_hold_counter_ > 0) {
    // Compute a desired gain that ensures that the peak level is amplified to
    // the target level.
    desired_gain = kTargetLcPeakLevel / peak_level;

    // Limit the desired gain so that it does not amplify the noise too much.
    float max_noise_energy = kMaxLcNoisePower * frame_length_;
    if (noise_energy * desired_gain * desired_gain > max_noise_energy) {
      RTC_DCHECK_LE(0.f, noise_energy);
      desired_gain = sqrtf(max_noise_energy / noise_energy);
    }
  } else {
    // If the signal has been stationary for a long while, apply a gain of 1 to
    // avoid amplifying pure noise.
    desired_gain = 1.0f;
  }

  // Smootly update the gain towards the desired gain.
  gain_ += 0.2f * (desired_gain - gain_);

  // Limit the gain to not exceed the maximum and the saturating gains, and to
  // ensure that the lowest possible gain is 1.
  gain_ = std::min(gain_, saturating_gain);
  gain_ = std::min(gain_, kMaxLcGain);
  gain_ = std::max(gain_, 1.f);

  return gain_;
}

}  // namespace webrtc
