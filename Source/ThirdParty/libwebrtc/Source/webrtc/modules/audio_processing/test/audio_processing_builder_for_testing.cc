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

namespace webrtc {

AudioProcessingBuilderForTesting::AudioProcessingBuilderForTesting() = default;
AudioProcessingBuilderForTesting::~AudioProcessingBuilderForTesting() = default;

#ifdef WEBRTC_EXCLUDE_AUDIO_PROCESSING_MODULE

rtc::scoped_refptr<AudioProcessing> AudioProcessingBuilderForTesting::Create() {
  return rtc::make_ref_counted<AudioProcessingImpl>(
      config_, std::move(capture_post_processing_),
      std::move(render_pre_processing_), std::move(echo_control_factory_),
      std::move(echo_detector_), std::move(capture_analyzer_));
}

#else

rtc::scoped_refptr<AudioProcessing> AudioProcessingBuilderForTesting::Create() {
  AudioProcessingBuilder builder;
  TransferOwnershipsToBuilder(&builder);
  return builder.SetConfig(config_).Create();
}

#endif

void AudioProcessingBuilderForTesting::TransferOwnershipsToBuilder(
    AudioProcessingBuilder* builder) {
  builder->SetCapturePostProcessing(std::move(capture_post_processing_));
  builder->SetRenderPreProcessing(std::move(render_pre_processing_));
  builder->SetEchoControlFactory(std::move(echo_control_factory_));
  builder->SetEchoDetector(std::move(echo_detector_));
  builder->SetCaptureAnalyzer(std::move(capture_analyzer_));
}

}  // namespace webrtc
