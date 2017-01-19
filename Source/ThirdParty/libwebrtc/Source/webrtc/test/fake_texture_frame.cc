/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/fake_texture_frame.h"

namespace webrtc {
namespace test {

VideoFrame FakeNativeHandle::CreateFrame(FakeNativeHandle* native_handle,
                                         int width,
                                         int height,
                                         uint32_t timestamp,
                                         int64_t render_time_ms,
                                         VideoRotation rotation) {
  return VideoFrame(new rtc::RefCountedObject<FakeNativeHandleBuffer>(
                        native_handle, width, height),
                    timestamp, render_time_ms, rotation);
}
}  // namespace test
}  // namespace webrtc
