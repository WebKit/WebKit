/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/fake_texture_frame.h"

#include <cstdint>

#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"

namespace webrtc {
namespace test {

VideoFrame FakeNativeBuffer::CreateFrame(int width,
                                         int height,
                                         uint32_t timestamp,
                                         int64_t render_time_ms,
                                         VideoRotation rotation) {
  return VideoFrame::Builder()
      .set_video_frame_buffer(make_ref_counted<FakeNativeBuffer>(width, height))
      .set_rtp_timestamp(timestamp)
      .set_timestamp_ms(render_time_ms)
      .set_rotation(rotation)
      .build();
}

VideoFrameBuffer::Type FakeNativeBuffer::type() const {
  return Type::kNative;
}

int FakeNativeBuffer::width() const {
  return width_;
}

int FakeNativeBuffer::height() const {
  return height_;
}

scoped_refptr<I420BufferInterface> FakeNativeBuffer::ToI420() {
  scoped_refptr<I420Buffer> buffer = I420Buffer::Create(width_, height_);
  I420Buffer::SetBlack(buffer.get());
  return buffer;
}

}  // namespace test
}  // namespace webrtc
