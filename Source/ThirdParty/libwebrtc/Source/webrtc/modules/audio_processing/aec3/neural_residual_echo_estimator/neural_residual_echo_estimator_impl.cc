/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/neural_residual_echo_estimator/neural_residual_echo_estimator_impl.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "api/audio/neural_residual_echo_estimator.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/neural_residual_echo_estimator/neural_feature_extractor.h"
#include "third_party/tflite/src/tensorflow/lite/c/c_api_types.h"
#ifdef WEBRTC_ANDROID_PLATFORM_BUILD
#include "external/webrtc/webrtc/modules/audio_processing/aec3/neural_residual_echo_estimator/neural_residual_echo_estimator.pb.h"
#else
#include "modules/audio_processing/aec3/neural_residual_echo_estimator/neural_residual_echo_estimator.pb.h"
#endif
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "third_party/tflite/src/tensorflow/lite/interpreter.h"
#include "third_party/tflite/src/tensorflow/lite/interpreter_builder.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/kernel_util.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"
#include "third_party/tflite/src/tensorflow/lite/op_resolver.h"

namespace webrtc {
namespace {
using ModelInputEnum = FeatureExtractor::ModelInputEnum;
using ModelOutputEnum = FeatureExtractor::ModelOutputEnum;
const std::array<int, 1> kSupportedFrameSizeSamples = {256};

// Field under which the ML-REE metadata is stored in a TFLite model.
constexpr char kTfLiteMetadataKey[] = "REE_METADATA";

// Reads the model metadata from the TFLite model. If the metadata is not
// present, it returns a default metadata with version 1. If the metadata is
// present but cannot be parsed, it returns nullopt.
std::optional<audioproc::ReeModelMetadata> ReadModelMetadata(
    const tflite::FlatBufferModel* model) {
  audioproc::ReeModelMetadata default_metadata;
  default_metadata.set_version(1);
  const auto metadata_records = model->ReadAllMetadata();
  const auto metadata_field = metadata_records.find(kTfLiteMetadataKey);
  if (metadata_field == metadata_records.end()) {
    return default_metadata;
  }
  audioproc::ReeModelMetadata metadata;
  if (metadata.ParseFromString(metadata_field->second)) {
    return metadata;
  }
  return std::nullopt;
}

// Encapsulates all the NeuralResidualEchoEstimatorImpl's interaction with
// TFLite. This allows the separation of rebuffering and similar AEC3-related
// bookkeeping from the TFLite-specific code, and makes it easier to test the
// former code by mocking.
class TfLiteModelRunner : public NeuralResidualEchoEstimatorImpl::ModelRunner {
 public:
  TfLiteModelRunner(std::unique_ptr<tflite::Interpreter> tflite_interpreter,
                    audioproc::ReeModelMetadata metadata)
      : input_tensor_size_(static_cast<int>(
            tflite::NumElements(tflite_interpreter->input_tensor(
                static_cast<int>(ModelInputEnum::kMic))))),
        frame_size_(metadata.version() == 1 ? input_tensor_size_
                                            : (input_tensor_size_ - 1) * 2),
        step_size_(frame_size_ / 2),
        frame_size_by_2_plus_1_(frame_size_ / 2 + 1),
        metadata_(metadata),
        model_state_(tflite::NumElements(tflite_interpreter->input_tensor(
                         static_cast<int>(ModelInputEnum::kModelState))),
                     0.0f),
        tflite_interpreter_(std::move(tflite_interpreter)) {
    for (const auto input_enum :
         {ModelInputEnum::kMic, ModelInputEnum::kLinearAecOutput,
          ModelInputEnum::kAecRef}) {
      ArrayView<float> input_tensor(
          tflite_interpreter_->typed_input_tensor<float>(
              static_cast<int>(input_enum)),
          input_tensor_size_);
      std::fill(input_tensor.begin(), input_tensor.end(), 0.0f);
    }

    RTC_CHECK_EQ(frame_size_ % kBlockSize, 0);
    RTC_CHECK(std::any_of(kSupportedFrameSizeSamples.cbegin(),
                          kSupportedFrameSizeSamples.cend(),
                          [this](int a) { return a == frame_size_; }));
    RTC_CHECK_EQ(tflite::NumElements(tflite_interpreter_->input_tensor(
                     static_cast<int>(ModelInputEnum::kLinearAecOutput))),
                 input_tensor_size_);
    RTC_CHECK_EQ(tflite::NumElements(tflite_interpreter_->input_tensor(
                     static_cast<int>(ModelInputEnum::kAecRef))),
                 input_tensor_size_);
    RTC_CHECK_EQ(tflite::NumElements(tflite_interpreter_->input_tensor(
                     static_cast<int>(ModelInputEnum::kModelState))),
                 tflite::NumElements(tflite_interpreter_->output_tensor(
                     static_cast<int>(ModelOutputEnum::kModelState))));
    RTC_CHECK_EQ(tflite::NumElements(tflite_interpreter_->output_tensor(
                     static_cast<int>(ModelOutputEnum::kEchoMask))),
                 frame_size_by_2_plus_1_);
  }

