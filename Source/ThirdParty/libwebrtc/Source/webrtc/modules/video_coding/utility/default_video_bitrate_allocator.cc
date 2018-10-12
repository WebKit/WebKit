/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/default_video_bitrate_allocator.h"

#include <stdint.h>

#include <algorithm>

namespace webrtc {

DefaultVideoBitrateAllocator::DefaultVideoBitrateAllocator(
    const VideoCodec& codec)
    : codec_(codec) {}

DefaultVideoBitrateAllocator::~DefaultVideoBitrateAllocator() {}

// TODO(http://crbug.com/webrtc/9671): Do not split bitrate between simulcast
// streams, but allocate everything to the first stream.
VideoBitrateAllocation DefaultVideoBitrateAllocator::GetAllocation(
    uint32_t total_bitrate_bps,
    uint32_t framerate) {
  VideoBitrateAllocation allocation;
  if (total_bitrate_bps == 0 || !codec_.active)
    return allocation;

  uint32_t allocated_bitrate_bps = total_bitrate_bps;
  allocated_bitrate_bps =
      std::max(allocated_bitrate_bps, codec_.minBitrate * 1000);
  if (codec_.maxBitrate > 0) {
    allocated_bitrate_bps =
        std::min(allocated_bitrate_bps, codec_.maxBitrate * 1000);
  }
  size_t num_simulcast_streams =
      std::max<size_t>(1, codec_.numberOfSimulcastStreams);
  // The bitrate is split between all the streams in proportion of powers of 2
  // e.g. 1:2, 1:2:4, etc.
  for (size_t i = 0; i < num_simulcast_streams; i++) {
    allocation.SetBitrate(
        i, 0,
        allocated_bitrate_bps * (1 << i) / ((1 << num_simulcast_streams) - 1));
  }

  return allocation;
}

}  // namespace webrtc
