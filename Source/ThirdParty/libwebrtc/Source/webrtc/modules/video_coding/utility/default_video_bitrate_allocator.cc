/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/utility/default_video_bitrate_allocator.h"

#include <stdint.h>

namespace webrtc {

DefaultVideoBitrateAllocator::DefaultVideoBitrateAllocator(
    const VideoCodec& codec)
    : codec_(codec) {}

DefaultVideoBitrateAllocator::~DefaultVideoBitrateAllocator() {}

BitrateAllocation DefaultVideoBitrateAllocator::GetAllocation(
    uint32_t total_bitrate_bps,
    uint32_t framerate) {
  BitrateAllocation allocation;
  if (total_bitrate_bps == 0)
    return allocation;

  if (total_bitrate_bps < codec_.minBitrate * 1000) {
    allocation.SetBitrate(0, 0, codec_.minBitrate * 1000);
  } else if (codec_.maxBitrate > 0 &&
             total_bitrate_bps > codec_.maxBitrate * 1000) {
    allocation.SetBitrate(0, 0, codec_.maxBitrate * 1000);
  } else {
    allocation.SetBitrate(0, 0, total_bitrate_bps);
  }
  return allocation;
}

uint32_t DefaultVideoBitrateAllocator::GetPreferredBitrateBps(
    uint32_t framerate) {
  return GetAllocation(codec_.maxBitrate * 1000, framerate).get_sum_bps();
}

}  // namespace webrtc
