/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_FRAME_TEXTURE_H_
#define MODULES_DESKTOP_CAPTURE_FRAME_TEXTURE_H_

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// FrameTexture is a base class for platform-specific texture handles. It stores
// the texture handle.
class RTC_EXPORT FrameTexture {
 public:
  // Platform-specific handle type for the GPU texture resource.
  // On Windows, this is a HANDLE (e.g., DXGI shared resource handle).
  // On other platforms, this is an integer file descriptor.
#if defined(WEBRTC_WIN)
  typedef void* Handle;
  static constexpr Handle kInvalidHandle = nullptr;
#else
  typedef int Handle;
  static constexpr Handle kInvalidHandle = -1;
#endif
  // Platform-specific handle of the texture.
  Handle handle() const { return handle_; }

  virtual ~FrameTexture();

  FrameTexture(const FrameTexture&) = delete;
  FrameTexture& operator=(const FrameTexture&) = delete;

 protected:
  explicit FrameTexture(Handle handle);

  Handle handle_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_FRAME_TEXTURE_H_
