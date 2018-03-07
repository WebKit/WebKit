/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_INCLUDE_VIDEO_BITRATE_ALLOCATOR_H_
#define COMMON_VIDEO_INCLUDE_VIDEO_BITRATE_ALLOCATOR_H_

#include "common_types.h"  // NOLINT(build/include)

namespace webrtc {

class VideoBitrateAllocator {
 public:
  VideoBitrateAllocator() {}
  virtual ~VideoBitrateAllocator() {}

  virtual BitrateAllocation GetAllocation(uint32_t total_bitrate,
                                          uint32_t framerate) = 0;
  virtual uint32_t GetPreferredBitrateBps(uint32_t framerate) = 0;
};

class VideoBitrateAllocationObserver {
 public:
  VideoBitrateAllocationObserver() {}
  virtual ~VideoBitrateAllocationObserver() {}

  virtual void OnBitrateAllocationUpdated(
      const BitrateAllocation& allocation) = 0;
};

}  // namespace webrtc

#endif  // COMMON_VIDEO_INCLUDE_VIDEO_BITRATE_ALLOCATOR_H_
