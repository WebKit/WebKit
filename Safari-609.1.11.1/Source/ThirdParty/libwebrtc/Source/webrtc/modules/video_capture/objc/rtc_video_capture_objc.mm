/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import <AVFoundation/AVFoundation.h>
#ifdef WEBRTC_IOS
#import <UIKit/UIKit.h>
#endif

#import "modules/video_capture/objc/device_info_objc.h"
#import "modules/video_capture/objc/rtc_video_capture_objc.h"

#include "rtc_base/logging.h"

using namespace webrtc;
using namespace webrtc::videocapturemodule;

@interface RTCVideoCaptureIosObjC (hidden)
- (int)changeCaptureInputWithName:(NSString*)captureDeviceName;
@end

@implementation RTCVideoCaptureIosObjC {
  webrtc::videocapturemodule::VideoCaptureIos* _owner;
  webrtc::VideoCaptureCapability _capability;
  AVCaptureSession* _captureSession;
  BOOL _orientationHasChanged;
  AVCaptureConnection* _connection;
  BOOL _captureChanging;  // Guarded by _captureChangingCondition.
  NSCondition* _captureChangingCondition;
}

@synthesize frameRotation = _framRotation;

- (id)initWithOwner:(VideoCaptureIos*)owner {
  if (self = [super init]) {
    _owner = owner;
    _captureSession = [[AVCaptureSession alloc] init];
#if defined(WEBRTC_IOS)
    _captureSession.usesApplicationAudioSession = NO;
#endif
    _captureChanging = NO;
    _captureChangingCondition = [[NSCondition alloc] init];

    if (!_captureSession || !_captureChangingCondition) {
      return nil;
    }

    // create and configure a new output (using callbacks)
    AVCaptureVideoDataOutput* captureOutput = [[AVCaptureVideoDataOutput alloc] init];
    NSString* key = (NSString*)kCVPixelBufferPixelFormatTypeKey;

    NSNumber* val = [NSNumber numberWithUnsignedInt:kCVPixelFormatType_422YpCbCr8];
    NSDictionary* videoSettings = [NSDictionary dictionaryWithObject:val forKey:key];
    captureOutput.videoSettings = videoSettings;

    // add new output
    if ([_captureSession canAddOutput:captureOutput]) {
      [_captureSession addOutput:captureOutput];
    } else {
      RTC_LOG(LS_ERROR) << __FUNCTION__ << ": Could not add output to AVCaptureSession";
    }

#ifdef WEBRTC_IOS
    [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];

    NSNotificationCenter* notify = [NSNotificationCenter defaultCenter];
    [notify addObserver:self
               selector:@selector(onVideoError:)
                   name:AVCaptureSessionRuntimeErrorNotification
                 object:_captureSession];
    [notify addObserver:self
               selector:@selector(deviceOrientationDidChange:)
                   name:UIDeviceOrientationDidChangeNotification
                 object:nil];
#endif
  }

  return self;
}

- (void)directOutputToSelf {
  [[self currentOutput]
      setSampleBufferDelegate:self
                        queue:dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)];
}

- (void)directOutputToNil {
  [[self currentOutput] setSampleBufferDelegate:nil queue:NULL];
}

