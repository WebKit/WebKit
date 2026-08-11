/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_GENERATOR_H_
#define API_VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_GENERATOR_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "api/video/corruption_detection/frame_instrumentation_data.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"

namespace webrtc {

// Class that, given raw input frames via `OnCapturedFrame()` and corresponding
// encoded frames via `OnEncodedImage()` will generate FrameInstrumentationData
// for a subset of those frames. This data can be written to RTP packets as
// corruption detection header extensions, allowing the receiver on the other
// end to validate whether the media stream contains any video corruptions or
// not.
class FrameInstrumentationGenerator {
 public:
  // TODO: bugs.webrtc.org/358039777 - Remove once downstream usage is gone.
  static std::unique_ptr<FrameInstrumentationGenerator> Create(
      VideoCodecType video_codec_type);

  virtual ~FrameInstrumentationGenerator() = default;

  virtual void OnCapturedFrame(VideoFrame frame) = 0;
  virtual std::optional<FrameInstrumentationData> OnEncodedImage(
      const EncodedImage& encoded_image) = 0;
  // Indicates that all encoding operations for this frame has been completed.
  virtual void OnFrameReleased(uint32_t rtp_timestamp) = 0;

  // Returns `std::nullopt` if there is no context for the given layer.
  // The layer id is the simulcast id or SVC spatial layer id depending on
  // which structure is used, or zero if no spatial scalability is used.
  virtual std::optional<int> GetHaltonSequenceIndex(int layer_id) const = 0;
  virtual void SetHaltonSequenceIndex(int index, int layer_id) = 0;

 protected:
  FrameInstrumentationGenerator() = default;
};

}  // namespace webrtc

#endif  // API_VIDEO_CORRUPTION_DETECTION_FRAME_INSTRUMENTATION_GENERATOR_H_