  ~TfLiteModelRunner() override {}

  int StepSize() const override { return step_size_; }

  ArrayView<float> GetInput(ModelInputEnum input_enum) override {
    int tensor_size = 0;
    switch (input_enum) {
      case ModelInputEnum::kMic:              // fall-through
      case ModelInputEnum::kLinearAecOutput:  // fall-through
      case ModelInputEnum::kAecRef:
        tensor_size = input_tensor_size_;
        break;
      case ModelInputEnum::kModelState:
        tensor_size = static_cast<int>(model_state_.size());
        break;
      case ModelInputEnum::kNumInputs:
        RTC_CHECK(false);
    }
    return ArrayView<float>(tflite_interpreter_->typed_input_tensor<float>(
                                static_cast<int>(input_enum)),
                            tensor_size);
  }

  ArrayView<const float> GetOutputEchoMask() override {
    return ArrayView<const float>(
        tflite_interpreter_->typed_output_tensor<const float>(
            static_cast<int>(ModelOutputEnum::kEchoMask)),
        frame_size_by_2_plus_1_);
  }

  const audioproc::ReeModelMetadata& GetMetadata() const override {
    return metadata_;
  }

  bool Invoke() override {
    auto input_state = GetInput(ModelInputEnum::kModelState);
    std::copy(model_state_.begin(), model_state_.end(), input_state.begin());

    const TfLiteStatus status = tflite_interpreter_->Invoke();
    if (status != kTfLiteOk && processing_error_log_counter_ <= 0) {
      RTC_LOG(LS_ERROR) << "TfLiteModelRunner::Estimate() "
                           "invocation error, status="
                        << status;
      // Wait ~1 second before logging this error again.
      processing_error_log_counter_ = 16000 / step_size_;
      return false;
    } else if (processing_error_log_counter_ > 0) {
      --processing_error_log_counter_;
    }

    auto output_state = ArrayView<const float>(
        tflite_interpreter_->typed_output_tensor<const float>(
            static_cast<int>(ModelOutputEnum::kModelState)),
        model_state_.size());
    std::copy(output_state.begin(), output_state.end(), model_state_.begin());

    constexpr float kStateDecay = 0.999f;
    for (float& state : model_state_) {
      state *= kStateDecay;
    }

    return true;
  }

 private:

  // Size of the input tensors.
  const int input_tensor_size_;

  // Frame size of the model.
  const int frame_size_;

  // Step size.
  const int step_size_;

  // Size of the spectrum mask that is returned by the model.
  const int frame_size_by_2_plus_1_;

  // Metadata of the model.
  const audioproc::ReeModelMetadata metadata_;

  // LSTM states that carry over to the next inference invocation.
  std::vector<float> model_state_;

  // TFLite model for residual echo estimation.
  // Must outlive `tflite_interpreter_`
  std::unique_ptr<tflite::FlatBufferModel> tflite_model_;

  // Used to run inference with `tflite_model_`.
  std::unique_ptr<tflite::Interpreter> tflite_interpreter_;

