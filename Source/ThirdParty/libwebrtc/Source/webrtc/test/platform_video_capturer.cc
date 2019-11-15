/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/platform_video_capturer.h"

#include "absl/memory/memory.h"
#if defined(WEBRTC_MAC)
#include "test/mac_capturer.h"
#else
#include "test/vcm_capturer.h"
#endif

namespace webrtc {
namespace test {

std::unique_ptr<TestVideoCapturer> CreateVideoCapturer(
    size_t width,
    size_t height,
    size_t target_fps,
    size_t capture_device_index) {
#if defined(WEBRTC_MAC)
  return absl::WrapUnique<TestVideoCapturer>(test::MacCapturer::Create(
      width, height, target_fps, capture_device_index));
#else
  return absl::WrapUnique<TestVideoCapturer>(test::VcmCapturer::Create(
      width, height, target_fps, capture_device_index));
#endif
}

}  // namespace test
}  // namespace webrtc
