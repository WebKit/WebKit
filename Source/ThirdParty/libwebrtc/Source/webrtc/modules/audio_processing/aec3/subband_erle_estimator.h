/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_SUBBAND_ERLE_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_SUBBAND_ERLE_ESTIMATOR_H_

#include <stddef.h>
#include <array>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

// Estimates the echo return loss enhancement for each frequency subband.
class SubbandErleEstimator {
 public:
  explicit SubbandErleEstimator(const EchoCanceller3Config& config);
  ~SubbandErleEstimator();

  // Resets the ERLE estimator.
  void Reset();

  // Updates the ERLE estimate.
  void Update(rtc::ArrayView<const float> X2,
              rtc::ArrayView<const float> Y2,
              rtc::ArrayView<const float> E2,
              bool converged_filter,
              bool onset_detection);

  // Returns the ERLE estimate.
  const std::array<float, kFftLengthBy2Plus1>& Erle() const { return erle_; }

  // Returns the ERLE estimate at onsets.
  rtc::ArrayView<const float> ErleOnsets() const { return erle_onsets_; }

  void Dump(const std::unique_ptr<ApmDataDumper>& data_dumper) const;

 private:
  struct AccumulatedSpectra {
    std::array<float, kFftLengthBy2Plus1> Y2_;
    std::array<float, kFftLengthBy2Plus1> E2_;
    std::array<bool, kFftLengthBy2Plus1> low_render_energy_;
    std::array<int, kFftLengthBy2Plus1> num_points_;
  };

  void UpdateAccumulatedSpectra(rtc::ArrayView<const float> X2,
                                rtc::ArrayView<const float> Y2,
                                rtc::ArrayView<const float> E2);

  void ResetAccumulatedSpectra();

  void UpdateBands(bool onset_detection);
  void DecreaseErlePerBandForLowRenderSignals();

  const float min_erle_;
  const std::array<float, kFftLengthBy2Plus1> max_erle_;
  const bool adapt_on_low_render_;
  AccumulatedSpectra accum_spectra_;
  std::array<float, kFftLengthBy2Plus1> erle_;
  std::array<float, kFftLengthBy2Plus1> erle_onsets_;
  std::array<bool, kFftLengthBy2Plus1> coming_onset_;
  std::array<int, kFftLengthBy2Plus1> hold_counters_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_SUBBAND_ERLE_ESTIMATOR_H_
