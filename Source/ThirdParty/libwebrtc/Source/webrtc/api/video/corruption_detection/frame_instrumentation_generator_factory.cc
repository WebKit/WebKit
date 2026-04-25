/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/corruption_detection/frame_instrumentation_generator_factory.h"

#include <memory>
#include <optional>

#include "api/environment/environment.h"
#include "api/video/corruption_detection/frame_instrumentation_generator.h"
#include "api/video/video_codec_type.h"
#include "api/video_codecs/scalability_mode.h"
#include "video/corruption_detection/frame_instrumentation_generator_impl.h"

namespace webrtc {

std::unique_ptr<FrameInstrumentationGenerator>
FrameInstrumentationGeneratorFactory::Create(VideoCodecType video_codec_type) {
  return std::make_unique<FrameInstrumentationGeneratorImpl>(
      /*environment=*/nullptr, video_codec_type,
      /*scalability_mode=*/std::nullopt);
}

std::unique_ptr<FrameInstrumentationGenerator>
FrameInstrumentationGeneratorFactory::Create(
    const Environment& environment,
    VideoCodecType video_codec_type,
    std::optional<ScalabilityMode> scalability_mode) {
  return std::make_unique<FrameInstrumentationGeneratorImpl>(
      &environment, video_codec_type, scalability_mode);
}

}  // namespace webrtc
