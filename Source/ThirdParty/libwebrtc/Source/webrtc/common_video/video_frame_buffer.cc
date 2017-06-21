/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/common_video/include/video_frame_buffer.h"

#include <string.h>

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/base/keep_ref_until_done.h"
#include "libyuv/convert.h"
#include "libyuv/planar_functions.h"
#include "libyuv/scale.h"

namespace webrtc {

NativeHandleBuffer::NativeHandleBuffer(void* native_handle,
                                       int width,
                                       int height)
    : native_handle_(native_handle), width_(width), height_(height) {
  RTC_DCHECK(native_handle != nullptr);
  RTC_DCHECK_GT(width, 0);
  RTC_DCHECK_GT(height, 0);
}

VideoFrameBuffer::Type NativeHandleBuffer::type() const {
  return Type::kNative;
}

int NativeHandleBuffer::width() const {
  return width_;
}

int NativeHandleBuffer::height() const {
  return height_;
}

const uint8_t* NativeHandleBuffer::DataY() const {
  RTC_NOTREACHED();  // Should not be called.
  return nullptr;
}
const uint8_t* NativeHandleBuffer::DataU() const {
  RTC_NOTREACHED();  // Should not be called.
  return nullptr;
}
const uint8_t* NativeHandleBuffer::DataV() const {
  RTC_NOTREACHED();  // Should not be called.
  return nullptr;
}

int NativeHandleBuffer::StrideY() const {
  RTC_NOTREACHED();  // Should not be called.
  return 0;
}
int NativeHandleBuffer::StrideU() const {
  RTC_NOTREACHED();  // Should not be called.
  return 0;
}
int NativeHandleBuffer::StrideV() const {
  RTC_NOTREACHED();  // Should not be called.
  return 0;
}

void* NativeHandleBuffer::native_handle() const {
  return native_handle_;
}

WrappedI420Buffer::WrappedI420Buffer(int width,
                                     int height,
                                     const uint8_t* y_plane,
                                     int y_stride,
                                     const uint8_t* u_plane,
                                     int u_stride,
                                     const uint8_t* v_plane,
                                     int v_stride,
                                     const rtc::Callback0<void>& no_longer_used)
    : width_(width),
      height_(height),
      y_plane_(y_plane),
      u_plane_(u_plane),
      v_plane_(v_plane),
      y_stride_(y_stride),
      u_stride_(u_stride),
      v_stride_(v_stride),
      no_longer_used_cb_(no_longer_used) {
}

WrappedI420Buffer::~WrappedI420Buffer() {
  no_longer_used_cb_();
}

int WrappedI420Buffer::width() const {
  return width_;
}

int WrappedI420Buffer::height() const {
  return height_;
}

const uint8_t* WrappedI420Buffer::DataY() const {
  return y_plane_;
}
const uint8_t* WrappedI420Buffer::DataU() const {
  return u_plane_;
}
const uint8_t* WrappedI420Buffer::DataV() const {
  return v_plane_;
}

int WrappedI420Buffer::StrideY() const {
  return y_stride_;
}
int WrappedI420Buffer::StrideU() const {
  return u_stride_;
}
int WrappedI420Buffer::StrideV() const {
  return v_stride_;
}

}  // namespace webrtc
