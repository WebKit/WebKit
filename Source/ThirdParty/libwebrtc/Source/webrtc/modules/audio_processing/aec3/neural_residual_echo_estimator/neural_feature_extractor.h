/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_NEURAL_RESIDUAL_ECHO_ESTIMATOR_NEURAL_FEATURE_EXTRACTOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_NEURAL_RESIDUAL_ECHO_ESTIMATOR_NEURAL_FEATURE_EXTRACTOR_H_

#include <cstring>
#include <memory>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "third_party/pffft/src/pffft.h"

namespace webrtc {

class FeatureExtractor {
 public:
  enum class ModelInputEnum {
    kMic = 0,
    kLinearAecOutput = 1,
    kAecRef = 2,
    kModelState = 3,
    kNumInputs = 4
  };
  enum class ModelOutputEnum {
    kEchoMask = 0,
    kUnboundedEchoMask = 1,
    kModelState = 2,
    kNumOutputs = 3
  };

  virtual ~FeatureExtractor() = default;

  // Returns true if the feature extractor has enough data to produce a full
  // set of features for the model input.
  virtual bool ReadyForInference() const = 0;

  // Buffers the frames for matching the expecting inference step size.
  virtual void UpdateBuffers(
      ArrayView<const ArrayView<const float, kBlockSize>> all_channels,
      ModelInputEnum input_type) = 0;

  // Uses the internal buffer data for producing the model input tensors.
  virtual void PrepareModelInput(ArrayView<float> model_input,
                                 ModelInputEnum input_type) = 0;

  // Resets the internal state of the feature extractor.
  virtual void Reset() = 0;
};

class TimeDomainFeatureExtractor : public FeatureExtractor {
 public:
  explicit TimeDomainFeatureExtractor(int step_size);
  ~TimeDomainFeatureExtractor() override;

  void Reset() override;

  bool ReadyForInference() const override;

  void UpdateBuffers(
      ArrayView<const ArrayView<const float, kBlockSize>> all_channels,
      ModelInputEnum input_type) override;

  void PrepareModelInput(ArrayView<float> model_input,
                         ModelInputEnum input_type) override;

 private:
  const size_t step_size_;
  std::vector<std::vector<float>> input_buffer_;
};

class FrequencyDomainFeatureExtractor : public FeatureExtractor {
 public:
  explicit FrequencyDomainFeatureExtractor(int step_size);
  ~FrequencyDomainFeatureExtractor() override;

  void Reset() override;

  bool ReadyForInference() const override;

  void UpdateBuffers(
      ArrayView<const ArrayView<const float, kBlockSize>> all_channels,
      ModelInputEnum input_type) override;

  void PrepareModelInput(ArrayView<float> model_input,
                         ModelInputEnum input_type) override;

 private:
  class PffftState {
   public:
    explicit PffftState(int frame_size)
        : data_(static_cast<float*>(
              pffft_aligned_malloc(frame_size * sizeof(float)))) {
      std::memset(data_, 0, sizeof(float) * frame_size);
    }
    float* data() { return data_; }
    ~PffftState() { pffft_aligned_free(data_); }

   private:
    float* const data_;
  };

  void ComputeAndAddPowerSpectra(ArrayView<const float> frame,
                                 std::unique_ptr<PffftState>& pffft_state,
                                 int number_channels,
                                 ArrayView<float> power_spectra);

  const size_t step_size_;
  const int frame_size_;
  const std::vector<float> sqrt_hanning_;
  float* const spectrum_;
  float* const work_;
  PFFFT_Setup* pffft_setup_;
  // Indexed by [ModelInputEnum][channel].
  std::vector<std::vector<std::unique_ptr<PffftState>>> pffft_states_;
  // Indexed by [ModelInputEnum][channel][sample].
  std::vector<std::vector<std::vector<float>>> input_buffer_;
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_PROCESSING_AEC3_NEURAL_RESIDUAL_ECHO_ESTIMATOR_NEURAL_FEATURE_EXTRACTOR_H_
