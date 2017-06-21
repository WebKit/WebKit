/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <OCMock/OCMock.h>

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#endif

#include "webrtc/base/gunit.h"

#import <WebRTC/RTCCameraVideoCapturer.h>
#import <WebRTC/RTCDispatcher.h>
#import <WebRTC/RTCVideoFrame.h>

#if TARGET_OS_IPHONE
// Helper method.
CMSampleBufferRef createTestSampleBufferRef() {

  // This image is already in the testing bundle.
  UIImage *image = [UIImage imageNamed:@"Default.png"];
  CGSize size = image.size;
  CGImageRef imageRef = [image CGImage];

  CVPixelBufferRef pixelBuffer = nullptr;
  CVPixelBufferCreate(kCFAllocatorDefault, size.width, size.height, kCVPixelFormatType_32ARGB, nil,
                      &pixelBuffer);

  CGColorSpaceRef rgbColorSpace = CGColorSpaceCreateDeviceRGB();
  // We don't care about bitsPerComponent and bytesPerRow so arbitrary value of 8 for both.
  CGContextRef context = CGBitmapContextCreate(nil, size.width, size.height, 8, 8 * size.width,
                                               rgbColorSpace, kCGImageAlphaPremultipliedFirst);

  CGContextDrawImage(
      context, CGRectMake(0, 0, CGImageGetWidth(imageRef), CGImageGetHeight(imageRef)), imageRef);

  CGColorSpaceRelease(rgbColorSpace);
  CGContextRelease(context);

  // We don't really care about the timing.
  CMSampleTimingInfo timing = {kCMTimeInvalid, kCMTimeInvalid, kCMTimeInvalid};
  CMVideoFormatDescriptionRef description = nullptr;
  CMVideoFormatDescriptionCreateForImageBuffer(NULL, pixelBuffer, &description);

  CMSampleBufferRef sampleBuffer = nullptr;
  CMSampleBufferCreateForImageBuffer(kCFAllocatorDefault, pixelBuffer, YES, NULL, NULL, description,
                                     &timing, &sampleBuffer);
  CFRelease(pixelBuffer);

  return sampleBuffer;

}
#endif
@interface RTCCameraVideoCapturer (Tests)<AVCaptureVideoDataOutputSampleBufferDelegate>
@end

@interface RTCCameraVideoCapturerTests : NSObject
@property(nonatomic, strong) id delegateMock;
@property(nonatomic, strong) id deviceMock;
@property(nonatomic, strong) RTCCameraVideoCapturer *capturer;
@end

@implementation RTCCameraVideoCapturerTests
@synthesize delegateMock = _delegateMock;
@synthesize capturer = _capturer;
@synthesize deviceMock = _deviceMock;

- (void)setup {
  self.delegateMock = OCMProtocolMock(@protocol(RTCVideoCapturerDelegate));
  self.capturer = [[RTCCameraVideoCapturer alloc] initWithDelegate:self.delegateMock];
  self.deviceMock = [self createDeviceMock];
}

- (void)tearDown {
  [self.delegateMock stopMocking];
  [self.deviceMock stopMocking];
  self.delegateMock = nil;
  self.deviceMock = nil;
  self.capturer = nil;
}

#pragma mark - utils

- (id)createDeviceMock {
  return OCMClassMock([AVCaptureDevice class]);
}

#pragma mark - test cases

- (void)testSetupSession {
  AVCaptureSession *session = self.capturer.captureSession;
  EXPECT_TRUE(session != nil);

#if TARGET_OS_IPHONE
  EXPECT_EQ(session.sessionPreset, AVCaptureSessionPresetInputPriority);
  EXPECT_EQ(session.usesApplicationAudioSession, NO);
#endif
  EXPECT_EQ(session.outputs.count, 1u);
}

- (void)testSetupSessionOutput {
  AVCaptureVideoDataOutput *videoOutput = self.capturer.captureSession.outputs[0];
  EXPECT_EQ(videoOutput.alwaysDiscardsLateVideoFrames, NO);
  EXPECT_EQ(videoOutput.sampleBufferDelegate, self.capturer);
}

- (void)testSupportedFormatsForDevice {
  // given
  id validFormat1 = OCMClassMock([AVCaptureDeviceFormat class]);
  CMVideoFormatDescriptionRef format;

  // We don't care about width and heigth so arbitrary 123 and 456 values.
  int width = 123;
  int height = 456;
  CMVideoFormatDescriptionCreate(nil, kCVPixelFormatType_420YpCbCr8PlanarFullRange, width, height,
                                 nil, &format);
  OCMStub([validFormat1 formatDescription]).andReturn(format);

  id validFormat2 = OCMClassMock([AVCaptureDeviceFormat class]);
  CMVideoFormatDescriptionCreate(nil, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, width,
                                 height, nil, &format);
  OCMStub([validFormat2 formatDescription]).andReturn(format);

  id invalidFormat = OCMClassMock([AVCaptureDeviceFormat class]);
  CMVideoFormatDescriptionCreate(nil, kCVPixelFormatType_422YpCbCr8_yuvs, width, height, nil,
                                 &format);
  OCMStub([invalidFormat formatDescription]).andReturn(format);

  NSArray *formats = @[ validFormat1, validFormat2, invalidFormat ];
  OCMStub([self.deviceMock formats]).andReturn(formats);

  // when
  NSArray *supportedFormats = [RTCCameraVideoCapturer supportedFormatsForDevice:self.deviceMock];

  // then
  EXPECT_EQ(supportedFormats.count, 2u);
  EXPECT_TRUE([supportedFormats containsObject:validFormat1]);
  EXPECT_TRUE([supportedFormats containsObject:validFormat2]);
  // cleanup
  [validFormat1 stopMocking];
  [validFormat2 stopMocking];
  [invalidFormat stopMocking];
  validFormat1 = nil;
  validFormat2 = nil;
  invalidFormat = nil;
}

