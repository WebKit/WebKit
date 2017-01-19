/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoFrame+Private.h"

#include <memory>

#include "webrtc/common_video/rotation.h"

@implementation RTCVideoFrame {
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> _videoBuffer;
  webrtc::VideoRotation _rotation;
  int64_t _timeStampNs;
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> _i420Buffer;
}

- (size_t)width {
  return _videoBuffer->width();
}

- (size_t)height {
  return _videoBuffer->height();
}

- (int)rotation {
  return static_cast<int>(_rotation);
}

// TODO(nisse): chromaWidth and chromaHeight are used only in
// RTCOpenGLVideoRenderer.mm. Update, and then delete these
// properties.
- (size_t)chromaWidth {
  return (self.width + 1) / 2;
}

- (size_t)chromaHeight {
  return (self.height + 1) / 2;
}

- (const uint8_t *)yPlane {
  if (!self.i420Buffer) {
    return nullptr;
  }
  return self.i420Buffer->DataY();
}

- (const uint8_t *)uPlane {
  if (!self.i420Buffer) {
    return nullptr;
  }
  return self.i420Buffer->DataU();
}

- (const uint8_t *)vPlane {
  if (!self.i420Buffer) {
    return nullptr;
  }
  return self.i420Buffer->DataV();
}

- (int32_t)yPitch {
  if (!self.i420Buffer) {
    return 0;
  }
  return self.i420Buffer->StrideY();
}

- (int32_t)uPitch {
  if (!self.i420Buffer) {
    return 0;
  }
  return self.i420Buffer->StrideU();
}

- (int32_t)vPitch {
  if (!self.i420Buffer) {
    return 0;
  }
  return self.i420Buffer->StrideV();
}

- (int64_t)timeStampNs {
  return _timeStampNs;
}

- (CVPixelBufferRef)nativeHandle {
  return static_cast<CVPixelBufferRef>(_videoBuffer->native_handle());
}

- (void)convertBufferIfNeeded {
  if (!_i420Buffer) {
    _i420Buffer = _videoBuffer->native_handle()
                      ? _videoBuffer->NativeToI420Buffer()
                      : _videoBuffer;
  }
}

#pragma mark - Private

- (instancetype)initWithVideoBuffer:
                    (rtc::scoped_refptr<webrtc::VideoFrameBuffer>)videoBuffer
                           rotation:(webrtc::VideoRotation)rotation
                        timeStampNs:(int64_t)timeStampNs {
  if (self = [super init]) {
    _videoBuffer = videoBuffer;
    _rotation = rotation;
    _timeStampNs = timeStampNs;
  }
  return self;
}

- (rtc::scoped_refptr<webrtc::VideoFrameBuffer>)i420Buffer {
  [self convertBufferIfNeeded];
  return _i420Buffer;
}

@end
