/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "components/capturer/RTCFileVideoCapturer.h"

#import <XCTest/XCTest.h>

#include "rtc_base/gunit.h"

NSString *const kTestFileName = @"foreman.mp4";
static const int kTestTimeoutMs = 5 * 1000;  // 5secs.

@interface MockCapturerDelegate : NSObject <RTCVideoCapturerDelegate>

@property(nonatomic, assign) NSInteger capturedFramesCount;

@end

@implementation MockCapturerDelegate
@synthesize capturedFramesCount = _capturedFramesCount;

- (void)capturer:(RTCVideoCapturer *)capturer didCaptureVideoFrame:(RTCVideoFrame *)frame {
  self.capturedFramesCount++;
}

@end

NS_CLASS_AVAILABLE_IOS(10)
@interface RTCFileVideoCapturerTests : XCTestCase

@property(nonatomic, strong) RTCFileVideoCapturer *capturer;
@property(nonatomic, strong) MockCapturerDelegate *mockDelegate;

@end

@implementation RTCFileVideoCapturerTests
@synthesize capturer = _capturer;
@synthesize mockDelegate = _mockDelegate;

- (void)setUp {
  self.mockDelegate = [[MockCapturerDelegate alloc] init];
  self.capturer = [[RTCFileVideoCapturer alloc] initWithDelegate:self.mockDelegate];
}

- (void)tearDown {
  self.capturer = nil;
  self.mockDelegate = nil;
}

- (void)testCaptureWhenFileNotInBundle {
  __block BOOL errorOccured = NO;

  RTCFileVideoCapturerErrorBlock errorBlock = ^void(NSError *error) {
    errorOccured = YES;
  };

  [self.capturer startCapturingFromFileNamed:@"not_in_bundle.mov" onError:errorBlock];
  ASSERT_TRUE_WAIT(errorOccured, kTestTimeoutMs);
}

- (void)testSecondStartCaptureCallFails {
  __block BOOL secondError = NO;

  RTCFileVideoCapturerErrorBlock firstErrorBlock = ^void(NSError *error) {
    // This block should never be called.
    NSLog(@"Error: %@", [error userInfo]);
    ASSERT_TRUE(false);
  };

  RTCFileVideoCapturerErrorBlock secondErrorBlock = ^void(NSError *error) {
    secondError = YES;
  };

  [self.capturer startCapturingFromFileNamed:kTestFileName onError:firstErrorBlock];
  [self.capturer startCapturingFromFileNamed:kTestFileName onError:secondErrorBlock];

  ASSERT_TRUE_WAIT(secondError, kTestTimeoutMs);
}

- (void)testStartStopCapturer {
#if defined(__IPHONE_11_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_11_0)
  if (@available(iOS 10, *)) {
    [self.capturer startCapturingFromFileNamed:kTestFileName onError:nil];

    __block BOOL done = NO;
    __block NSInteger capturedFrames = -1;
    NSInteger capturedFramesAfterStop = -1;

    // We're dispatching the `stopCapture` with delay to ensure the capturer has
    // had the chance to capture several frames.
    dispatch_time_t captureDelay = dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC);  // 2secs.
    dispatch_after(captureDelay, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      capturedFrames = self.mockDelegate.capturedFramesCount;
      [self.capturer stopCapture];
      done = YES;
    });
    WAIT(done, kTestTimeoutMs);

    capturedFramesAfterStop = self.mockDelegate.capturedFramesCount;
    ASSERT_TRUE(capturedFrames != -1);
    ASSERT_EQ(capturedFrames, capturedFramesAfterStop);
  }
#endif
}

@end
