/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_NEURAL_RESIDUAL_ECHO_ESTIMATOR_NEURAL_RESIDUAL_ECHO_ESTIMATOR_IMPL_H_
#define MODULES_AUDIO_PROCESSING_AEC3_NEURAL_RESIDUAL_ECHO_ESTIMATOR_NEURAL_RESIDUAL_ECHO_ESTIMATOR_IMPL_H_

#include <array>
#include <memory>
#include <vector>

#include "absl/base/nullability.h"
#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "api/audio/neural_residual_echo_estimator.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/neural_residual_echo_estimator/neural_feature_extractor.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"
#include "third_party/tflite/src/tensorflow/lite/op_resolver.h"
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_processing/aec3/neural_residual_echo_estimator/neural_residual_echo_estimator.pb.h"
#else
#include "modules/audio_processing/aec3/neural_residual_echo_estimator/neural_residual_echo_estimator.pb.h"
#endif

namespace webrtc {

// Implements the NeuralResidualEchoEstimator's virtual methods to estimate
// residual echo not fully removed by the linear AEC3 estimator. It uses a
// provided model to generate an echo residual mask from the linear AEC output
// and render signal. This mask is then used for estimating the echo residual
// that the AEC3 suppressor needs for computing the suppression gains.
class NeuralResidualEchoEstimatorImpl : public NeuralResidualEchoEstimator {
 public:
  // Executes a residual echo estimation model on given inputs.
  class ModelRunner {
   public:
    virtual ~ModelRunner() = default;

    virtual int StepSize() const = 0;
    virtual ArrayView<float> GetInput(
        FeatureExtractor::ModelInputEnum input_enum) = 0;
    virtual ArrayView<const float> GetOutputEchoMask() = 0;
    virtual const audioproc::ReeModelMetadata& GetMetadata() const = 0;
    virtual bool Invoke() = 0;
  };

  // Loads a model into a ModelRunner and creates a NeuralResidualEchoEstimator
  // from it. Returns nullptr if any file read or initialization step fails.
  static absl_nullable std::unique_ptr<NeuralResidualEchoEstimator> Create(
      const tflite::FlatBufferModel* model,
      const tflite::OpResolver& op_resolver);

  // Load a TF Lite model into a ModelRunner. Exposed for testing.
  static std::unique_ptr<ModelRunner> LoadTfLiteModel(
      const tflite::FlatBufferModel* model,
      const tflite::OpResolver& op_resolver);

  // Constructor used for testing with a mock ModelRunner.
  explicit NeuralResidualEchoEstimatorImpl(
      absl_nonnull std::unique_ptr<ModelRunner> model_runner);

  void Estimate(
      ArrayView<const float> x,
      ArrayView<const std::array<float, kBlockSize>> y,
      ArrayView<const std::array<float, kBlockSize>> e,
      ArrayView<const std::array<float, kFftLengthBy2Plus1>> S2,
      ArrayView<const std::array<float, kFftLengthBy2Plus1>> Y2,
      ArrayView<const std::array<float, kFftLengthBy2Plus1>> E2,
      ArrayView<std::array<float, kFftLengthBy2Plus1>> R2,
      ArrayView<std::array<float, kFftLengthBy2Plus1>> R2_unbounded) override;

  EchoCanceller3Config GetConfiguration(bool multi_channel) const override;

 private:
  void DumpInputs();

  // Encapsulates all ML model invocation work.
  const std::unique_ptr<ModelRunner> model_runner_;
  std::unique_ptr<FeatureExtractor> feature_extractor_;

  // Input buffers for translating from the 4 ms FloatS16 block format of AEC3
  // to the model scale and frame size.
  std::vector<float> input_mic_buffer_;
  std::vector<float> input_linear_aec_output_buffer_;
  std::vector<float> input_aec_ref_buffer_;

  // Downsampled model output for what fraction of the power content in the
  // linear AEC output is echo for each bin.
  std::array<float, kFftLengthBy2Plus1> output_mask_;

  static int instance_count_;
  // Pointer to a data dumper that is used for debugging purposes.
  std::unique_ptr<ApmDataDumper> data_dumper_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_NEURAL_RESIDUAL_ECHO_ESTIMATOR_NEURAL_RESIDUAL_ECHO_ESTIMATOR_IMPL_H_
