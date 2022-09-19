/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/objc/native/src/objc_frame_buffer.h"

#import "base/RTCVideoFrameBuffer.h"
#import "api/make_ref_counted.h"
#import "sdk/objc/api/video_frame_buffer/RTCNativeI420Buffer+Private.h"
#import "sdk/objc/components/video_frame_buffer/RTCCVPixelBuffer.h"

namespace webrtc {

namespace {

/** ObjCFrameBuffer that conforms to I420BufferInterface by wrapping RTCI420Buffer */
class ObjCI420FrameBuffer : public I420BufferInterface {
 public:
  explicit ObjCI420FrameBuffer(id<RTCI420Buffer> frame_buffer)
      : frame_buffer_(frame_buffer), width_(frame_buffer.width), height_(frame_buffer.height) {}
  ~ObjCI420FrameBuffer() override {
#if defined(WEBRTC_WEBKIT_BUILD)
    [frame_buffer_ close];
#endif
  }

  int width() const override { return width_; }

  int height() const override { return height_; }

  const uint8_t* DataY() const override { return frame_buffer_.dataY; }

  const uint8_t* DataU() const override { return frame_buffer_.dataU; }

  const uint8_t* DataV() const override { return frame_buffer_.dataV; }

  int StrideY() const override { return frame_buffer_.strideY; }

  int StrideU() const override { return frame_buffer_.strideU; }

  int StrideV() const override { return frame_buffer_.strideV; }

 private:
  id<RTCI420Buffer> frame_buffer_;
  int width_;
  int height_;
};

}  // namespace

ObjCFrameBuffer::ObjCFrameBuffer(id<RTCVideoFrameBuffer> frame_buffer)
    : frame_buffer_(frame_buffer), width_(frame_buffer.width), height_(frame_buffer.height) {}

ObjCFrameBuffer::ObjCFrameBuffer(BufferProvider provider, int width, int height)
    : frame_buffer_provider_(provider), width_(width), height_(height) { }

ObjCFrameBuffer::~ObjCFrameBuffer() {
#if defined(WEBRTC_WEBKIT_BUILD)
  [frame_buffer_ close];
#endif
  if (frame_buffer_provider_.releaseBuffer)
    frame_buffer_provider_.releaseBuffer(frame_buffer_provider_.pointer);
}

VideoFrameBuffer::Type ObjCFrameBuffer::type() const {
  return Type::kNative;
}

int ObjCFrameBuffer::width() const {
  return width_;
}

int ObjCFrameBuffer::height() const {
  return height_;
}

rtc::scoped_refptr<I420BufferInterface> ObjCFrameBuffer::ToI420() {
  rtc::scoped_refptr<I420BufferInterface> buffer =
    rtc::make_ref_counted<ObjCI420FrameBuffer>([wrapped_frame_buffer() toI420]);

  return buffer;
}

id<RTCVideoFrameBuffer> ObjCFrameBuffer::wrapped_frame_buffer() const {
  if (frame_buffer_)
    return frame_buffer_;

  {
    webrtc::MutexLock lock(&mutex_);
    if (!frame_buffer_ && frame_buffer_provider_.getBuffer)
      const_cast<ObjCFrameBuffer*>(this)->frame_buffer_ = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:frame_buffer_provider_.getBuffer(frame_buffer_provider_.pointer)];
  }

  return frame_buffer_;
}

id<RTCVideoFrameBuffer> ToObjCVideoFrameBuffer(const rtc::scoped_refptr<VideoFrameBuffer>& buffer) {
  if (buffer->type() == VideoFrameBuffer::Type::kNative) {
    return static_cast<ObjCFrameBuffer*>(buffer.get())->wrapped_frame_buffer();
  } else {
    return [[RTCI420Buffer alloc] initWithFrameBuffer:buffer->ToI420()];
  }
}

}  // namespace webrtc
