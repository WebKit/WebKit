/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "api/logging/RTCCallbackLogger.h"

#import <XCTest/XCTest.h>

@interface RTCCallbackLoggerTests : XCTestCase

@property(nonatomic, strong) RTCCallbackLogger *logger;

@end

@implementation RTCCallbackLoggerTests

@synthesize logger;

- (void)setUp {
  self.logger = [[RTCCallbackLogger alloc] init];
}

- (void)tearDown {
  self.logger = nil;
}

- (void)testDefaultSeverityLevel {
  XCTAssertEqual(self.logger.severity, RTCLoggingSeverityInfo);
}

- (void)testCallbackGetsCalledForAppropriateLevel {
  self.logger.severity = RTCLoggingSeverityWarning;

  XCTestExpectation *callbackExpectation = [self expectationWithDescription:@"callbackWarning"];

  [self.logger start:^(NSString *message) {
    XCTAssertTrue([message hasSuffix:@"Horrible error\n"]);
    [callbackExpectation fulfill];
  }];

  RTCLogError("Horrible error");

  [self waitForExpectations:@[ callbackExpectation ] timeout:10.0];
}

- (void)testCallbackDoesNotGetCalledForOtherLevels {
  self.logger.severity = RTCLoggingSeverityError;

  XCTestExpectation *callbackExpectation = [self expectationWithDescription:@"callbackError"];

  [self.logger start:^(NSString *message) {
    XCTAssertTrue([message hasSuffix:@"Horrible error\n"]);
    [callbackExpectation fulfill];
  }];

  RTCLogInfo("Just some info");
  RTCLogWarning("Warning warning");
  RTCLogError("Horrible error");

  [self waitForExpectations:@[ callbackExpectation ] timeout:10.0];
}

- (void)testStartingWithNilCallbackDoesNotCrash {
  [self.logger start:nil];

  RTCLogError("Horrible error");
}

- (void)testStopCallbackLogger {
  XCTestExpectation *callbackExpectation = [self expectationWithDescription:@"stopped"];

  [self.logger start:^(NSString *message) {
    [callbackExpectation fulfill];
  }];

  [self.logger stop];

  RTCLogInfo("Just some info");

  XCTWaiter *waiter = [[XCTWaiter alloc] init];
  XCTWaiterResult result = [waiter waitForExpectations:@[ callbackExpectation ] timeout:1.0];
  XCTAssertEqual(result, XCTWaiterResultTimedOut);
}

- (void)testDestroyingCallbackLogger {
  XCTestExpectation *callbackExpectation = [self expectationWithDescription:@"destroyed"];

  [self.logger start:^(NSString *message) {
    [callbackExpectation fulfill];
  }];

  self.logger = nil;

  RTCLogInfo("Just some info");

  XCTWaiter *waiter = [[XCTWaiter alloc] init];
  XCTWaiterResult result = [waiter waitForExpectations:@[ callbackExpectation ] timeout:1.0];
  XCTAssertEqual(result, XCTWaiterResultTimedOut);
}

@end
