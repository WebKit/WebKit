/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_OBJC_NATIVE_SRC_OBJC_FRAME_BUFFER_H_
#define SDK_OBJC_NATIVE_SRC_OBJC_FRAME_BUFFER_H_

#import <CoreVideo/CoreVideo.h>

#include "common_video/include/video_frame_buffer.h"
#include "rtc_base/synchronization/mutex.h"

@protocol RTCVideoFrameBuffer;

namespace webrtc {

typedef CVPixelBufferRef (*GetBufferCallback)(void*);
typedef void (*ReleaseBufferCallback)(void*);

class ObjCFrameBuffer : public VideoFrameBuffer {
 public:
  explicit ObjCFrameBuffer(id<RTCVideoFrameBuffer>);
  ~ObjCFrameBuffer() override;

  struct BufferProvider {
    void *pointer { nullptr };
    GetBufferCallback getBuffer { nullptr };
    ReleaseBufferCallback releaseBuffer { nullptr };
  };
  ObjCFrameBuffer(BufferProvider, int width, int height);
  ObjCFrameBuffer(id<RTCVideoFrameBuffer>, int width, int height);

  Type type() const override;

  int width() const override;
  int height() const override;

  rtc::scoped_refptr<I420BufferInterface> ToI420() override;

  id<RTCVideoFrameBuffer> wrapped_frame_buffer() const;
  void* frame_buffer_provider() { return frame_buffer_provider_.pointer; }

 private:
   rtc::scoped_refptr<VideoFrameBuffer> CropAndScale(int offset_x, int offset_y, int crop_width, int crop_height, int scaled_width, int scaled_height) final;

  void set_original_frame(ObjCFrameBuffer& frame) { original_frame_ = &frame; }

  id<RTCVideoFrameBuffer> frame_buffer_;
  BufferProvider frame_buffer_provider_;
  rtc::scoped_refptr<ObjCFrameBuffer> original_frame_;
  int width_;
  int height_;
  mutable webrtc::Mutex mutex_;
};

id<RTCVideoFrameBuffer> ToObjCVideoFrameBuffer(
    const rtc::scoped_refptr<VideoFrameBuffer>& buffer);

}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_SRC_OBJC_FRAME_BUFFER_H_
