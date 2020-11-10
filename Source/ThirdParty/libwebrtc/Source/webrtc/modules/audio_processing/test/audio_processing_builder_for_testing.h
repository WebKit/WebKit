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
  ~AudioProcessingBuilderForTesting();
  // The AudioProcessingBuilderForTesting takes ownership of the
  // echo_control_factory.
  AudioProcessingBuilderForTesting& SetEchoControlFactory(
      std::unique_ptr<EchoControlFactory> echo_control_factory) {
    echo_control_factory_ = std::move(echo_control_factory);
    return *this;
  }
  // The AudioProcessingBuilderForTesting takes ownership of the
  // capture_post_processing.
  AudioProcessingBuilderForTesting& SetCapturePostProcessing(
      std::unique_ptr<CustomProcessing> capture_post_processing) {
    capture_post_processing_ = std::move(capture_post_processing);
    return *this;
  }
  // The AudioProcessingBuilderForTesting takes ownership of the
  // render_pre_processing.
  AudioProcessingBuilderForTesting& SetRenderPreProcessing(
      std::unique_ptr<CustomProcessing> render_pre_processing) {
    render_pre_processing_ = std::move(render_pre_processing);
    return *this;
  }
  // The AudioProcessingBuilderForTesting takes ownership of the echo_detector.
  AudioProcessingBuilderForTesting& SetEchoDetector(
      rtc::scoped_refptr<EchoDetector> echo_detector) {
    echo_detector_ = std::move(echo_detector);
    return *this;
  }
  // The AudioProcessingBuilderForTesting takes ownership of the
  // capture_analyzer.
  AudioProcessingBuilderForTesting& SetCaptureAnalyzer(
      std::unique_ptr<CustomAudioAnalyzer> capture_analyzer) {
    capture_analyzer_ = std::move(capture_analyzer);
    return *this;
  }
  // This creates an APM instance using the previously set components. Calling
  // the Create function resets the AudioProcessingBuilderForTesting to its
  // initial state.
  AudioProcessing* Create();
  AudioProcessing* Create(const webrtc::Config& config);

 private:
  // Transfers the ownership to a non-testing builder.
  void TransferOwnershipsToBuilder(AudioProcessingBuilder* builder);

  std::unique_ptr<EchoControlFactory> echo_control_factory_;
  std::unique_ptr<CustomProcessing> capture_post_processing_;
  std::unique_ptr<CustomProcessing> render_pre_processing_;
  rtc::scoped_refptr<EchoDetector> echo_detector_;
  std::unique_ptr<CustomAudioAnalyzer> capture_analyzer_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_TEST_AUDIO_PROCESSING_BUILDER_FOR_TESTING_H_
