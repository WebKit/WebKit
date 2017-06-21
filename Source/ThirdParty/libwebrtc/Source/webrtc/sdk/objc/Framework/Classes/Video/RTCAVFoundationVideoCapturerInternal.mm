/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCAVFoundationVideoCapturerInternal.h"

#import <Foundation/Foundation.h>
#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#import "WebRTC/UIDevice+RTCDevice.h"
#endif

#import "RTCDispatcher+Private.h"
#import "WebRTC/RTCLogging.h"

#include "avfoundationformatmapper.h"

@implementation RTCAVFoundationVideoCapturerInternal {
  // Keep pointers to inputs for convenience.
  AVCaptureDeviceInput *_frontCameraInput;
  AVCaptureDeviceInput *_backCameraInput;
  AVCaptureVideoDataOutput *_videoDataOutput;
  // The cricket::VideoCapturer that owns this class. Should never be NULL.
  webrtc::AVFoundationVideoCapturer *_capturer;
  webrtc::VideoRotation _rotation;
  BOOL _hasRetriedOnFatalError;
  BOOL _isRunning;
  BOOL _hasStarted;
  rtc::CriticalSection _crit;
}

@synthesize captureSession = _captureSession;
@synthesize frameQueue = _frameQueue;
@synthesize useBackCamera = _useBackCamera;

@synthesize isRunning = _isRunning;
@synthesize hasStarted = _hasStarted;

// This is called from the thread that creates the video source, which is likely
// the main thread.
- (instancetype)initWithCapturer:(webrtc::AVFoundationVideoCapturer *)capturer {
  RTC_DCHECK(capturer);
  if (self = [super init]) {
    _capturer = capturer;
    // Create the capture session and all relevant inputs and outputs. We need
    // to do this in init because the application may want the capture session
    // before we start the capturer for e.g. AVCapturePreviewLayer. All objects
    // created here are retained until dealloc and never recreated.
    if (![self setupCaptureSession]) {
      return nil;
    }
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
#if TARGET_OS_IPHONE
    [center addObserver:self
               selector:@selector(deviceOrientationDidChange:)
                   name:UIDeviceOrientationDidChangeNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(handleCaptureSessionInterruption:)
                   name:AVCaptureSessionWasInterruptedNotification
                 object:_captureSession];
    [center addObserver:self
               selector:@selector(handleCaptureSessionInterruptionEnded:)
                   name:AVCaptureSessionInterruptionEndedNotification
                 object:_captureSession];
    [center addObserver:self
               selector:@selector(handleApplicationDidBecomeActive:)
                   name:UIApplicationDidBecomeActiveNotification
                 object:[UIApplication sharedApplication]];
#endif
    [center addObserver:self
               selector:@selector(handleCaptureSessionRuntimeError:)
                   name:AVCaptureSessionRuntimeErrorNotification
                 object:_captureSession];
    [center addObserver:self
               selector:@selector(handleCaptureSessionDidStartRunning:)
                   name:AVCaptureSessionDidStartRunningNotification
                 object:_captureSession];
    [center addObserver:self
               selector:@selector(handleCaptureSessionDidStopRunning:)
                   name:AVCaptureSessionDidStopRunningNotification
                 object:_captureSession];
  }
  return self;
}

- (void)dealloc {
  RTC_DCHECK(!self.hasStarted);
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  _capturer = nullptr;
}

- (AVCaptureSession *)captureSession {
  return _captureSession;
}

- (AVCaptureDevice *)getActiveCaptureDevice {
  return self.useBackCamera ? _backCameraInput.device : _frontCameraInput.device;
}

- (nullable AVCaptureDevice *)frontCaptureDevice {
  return _frontCameraInput.device;
}

- (nullable AVCaptureDevice *)backCaptureDevice {
  return _backCameraInput.device;
}

