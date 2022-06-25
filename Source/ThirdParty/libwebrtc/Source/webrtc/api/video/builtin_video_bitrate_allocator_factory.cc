/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/builtin_video_bitrate_allocator_factory.h"

#include <memory>

#include "absl/base/macros.h"
#include "api/video/video_bitrate_allocator.h"
#include "api/video_codecs/video_codec.h"
#include "modules/video_coding/svc/svc_rate_allocator.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"

namespace webrtc {

namespace {

class BuiltinVideoBitrateAllocatorFactory
    : public VideoBitrateAllocatorFactory {
 public:
  BuiltinVideoBitrateAllocatorFactory() = default;
  ~BuiltinVideoBitrateAllocatorFactory() override = default;

  std::unique_ptr<VideoBitrateAllocator> CreateVideoBitrateAllocator(
      const VideoCodec& codec) override {
    switch (codec.codecType) {
      case kVideoCodecAV1:
      case kVideoCodecVP9:
        return std::make_unique<SvcRateAllocator>(codec);
      default:
        return std::make_unique<SimulcastRateAllocator>(codec);
    }
  }
};

}  // namespace

std::unique_ptr<VideoBitrateAllocatorFactory>
CreateBuiltinVideoBitrateAllocatorFactory() {
  return std::make_unique<BuiltinVideoBitrateAllocatorFactory>();
}

}  // namespace webrtc
