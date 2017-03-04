/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_VIDEO_VIDEO_FRAME_BUFFER_H_
#define WEBRTC_API_VIDEO_VIDEO_FRAME_BUFFER_H_

#include <stdint.h>

#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ref_ptr.h"

namespace webrtc {

// Interface of a simple frame buffer containing pixel data. This interface does
// not contain any frame metadata such as rotation, timestamp, pixel_width, etc.
class VideoFrameBuffer : public rtc::RefCountInterface {
 public:
  // The resolution of the frame in pixels. For formats where some planes are
  // subsampled, this is the highest-resolution plane.
  virtual int width() const = 0;
  virtual int height() const = 0;

  // Returns pointer to the pixel data for a given plane. The memory is owned by
  // the VideoFrameBuffer object and must not be freed by the caller.
  virtual const uint8_t* DataY() const = 0;
  virtual const uint8_t* DataU() const = 0;
  virtual const uint8_t* DataV() const = 0;

  // Returns the number of bytes between successive rows for a given plane.
  virtual int StrideY() const = 0;
  virtual int StrideU() const = 0;
  virtual int StrideV() const = 0;

  // Return the handle of the underlying video frame. This is used when the
  // frame is backed by a texture.
  virtual void* native_handle() const = 0;

  // Returns a new memory-backed frame buffer converted from this buffer's
  // native handle.
  virtual rtc::scoped_refptr<VideoFrameBuffer> NativeToI420Buffer() = 0;

 protected:
  ~VideoFrameBuffer() override {}
};

}  // namespace webrtc

#endif  // WEBRTC_API_VIDEO_VIDEO_FRAME_BUFFER_H_
