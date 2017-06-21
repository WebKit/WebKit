/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <OCMock/OCMock.h>

#include "webrtc/base/gunit.h"

#include "Video/avfoundationformatmapper.h"


// Width and height don't play any role so lets use predefined values throughout
// the tests.
static const int kFormatWidth = 789;
static const int kFormatHeight = 987;

// Hardcoded framrate to be used throughout the tests.
static const int kFramerate = 30;

// Same width and height is used so it's ok to expect same cricket::VideoFormat
static cricket::VideoFormat expectedFormat =
    cricket::VideoFormat(kFormatWidth,
                         kFormatHeight,
                         cricket::VideoFormat::FpsToInterval(kFramerate),
                         cricket::FOURCC_NV12);

// Mock class for AVCaptureDeviceFormat.
// Custom implementation needed because OCMock cannot handle the
// CMVideoDescriptionRef mocking.
@interface AVCaptureDeviceFormatMock : NSObject

@property (nonatomic, assign) CMVideoFormatDescriptionRef format;
@property (nonatomic, strong) OCMockObject *rangeMock;

- (instancetype)initWithMediaSubtype:(FourCharCode)subtype
                              minFps:(float)minFps
                              maxFps:(float)maxFps;
+ (instancetype)validFormat;
+ (instancetype)invalidFpsFormat;
+ (instancetype)invalidMediaSubtypeFormat;

@end

@implementation AVCaptureDeviceFormatMock

@synthesize format = _format;
@synthesize rangeMock = _rangeMock;

- (instancetype)initWithMediaSubtype:(FourCharCode)subtype
                              minFps:(float)minFps
                              maxFps:(float)maxFps {
  if (self = [super init]) {
    CMVideoFormatDescriptionCreate(nil, subtype, kFormatWidth, kFormatHeight,
                                   nil, &_format);
    // We can use OCMock for the range.
    _rangeMock = [OCMockObject mockForClass:[AVFrameRateRange class]];
    [[[_rangeMock stub] andReturnValue:@(minFps)] minFrameRate];
    [[[_rangeMock stub] andReturnValue:@(maxFps)] maxFrameRate];
  }

  return self;
}

+ (instancetype)validFormat {
  AVCaptureDeviceFormatMock *instance = [[AVCaptureDeviceFormatMock alloc]
      initWithMediaSubtype:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
                    minFps:0.0
                    maxFps:30.0];
  return instance;
}

+ (instancetype)invalidFpsFormat {
  AVCaptureDeviceFormatMock *instance = [[AVCaptureDeviceFormatMock alloc]
      initWithMediaSubtype:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
                    minFps:0.0
                    maxFps:22.0];
  return instance;
}

+ (instancetype)invalidMediaSubtypeFormat {
  AVCaptureDeviceFormatMock *instance = [[AVCaptureDeviceFormatMock alloc]
      initWithMediaSubtype:kCVPixelFormatType_420YpCbCr8Planar
                    minFps:0.0
                    maxFps:60.0];
  return instance;
}

- (void)dealloc {
  if (_format != nil) {
    CFRelease(_format);
    _format = nil;
  }
}

// Redefinition of AVCaptureDevice methods we want to mock.
- (CMVideoFormatDescriptionRef)formatDescription {
  return self.format;
}

- (NSArray *)videoSupportedFrameRateRanges {
  return @[ self.rangeMock ];
}

@end

TEST(AVFormatMapperTest, SuportedCricketFormatsWithInvalidFramerateFormats) {
  // given
  id mockDevice = OCMClassMock([AVCaptureDevice class]);

  // Valid media subtype, invalid framerate
  AVCaptureDeviceFormatMock* mock =
      [AVCaptureDeviceFormatMock invalidFpsFormat];
  OCMStub([mockDevice formats]).andReturn(@[ mock ]);

  // when
  std::set<cricket::VideoFormat> result =
      webrtc::GetSupportedVideoFormatsForDevice(mockDevice);

  // then
  EXPECT_TRUE(result.empty());
}

TEST(AVFormatMapperTest, SuportedCricketFormatsWithInvalidFormats) {
  // given
  id mockDevice = OCMClassMock([AVCaptureDevice class]);

  // Invalid media subtype, valid framerate
  AVCaptureDeviceFormatMock* mock =
      [AVCaptureDeviceFormatMock invalidMediaSubtypeFormat];
  OCMStub([mockDevice formats]).andReturn(@[ mock ]);

  // when
  std::set<cricket::VideoFormat> result =
      webrtc::GetSupportedVideoFormatsForDevice(mockDevice);

  // then
  EXPECT_TRUE(result.empty());
}