- (void)deviceOrientationDidChange:(NSNotification*)notification {
  _orientationHasChanged = YES;
  [self setRelativeVideoOrientation];
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (BOOL)setCaptureDeviceByUniqueId:(NSString*)uniqueId {
  [self waitForCaptureChangeToFinish];
  // check to see if the camera is already set
  if (_captureSession) {
    NSArray* currentInputs = [NSArray arrayWithArray:[_captureSession inputs]];
    if ([currentInputs count] > 0) {
      AVCaptureDeviceInput* currentInput = [currentInputs objectAtIndex:0];
      if ([uniqueId isEqualToString:[currentInput.device localizedName]]) {
        return YES;
      }
    }
  }

  return [self changeCaptureInputByUniqueId:uniqueId];
}

- (BOOL)startCaptureWithCapability:(const VideoCaptureCapability&)capability {
  [self waitForCaptureChangeToFinish];
  if (!_captureSession) {
    return NO;
  }

  // check limits of the resolution
  if (capability.maxFPS < 0 || capability.maxFPS > 60) {
    return NO;
  }

  if ([_captureSession canSetSessionPreset:AVCaptureSessionPreset1280x720]) {
    if (capability.width > 1280 || capability.height > 720) {
      return NO;
    }
  } else if ([_captureSession canSetSessionPreset:AVCaptureSessionPreset640x480]) {
    if (capability.width > 640 || capability.height > 480) {
      return NO;
    }
  } else if ([_captureSession canSetSessionPreset:AVCaptureSessionPreset352x288]) {
    if (capability.width > 352 || capability.height > 288) {
      return NO;
    }
  } else if (capability.width < 0 || capability.height < 0) {
    return NO;
  }

  _capability = capability;

  AVCaptureVideoDataOutput* currentOutput = [self currentOutput];
  if (!currentOutput) return NO;

  [self directOutputToSelf];

  _orientationHasChanged = NO;
  _captureChanging = YES;
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    [self startCaptureInBackgroundWithOutput:currentOutput];
  });
  return YES;
}

- (AVCaptureVideoDataOutput*)currentOutput {
  return [[_captureSession outputs] firstObject];
}

- (void)startCaptureInBackgroundWithOutput:(AVCaptureVideoDataOutput*)currentOutput {
  NSString* captureQuality = [NSString stringWithString:AVCaptureSessionPresetLow];
  if (_capability.width >= 1280 || _capability.height >= 720) {
    captureQuality = [NSString stringWithString:AVCaptureSessionPreset1280x720];
  } else if (_capability.width >= 640 || _capability.height >= 480) {
    captureQuality = [NSString stringWithString:AVCaptureSessionPreset640x480];
  } else if (_capability.width >= 352 || _capability.height >= 288) {
    captureQuality = [NSString stringWithString:AVCaptureSessionPreset352x288];
  }

  // begin configuration for the AVCaptureSession
  [_captureSession beginConfiguration];

  // picture resolution
  [_captureSession setSessionPreset:captureQuality];

  _connection = [currentOutput connectionWithMediaType:AVMediaTypeVideo];
  [self setRelativeVideoOrientation];

  // finished configuring, commit settings to AVCaptureSession.
  [_captureSession commitConfiguration];

  [_captureSession startRunning];
  [self signalCaptureChangeEnd];
}

- (void)setRelativeVideoOrientation {
  if (!_connection.supportsVideoOrientation) {
    return;
  }
#ifndef WEBRTC_IOS
  _connection.videoOrientation = AVCaptureVideoOrientationLandscapeRight;
  return;
#else
  switch ([UIDevice currentDevice].orientation) {
    case UIDeviceOrientationPortrait:
      _connection.videoOrientation = AVCaptureVideoOrientationPortrait;
      break;
    case UIDeviceOrientationPortraitUpsideDown:
      _connection.videoOrientation = AVCaptureVideoOrientationPortraitUpsideDown;
      break;
    case UIDeviceOrientationLandscapeLeft:
      _connection.videoOrientation = AVCaptureVideoOrientationLandscapeRight;
      break;
    case UIDeviceOrientationLandscapeRight:
      _connection.videoOrientation = AVCaptureVideoOrientationLandscapeLeft;
      break;
    case UIDeviceOrientationFaceUp:
    case UIDeviceOrientationFaceDown:
    case UIDeviceOrientationUnknown:
      if (!_orientationHasChanged) {
        _connection.videoOrientation = AVCaptureVideoOrientationPortrait;
      }
      break;
  }
#endif
}

- (void)onVideoError:(NSNotification*)notification {
  NSLog(@"onVideoError: %@", notification);
  // TODO(sjlee): make the specific error handling with this notification.
  RTC_LOG(LS_ERROR) << __FUNCTION__ << ": [AVCaptureSession startRunning] error.";
}

