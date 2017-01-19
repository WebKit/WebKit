/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/level_controller/saturating_gain_estimator.h"

#include <math.h>
#include <algorithm>

#include "webrtc/modules/audio_processing/level_controller/level_controller_constants.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

SaturatingGainEstimator::SaturatingGainEstimator() {
  Initialize();
}

SaturatingGainEstimator::~SaturatingGainEstimator() {}

void SaturatingGainEstimator::Initialize() {
  saturating_gain_ = kMaxLcGain;
  saturating_gain_hold_counter_ = 0;
}

void SaturatingGainEstimator::Update(float gain, int num_saturations) {
  bool too_many_saturations = (num_saturations > 2);

  if (too_many_saturations) {
    saturating_gain_ = 0.95f * gain;
    saturating_gain_hold_counter_ = 1000;
  } else {
    saturating_gain_hold_counter_ =
        std::max(0, saturating_gain_hold_counter_ - 1);
    if (saturating_gain_hold_counter_ == 0) {
      saturating_gain_ *= 1.001f;
      saturating_gain_ = std::min(kMaxLcGain, saturating_gain_);
    }
  }
}

}  // namespace webrtc
