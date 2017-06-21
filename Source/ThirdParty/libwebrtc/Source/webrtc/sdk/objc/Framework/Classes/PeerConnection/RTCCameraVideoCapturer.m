/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import "WebRTC/RTCCameraVideoCapturer.h"
#import "WebRTC/RTCLogging.h"

#if TARGET_OS_IPHONE
#import "WebRTC/UIDevice+RTCDevice.h"
#endif

#import "RTCDispatcher+Private.h"

const int64_t kNanosecondsPerSecond = 1000000000;

static inline BOOL IsMediaSubTypeSupported(FourCharCode mediaSubType) {
  return (mediaSubType == kCVPixelFormatType_420YpCbCr8PlanarFullRange ||
          mediaSubType == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
}

@interface RTCCameraVideoCapturer ()<AVCaptureVideoDataOutputSampleBufferDelegate>
@property(nonatomic, readonly) dispatch_queue_t frameQueue;
@end

@implementation RTCCameraVideoCapturer {
  AVCaptureVideoDataOutput *_videoDataOutput;
  AVCaptureSession *_captureSession;
  AVCaptureDevice *_currentDevice;
  RTCVideoRotation _rotation;
  BOOL _hasRetriedOnFatalError;
  BOOL _isRunning;
  // Will the session be running once all asynchronous operations have been completed?
  BOOL _willBeRunning;
}

@synthesize frameQueue = _frameQueue;
@synthesize captureSession = _captureSession;

- (instancetype)initWithDelegate:(__weak id<RTCVideoCapturerDelegate>)delegate {
  if (self = [super initWithDelegate:delegate]) {
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
  NSAssert(
      !_willBeRunning,
      @"Session was still running in RTCCameraVideoCapturer dealloc. Forgot to call stopCapture?");
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

+ (NSArray<AVCaptureDevice *> *)captureDevices {
  return [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
}

+ (NSArray<AVCaptureDeviceFormat *> *)supportedFormatsForDevice:(AVCaptureDevice *)device {
  NSMutableArray<AVCaptureDeviceFormat *> *eligibleDeviceFormats = [NSMutableArray array];

  for (AVCaptureDeviceFormat *format in device.formats) {
    // Filter out subTypes that we currently don't support in the stack
    FourCharCode mediaSubType = CMFormatDescriptionGetMediaSubType(format.formatDescription);
    if (IsMediaSubTypeSupported(mediaSubType)) {
      [eligibleDeviceFormats addObject:format];
    }
  }

  return eligibleDeviceFormats;
}

- (void)startCaptureWithDevice:(AVCaptureDevice *)device
                        format:(AVCaptureDeviceFormat *)format
                           fps:(NSInteger)fps {
  _willBeRunning = true;
  [RTCDispatcher
      dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                    block:^{
                      RTCLogInfo("startCaptureWithDevice %@ @ %zd fps", format, fps);

#if TARGET_OS_IPHONE
                      [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
#endif

                      _currentDevice = device;

                      NSError *error = nil;
                      if ([_currentDevice lockForConfiguration:&error]) {
                        [self updateDeviceCaptureFormat:format fps:fps];
                      } else {
                        RTCLogError(@"Failed to lock device %@. Error: %@", _currentDevice,
                                    error.userInfo);
                        return;
                      }

                      [self reconfigureCaptureSessionInput];
                      [self updateOrientation];
                      [_captureSession startRunning];

                      [_currentDevice unlockForConfiguration];
                      _isRunning = true;
                    }];
}

- (void)stopCapture {
  _willBeRunning = false;
  [RTCDispatcher
      dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                    block:^{
                      RTCLogInfo("Stop");
                      _currentDevice = nil;
                      for (AVCaptureDeviceInput *oldInput in [_captureSession.inputs copy]) {
                        [_captureSession removeInput:oldInput];
                      }
                      [_captureSession stopRunning];

#if TARGET_OS_IPHONE
                      [[UIDevice currentDevice] endGeneratingDeviceOrientationNotifications];
#endif
                      _isRunning = false;
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

  if (CMSampleBufferGetNumSamples(sampleBuffer) != 1 || !CMSampleBufferIsValid(sampleBuffer) ||
      !CMSampleBufferDataIsReady(sampleBuffer)) {
    return;
  }

  CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
  if (pixelBuffer == nil) {
    return;
  }

  int64_t timeStampNs = CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sampleBuffer)) *
                        kNanosecondsPerSecond;
  RTCVideoFrame *videoFrame = [[RTCVideoFrame alloc] initWithPixelBuffer:pixelBuffer
                                                                rotation:_rotation
                                                             timeStampNs:timeStampNs];
  [self.delegate capturer:self didCaptureVideoFrame:videoFrame];
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
}

- (void)handleCaptureSessionInterruptionEnded:(NSNotification *)notification {
  RTCLog(@"Capture session interruption ended.");
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

  [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                               block:^{
                                 // If we successfully restarted after an unknown error,
                                 // allow future retries on fatal errors.
                                 _hasRetriedOnFatalError = NO;
                               }];
}

- (void)handleCaptureSessionDidStopRunning:(NSNotification *)notification {
  RTCLog(@"Capture session stopped.");
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
                                 RTCLog(@"Restarting capture session after error.");
                                 if (_isRunning) {
                                   [_captureSession startRunning];
                                 }
                               }];
}

#if TARGET_OS_IPHONE

#pragma mark - UIApplication notifications

- (void)handleApplicationDidBecomeActive:(NSNotification *)notification {
  [RTCDispatcher dispatchAsyncOnType:RTCDispatcherTypeCaptureSession
                               block:^{
                                 if (_isRunning && !_captureSession.isRunning) {
                                   RTCLog(@"Restarting capture session on active.");
                                   [_captureSession startRunning];
                                 }
                               }];
}

#endif  // TARGET_OS_IPHONE

#pragma mark - Private

- (dispatch_queue_t)frameQueue {
  if (!_frameQueue) {
    _frameQueue =
        dispatch_queue_create("org.webrtc.avfoundationvideocapturer.video", DISPATCH_QUEUE_SERIAL);
    dispatch_set_target_queue(_frameQueue,
                              dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
  }
  return _frameQueue;
}

- (BOOL)setupCaptureSession {
  NSAssert(_captureSession == nil, @"Setup capture session called twice.");
  _captureSession = [[AVCaptureSession alloc] init];
#if defined(WEBRTC_IOS)
  _captureSession.sessionPreset = AVCaptureSessionPresetInputPriority;
  _captureSession.usesApplicationAudioSession = NO;
#endif
  [self setupVideoDataOutput];
  // Add the output.
  if (![_captureSession canAddOutput:_videoDataOutput]) {
    RTCLogError(@"Video data output unsupported.");
    return NO;
  }
  [_captureSession addOutput:_videoDataOutput];

  return YES;
}

- (void)setupVideoDataOutput {
  NSAssert(_videoDataOutput == nil, @"Setup video data output called twice.");
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

#pragma mark - Private, called inside capture queue

- (void)updateDeviceCaptureFormat:(AVCaptureDeviceFormat *)format fps:(NSInteger)fps {
  NSAssert([RTCDispatcher isOnQueueForType:RTCDispatcherTypeCaptureSession],
           @"updateDeviceCaptureFormat must be called on the capture queue.");
  @try {
    _currentDevice.activeFormat = format;
    _currentDevice.activeVideoMinFrameDuration = CMTimeMake(1, fps);
  } @catch (NSException *exception) {
    RTCLogError(@"Failed to set active format!\n User info:%@", exception.userInfo);
    return;
  }
}

- (void)reconfigureCaptureSessionInput {
  NSAssert([RTCDispatcher isOnQueueForType:RTCDispatcherTypeCaptureSession],
           @"reconfigureCaptureSessionInput must be called on the capture queue.");
  NSError *error = nil;
  AVCaptureDeviceInput *input =
      [AVCaptureDeviceInput deviceInputWithDevice:_currentDevice error:&error];
  if (!input) {
    RTCLogError(@"Failed to create front camera input: %@", error.localizedDescription);
    return;
  }
  [_captureSession beginConfiguration];
  for (AVCaptureDeviceInput *oldInput in [_captureSession.inputs copy]) {
    [_captureSession removeInput:oldInput];
  }
  if ([_captureSession canAddInput:input]) {
    [_captureSession addInput:input];
  } else {
    RTCLogError(@"Cannot add camera as an input to the session.");
  }
  [_captureSession commitConfiguration];
}

- (void)updateOrientation {
  NSAssert([RTCDispatcher isOnQueueForType:RTCDispatcherTypeCaptureSession],
           @"updateOrientation must be called on the capture queue.");
#if TARGET_OS_IPHONE
  BOOL usingFrontCamera = _currentDevice.position == AVCaptureDevicePositionFront;
  switch ([UIDevice currentDevice].orientation) {
    case UIDeviceOrientationPortrait:
      _rotation = RTCVideoRotation_90;
      break;
    case UIDeviceOrientationPortraitUpsideDown:
      _rotation = RTCVideoRotation_270;
      break;
    case UIDeviceOrientationLandscapeLeft:
      _rotation = usingFrontCamera ? RTCVideoRotation_180 : RTCVideoRotation_0;
      break;
    case UIDeviceOrientationLandscapeRight:
      _rotation = usingFrontCamera ? RTCVideoRotation_0 : RTCVideoRotation_180;
      break;
    case UIDeviceOrientationFaceUp:
    case UIDeviceOrientationFaceDown:
    case UIDeviceOrientationUnknown:
      // Ignore.
      break;
  }
#endif
}

@end
