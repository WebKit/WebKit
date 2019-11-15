/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_RESIDUAL_ECHO_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_RESIDUAL_ECHO_ESTIMATOR_H_

#include <array>
#include <memory>

#include "absl/types/optional.h"
#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/aec_state.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/aec3/reverb_model.h"
#include "modules/audio_processing/aec3/spectrum_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructor_magic.h"

namespace webrtc {

class ResidualEchoEstimator {
 public:
  explicit ResidualEchoEstimator(const EchoCanceller3Config& config);
  ~ResidualEchoEstimator();

  void Estimate(const AecState& aec_state,
                const RenderBuffer& render_buffer,
                const std::array<float, kFftLengthBy2Plus1>& S2_linear,
                const std::array<float, kFftLengthBy2Plus1>& Y2,
                std::array<float, kFftLengthBy2Plus1>* R2);

  // Returns the reverberant power spectrum contributions to the echo residual.
  rtc::ArrayView<const float> GetReverbPowerSpectrum() const {
    return echo_reverb_.GetPowerSpectrum();
  }

 private:
  // Resets the state.
  void Reset();

  // Estimates the residual echo power based on the echo return loss enhancement
  // (ERLE) and the linear power estimate.
  void LinearEstimate(const std::array<float, kFftLengthBy2Plus1>& S2_linear,
                      const std::array<float, kFftLengthBy2Plus1>& erle,
                      absl::optional<float> erle_uncertainty,
                      std::array<float, kFftLengthBy2Plus1>* R2);

  // Estimates the residual echo power based on the estimate of the echo path
  // gain.
  void NonLinearEstimate(float echo_path_gain,
                         const std::array<float, kFftLengthBy2Plus1>& X2,
                         std::array<float, kFftLengthBy2Plus1>* R2);

  // Estimates the echo generating signal power as gated maximal power over a
  // time window.
  void EchoGeneratingPower(const SpectrumBuffer& spectrum_buffer,
                           const EchoCanceller3Config::EchoModel& echo_model,
                           int filter_delay_blocks,
                           bool apply_noise_gating,
                           std::array<float, kFftLengthBy2Plus1>* X2) const;

  // Updates estimate for the power of the stationary noise component in the
  // render signal.
  void RenderNoisePower(
      const RenderBuffer& render_buffer,
      std::array<float, kFftLengthBy2Plus1>* X2_noise_floor,
      std::array<int, kFftLengthBy2Plus1>* X2_noise_floor_counter) const;

  const EchoCanceller3Config config_;
  std::array<float, kFftLengthBy2Plus1> X2_noise_floor_;
  std::array<int, kFftLengthBy2Plus1> X2_noise_floor_counter_;
  ReverbModel echo_reverb_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(ResidualEchoEstimator);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_RESIDUAL_ECHO_ESTIMATOR_H_
