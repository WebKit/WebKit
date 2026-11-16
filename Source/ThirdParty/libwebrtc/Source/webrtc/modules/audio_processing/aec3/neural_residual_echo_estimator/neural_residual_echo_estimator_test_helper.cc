/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/neural_residual_echo_estimator/neural_residual_echo_estimator_test_helper.h"

#include <memory>
#include <string>

#include "api/audio/neural_residual_echo_estimator.h"
#include "api/audio/neural_residual_echo_estimator_creator.h"
#include "rtc_base/checks.h"
#include "test/testsupport/file_utils.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/register.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace webrtc {

class NeuralResidualEchoEstimatorTestHelperImpl
    : public NeuralResidualEchoEstimatorTestHelper {
 public:
  NeuralResidualEchoEstimatorTestHelperImpl() {
    std::string model_path = test::ResourcePath(
        "audio_processing/aec3/noop_ml_aec_model_for_testing", "tflite");
    model_ = tflite::FlatBufferModel::BuildFromFile(model_path.c_str());
    RTC_CHECK(model_);
  }

  ~NeuralResidualEchoEstimatorTestHelperImpl() = default;

  std::unique_ptr<NeuralResidualEchoEstimator> GetNeuralResidualEchoEstimator()
      override {
    tflite::ops::builtin::BuiltinOpResolver ops_resolver;
    return CreateNeuralResidualEchoEstimator(model_.get(), &ops_resolver);
  }

 private:
  std::unique_ptr<tflite::FlatBufferModel> model_;
};

std::unique_ptr<NeuralResidualEchoEstimatorTestHelper>
CreateNeuralResidualEchoEstimatorTestHelper() {
  return std::make_unique<NeuralResidualEchoEstimatorTestHelperImpl>();
}

}  // namespace webrtc