  // Counter to avoid logging processing errors too often.
  int processing_error_log_counter_ = 0;
};
}  // namespace

std::unique_ptr<NeuralResidualEchoEstimatorImpl::ModelRunner>
NeuralResidualEchoEstimatorImpl::LoadTfLiteModel(
    const tflite::FlatBufferModel* model,
    const tflite::OpResolver& op_resolver) {
  if (!model) {
    RTC_LOG(LS_ERROR) << "Nothing to load.";
    return nullptr;
  }
  std::unique_ptr<tflite::Interpreter> interpreter;
  if (tflite::InterpreterBuilder(*model, op_resolver)(&interpreter) !=
      kTfLiteOk) {
    RTC_LOG(LS_ERROR) << "Error creating interpreter";
    return nullptr;
  }
  if (interpreter->AllocateTensors() != kTfLiteOk) {
    RTC_LOG(LS_ERROR) << "Error allocating tensors";
    return nullptr;
  }
  if (interpreter->inputs().size() !=
      static_cast<int>(ModelInputEnum::kNumInputs)) {
    RTC_LOG(LS_ERROR) << "Model input number mismatch, got "
                      << interpreter->inputs().size() << " expected "
                      << static_cast<int>(ModelInputEnum::kNumInputs);
    return nullptr;
  }
  if (interpreter->outputs().size() !=
      static_cast<int>(ModelOutputEnum::kNumOutputs)) {
    RTC_LOG(LS_ERROR) << "Model output number mismatch, got "
                      << interpreter->outputs().size() << " expected "
                      << static_cast<int>(ModelOutputEnum::kNumOutputs);
    return nullptr;
  }
  auto metadata = ReadModelMetadata(model);
  if (!metadata.has_value()) {
    RTC_LOG(LS_ERROR) << "Error reading model metadata";
    return nullptr;
  }
  if (metadata->version() < 1 || metadata->version() > 2) {
    RTC_LOG(LS_ERROR) << "Model version mismatch, got " << metadata->version()
                      << " expected 1 or 2.";
    return nullptr;
  }
  return std::make_unique<TfLiteModelRunner>(std::move(interpreter), *metadata);
}

absl_nullable std::unique_ptr<NeuralResidualEchoEstimator>
NeuralResidualEchoEstimatorImpl::Create(const tflite::FlatBufferModel* model,
                                        const tflite::OpResolver& op_resolver) {
  std::unique_ptr<ModelRunner> model_runner =
      NeuralResidualEchoEstimatorImpl::LoadTfLiteModel(model, op_resolver);
  if (!model_runner) {
    return nullptr;
  }
  return std::make_unique<NeuralResidualEchoEstimatorImpl>(
      std::move(model_runner));
}

int NeuralResidualEchoEstimatorImpl::instance_count_ = 0;

NeuralResidualEchoEstimatorImpl::NeuralResidualEchoEstimatorImpl(
    absl_nonnull std::unique_ptr<ModelRunner> model_runner)
    : model_runner_(std::move(model_runner)),
      data_dumper_(new ApmDataDumper(++instance_count_)) {
  input_mic_buffer_.reserve(model_runner_->StepSize());
  input_linear_aec_output_buffer_.reserve(model_runner_->StepSize());
  input_aec_ref_buffer_.reserve(model_runner_->StepSize());
  output_mask_.fill(0.0f);
  if (model_runner_->GetMetadata().version() == 1) {
    feature_extractor_ = std::make_unique<TimeDomainFeatureExtractor>();
  } else {
    feature_extractor_ = std::make_unique<FrequencyDomainFeatureExtractor>(
        /*step_size=*/model_runner_->StepSize());
  }
}

void NeuralResidualEchoEstimatorImpl::Estimate(
    ArrayView<const float> x,
    ArrayView<const std::array<float, kBlockSize>> y,
    ArrayView<const std::array<float, kBlockSize>> e,
    ArrayView<const std::array<float, kFftLengthBy2Plus1>> S2,
    ArrayView<const std::array<float, kFftLengthBy2Plus1>> Y2,
    ArrayView<const std::array<float, kFftLengthBy2Plus1>> E2,
    ArrayView<std::array<float, kFftLengthBy2Plus1>> R2,
    ArrayView<std::array<float, kFftLengthBy2Plus1>> R2_unbounded) {
  // The input is buffered for model inference; multi-channel data is handled by
  // summing the content of all channels.
  input_mic_buffer_.insert(input_mic_buffer_.end(), y[0].begin(), y[0].end());
  input_linear_aec_output_buffer_.insert(input_linear_aec_output_buffer_.end(),
                                         e[0].begin(), e[0].end());
  for (size_t ch = 1; ch < y.size(); ++ch) {
    std::transform(y[ch].begin(), y[ch].end(),
                   input_mic_buffer_.end() - kBlockSize,
                   input_mic_buffer_.end() - kBlockSize, std::plus<float>());
    std::transform(e[ch].begin(), e[ch].end(),
                   input_linear_aec_output_buffer_.end() - kBlockSize,
                   input_linear_aec_output_buffer_.end() - kBlockSize,
                   std::plus<float>());
  }
  input_aec_ref_buffer_.insert(input_aec_ref_buffer_.end(), x.begin(), x.end());

  if (static_cast<int>(input_mic_buffer_.size()) == model_runner_->StepSize()) {
    DumpInputs();
    feature_extractor_->PushFeaturesToModelInput(
        input_mic_buffer_, model_runner_->GetInput(ModelInputEnum::kMic),
        ModelInputEnum::kMic);
    feature_extractor_->PushFeaturesToModelInput(
        input_linear_aec_output_buffer_,
        model_runner_->GetInput(ModelInputEnum::kLinearAecOutput),
        ModelInputEnum::kLinearAecOutput);
    feature_extractor_->PushFeaturesToModelInput(
        input_aec_ref_buffer_, model_runner_->GetInput(ModelInputEnum::kAecRef),
        ModelInputEnum::kAecRef);
    if (model_runner_->Invoke()) {
      // Downsample output mask to match the AEC3 frequency resolution.
      ArrayView<const float> output_mask = model_runner_->GetOutputEchoMask();
      const int kDownsampleFactor = (output_mask.size() - 1) / kFftLengthBy2;
      output_mask_[0] = output_mask[0];
      for (size_t i = 1; i < kFftLengthBy2Plus1; ++i) {
        const auto* output_mask_ptr =
            &output_mask[kDownsampleFactor * (i - 1) + 1];
        output_mask_[i] = *std::max_element(
            output_mask_ptr, output_mask_ptr + kDownsampleFactor);
      }
      // The model is trained to predict the nearend magnitude spectrum but
      // exposes 1 minus that mask. The next transformation computes the mask
      // that estimates the echo power spectrum assuming that the sum of the
      // power spectra of the nearend and the echo produces the power spectrum
      // of the input microphone signal.
      for (float& m : output_mask_) {
        m = 1.0f - (1.0f - m) * (1.0f - m);
      }
      data_dumper_->DumpRaw("ml_ree_model_mask", output_mask);
      data_dumper_->DumpRaw("ml_ree_output_mask", output_mask_);
    }
  }

  // Use the latest output mask to produce output echo power estimates.
  for (size_t ch = 0; ch < E2.size(); ++ch) {
    std::transform(E2[ch].begin(), E2[ch].end(), output_mask_.begin(),
                   R2[ch].begin(),
                   [](float power, float mask) { return power * mask; });
    std::copy(R2[ch].begin(), R2[ch].end(), R2_unbounded[ch].begin());
  }
}

EchoCanceller3Config NeuralResidualEchoEstimatorImpl::GetConfiguration(
    bool multi_channel) const {
  EchoCanceller3Config config;
  EchoCanceller3Config::Suppressor::MaskingThresholds tuning_masking_thresholds(
      /*enr_transparent=*/0.0f, /*enr_suppress=*/1.0f,
      /*emr_transparent=*/0.3f);
  EchoCanceller3Config::Suppressor::Tuning tuning(
      /*mask_lf=*/tuning_masking_thresholds,
      /*mask_hf=*/tuning_masking_thresholds, /*max_inc_factor=*/100.0f,
      /*max_dec_factor_lf=*/0.0f);
  config.filter.enable_coarse_filter_output_usage = false;
  config.suppressor.nearend_average_blocks = 1;
  config.suppressor.normal_tuning = tuning;
  config.suppressor.nearend_tuning = tuning;
  config.suppressor.dominant_nearend_detection.enr_threshold = 0.5f;
  config.suppressor.dominant_nearend_detection.trigger_threshold = 2;
  config.suppressor.high_frequency_suppression.limiting_gain_band = 24;
  config.suppressor.high_frequency_suppression.bands_in_limiting_gain = 3;
  return config;
}

void NeuralResidualEchoEstimatorImpl::DumpInputs() {
  data_dumper_->DumpWav("ml_ree_mic_input", input_mic_buffer_, 16000, 1);
  data_dumper_->DumpWav("ml_ree_linear_aec_output",
                        input_linear_aec_output_buffer_, 16000, 1);
  data_dumper_->DumpWav("ml_ree_aec_ref", input_aec_ref_buffer_, 16000, 1);
}

}  // namespace webrtc
