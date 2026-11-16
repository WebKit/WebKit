/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CORRUPTION_DETECTION_VIDEO_FRAME_SAMPLER_H_
#define VIDEO_CORRUPTION_DETECTION_VIDEO_FRAME_SAMPLER_H_

#include <cstdint>
#include <memory>

#include "api/video/video_frame.h"

namespace webrtc {

class VideoFrameSampler {
 public:
  static std::unique_ptr<VideoFrameSampler> Create(const VideoFrame& frame);
  virtual ~VideoFrameSampler() = default;

  enum class ChannelType { Y, U, V };
  virtual uint8_t GetSampleValue(ChannelType channel,
                                 int col,
                                 int row) const = 0;
  virtual int width(ChannelType channel) const = 0;
  virtual int height(ChannelType channel) const = 0;
};

}  // namespace webrtc

#endif  // VIDEO_CORRUPTION_DETECTION_VIDEO_FRAME_SAMPLER_H_
