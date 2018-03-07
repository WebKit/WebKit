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

#import "WebRTC/RTCVideoFrame.h"
#import "WebRTC/RTCVideoFrameBuffer.h"

#include "api/video/video_frame.h"
#include "rtc_base/timeutils.h"

id<RTCVideoFrameBuffer> nativeToRtcFrameBuffer(
    const rtc::scoped_refptr<webrtc::VideoFrameBuffer> &buffer) {
  return buffer->type() == webrtc::VideoFrameBuffer::Type::kNative ?
      static_cast<webrtc::ObjCFrameBuffer *>(buffer.get())->wrapped_frame_buffer() :
      [[RTCI420Buffer alloc] initWithFrameBuffer:buffer->ToI420()];
}

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

- (instancetype)initWithNativeVideoFrame:(const webrtc::VideoFrame &)frame {
  if (self = [self initWithBuffer:nativeToRtcFrameBuffer(frame.video_frame_buffer())
                         rotation:RTCVideoRotation(frame.rotation())
                      timeStampNs:frame.timestamp_us() * rtc::kNumNanosecsPerMicrosec]) {
    self.timeStamp = frame.timestamp();
  }

  return self;
}

- (webrtc::VideoFrame)nativeVideoFrame {
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frameBuffer =
      new rtc::RefCountedObject<webrtc::ObjCFrameBuffer>(self.buffer);
  webrtc::VideoFrame videoFrame(frameBuffer,
                                (webrtc::VideoRotation)self.rotation,
                                self.timeStampNs / rtc::kNumNanosecsPerMicrosec);
  videoFrame.set_timestamp(self.timeStamp);
  return videoFrame;
}

@end
