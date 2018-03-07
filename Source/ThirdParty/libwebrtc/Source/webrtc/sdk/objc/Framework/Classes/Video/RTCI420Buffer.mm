/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoFrameBuffer.h"

#include "api/video/i420_buffer.h"

@implementation RTCI420Buffer {
 @protected
  rtc::scoped_refptr<webrtc::I420BufferInterface> _i420Buffer;
}

- (instancetype)initWithWidth:(int)width height:(int)height {
  if (self = [super init]) {
    _i420Buffer = webrtc::I420Buffer::Create(width, height);
  }

  return self;
}

- (instancetype)initWithWidth:(int)width
                       height:(int)height
                      strideY:(int)strideY
                      strideU:(int)strideU
                      strideV:(int)strideV {
  if (self = [super init]) {
    _i420Buffer = webrtc::I420Buffer::Create(width, height, strideY, strideU, strideV);
  }

  return self;
}

- (instancetype)initWithFrameBuffer:(rtc::scoped_refptr<webrtc::I420BufferInterface>)i420Buffer {
  if (self = [super init]) {
    _i420Buffer = i420Buffer;
  }

  return self;
}

- (int)width {
  return _i420Buffer->width();
}

- (int)height {
  return _i420Buffer->height();
}

- (int)strideY {
  return _i420Buffer->StrideY();
}

- (int)strideU {
  return _i420Buffer->StrideU();
}

- (int)strideV {
  return _i420Buffer->StrideV();
}

- (int)chromaWidth {
  return _i420Buffer->ChromaWidth();
}

- (int)chromaHeight {
  return _i420Buffer->ChromaHeight();
}

- (const uint8_t *)dataY {
  return _i420Buffer->DataY();
}

- (const uint8_t *)dataU {
  return _i420Buffer->DataU();
}

- (const uint8_t *)dataV {
  return _i420Buffer->DataV();
}

- (id<RTCI420Buffer>)toI420 {
  return self;
}

@end

@implementation RTCMutableI420Buffer

- (uint8_t *)mutableDataY {
  return static_cast<webrtc::I420Buffer *>(_i420Buffer.get())->MutableDataY();
}

- (uint8_t *)mutableDataU {
  return static_cast<webrtc::I420Buffer *>(_i420Buffer.get())->MutableDataU();
}

- (uint8_t *)mutableDataV {
  return static_cast<webrtc::I420Buffer *>(_i420Buffer.get())->MutableDataV();
}

@end
