/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/builtin_video_bitrate_allocator_factory.h"

#include "absl/memory/memory.h"
#include "media/base/codec.h"
#include "modules/video_coding/codecs/vp9/svc_rate_allocator.h"
#include "modules/video_coding/utility/default_video_bitrate_allocator.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "rtc_base/system/fallthrough.h"

namespace webrtc {

namespace {

class BuiltinVideoBitrateAllocatorFactory
    : public VideoBitrateAllocatorFactory {
 public:
  BuiltinVideoBitrateAllocatorFactory() = default;
  ~BuiltinVideoBitrateAllocatorFactory() override = default;

  std::unique_ptr<VideoBitrateAllocator> CreateVideoBitrateAllocator(
      const VideoCodec& codec) override {
    std::unique_ptr<VideoBitrateAllocator> rate_allocator;
    switch (codec.codecType) {
      case kVideoCodecVP8:
        RTC_FALLTHROUGH();
      case kVideoCodecH264:
        rate_allocator.reset(new SimulcastRateAllocator(codec));
        break;
      case kVideoCodecVP9:
        rate_allocator.reset(new SvcRateAllocator(codec));
        break;
      default:
        rate_allocator.reset(new DefaultVideoBitrateAllocator(codec));
    }
    return rate_allocator;
  }
};

}  // namespace

std::unique_ptr<VideoBitrateAllocatorFactory>
CreateBuiltinVideoBitrateAllocatorFactory() {
  return absl::make_unique<BuiltinVideoBitrateAllocatorFactory>();
}

}  // namespace webrtc