- (dispatch_queue_t)frameQueue {
  if (!_frameQueue) {
    _frameQueue =
        dispatch_queue_create("org.webrtc.avfoundationvideocapturer.video", DISPATCH_QUEUE_SERIAL);
    dispatch_set_target_queue(_frameQueue,
                              dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
  }
  return _frameQueue;
}

// Called from any thread (likely main thread).
- (BOOL)canUseBackCamera {
  return _backCameraInput != nil;
}

// Called from any thread (likely main thread).
- (BOOL)useBackCamera {
  @synchronized(self) {
    return _useBackCamera;
  }
}

// Called from any thread (likely main thread).
- (void)setUseBackCamera:(BOOL)useBackCamera {
  if (!self.canUseBackCamera) {
    if (useBackCamera) {
      RTCLogWarning(@"No rear-facing camera exists or it cannot be used;"
                     "not switching.");
    }
    return;
  }
  @synchronized(self) {
    if (_useBackCamera == useBackCamera) {
      return;
    }
    _useBackCamera = useBackCamera;
    [self updateSessionInputForUseBackCamera:useBackCamera];
  }
}

// Called from WebRTC thread.
- (void)start {
  if (self.hasStarted) {
    return;
  }
  self.hasStarted = YES;
  [RTCDispatcher
      dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                    block:^{
#if TARGET_OS_IPHONE
                        // Default to portrait orientation on iPhone. This will be reset in
                        // updateOrientation unless orientation is unknown/faceup/facedown.
                        _rotation = webrtc::kVideoRotation_90;
#else
                        // No rotation on Mac.
                        _rotation = webrtc::kVideoRotation_0;
#endif
                        [self updateOrientation];
#if TARGET_OS_IPHONE
                        [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
#endif
                        AVCaptureSession *captureSession = self.captureSession;
                        [captureSession startRunning];
                    }];
}

// Called from same thread as start.
- (void)stop {
  if (!self.hasStarted) {
    return;
  }
  self.hasStarted = NO;
  // Due to this async block, it's possible that the ObjC object outlives the
  // C++ one. In order to not invoke functions on the C++ object, we set
  // hasStarted immediately instead of dispatching it async.
  [RTCDispatcher
      dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                    block:^{
                        [_videoDataOutput setSampleBufferDelegate:nil queue:nullptr];
                        [_captureSession stopRunning];
#if TARGET_OS_IPHONE
                        [[UIDevice currentDevice] endGeneratingDeviceOrientationNotifications];
#endif
                    }];
}

#pragma mark iOS notifications

#if TARGET_OS_IPHONE
- (void)deviceOrientationDidChange:(NSNotification *)notification {
  [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                               block:^{
                                   [self updateOrientation];
                               }];
}
#endif

#pragma mark AVCaptureVideoDataOutputSampleBufferDelegate

- (void)captureOutput:(AVCaptureOutput *)captureOutput
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection *)connection {
  NSParameterAssert(captureOutput == _videoDataOutput);
  if (!self.hasStarted) {
    return;
  }
  _capturer->CaptureSampleBuffer(sampleBuffer, _rotation);
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput
    didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer
         fromConnection:(AVCaptureConnection *)connection {
  RTCLogError(@"Dropped sample buffer.");
}

#pragma mark - AVCaptureSession notifications

- (void)handleCaptureSessionInterruption:(NSNotification *)notification {
  NSString *reasonString = nil;
#if defined(__IPHONE_9_0) && defined(__IPHONE_OS_VERSION_MAX_ALLOWED) && \
    __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_9_0
  if ([UIDevice isIOS9OrLater]) {
    NSNumber *reason = notification.userInfo[AVCaptureSessionInterruptionReasonKey];
    if (reason) {
      switch (reason.intValue) {
        case AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableInBackground:
          reasonString = @"VideoDeviceNotAvailableInBackground";
          break;
        case AVCaptureSessionInterruptionReasonAudioDeviceInUseByAnotherClient:
          reasonString = @"AudioDeviceInUseByAnotherClient";
          break;
        case AVCaptureSessionInterruptionReasonVideoDeviceInUseByAnotherClient:
          reasonString = @"VideoDeviceInUseByAnotherClient";
          break;
        case AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableWithMultipleForegroundApps:
          reasonString = @"VideoDeviceNotAvailableWithMultipleForegroundApps";
          break;
      }
    }
  }
#endif
  RTCLog(@"Capture session interrupted: %@", reasonString);
  // TODO(tkchin): Handle this case.
}

- (void)handleCaptureSessionInterruptionEnded:(NSNotification *)notification {
  RTCLog(@"Capture session interruption ended.");
  // TODO(tkchin): Handle this case.
}

- (void)handleCaptureSessionRuntimeError:(NSNotification *)notification {
  NSError *error = [notification.userInfo objectForKey:AVCaptureSessionErrorKey];
  RTCLogError(@"Capture session runtime error: %@", error);

  [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                               block:^{
#if TARGET_OS_IPHONE
                                   if (error.code == AVErrorMediaServicesWereReset) {
                                     [self handleNonFatalError];
                                   } else {
                                     [self handleFatalError];
                                   }
#else
                                   [self handleFatalError];
#endif
                               }];
}

- (void)handleCaptureSessionDidStartRunning:(NSNotification *)notification {
  RTCLog(@"Capture session started.");

  self.isRunning = YES;
  [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                               block:^{
                                   // If we successfully restarted after an unknown error,
                                   // allow future retries on fatal errors.
                                   _hasRetriedOnFatalError = NO;
                               }];
}

- (void)handleCaptureSessionDidStopRunning:(NSNotification *)notification {
  RTCLog(@"Capture session stopped.");
  self.isRunning = NO;
}

- (void)handleFatalError {
  [RTCDispatcher
      dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                    block:^{
                        if (!_hasRetriedOnFatalError) {
                          RTCLogWarning(@"Attempting to recover from fatal capture error.");
                          [self handleNonFatalError];
                          _hasRetriedOnFatalError = YES;
                        } else {
                          RTCLogError(@"Previous fatal error recovery failed.");
                        }
                    }];
}

- (void)handleNonFatalError {
  [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                               block:^{
                                   if (self.hasStarted) {
                                     RTCLog(@"Restarting capture session after error.");
                                     [self.captureSession startRunning];
                                   }
                               }];
}

#if TARGET_OS_IPHONE

#pragma mark - UIApplication notifications

- (void)handleApplicationDidBecomeActive:(NSNotification *)notification {
  [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                               block:^{
                                   if (self.hasStarted && !self.captureSession.isRunning) {
                                     RTCLog(@"Restarting capture session on active.");
                                     [self.captureSession startRunning];
                                   }
                               }];
}

#endif  // TARGET_OS_IPHONE

#pragma mark - Private

- (BOOL)setupCaptureSession {
  AVCaptureSession *captureSession = [[AVCaptureSession alloc] init];
#if defined(WEBRTC_IOS)
  captureSession.usesApplicationAudioSession = NO;
#endif
  // Add the output.
  AVCaptureVideoDataOutput *videoDataOutput = [self videoDataOutput];
  if (![captureSession canAddOutput:videoDataOutput]) {
    RTCLogError(@"Video data output unsupported.");
    return NO;
  }
  [captureSession addOutput:videoDataOutput];

  // Get the front and back cameras. If there isn't a front camera
  // give up.
  AVCaptureDeviceInput *frontCameraInput = [self frontCameraInput];
  AVCaptureDeviceInput *backCameraInput = [self backCameraInput];
  if (!frontCameraInput) {
    RTCLogError(@"No front camera for capture session.");
    return NO;
  }

  // Add the inputs.
  if (![captureSession canAddInput:frontCameraInput] ||
      (backCameraInput && ![captureSession canAddInput:backCameraInput])) {
    RTCLogError(@"Session does not support capture inputs.");
    return NO;
  }
  AVCaptureDeviceInput *input = self.useBackCamera ? backCameraInput : frontCameraInput;
  [captureSession addInput:input];

  _captureSession = captureSession;
  return YES;
}

- (AVCaptureVideoDataOutput *)videoDataOutput {
  if (!_videoDataOutput) {
    // Make the capturer output NV12. Ideally we want I420 but that's not
    // currently supported on iPhone / iPad.
    AVCaptureVideoDataOutput *videoDataOutput = [[AVCaptureVideoDataOutput alloc] init];
    videoDataOutput.videoSettings = @{
      (NSString *)
      // TODO(denicija): Remove this color conversion and use the original capture format directly.
      kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)
    };
    videoDataOutput.alwaysDiscardsLateVideoFrames = NO;
    [videoDataOutput setSampleBufferDelegate:self queue:self.frameQueue];
    _videoDataOutput = videoDataOutput;
  }
  return _videoDataOutput;
}

- (AVCaptureDevice *)videoCaptureDeviceForPosition:(AVCaptureDevicePosition)position {
  for (AVCaptureDevice *captureDevice in [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo]) {
    if (captureDevice.position == position) {
      return captureDevice;
    }
  }
  return nil;
}

- (AVCaptureDeviceInput *)frontCameraInput {
  if (!_frontCameraInput) {
#if TARGET_OS_IPHONE
    AVCaptureDevice *frontCameraDevice =
        [self videoCaptureDeviceForPosition:AVCaptureDevicePositionFront];
#else
    AVCaptureDevice *frontCameraDevice =
        [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
#endif
    if (!frontCameraDevice) {
      RTCLogWarning(@"Failed to find front capture device.");
      return nil;
    }
    NSError *error = nil;
    AVCaptureDeviceInput *frontCameraInput =
        [AVCaptureDeviceInput deviceInputWithDevice:frontCameraDevice error:&error];
    if (!frontCameraInput) {
      RTCLogError(@"Failed to create front camera input: %@", error.localizedDescription);
      return nil;
    }
    _frontCameraInput = frontCameraInput;
  }
  return _frontCameraInput;
}

- (AVCaptureDeviceInput *)backCameraInput {
  if (!_backCameraInput) {
    AVCaptureDevice *backCameraDevice =
        [self videoCaptureDeviceForPosition:AVCaptureDevicePositionBack];
    if (!backCameraDevice) {
      RTCLogWarning(@"Failed to find front capture device.");
      return nil;
    }
    NSError *error = nil;
    AVCaptureDeviceInput *backCameraInput =
        [AVCaptureDeviceInput deviceInputWithDevice:backCameraDevice error:&error];
    if (!backCameraInput) {
      RTCLogError(@"Failed to create front camera input: %@", error.localizedDescription);
      return nil;
    }
    _backCameraInput = backCameraInput;
  }
  return _backCameraInput;
}

// Called from capture session queue.
- (void)updateOrientation {
#if TARGET_OS_IPHONE
  switch ([UIDevice currentDevice].orientation) {
    case UIDeviceOrientationPortrait:
      _rotation = webrtc::kVideoRotation_90;
      break;
    case UIDeviceOrientationPortraitUpsideDown:
      _rotation = webrtc::kVideoRotation_270;
      break;
    case UIDeviceOrientationLandscapeLeft:
      _rotation =
          _capturer->GetUseBackCamera() ? webrtc::kVideoRotation_0 : webrtc::kVideoRotation_180;
      break;
    case UIDeviceOrientationLandscapeRight:
      _rotation =
          _capturer->GetUseBackCamera() ? webrtc::kVideoRotation_180 : webrtc::kVideoRotation_0;
      break;
    case UIDeviceOrientationFaceUp:
    case UIDeviceOrientationFaceDown:
    case UIDeviceOrientationUnknown:
      // Ignore.
      break;
  }
#endif
}

// Update the current session input to match what's stored in _useBackCamera.
- (void)updateSessionInputForUseBackCamera:(BOOL)useBackCamera {
  [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                               block:^{
                                   [_captureSession beginConfiguration];
                                   AVCaptureDeviceInput *oldInput = _backCameraInput;
                                   AVCaptureDeviceInput *newInput = _frontCameraInput;
                                   if (useBackCamera) {
                                     oldInput = _frontCameraInput;
                                     newInput = _backCameraInput;
                                   }
                                   if (oldInput) {
                                     // Ok to remove this even if it's not attached. Will be no-op.
                                     [_captureSession removeInput:oldInput];
                                   }
                                   if (newInput) {
                                     [_captureSession addInput:newInput];
                                   }
                                   [self updateOrientation];
                                   AVCaptureDevice *newDevice = newInput.device;
                                   const cricket::VideoFormat *format =
                                     _capturer->GetCaptureFormat();
                                   webrtc::SetFormatForCaptureDevice(
                                       newDevice, _captureSession, *format);
                                   [_captureSession commitConfiguration];
                               }];
}

@end
