/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_NEURAL_RESIDUAL_ECHO_ESTIMATOR_NEURAL_RESIDUAL_ECHO_ESTIMATOR_TEST_HELPER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_NEURAL_RESIDUAL_ECHO_ESTIMATOR_NEURAL_RESIDUAL_ECHO_ESTIMATOR_TEST_HELPER_H_

#include <memory>

#include "api/audio/neural_residual_echo_estimator.h"

namespace webrtc {
// Abstract interface for testing the NeuralResidualEchoEstimator.
// Encapsulating the underlying implementation here allows test suites to
// inject the estimator without requiring direct linkage or visibility into
// TFLite headers and dependencies.
//
// The NeuralResidualEchoEstimatorTestHelper instance must outlive the usage of
// the returned NeuralResidualEchoEstimator, as it owns the underlying model.
class NeuralResidualEchoEstimatorTestHelper {
 public:
  virtual ~NeuralResidualEchoEstimatorTestHelper() = default;

  // Returns the encapsulated estimator instance, transferring ownership to the
  // caller. For example, this can be used for injection into an APM pipeline
  // via BuiltinAudioProcessingBuilder::SetNeuralResidualEchoEstimator().
  virtual std::unique_ptr<NeuralResidualEchoEstimator>
  GetNeuralResidualEchoEstimator() = 0;
};

// Creates a test helper instance initializing a no-op TFLite model and
// estimator for injection.
std::unique_ptr<NeuralResidualEchoEstimatorTestHelper>
CreateNeuralResidualEchoEstimatorTestHelper();

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_NEURAL_RESIDUAL_ECHO_ESTIMATOR_NEURAL_RESIDUAL_ECHO_ESTIMATOR_TEST_HELPER_H_
