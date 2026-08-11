/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/corruption_detection/frame_instrumentation_generator.h"

#include <memory>

#include "api/video/corruption_detection/frame_instrumentation_generator_factory.h"
#include "api/video/video_codec_type.h"

namespace webrtc {

// TODO: bugs.webrtc.org/358039777 - Remove once downstream usage is gone.
std::unique_ptr<FrameInstrumentationGenerator>
FrameInstrumentationGenerator::Create(VideoCodecType video_codec_type) {
  return FrameInstrumentationGeneratorFactory::Create(video_codec_type);
}

}  // namespace webrtc
