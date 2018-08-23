/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_BITRATE_ALLOCATOR_H_
#define API_VIDEO_VIDEO_BITRATE_ALLOCATOR_H_

#include "api/video/video_bitrate_allocation.h"

namespace webrtc {

class VideoBitrateAllocator {
 public:
  VideoBitrateAllocator() {}
  virtual ~VideoBitrateAllocator() {}

  virtual VideoBitrateAllocation GetAllocation(uint32_t total_bitrate_bps,
                                               uint32_t framerate) = 0;
};

class VideoBitrateAllocationObserver {
 public:
  VideoBitrateAllocationObserver() {}
  virtual ~VideoBitrateAllocationObserver() {}

  virtual void OnBitrateAllocationUpdated(
      const VideoBitrateAllocation& allocation) = 0;
};

}  // namespace webrtc

#endif  // API_VIDEO_VIDEO_BITRATE_ALLOCATOR_H_
