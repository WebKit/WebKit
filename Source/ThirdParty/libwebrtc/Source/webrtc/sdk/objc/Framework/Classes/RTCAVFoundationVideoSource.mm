/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCAVFoundationVideoSource+Private.h"

#import "RTCMediaConstraints+Private.h"
#import "RTCPeerConnectionFactory+Private.h"
#import "RTCVideoSource+Private.h"

@implementation RTCAVFoundationVideoSource {
  webrtc::AVFoundationVideoCapturer *_capturer;
}

- (instancetype)initWithFactory:(RTCPeerConnectionFactory *)factory
                    constraints:(RTCMediaConstraints *)constraints {
  NSParameterAssert(factory);
  // We pass ownership of the capturer to the source, but since we own
  // the source, it should be ok to keep a raw pointer to the
  // capturer.
  _capturer = new webrtc::AVFoundationVideoCapturer();
  rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> source =
      factory.nativeFactory->CreateVideoSource(
          _capturer, constraints.nativeConstraints.get());

  return [super initWithNativeVideoSource:source];
}

- (BOOL)canUseBackCamera {
  return self.capturer->CanUseBackCamera();
}

- (BOOL)useBackCamera {
  return self.capturer->GetUseBackCamera();
}

- (void)setUseBackCamera:(BOOL)useBackCamera {
  self.capturer->SetUseBackCamera(useBackCamera);
}

- (AVCaptureSession *)captureSession {
  return self.capturer->GetCaptureSession();
}

- (webrtc::AVFoundationVideoCapturer *)capturer {
  return _capturer;
}

@end
