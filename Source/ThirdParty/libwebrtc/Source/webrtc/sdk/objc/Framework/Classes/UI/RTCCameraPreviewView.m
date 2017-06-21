/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCCameraPreviewView.h"

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>

#import "RTCDispatcher+Private.h"

@implementation RTCCameraPreviewView

@synthesize captureSession = _captureSession;

+ (Class)layerClass {
  return [AVCaptureVideoPreviewLayer class];
}

- (void)setCaptureSession:(AVCaptureSession *)captureSession {
  if (_captureSession == captureSession) {
    return;
  }
  _captureSession = captureSession;
  AVCaptureVideoPreviewLayer *previewLayer = [self previewLayer];
  [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                               block:^{
    previewLayer.session = captureSession;
  }];
}

- (void)layoutSubviews {
  [super layoutSubviews];

  // Update the video orientation based on the device orientation.
  [self setCorrectVideoOrientation];
}

- (void)setCorrectVideoOrientation {
  // Get current device orientation.
  UIDeviceOrientation deviceOrientation = [UIDevice currentDevice].orientation;
  AVCaptureVideoPreviewLayer *previewLayer = [self previewLayer];

  // First check if we are allowed to set the video orientation.
  if (previewLayer.connection.isVideoOrientationSupported) {
    // Set the video orientation based on device orientation.
    if (deviceOrientation == UIInterfaceOrientationPortraitUpsideDown) {
      previewLayer.connection.videoOrientation =
          AVCaptureVideoOrientationPortraitUpsideDown;
    } else if (deviceOrientation == UIInterfaceOrientationLandscapeRight) {
      previewLayer.connection.videoOrientation =
          AVCaptureVideoOrientationLandscapeRight;
    } else if (deviceOrientation == UIInterfaceOrientationLandscapeLeft) {
      previewLayer.connection.videoOrientation =
          AVCaptureVideoOrientationLandscapeLeft;
    } else {
      previewLayer.connection.videoOrientation =
          AVCaptureVideoOrientationPortrait;
    }
  }
}

#pragma mark - Private

- (AVCaptureVideoPreviewLayer *)previewLayer {
  return (AVCaptureVideoPreviewLayer *)self.layer;
}

@end
