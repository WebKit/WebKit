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

extern const float kSpatialLayeringRateScalingFactor;
extern const float kTemporalLayeringRateScalingFactor;

class SvcRateAllocator : public VideoBitrateAllocator {
 public:
  explicit SvcRateAllocator(const VideoCodec& codec);

  VideoBitrateAllocation GetAllocation(uint32_t total_bitrate_bps,
                                       uint32_t framerate_fps) override;

 private:
  VideoBitrateAllocation GetAllocationNormalVideo(
      uint32_t total_bitrate_bps) const;
  VideoBitrateAllocation GetAllocationScreenSharing(
      uint32_t total_bitrate_bps) const;
  std::vector<size_t> SplitBitrate(size_t num_layers,
                                   size_t total_bitrate,
                                   float rate_scaling_factor) const;
  bool AdjustAndVerify(std::vector<size_t>* spatial_layer_bitrate_bps) const;

  const VideoCodec codec_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_VP9_SVC_RATE_ALLOCATOR_H_
