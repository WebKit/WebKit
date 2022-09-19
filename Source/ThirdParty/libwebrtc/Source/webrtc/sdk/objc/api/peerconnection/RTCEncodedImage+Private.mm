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

#import <objc/runtime.h>

#include "rtc_base/numerics/safe_conversions.h"
#include "api/make_ref_counted.h"

namespace {
// An implementation of EncodedImageBufferInterface that doesn't perform any copies.
class ObjCEncodedImageBuffer : public webrtc::EncodedImageBufferInterface {
 public:
  static rtc::scoped_refptr<ObjCEncodedImageBuffer> Create(NSData *data) {
    return rtc::make_ref_counted<ObjCEncodedImageBuffer>(data);
  }
  const uint8_t *data() const override { return static_cast<const uint8_t *>(data_.bytes); }
  // TODO(bugs.webrtc.org/9378): delete this non-const data method.
  uint8_t *data() override {
    return const_cast<uint8_t *>(static_cast<const uint8_t *>(data_.bytes));
  }
  size_t size() const override { return data_.length; }

 protected:
  explicit ObjCEncodedImageBuffer(NSData *data) : data_(data) {}
  ~ObjCEncodedImageBuffer() {}

  NSData *data_;
};
}

// A simple wrapper around webrtc::EncodedImageBufferInterface to make it usable with associated
// objects.
__attribute__((objc_runtime_name("WK_RTCWrappedEncodedImageBuffer")))
@interface RTCWrappedEncodedImageBuffer : NSObject
@property(nonatomic) rtc::scoped_refptr<webrtc::EncodedImageBufferInterface> buffer;
- (instancetype)initWithEncodedImageBuffer:
    (rtc::scoped_refptr<webrtc::EncodedImageBufferInterface>)buffer;
@end
@implementation RTCWrappedEncodedImageBuffer
@synthesize buffer = _buffer;
- (instancetype)initWithEncodedImageBuffer:
    (rtc::scoped_refptr<webrtc::EncodedImageBufferInterface>)buffer {
  self = [super init];
  if (self) {
    _buffer = buffer;
  }
  return self;
}
@end

@implementation RTCEncodedImage (Private)

- (rtc::scoped_refptr<webrtc::EncodedImageBufferInterface>)encodedData {
  RTCWrappedEncodedImageBuffer *wrappedBuffer =
      objc_getAssociatedObject(self, @selector(encodedData));
  return wrappedBuffer.buffer;
}

- (void)setEncodedData:(rtc::scoped_refptr<webrtc::EncodedImageBufferInterface>)buffer {
  return objc_setAssociatedObject(
      self,
      @selector(encodedData),
      [[RTCWrappedEncodedImageBuffer alloc] initWithEncodedImageBuffer:buffer],
      OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (instancetype)initWithNativeEncodedImage:(const webrtc::EncodedImage &)encodedImage {
  if (self = [super init]) {
    // A reference to the encodedData must be stored so that it's kept alive as long
    // self.buffer references its underlying data.
    self.encodedData = encodedImage.GetEncodedData();
    // Wrap the buffer in NSData without copying, do not take ownership.
    self.buffer = [NSData dataWithBytesNoCopy:self.encodedData->data()
                                       length:self.encodedData->size()
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
    self.qp = @(encodedImage.qp_);
    self.contentType = (encodedImage.content_type_ == webrtc::VideoContentType::SCREENSHARE) ?
        RTCVideoContentTypeScreenshare :
        RTCVideoContentTypeUnspecified;
  }

  return self;
}

- (webrtc::EncodedImage)nativeEncodedImage {
  // Return the pointer without copying.
  webrtc::EncodedImage encodedImage;
  if (self.encodedData) {
    encodedImage.SetEncodedData(self.encodedData);
  } else if (self.buffer) {
    encodedImage.SetEncodedData(ObjCEncodedImageBuffer::Create(self.buffer));
  }
  encodedImage.set_size(self.buffer.length);
  encodedImage._encodedWidth = rtc::dchecked_cast<uint32_t>(self.encodedWidth);
  encodedImage._encodedHeight = rtc::dchecked_cast<uint32_t>(self.encodedHeight);
  encodedImage.SetTimestamp(self.timeStamp);
  encodedImage.capture_time_ms_ = self.captureTimeMs;
  encodedImage.ntp_time_ms_ = self.ntpTimeMs;
  encodedImage.timing_.flags = self.flags;
  encodedImage.timing_.encode_start_ms = self.encodeStartMs;
  encodedImage.timing_.encode_finish_ms = self.encodeFinishMs;
  encodedImage._frameType = webrtc::VideoFrameType(self.frameType);
  encodedImage.rotation_ = webrtc::VideoRotation(self.rotation);
  encodedImage.qp_ = self.qp ? self.qp.intValue : -1;
  encodedImage.content_type_ = (self.contentType == RTCVideoContentTypeScreenshare) ?
      webrtc::VideoContentType::SCREENSHARE :
      webrtc::VideoContentType::UNSPECIFIED;

  return encodedImage;
}

@end
