/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/mac_capturer.h"

#import "sdk/objc/base/RTCVideoCapturer.h"
#import "sdk/objc/components/capturer/RTCCameraVideoCapturer.h"
#import "sdk/objc/native/api/video_capturer.h"
#import "sdk/objc/native/src/objc_frame_buffer.h"

@interface RTCTestVideoSourceAdapter : NSObject <RTC_OBJC_TYPE (RTCVideoCapturerDelegate)>
@property(nonatomic) webrtc::test::MacCapturer *capturer;
@end

@implementation RTCTestVideoSourceAdapter
@synthesize capturer = _capturer;

- (void)capturer:(RTC_OBJC_TYPE(RTCVideoCapturer) *)capturer
    didCaptureVideoFrame:(RTC_OBJC_TYPE(RTCVideoFrame) *)frame {
  const int64_t timestamp_us = frame.timeStampNs / rtc::kNumNanosecsPerMicrosec;
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer =
      rtc::make_ref_counted<webrtc::ObjCFrameBuffer>(frame.buffer);
  _capturer->OnFrame(webrtc::VideoFrame::Builder()
                         .set_video_frame_buffer(buffer)
                         .set_rotation(webrtc::kVideoRotation_0)
                         .set_timestamp_us(timestamp_us)
                         .build());
}

@end

namespace {

AVCaptureDeviceFormat *SelectClosestFormat(AVCaptureDevice *device, size_t width, size_t height) {
  NSArray<AVCaptureDeviceFormat *> *formats =
      [RTC_OBJC_TYPE(RTCCameraVideoCapturer) supportedFormatsForDevice:device];
  AVCaptureDeviceFormat *selectedFormat = nil;
  int currentDiff = INT_MAX;
  for (AVCaptureDeviceFormat *format in formats) {
    CMVideoDimensions dimension = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
    int diff =
        std::abs((int64_t)width - dimension.width) + std::abs((int64_t)height - dimension.height);
    if (diff < currentDiff) {
      selectedFormat = format;
      currentDiff = diff;
    }
  }
  return selectedFormat;
}

}  // namespace

namespace webrtc {
namespace test {

MacCapturer::MacCapturer(size_t width,
                         size_t height,
                         size_t target_fps,
                         size_t capture_device_index) {
  RTCTestVideoSourceAdapter *adapter = [[RTCTestVideoSourceAdapter alloc] init];
  adapter_ = (__bridge_retained void *)adapter;
  adapter.capturer = this;

  RTC_OBJC_TYPE(RTCCameraVideoCapturer) *capturer =
      [[RTC_OBJC_TYPE(RTCCameraVideoCapturer) alloc] initWithDelegate:adapter];
  capturer_ = (__bridge_retained void *)capturer;

  AVCaptureDevice *device =
      [[RTC_OBJC_TYPE(RTCCameraVideoCapturer) captureDevices] objectAtIndex:capture_device_index];
  AVCaptureDeviceFormat *format = SelectClosestFormat(device, width, height);
  [capturer startCaptureWithDevice:device format:format fps:target_fps];
}

MacCapturer *MacCapturer::Create(size_t width,
                                 size_t height,
                                 size_t target_fps,
                                 size_t capture_device_index) {
  return new MacCapturer(width, height, target_fps, capture_device_index);
}

void MacCapturer::Destroy() {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
  RTCTestVideoSourceAdapter *adapter = (__bridge_transfer RTCTestVideoSourceAdapter *)adapter_;
  RTC_OBJC_TYPE(RTCCameraVideoCapturer) *capturer =
      (__bridge_transfer RTC_OBJC_TYPE(RTCCameraVideoCapturer) *)capturer_;
  [capturer stopCapture];
#pragma clang diagnostic pop
}

MacCapturer::~MacCapturer() {
  Destroy();
}

void MacCapturer::OnFrame(const VideoFrame &frame) {
  TestVideoCapturer::OnFrame(frame);
}

}  // namespace test
}  // namespace webrtc
