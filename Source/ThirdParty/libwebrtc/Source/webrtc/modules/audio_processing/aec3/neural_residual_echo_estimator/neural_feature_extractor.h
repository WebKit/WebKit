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
#include "third_party/pffft/src/pffft.h"

namespace webrtc {

class FeatureExtractor {
 public:
  enum class ModelInputEnum {
    kModelState = 0,
    kMic = 1,
    kLinearAecOutput = 2,
    kAecRef = 3,
    kNumInputs = 4
  };
  enum class ModelOutputEnum {
    kEchoMask = 0,
    kModelState = 1,
    kNumOutputs = 2
  };

  virtual ~FeatureExtractor() = default;
  virtual void PushFeaturesToModelInput(std::vector<float>& frame,
                                        ArrayView<float> model_input,
                                        ModelInputEnum input_enum) = 0;
};

class TimeDomainFeatureExtractor : public FeatureExtractor {
  void PushFeaturesToModelInput(std::vector<float>& frame,
                                ArrayView<float> model_input,
                                ModelInputEnum input_enum) override;
};

class FrequencyDomainFeatureExtractor : public FeatureExtractor {
 public:
  explicit FrequencyDomainFeatureExtractor(int step_size);
  ~FrequencyDomainFeatureExtractor();
  void PushFeaturesToModelInput(std::vector<float>& frame,
                                ArrayView<float> model_input,
                                ModelInputEnum input_enum) override;

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
  const int step_size_;
  const int frame_size_;
  const std::vector<float> sqrt_hanning_;
  float* const spectrum_;
  float* const work_;
  PFFFT_Setup* pffft_setup_;
  std::vector<std::unique_ptr<PffftState>> pffft_states_;
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_PROCESSING_AEC3_NEURAL_RESIDUAL_ECHO_ESTIMATOR_NEURAL_FEATURE_EXTRACTOR_H_
