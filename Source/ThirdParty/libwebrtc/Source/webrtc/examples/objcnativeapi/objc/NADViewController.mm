/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "NADViewController.h"

#import "sdk/objc/base/RTCVideoRenderer.h"
#import "sdk/objc/components/capturer/RTCCameraVideoCapturer.h"
#if defined(RTC_SUPPORTS_METAL)
#import "sdk/objc/components/renderer/metal/RTCMTLVideoView.h"  // nogncheck
#endif
#import "sdk/objc/components/renderer/opengl/RTCEAGLVideoView.h"
#import "sdk/objc/helpers/RTCCameraPreviewView.h"

#include <memory>

#include "examples/objcnativeapi/objc/objc_call_client.h"

@interface NADViewController ()

@property(nonatomic) RTCCameraVideoCapturer *capturer;
@property(nonatomic) RTCCameraPreviewView *localVideoView;
@property(nonatomic) __kindof UIView<RTCVideoRenderer> *remoteVideoView;
@property(nonatomic) UIButton *callButton;
@property(nonatomic) UIButton *hangUpButton;

@end

@implementation NADViewController {
  std::unique_ptr<webrtc_examples::ObjCCallClient> _call_client;

  UIView *_view;
}

@synthesize capturer = _capturer;
@synthesize localVideoView = _localVideoView;
@synthesize remoteVideoView = _remoteVideoView;
@synthesize callButton = _callButton;
@synthesize hangUpButton = _hangUpButton;

#pragma mark - View controller lifecycle

- (void)loadView {
  _view = [[UIView alloc] initWithFrame:CGRectZero];

#if defined(RTC_SUPPORTS_METAL)
  _remoteVideoView = [[RTCMTLVideoView alloc] initWithFrame:CGRectZero];
#else
  _remoteVideoView = [[RTCEAGLVideoView alloc] initWithFrame:CGRectZero];
#endif
  _remoteVideoView.translatesAutoresizingMaskIntoConstraints = NO;
  [_view addSubview:_remoteVideoView];

  _localVideoView = [[RTCCameraPreviewView alloc] initWithFrame:CGRectZero];
  _localVideoView.translatesAutoresizingMaskIntoConstraints = NO;
  [_view addSubview:_localVideoView];

  _callButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _callButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_callButton setTitle:@"Call" forState:UIControlStateNormal];
  [_callButton addTarget:self action:@selector(call:) forControlEvents:UIControlEventTouchUpInside];
  [_view addSubview:_callButton];

  _hangUpButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _hangUpButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_hangUpButton setTitle:@"Hang up" forState:UIControlStateNormal];
  [_hangUpButton addTarget:self
                    action:@selector(hangUp:)
          forControlEvents:UIControlEventTouchUpInside];
  [_view addSubview:_hangUpButton];

  UILayoutGuide *margin = _view.layoutMarginsGuide;
  [_remoteVideoView.leadingAnchor constraintEqualToAnchor:margin.leadingAnchor].active = YES;
  [_remoteVideoView.topAnchor constraintEqualToAnchor:margin.topAnchor].active = YES;
  [_remoteVideoView.trailingAnchor constraintEqualToAnchor:margin.trailingAnchor].active = YES;
  [_remoteVideoView.bottomAnchor constraintEqualToAnchor:margin.bottomAnchor].active = YES;

  [_localVideoView.leadingAnchor constraintEqualToAnchor:margin.leadingAnchor constant:8.0].active =
      YES;
  [_localVideoView.topAnchor constraintEqualToAnchor:margin.topAnchor constant:8.0].active = YES;
  [_localVideoView.widthAnchor constraintEqualToConstant:60].active = YES;
  [_localVideoView.heightAnchor constraintEqualToConstant:60].active = YES;

  [_callButton.leadingAnchor constraintEqualToAnchor:margin.leadingAnchor constant:8.0].active =
      YES;
  [_callButton.bottomAnchor constraintEqualToAnchor:margin.bottomAnchor constant:8.0].active = YES;
  [_callButton.widthAnchor constraintEqualToConstant:100].active = YES;
  [_callButton.heightAnchor constraintEqualToConstant:40].active = YES;

  [_hangUpButton.trailingAnchor constraintEqualToAnchor:margin.trailingAnchor constant:8.0].active =
      YES;
  [_hangUpButton.bottomAnchor constraintEqualToAnchor:margin.bottomAnchor constant:8.0].active =
      YES;
  [_hangUpButton.widthAnchor constraintEqualToConstant:100].active = YES;
  [_hangUpButton.heightAnchor constraintEqualToConstant:40].active = YES;

  self.view = _view;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.capturer = [[RTCCameraVideoCapturer alloc] init];
  self.localVideoView.captureSession = self.capturer.captureSession;

  _call_client.reset(new webrtc_examples::ObjCCallClient());

  // Start capturer.
  AVCaptureDevice *selectedDevice = nil;
  NSArray<AVCaptureDevice *> *captureDevices = [RTCCameraVideoCapturer captureDevices];
  for (AVCaptureDevice *device in captureDevices) {
    if (device.position == AVCaptureDevicePositionFront) {
      selectedDevice = device;
      break;
    }
  }

  AVCaptureDeviceFormat *selectedFormat = nil;
  int targetWidth = 640;
  int targetHeight = 480;
  int currentDiff = INT_MAX;
  NSArray<AVCaptureDeviceFormat *> *formats =
      [RTCCameraVideoCapturer supportedFormatsForDevice:selectedDevice];
  for (AVCaptureDeviceFormat *format in formats) {
    CMVideoDimensions dimension = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
    FourCharCode pixelFormat = CMFormatDescriptionGetMediaSubType(format.formatDescription);
    int diff = abs(targetWidth - dimension.width) + abs(targetHeight - dimension.height);
    if (diff < currentDiff) {
      selectedFormat = format;
      currentDiff = diff;
    } else if (diff == currentDiff && pixelFormat == [_capturer preferredOutputPixelFormat]) {
      selectedFormat = format;
    }
  }

  [self.capturer startCaptureWithDevice:selectedDevice format:selectedFormat fps:30];
}

- (void)didReceiveMemoryWarning {
  [super didReceiveMemoryWarning];
  // Dispose of any resources that can be recreated.
}

#pragma mark - Actions

- (IBAction)call:(id)sender {
  _call_client->Call(self.capturer, self.remoteVideoView);
}

- (IBAction)hangUp:(id)sender {
  _call_client->Hangup();
}

@end
