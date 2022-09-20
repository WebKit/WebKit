/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_TEST_AUDIO_PROCESSING_BUILDER_FOR_TESTING_H_
#define MODULES_AUDIO_PROCESSING_TEST_AUDIO_PROCESSING_BUILDER_FOR_TESTING_H_

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {

// Facilitates building of AudioProcessingImp for the tests.
class AudioProcessingBuilderForTesting {
 public:
  AudioProcessingBuilderForTesting();
  AudioProcessingBuilderForTesting(const AudioProcessingBuilderForTesting&) =
      delete;
  AudioProcessingBuilderForTesting& operator=(
      const AudioProcessingBuilderForTesting&) = delete;
  ~AudioProcessingBuilderForTesting();

  // Sets the APM configuration.
  AudioProcessingBuilderForTesting& SetConfig(
      const AudioProcessing::Config& config) {
    config_ = config;
    return *this;
  }

  // Sets the echo controller factory to inject when APM is created.
  AudioProcessingBuilderForTesting& SetEchoControlFactory(
      std::unique_ptr<EchoControlFactory> echo_control_factory) {
    echo_control_factory_ = std::move(echo_control_factory);
    return *this;
  }

  // Sets the capture post-processing sub-module to inject when APM is created.
  AudioProcessingBuilderForTesting& SetCapturePostProcessing(
      std::unique_ptr<CustomProcessing> capture_post_processing) {
    capture_post_processing_ = std::move(capture_post_processing);
    return *this;
  }

  // Sets the render pre-processing sub-module to inject when APM is created.
  AudioProcessingBuilderForTesting& SetRenderPreProcessing(
      std::unique_ptr<CustomProcessing> render_pre_processing) {
    render_pre_processing_ = std::move(render_pre_processing);
    return *this;
  }

  // Sets the echo detector to inject when APM is created.
  AudioProcessingBuilderForTesting& SetEchoDetector(
      rtc::scoped_refptr<EchoDetector> echo_detector) {
    echo_detector_ = std::move(echo_detector);
    return *this;
  }

  // Sets the capture analyzer sub-module to inject when APM is created.
  AudioProcessingBuilderForTesting& SetCaptureAnalyzer(
      std::unique_ptr<CustomAudioAnalyzer> capture_analyzer) {
    capture_analyzer_ = std::move(capture_analyzer);
    return *this;
  }

  // Creates an APM instance with the specified config or the default one if
  // unspecified. Injects the specified components transferring the ownership
  // to the newly created APM instance - i.e., except for the config, the
  // builder is reset to its initial state.
  rtc::scoped_refptr<AudioProcessing> Create();

 private:
  // Transfers the ownership to a non-testing builder.
  void TransferOwnershipsToBuilder(AudioProcessingBuilder* builder);

  AudioProcessing::Config config_;
  std::unique_ptr<EchoControlFactory> echo_control_factory_;
  std::unique_ptr<CustomProcessing> capture_post_processing_;
  std::unique_ptr<CustomProcessing> render_pre_processing_;
  rtc::scoped_refptr<EchoDetector> echo_detector_;
  std::unique_ptr<CustomAudioAnalyzer> capture_analyzer_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_TEST_AUDIO_PROCESSING_BUILDER_FOR_TESTING_H_