TEST(AVFormatMapperTest, SuportedCricketFormats) {
  // given
  id mockDevice = OCMClassMock([AVCaptureDevice class]);

  // valid media subtype, valid framerate
  AVCaptureDeviceFormatMock* mock = [AVCaptureDeviceFormatMock validFormat];
  OCMStub([mockDevice formats]).andReturn(@[ mock ]);

  // when
  std::set<cricket::VideoFormat> result =
      webrtc::GetSupportedVideoFormatsForDevice(mockDevice);

  // then
  EXPECT_EQ(1u, result.size());
  // make sure the set has the expected format
  EXPECT_EQ(expectedFormat, *result.begin());
}

TEST(AVFormatMapperTest, MediaSubtypePreference) {
  // given
  id mockDevice = OCMClassMock([AVCaptureDevice class]);

  // valid media subtype, valid framerate
  AVCaptureDeviceFormatMock* mockOne = [[AVCaptureDeviceFormatMock alloc]
      initWithMediaSubtype:kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
                    minFps:0.0
                    maxFps:30.0];
  // valid media subtype, valid framerate.
  // This media subtype should be the preffered one.
  AVCaptureDeviceFormatMock* mockTwo = [[AVCaptureDeviceFormatMock alloc]
      initWithMediaSubtype:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
                    minFps:0.0
                    maxFps:30.0];
  OCMStub([mockDevice lockForConfiguration:[OCMArg setTo:nil]]).andReturn(YES);
  OCMStub([mockDevice unlockForConfiguration]);
  NSArray* array = @[ mockOne, mockTwo ];
  OCMStub([mockDevice formats]).andReturn(array);

  // to verify
  OCMExpect([mockDevice setActiveFormat:(AVCaptureDeviceFormat*)mockTwo]);
  OCMExpect(
      [mockDevice setActiveVideoMinFrameDuration:CMTimeMake(1, kFramerate)]);

  // when
  bool resultFormat =
      webrtc::SetFormatForCaptureDevice(mockDevice, nil, expectedFormat);

  // then
  EXPECT_TRUE(resultFormat);
  [mockDevice verify];
}

TEST(AVFormatMapperTest, SetFormatWhenDeviceCannotLock) {
  // given
  id mockDevice = OCMClassMock([AVCaptureDevice class]);
  [[[mockDevice stub] andReturnValue:@(NO)]
      lockForConfiguration:[OCMArg setTo:nil]];
  [[[mockDevice stub] andReturn:@[]] formats];

  // when
  bool resultFormat = webrtc::SetFormatForCaptureDevice(mockDevice, nil,
                                                        cricket::VideoFormat());

  // then
  EXPECT_FALSE(resultFormat);
}

TEST(AVFormatMapperTest, SetFormatWhenFormatIsIncompatible) {
  // given
  id mockDevice = OCMClassMock([AVCaptureDevice class]);
  OCMStub([mockDevice formats]).andReturn(@[]);
  OCMStub([mockDevice lockForConfiguration:[OCMArg setTo:nil]]).andReturn(YES);
  NSException* testException =
      [NSException exceptionWithName:@"Test exception"
                              reason:@"Raised from unit tests"
                            userInfo:nil];
  OCMStub([mockDevice setActiveFormat:[OCMArg any]]).andThrow(testException);
  OCMExpect([mockDevice unlockForConfiguration]);

  // when
  bool resultFormat = webrtc::SetFormatForCaptureDevice(mockDevice, nil,
                                                        cricket::VideoFormat());

  // then
  EXPECT_FALSE(resultFormat);

  // TODO(denicija): Remove try-catch when Chromium rolls this change:
  // https://github.com/erikdoe/ocmock/commit/de1419415581dc307045e54bfe9c98c86efea96b
  // Without it, stubbed exceptions are being re-raised on [mock verify].
  // More information here:
  //https://github.com/erikdoe/ocmock/issues/241
  @try {
    [mockDevice verify];
  } @catch (NSException* exception) {
    if ([exception.reason isEqual:testException.reason]) {
      // Nothing dangerous here
      EXPECT_TRUE([exception.reason isEqualToString:exception.reason]);
    }
  }
}
