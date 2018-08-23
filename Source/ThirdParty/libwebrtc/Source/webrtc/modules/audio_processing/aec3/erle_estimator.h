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

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
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
              rtc::ArrayView<const float> subtractor_spectrum,
              bool converged_filter);

  // Returns the most recent ERLE estimate.
  const std::array<float, kFftLengthBy2Plus1>& Erle() const { return erle_; }
  // Returns the ERLE that is estimated during onsets. Use for logging/testing.
  const std::array<float, kFftLengthBy2Plus1>& ErleOnsets() const {
    return erle_onsets_;
  }
  float ErleTimeDomainLog2() const { return erle_time_domain_log2_; }

  absl::optional<float> GetInstLinearQualityEstimate() const {
    return erle_time_inst_.GetInstQualityEstimate();
  }

  void Dump(const std::unique_ptr<ApmDataDumper>& data_dumper);

  class ErleTimeInstantaneous {
   public:
    ErleTimeInstantaneous(int points_to_accumulate);
    ~ErleTimeInstantaneous();
    // Update the estimator with a new point, returns true
    // if the instantaneous erle was updated due to having enough
    // points for performing the estimate.
    bool Update(const float Y2_sum, const float E2_sum);
    // Reset all the members of the class.
    void Reset();
    // Reset the members realated with an instantaneous estimate.
    void ResetAccumulators();
    // Returns the instantaneous ERLE in log2 units.
    absl::optional<float> GetInstErle_log2() const { return erle_log2_; }
    // Get an indication between 0 and 1 of the performance of the linear filter
    // for the current time instant.
    absl::optional<float> GetInstQualityEstimate() const {
      return erle_log2_ ? absl::optional<float>(inst_quality_estimate_)
                        : absl::nullopt;
    }
    void Dump(const std::unique_ptr<ApmDataDumper>& data_dumper);

   private:
    void UpdateMaxMin();
    void UpdateQualityEstimate();
    absl::optional<float> erle_log2_;
    float inst_quality_estimate_;
    float max_erle_log2_;
    float min_erle_log2_;
    float Y2_acum_;
    float E2_acum_;
    int num_points_;
    const int points_to_accumulate_;
  };

  class ErleFreqInstantaneous {
   public:
    ErleFreqInstantaneous(int points_to_accumulate);
    ~ErleFreqInstantaneous();
    // Updates the ERLE for a band with a new block. Returns absl::nullopt
    // if not enough points were accuulated for doing the estimation.
    absl::optional<float> Update(float Y2, float E2, size_t band);
    // Reset all the member of the class.
    void Reset();

   private:
    std::array<float, kFftLengthBy2Plus1> Y2_acum_;
    std::array<float, kFftLengthBy2Plus1> E2_acum_;
    std::array<int, kFftLengthBy2Plus1> num_points_;
    const int points_to_accumulate_;
  };

 private:
  std::array<float, kFftLengthBy2Plus1> erle_;
  std::array<float, kFftLengthBy2Plus1> erle_onsets_;
  std::array<bool, kFftLengthBy2Plus1> coming_onset_;
  std::array<int, kFftLengthBy2Plus1> hold_counters_;
  int hold_counter_time_domain_;
  float erle_time_domain_log2_;
  const float min_erle_;
  const float min_erle_log2_;
  const float max_erle_lf_;
  const float max_erle_lf_log2;
  const float max_erle_hf_;
  ErleFreqInstantaneous erle_freq_inst_;
  ErleTimeInstantaneous erle_time_inst_;
  RTC_DISALLOW_COPY_AND_ASSIGN(ErleEstimator);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_ERLE_ESTIMATOR_H_
