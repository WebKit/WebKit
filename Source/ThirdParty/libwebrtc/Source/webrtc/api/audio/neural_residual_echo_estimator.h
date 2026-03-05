/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_NEURAL_RESIDUAL_ECHO_ESTIMATOR_H_
#define API_AUDIO_NEURAL_RESIDUAL_ECHO_ESTIMATOR_H_

#include <array>

#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"

namespace webrtc {

// Interface for a neural residual echo estimator module injected into the echo
// canceller.
// This estimator estimates the echo residual that is not fully removed by the
// linear AEC3 estimator.
class NeuralResidualEchoEstimator {
 public:
  virtual ~NeuralResidualEchoEstimator() {}

  // Estimates residual echo power spectrum in the signal after linear AEC
  // subtraction. Returns two estimates:
  //   * R2: A conservative estimate.
  //   * R2_unbounded: A less conservative estimate.
  //
  // Input signals:
  //   * x: Render signal (time-domain)
  //   * y: Microphone signal (time-domain)
  //   * e: Output from linear subtraction stage (time-domain)
  //
  // Input power spectra:
  //   * S2: Linear echo estimate
  //   * Y2: Microphone input
  //   * E2: Output of linear stage
  virtual void Estimate(ArrayView<const float> x,
                        ArrayView<const std::array<float, 64>> y,
                        ArrayView<const std::array<float, 64>> e,
                        ArrayView<const std::array<float, 65>> S2,
                        ArrayView<const std::array<float, 65>> Y2,
                        ArrayView<const std::array<float, 65>> E2,
                        ArrayView<std::array<float, 65>> R2,
                        ArrayView<std::array<float, 65>> R2_unbounded) = 0;

  // Returns a recommended AEC3 configuration for this estimator.
  virtual EchoCanceller3Config GetConfiguration(bool multi_channel) const = 0;
};
}  // namespace webrtc

#endif  // API_AUDIO_NEURAL_RESIDUAL_ECHO_ESTIMATOR_H_
