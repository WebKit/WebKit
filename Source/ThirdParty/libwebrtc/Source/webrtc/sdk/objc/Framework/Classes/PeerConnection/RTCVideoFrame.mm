/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoFrame.h"

#import "WebRTC/RTCVideoFrameBuffer.h"

@implementation RTCVideoFrame {
  RTCVideoRotation _rotation;
  int64_t _timeStampNs;
}

@synthesize buffer = _buffer;
@synthesize timeStamp;

- (int)width {
  return _buffer.width;
}

- (int)height {
  return _buffer.height;
}

- (RTCVideoRotation)rotation {
  return _rotation;
}

- (int64_t)timeStampNs {
  return _timeStampNs;
}

- (RTCVideoFrame *)newI420VideoFrame {
  return [[RTCVideoFrame alloc] initWithBuffer:[_buffer toI420]
                                      rotation:_rotation
                                   timeStampNs:_timeStampNs];
}

- (instancetype)initWithPixelBuffer:(CVPixelBufferRef)pixelBuffer
                           rotation:(RTCVideoRotation)rotation
                        timeStampNs:(int64_t)timeStampNs {
  return [self initWithBuffer:[[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixelBuffer]
                     rotation:rotation
                  timeStampNs:timeStampNs];
}

- (instancetype)initWithPixelBuffer:(CVPixelBufferRef)pixelBuffer
                        scaledWidth:(int)scaledWidth
                       scaledHeight:(int)scaledHeight
                          cropWidth:(int)cropWidth
                         cropHeight:(int)cropHeight
                              cropX:(int)cropX
                              cropY:(int)cropY
                           rotation:(RTCVideoRotation)rotation
                        timeStampNs:(int64_t)timeStampNs {
  RTCCVPixelBuffer *rtcPixelBuffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer:pixelBuffer
                                                                      adaptedWidth:scaledWidth
                                                                     adaptedHeight:scaledHeight
                                                                         cropWidth:cropWidth
                                                                        cropHeight:cropHeight
                                                                             cropX:cropX
                                                                             cropY:cropY];
  return [self initWithBuffer:rtcPixelBuffer rotation:rotation timeStampNs:timeStampNs];
}

- (instancetype)initWithBuffer:(id<RTCVideoFrameBuffer>)buffer
                      rotation:(RTCVideoRotation)rotation
                   timeStampNs:(int64_t)timeStampNs {
  if (self = [super init]) {
    _buffer = buffer;
    _rotation = rotation;
    _timeStampNs = timeStampNs;
  }

  return self;
}

@end
