/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_DEFAULT_VIDEO_BITRATE_ALLOCATOR_H_
#define MODULES_VIDEO_CODING_UTILITY_DEFAULT_VIDEO_BITRATE_ALLOCATOR_H_

#include <stdint.h>

#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_bitrate_allocator.h"
#include "api/video_codecs/video_codec.h"

namespace webrtc {

class DefaultVideoBitrateAllocator : public VideoBitrateAllocator {
 public:
  explicit DefaultVideoBitrateAllocator(const VideoCodec& codec);
  ~DefaultVideoBitrateAllocator() override;

  VideoBitrateAllocation Allocate(
      VideoBitrateAllocationParameters parameters) override;

 private:
  const VideoCodec codec_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_UTILITY_DEFAULT_VIDEO_BITRATE_ALLOCATOR_H_
