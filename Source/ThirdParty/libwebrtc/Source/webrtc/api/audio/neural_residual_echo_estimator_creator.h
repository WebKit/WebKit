/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_NEURAL_RESIDUAL_ECHO_ESTIMATOR_CREATOR_H_
#define API_AUDIO_NEURAL_RESIDUAL_ECHO_ESTIMATOR_CREATOR_H_

#include <memory>

#include "absl/base/nullability.h"
#include "api/audio/neural_residual_echo_estimator.h"
#include "api/audio/tflite_model_handle.h"
#include "api/environment/environment.h"
#include "api/scoped_refptr.h"
#include "rtc_base/system/rtc_export.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace tflite {
class OpResolver;
}  // namespace tflite

namespace webrtc {

// Returns an instance of the WebRTC implementation of a residual echo detector.
// Returns nullptr if unable to read the file or initialize a model from the
// file contents.
//
// `model` needs to outlive the created estimator. `op_resolver` is only used
// during this call.
RTC_EXPORT
absl_nullable std::unique_ptr<NeuralResidualEchoEstimator>
CreateNeuralResidualEchoEstimator(const tflite::FlatBufferModel* model,
                                  const tflite::OpResolver* absl_nonnull
                                      op_resolver);

// Returns an instance of the WebRTC implementation of a residual echo detector
// initialized asynchronously on a background thread.
//
// `model_handle` and `op_resolver` are used to load the model on a background
// thread.
RTC_EXPORT
absl_nonnull std::unique_ptr<NeuralResidualEchoEstimator>
CreateNeuralResidualEchoEstimatorAsync(
    const Environment& env,
    scoped_refptr<TfliteModelHandle> model_handle,
    std::unique_ptr<tflite::OpResolver> op_resolver);

}  // namespace webrtc

#endif  // API_AUDIO_NEURAL_RESIDUAL_ECHO_ESTIMATOR_CREATOR_H_
