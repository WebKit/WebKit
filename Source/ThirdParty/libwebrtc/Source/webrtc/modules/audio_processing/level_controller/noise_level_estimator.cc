/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/level_controller/noise_level_estimator.h"

#include <algorithm>

#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

NoiseLevelEstimator::NoiseLevelEstimator() {
  Initialize(AudioProcessing::kSampleRate48kHz);
}

NoiseLevelEstimator::~NoiseLevelEstimator() {}

void NoiseLevelEstimator::Initialize(int sample_rate_hz) {
  noise_energy_ = 1.f;
  first_update_ = true;
  min_noise_energy_ = sample_rate_hz * 2.f * 2.f / 100.f;
  noise_energy_hold_counter_ = 0;
}

float NoiseLevelEstimator::Analyze(SignalClassifier::SignalType signal_type,
                                   float frame_energy) {
  if (frame_energy <= 0.f) {
    return noise_energy_;
  }

  if (first_update_) {
    // Initialize the noise energy to the frame energy.
    first_update_ = false;
    return noise_energy_ = std::max(frame_energy, min_noise_energy_);
  }

  // Update the noise estimate in a minimum statistics-type manner.
  if (signal_type == SignalClassifier::SignalType::kStationary) {
    if (frame_energy > noise_energy_) {
      // Leak the estimate upwards towards the frame energy if no recent
      // downward update.
      noise_energy_hold_counter_ = std::max(noise_energy_hold_counter_ - 1, 0);

      if (noise_energy_hold_counter_ == 0) {
        noise_energy_ = std::min(noise_energy_ * 1.01f, frame_energy);
      }
    } else {
      // Update smoothly downwards with a limited maximum update magnitude.
      noise_energy_ =
          std::max(noise_energy_ * 0.9f,
                   noise_energy_ + 0.05f * (frame_energy - noise_energy_));
      noise_energy_hold_counter_ = 1000;
    }
  } else {
    // For a non-stationary signal, leak the estimate downwards in order to
    // avoid estimate locking due to incorrect signal classification.
    noise_energy_ = noise_energy_ * 0.99f;
  }

  // Ensure a minimum of the estimate.
  return noise_energy_ = std::max(noise_energy_, min_noise_energy_);
}

}  // namespace webrtc
