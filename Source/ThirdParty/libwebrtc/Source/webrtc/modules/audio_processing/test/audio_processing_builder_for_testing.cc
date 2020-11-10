/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/audio_processing_builder_for_testing.h"

#include <memory>
#include <utility>

#include "modules/audio_processing/audio_processing_impl.h"
#include "rtc_base/ref_counted_object.h"

namespace webrtc {

AudioProcessingBuilderForTesting::AudioProcessingBuilderForTesting() = default;
AudioProcessingBuilderForTesting::~AudioProcessingBuilderForTesting() = default;

#ifdef WEBRTC_EXCLUDE_AUDIO_PROCESSING_MODULE

AudioProcessing* AudioProcessingBuilderForTesting::Create() {
  webrtc::Config config;
  return Create(config);
}

AudioProcessing* AudioProcessingBuilderForTesting::Create(
    const webrtc::Config& config) {
  return new rtc::RefCountedObject<AudioProcessingImpl>(
      config, std::move(capture_post_processing_),
      std::move(render_pre_processing_), std::move(echo_control_factory_),
      std::move(echo_detector_), std::move(capture_analyzer_));
}

#else

AudioProcessing* AudioProcessingBuilderForTesting::Create() {
  AudioProcessingBuilder builder;
  TransferOwnershipsToBuilder(&builder);
  return builder.Create();
}

AudioProcessing* AudioProcessingBuilderForTesting::Create(
    const webrtc::Config& config) {
  AudioProcessingBuilder builder;
  TransferOwnershipsToBuilder(&builder);
  return builder.Create(config);
}

#endif

void AudioProcessingBuilderForTesting::TransferOwnershipsToBuilder(
    AudioProcessingBuilder* builder) {
  builder->SetCapturePostProcessing(std::move(capture_post_processing_));
  builder->SetRenderPreProcessing(std::move(render_pre_processing_));
  builder->SetCaptureAnalyzer(std::move(capture_analyzer_));
  builder->SetEchoControlFactory(std::move(echo_control_factory_));
  builder->SetEchoDetector(std::move(echo_detector_));
}

}  // namespace webrtc
