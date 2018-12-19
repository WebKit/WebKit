/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_VP9_SVC_RATE_ALLOCATOR_H_
#define MODULES_VIDEO_CODING_CODECS_VP9_SVC_RATE_ALLOCATOR_H_

#include <stdint.h>

#include <vector>

#include "api/video/video_bitrate_allocator.h"
#include "api/video_codecs/video_codec.h"

namespace webrtc {

class SvcRateAllocator : public VideoBitrateAllocator {
 public:
  explicit SvcRateAllocator(const VideoCodec& codec);

  VideoBitrateAllocation GetAllocation(uint32_t total_bitrate_bps,
                                       uint32_t framerate_fps) override;

  static uint32_t GetMaxBitrateBps(const VideoCodec& codec);
  static uint32_t GetPaddingBitrateBps(const VideoCodec& codec);

 private:
  VideoBitrateAllocation GetAllocationNormalVideo(
      uint32_t total_bitrate_bps,
      size_t num_spatial_layers) const;

  VideoBitrateAllocation GetAllocationScreenSharing(
      uint32_t total_bitrate_bps,
      size_t num_spatial_layers) const;

  const VideoCodec codec_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_VP9_SVC_RATE_ALLOCATOR_H_