- (BOOL)stopCapture {
#ifdef WEBRTC_IOS
  [[UIDevice currentDevice] endGeneratingDeviceOrientationNotifications];
#endif
  _orientationHasChanged = NO;
  [self waitForCaptureChangeToFinish];
  [self directOutputToNil];

  if (!_captureSession) {
    return NO;
  }

  _captureChanging = YES;
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(void) {
    [self stopCaptureInBackground];
  });
  return YES;
}

- (void)stopCaptureInBackground {
  [_captureSession stopRunning];
  [self signalCaptureChangeEnd];
}

- (BOOL)changeCaptureInputByUniqueId:(NSString*)uniqueId {
  [self waitForCaptureChangeToFinish];
  NSArray* currentInputs = [_captureSession inputs];
  // remove current input
  if ([currentInputs count] > 0) {
    AVCaptureInput* currentInput = (AVCaptureInput*)[currentInputs objectAtIndex:0];

    [_captureSession removeInput:currentInput];
  }

  // Look for input device with the name requested (as our input param)
  // get list of available capture devices
  int captureDeviceCount = [DeviceInfoIosObjC captureDeviceCount];
  if (captureDeviceCount <= 0) {
    return NO;
  }

  AVCaptureDevice* captureDevice = [DeviceInfoIosObjC captureDeviceForUniqueId:uniqueId];

  if (!captureDevice) {
    return NO;
  }

  // now create capture session input out of AVCaptureDevice
  NSError* deviceError = nil;
  AVCaptureDeviceInput* newCaptureInput =
      [AVCaptureDeviceInput deviceInputWithDevice:captureDevice error:&deviceError];

  if (!newCaptureInput) {
    const char* errorMessage = [[deviceError localizedDescription] UTF8String];

    RTC_LOG(LS_ERROR) << __FUNCTION__ << ": deviceInputWithDevice error:" << errorMessage;

    return NO;
  }

  // try to add our new capture device to the capture session
  [_captureSession beginConfiguration];

  BOOL addedCaptureInput = NO;
  if ([_captureSession canAddInput:newCaptureInput]) {
    [_captureSession addInput:newCaptureInput];
    addedCaptureInput = YES;
  } else {
    addedCaptureInput = NO;
  }

  [_captureSession commitConfiguration];

  return addedCaptureInput;
}

- (void)captureOutput:(AVCaptureOutput*)captureOutput
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection*)connection {
  const int kFlags = 0;
  CVImageBufferRef videoFrame = CMSampleBufferGetImageBuffer(sampleBuffer);

  if (CVPixelBufferLockBaseAddress(videoFrame, kFlags) != kCVReturnSuccess) {
    return;
  }

  uint8_t* baseAddress = (uint8_t*)CVPixelBufferGetBaseAddress(videoFrame);
  const size_t width = CVPixelBufferGetWidth(videoFrame);
  const size_t height = CVPixelBufferGetHeight(videoFrame);
  const size_t frameSize = width * height * 2;

  VideoCaptureCapability tempCaptureCapability;
  tempCaptureCapability.width = width;
  tempCaptureCapability.height = height;
  tempCaptureCapability.maxFPS = _capability.maxFPS;
  tempCaptureCapability.videoType = VideoType::kUYVY;

  _owner->IncomingFrame(baseAddress, frameSize, tempCaptureCapability, 0);

  CVPixelBufferUnlockBaseAddress(videoFrame, kFlags);
}

- (void)signalCaptureChangeEnd {
  [_captureChangingCondition lock];
  _captureChanging = NO;
  [_captureChangingCondition signal];
  [_captureChangingCondition unlock];
}

- (void)waitForCaptureChangeToFinish {
  [_captureChangingCondition lock];
  while (_captureChanging) {
    [_captureChangingCondition wait];
  }
  [_captureChangingCondition unlock];
}
@end