- (void)testCaptureDevices {
  OCMStub([self.deviceMock devicesWithMediaType:AVMediaTypeVideo]).andReturn(@[ [NSObject new] ]);
  OCMStub([self.deviceMock devicesWithMediaType:AVMediaTypeAudio]).andReturn(@[ [NSObject new] ]);

  NSArray *captureDevices = [RTCCameraVideoCapturer captureDevices];

  EXPECT_EQ(captureDevices.count, 1u);
}

- (void)testDelegateCallbackNotCalledWhenInvalidBuffer {
  // given
  CMSampleBufferRef sampleBuffer = nullptr;
  [[self.delegateMock reject] capturer:[OCMArg any] didCaptureVideoFrame:[OCMArg any]];

  // when
  [self.capturer captureOutput:self.capturer.captureSession.outputs[0]
         didOutputSampleBuffer:sampleBuffer
                fromConnection:nil];

  // then
  [self.delegateMock verify];
}


- (void)testDelegateCallbackWithValidBufferAndOrientationUpdate {
#if TARGET_OS_IPHONE
  // given
  UIDevice *currentDeviceMock = OCMClassMock([UIDevice class]);
  // UpsideDown -> RTCVideoRotation_270.
  OCMStub(currentDeviceMock.orientation).andReturn(UIDeviceOrientationPortraitUpsideDown);
  id classMock = OCMClassMock([UIDevice class]);
  OCMStub([classMock currentDevice]).andReturn(currentDeviceMock);

  CMSampleBufferRef sampleBuffer = createTestSampleBufferRef();

  // then
  [[self.delegateMock expect] capturer:self.capturer
                  didCaptureVideoFrame:[OCMArg checkWithBlock:^BOOL(RTCVideoFrame *expectedFrame) {
                    EXPECT_EQ(expectedFrame.rotation, RTCVideoRotation_270);
                    return YES;
                  }]];

  // when
  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  [center postNotificationName:UIDeviceOrientationDidChangeNotification object:nil];

  // We need to wait for the dispatch to finish.
  WAIT(0, 1000);

  [self.capturer captureOutput:self.capturer.captureSession.outputs[0]
         didOutputSampleBuffer:sampleBuffer
                fromConnection:nil];

  [self.delegateMock verify];

  [(id)currentDeviceMock stopMocking];
  currentDeviceMock = nil;
  [classMock stopMocking];
  classMock = nil;
  CFRelease(sampleBuffer);
#endif
}

@end

TEST(RTCCameraVideoCapturerTests, SetupSession) {
  RTCCameraVideoCapturerTests *test = [[RTCCameraVideoCapturerTests alloc] init];
  [test setup];
  [test testSetupSession];
  [test tearDown];
}

TEST(RTCCameraVideoCapturerTests, SetupSessionOutput) {
  RTCCameraVideoCapturerTests *test = [[RTCCameraVideoCapturerTests alloc] init];
  [test setup];
  [test testSetupSessionOutput];
  [test tearDown];
}

TEST(RTCCameraVideoCapturerTests, SupportedFormatsForDevice) {
  RTCCameraVideoCapturerTests *test = [[RTCCameraVideoCapturerTests alloc] init];
  [test setup];
  [test testSupportedFormatsForDevice];
  [test tearDown];
}

TEST(RTCCameraVideoCapturerTests, CaptureDevices) {
  RTCCameraVideoCapturerTests *test = [[RTCCameraVideoCapturerTests alloc] init];
  [test setup];
  [test testCaptureDevices];
  [test tearDown];
}

TEST(RTCCameraVideoCapturerTests, DelegateCallbackNotCalledWhenInvalidBuffer) {
  RTCCameraVideoCapturerTests *test = [[RTCCameraVideoCapturerTests alloc] init];
  [test setup];
  [test testDelegateCallbackNotCalledWhenInvalidBuffer];
  [test tearDown];
}

TEST(RTCCameraVideoCapturerTests, DelegateCallbackWithValidBufferAndOrientationUpdate) {
  RTCCameraVideoCapturerTests *test = [[RTCCameraVideoCapturerTests alloc] init];
  [test setup];
  [test testDelegateCallbackWithValidBufferAndOrientationUpdate];
  [test tearDown];
}
