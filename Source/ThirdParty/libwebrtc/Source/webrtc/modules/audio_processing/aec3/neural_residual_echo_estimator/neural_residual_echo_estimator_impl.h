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
#include <span>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "api/audio/echo_canceller3_config.h"
#include "api/audio/neural_residual_echo_estimator.h"
#include "api/audio/tflite_model_handle.h"
#include "api/ref_count.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/neural_residual_echo_estimator/neural_feature_extractor.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
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
    virtual std::span<float> GetInput(
        FeatureExtractor::ModelInputEnum input_enum) = 0;
    virtual std::span<const float> GetOutput(
        FeatureExtractor::ModelOutputEnum output_enum) = 0;
    virtual const audioproc::ReeModelMetadata& GetMetadata() const = 0;
    virtual bool Invoke() = 0;
    virtual void Reset() = 0;
  };

  // Loads a model into a ModelRunner and creates a NeuralResidualEchoEstimator
  // from it. Returns nullptr if any file read or initialization step fails.
  static absl_nullable std::unique_ptr<NeuralResidualEchoEstimator> Create(
      const tflite::FlatBufferModel* model,
      const tflite::OpResolver& op_resolver);

  // Same as `Create`, but always returns a valid echo estimator and initializes
  // asynchronously on a background thread.
  // All APIs may be called before initialization, but will be no-ops. Use
  // `IsInitialized()` to poll for when the estimator starts producing real
  // estimates.
  static absl_nonnull std::unique_ptr<NeuralResidualEchoEstimator> CreateAsync(
      TaskQueueFactory& task_queue_factory,
      std::unique_ptr<tflite::OpResolver> op_resolver,
      scoped_refptr<TfliteModelHandle> model_handle);

  // Load a TF Lite model into a ModelRunner. Exposed for testing.
  static std::unique_ptr<ModelRunner> LoadTfLiteModel(
      const tflite::FlatBufferModel* model,
      const tflite::OpResolver& op_resolver);

  // Constructor used for synchronous initialization.
  explicit NeuralResidualEchoEstimatorImpl(
      std::unique_ptr<ModelRunner> model_runner);

  void Estimate(
      const Block& render,
      std::span<const std::array<float, kBlockSize>> y,
      std::span<const std::array<float, kBlockSize>> e,
      std::span<const std::array<float, kFftLengthBy2Plus1>> S2,
      std::span<const std::array<float, kFftLengthBy2Plus1>> Y2,
      std::span<const std::array<float, kFftLengthBy2Plus1>> E2,
      bool dominant_nearend,
      std::span<std::array<float, kFftLengthBy2Plus1>> R2,
      std::span<std::array<float, kFftLengthBy2Plus1>> R2_unbounded) override;

  EchoCanceller3Config::Suppressor AdjustConfig(
      const EchoCanceller3Config::Suppressor& config) const override;

  void Reset() override;

  bool IsInitialized() override;

 private:
  // State needed for inference, initialized separately from the estimator.
  struct ModelBundle {
    // If present, contains ML model data used by `model_runner`.
    // Keep declaration order: Must outlive `model_runner`.
    scoped_refptr<TfliteModelHandle> model_handle;
    // Encapsulates all ML inference work.
    std::unique_ptr<ModelRunner> model_runner;
    // Feature extractor for the `model_runner`.
    std::unique_ptr<FeatureExtractor> feature_extractor;
    // Metadata from the ML model about how to interpret its outputs.
    bool use_unbounded_mask = false;
  };

  // Thread-safe accessor for sending an initialized model bundle across
  // threads. A background thread uses `Set()` to pass initialized model data
  // when ready. The realtime capture thread uses `TryGet()` to access the data.
  //
  // All state is guarded by mutex. Minimize access on realtime threads.
  struct CrossThreadState : public RefCountInterface {
   public:
    // Sets the initialized model data to be used for processing.
    // Should only be called once, as the capture thread will stop polling
    // `TryGet()` after receiving a model.
    void Set(std::unique_ptr<ModelBundle> bundle) {
      webrtc::MutexLock lock(&mutex_);
      RTC_DCHECK(!model_bundle_);
      model_bundle_ = std::move(bundle);
    }

    // Retrieves the model data to be used for processing, if available.
    std::unique_ptr<ModelBundle> TryGet() {
      webrtc::MutexLock lock(&mutex_);
      return std::move(model_bundle_);
    }

   private:
    mutable webrtc::Mutex mutex_;
    std::unique_ptr<ModelBundle> model_bundle_ RTC_GUARDED_BY(mutex_);
  };

  // Constructor used for async initialization. See CreateAsync for details.
  NeuralResidualEchoEstimatorImpl(
      TaskQueueFactory& task_queue_factory,
      std::unique_ptr<tflite::OpResolver> op_resolver,
      scoped_refptr<TfliteModelHandle> model_handle);

  void DumpInputs(const Block& render,
                  std::span<const std::array<float, kBlockSize>> y,
                  std::span<const std::array<float, kBlockSize>> e);

  // Synchronization state for initializing inference objects off the main
  // thread or realtime threads.
  const scoped_refptr<CrossThreadState> cross_thread_state_;
  // Once initialized, this contains the objects needed for ML inference.
  std::unique_ptr<ModelBundle> model_bundle_;
  // Task queue used to manage background thread initialization.
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> init_queue_;

  // Downsampled model output for what fraction of the power content in the
  // linear AEC output is echo for each bin.
  std::array<float, kFftLengthBy2Plus1> output_mask_;
  std::array<float, kFftLengthBy2Plus1> output_mask_unbounded_;

  std::vector<std::span<const float, kBlockSize>> render_channels_;
  std::vector<std::span<const float, kBlockSize>> y_channels_;
  std::vector<std::span<const float, kBlockSize>> e_channels_;

  static int instance_count_;
  // Pointer to a data dumper that is used for debugging purposes.
  std::unique_ptr<ApmDataDumper> data_dumper_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_NEURAL_RESIDUAL_ECHO_ESTIMATOR_NEURAL_RESIDUAL_ECHO_ESTIMATOR_IMPL_H_
