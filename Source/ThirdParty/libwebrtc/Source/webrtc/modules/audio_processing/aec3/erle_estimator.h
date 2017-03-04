/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ERLE_ESTIMATOR_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ERLE_ESTIMATOR_H_

#include <array>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {

// Estimates the echo return loss enhancement based on the signal spectra.
class ErleEstimator {
 public:
  ErleEstimator();
  ~ErleEstimator();

  // Updates the ERLE estimate.
  void Update(const std::array<float, kFftLengthBy2Plus1>& render_spectrum,
              const std::array<float, kFftLengthBy2Plus1>& capture_spectrum,
              const std::array<float, kFftLengthBy2Plus1>& subtractor_spectrum);

  // Returns the most recent ERLE estimate.
  const std::array<float, kFftLengthBy2Plus1>& Erle() const { return erle_; }

 private:
  std::array<float, kFftLengthBy2Plus1> erle_;
  std::array<int, kFftLengthBy2Minus1> hold_counters_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ErleEstimator);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ERLE_ESTIMATOR_H_
