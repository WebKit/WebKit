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
#import <OCMock/OCMock.h>
#import <XCTest/XCTest.h>

#import "ARDFileCaptureController.h"

#import "sdk/objc/components/capturer/RTCFileVideoCapturer.h"

NS_CLASS_AVAILABLE_IOS(10)
@interface ARDFileCaptureControllerTests : XCTestCase

@property(nonatomic, strong) ARDFileCaptureController *fileCaptureController;
@property(nonatomic, strong) id fileCapturerMock;

@end

@implementation ARDFileCaptureControllerTests

@synthesize fileCaptureController = _fileCaptureController;
@synthesize fileCapturerMock = _fileCapturerMock;

- (void)setUp {
  [super setUp];
  self.fileCapturerMock = OCMClassMock([RTC_OBJC_TYPE(RTCFileVideoCapturer) class]);
  self.fileCaptureController =
      [[ARDFileCaptureController alloc] initWithCapturer:self.fileCapturerMock];
}

- (void)tearDown {
  self.fileCaptureController = nil;
  [self.fileCapturerMock stopMocking];
  self.fileCapturerMock = nil;
  [super tearDown];
}

- (void)testCaptureIsStarted {
  [[self.fileCapturerMock expect] startCapturingFromFileNamed:[OCMArg any] onError:[OCMArg any]];

  [self.fileCaptureController startCapture];

  [self.fileCapturerMock verify];
}

- (void)testCaptureIsStoped {
  [[self.fileCapturerMock expect] stopCapture];

  [self.fileCaptureController stopCapture];

  [self.fileCapturerMock verify];
}

@end
