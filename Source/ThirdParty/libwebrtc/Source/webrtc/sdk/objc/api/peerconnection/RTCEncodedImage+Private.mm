/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCEncodedImage+Private.h"

#include "rtc_base/numerics/safe_conversions.h"

@implementation RTCEncodedImage (Private)

- (instancetype)initWithNativeEncodedImage:(webrtc::EncodedImage)encodedImage {
  if (self = [super init]) {
    // Wrap the buffer in NSData without copying, do not take ownership.
    self.buffer = [NSData dataWithBytesNoCopy:encodedImage._buffer
                                   length:encodedImage._length
                             freeWhenDone:NO];
    self.encodedWidth = rtc::dchecked_cast<int32_t>(encodedImage._encodedWidth);
    self.encodedHeight = rtc::dchecked_cast<int32_t>(encodedImage._encodedHeight);
    self.timeStamp = encodedImage.Timestamp();
    self.captureTimeMs = encodedImage.capture_time_ms_;
    self.ntpTimeMs = encodedImage.ntp_time_ms_;
    self.flags = encodedImage.timing_.flags;
    self.encodeStartMs = encodedImage.timing_.encode_start_ms;
    self.encodeFinishMs = encodedImage.timing_.encode_finish_ms;
    self.frameType = static_cast<RTCFrameType>(encodedImage._frameType);
    self.rotation = static_cast<RTCVideoRotation>(encodedImage.rotation_);
    self.completeFrame = encodedImage._completeFrame;
    self.qp = @(encodedImage.qp_);
    self.contentType = (encodedImage.content_type_ == webrtc::VideoContentType::SCREENSHARE) ?
        RTCVideoContentTypeScreenshare :
        RTCVideoContentTypeUnspecified;
    self.spatialIndex = encodedImage.SpatialIndex() ? *encodedImage.SpatialIndex() : 0;
  }

  return self;
}

- (webrtc::EncodedImage)nativeEncodedImage {
  // Return the pointer without copying.
  webrtc::EncodedImage encodedImage(
      (uint8_t *)self.buffer.bytes, (size_t)self.buffer.length, (size_t)self.buffer.length);
  encodedImage._encodedWidth = rtc::dchecked_cast<uint32_t>(self.encodedWidth);
  encodedImage._encodedHeight = rtc::dchecked_cast<uint32_t>(self.encodedHeight);
  encodedImage.SetTimestamp(self.timeStamp);
  encodedImage.capture_time_ms_ = self.captureTimeMs;
  encodedImage.ntp_time_ms_ = self.ntpTimeMs;
  encodedImage.timing_.flags = self.flags;
  encodedImage.timing_.encode_start_ms = self.encodeStartMs;
  encodedImage.timing_.encode_finish_ms = self.encodeFinishMs;
  encodedImage._frameType = webrtc::FrameType(self.frameType);
  encodedImage.rotation_ = webrtc::VideoRotation(self.rotation);
  encodedImage._completeFrame = self.completeFrame;
  encodedImage.qp_ = self.qp ? self.qp.intValue : -1;
  encodedImage.content_type_ = (self.contentType == RTCVideoContentTypeScreenshare) ?
      webrtc::VideoContentType::SCREENSHARE :
      webrtc::VideoContentType::UNSPECIFIED;
  if (self.spatialIndex)
    encodedImage.SetSpatialIndex(self.spatialIndex);
  return encodedImage;
}

@end
