/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_ERLE_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_ERLE_ESTIMATOR_H_

#include <array>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

// Estimates the echo return loss enhancement based on the signal spectra.
class ErleEstimator {
 public:
  ErleEstimator(float min_erle, float max_erle_lf, float max_erle_hf);
  ~ErleEstimator();

  // Updates the ERLE estimate.
  void Update(rtc::ArrayView<const float> render_spectrum,
              rtc::ArrayView<const float> capture_spectrum,
              rtc::ArrayView<const float> subtractor_spectrum);

  // Returns the most recent ERLE estimate.
  const std::array<float, kFftLengthBy2Plus1>& Erle() const { return erle_; }
  float ErleTimeDomain() const { return erle_time_domain_; }

 private:
  std::array<float, kFftLengthBy2Plus1> erle_;
  std::array<int, kFftLengthBy2Minus1> hold_counters_;
  float erle_time_domain_;
  int hold_counter_time_domain_;
  const float min_erle_;
  const float max_erle_lf_;
  const float max_erle_hf_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ErleEstimator);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_ERLE_ESTIMATOR_H_
