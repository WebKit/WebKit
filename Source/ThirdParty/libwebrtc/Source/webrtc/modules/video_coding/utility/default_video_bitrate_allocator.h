/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_UTILITY_DEFAULT_VIDEO_BITRATE_ALLOCATOR_H_
#define WEBRTC_MODULES_VIDEO_CODING_UTILITY_DEFAULT_VIDEO_BITRATE_ALLOCATOR_H_

#include "webrtc/common_video/include/video_bitrate_allocator.h"

namespace webrtc {

class DefaultVideoBitrateAllocator : public VideoBitrateAllocator {
 public:
  explicit DefaultVideoBitrateAllocator(const VideoCodec& codec);
  ~DefaultVideoBitrateAllocator() override;

  BitrateAllocation GetAllocation(uint32_t total_bitrate,
                                  uint32_t framerate) override;
  uint32_t GetPreferredBitrateBps(uint32_t framerate) override;

 private:
  const VideoCodec codec_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_UTILITY_DEFAULT_VIDEO_BITRATE_ALLOCATOR_H_
