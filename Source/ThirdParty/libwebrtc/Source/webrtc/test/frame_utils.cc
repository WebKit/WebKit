/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <string.h>

#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/test/frame_utils.h"

namespace webrtc {
namespace test {

bool EqualPlane(const uint8_t* data1,
                const uint8_t* data2,
                int stride1,
                int stride2,
                int width,
                int height) {
  for (int y = 0; y < height; ++y) {
    if (memcmp(data1, data2, width) != 0)
      return false;
    data1 += stride1;
    data2 += stride2;
  }
  return true;
}

bool FramesEqual(const webrtc::VideoFrame& f1, const webrtc::VideoFrame& f2) {
  if (f1.timestamp() != f2.timestamp() ||
      f1.ntp_time_ms() != f2.ntp_time_ms() ||
      f1.render_time_ms() != f2.render_time_ms()) {
    return false;
  }
  return FrameBufsEqual(f1.video_frame_buffer(), f2.video_frame_buffer());
}

bool FrameBufsEqual(const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& f1,
                    const rtc::scoped_refptr<webrtc::VideoFrameBuffer>& f2) {
  if (f1 == f2) {
    return true;
  }
  // Exlude nullptr (except if both are nullptr, as above)
  if (!f1 || !f2) {
    return false;
  }

  if (f1->width() != f2->width() || f1->height() != f2->height()) {
    return false;
  }
  // Exclude native handle
  if (f1->native_handle()) {
    return f1->native_handle() == f2->native_handle();
  }

  if (f2->native_handle()) {
    return false;
  }
  const int half_width = (f1->width() + 1) / 2;
  const int half_height = (f1->height() + 1) / 2;
  return EqualPlane(f1->DataY(), f2->DataY(),
                    f1->StrideY(), f2->StrideY(),
                    f1->width(), f1->height()) &&
         EqualPlane(f1->DataU(), f2->DataU(),
                    f1->StrideU(), f2->StrideU(),
                    half_width, half_height) &&
         EqualPlane(f1->DataV(), f2->DataV(),
                    f1->StrideV(), f2->StrideV(),
                    half_width, half_height);
}

rtc::scoped_refptr<I420Buffer> ReadI420Buffer(int width, int height, FILE *f) {
  int half_width = (width + 1) / 2;
  rtc::scoped_refptr<I420Buffer> buffer(
      // Explicit stride, no padding between rows.
      I420Buffer::Create(width, height, width, half_width, half_width));
  size_t size_y = static_cast<size_t>(width) * height;
  size_t size_uv = static_cast<size_t>(half_width) * ((height + 1) / 2);

  if (fread(buffer->MutableDataY(), 1, size_y, f) < size_y)
    return nullptr;
  if (fread(buffer->MutableDataU(), 1, size_uv, f) < size_uv)
    return nullptr;
  if (fread(buffer->MutableDataV(), 1, size_uv, f) < size_uv)
    return nullptr;
  return buffer;
}

}  // namespace test
}  // namespace webrtc
