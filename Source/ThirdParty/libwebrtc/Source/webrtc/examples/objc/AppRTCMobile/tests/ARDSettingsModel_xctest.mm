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
#import <XCTest/XCTest.h>

#import "WebRTC/RTCMediaConstraints.h"

#import "ARDSettingsModel+Private.h"
#import "ARDSettingsStore.h"


@interface ARDSettingsModelTests : XCTestCase {
  ARDSettingsModel *_model;
}
@end

@implementation ARDSettingsModelTests

- (id)setupMockStoreWithMediaConstraintString:(NSString *)constraintString {
  id storeMock = [OCMockObject mockForClass:[ARDSettingsStore class]];
  [([[storeMock stub] andReturn:constraintString]) videoResolutionConstraints];

  id partialMock = [OCMockObject partialMockForObject:_model];
  [[[partialMock stub] andReturn:storeMock] settingsStore];

  return storeMock;
}

- (void)setUp {
  _model = [[ARDSettingsModel alloc] init];
}

- (void)testDefaultMediaFromStore {
  id storeMock = [self setupMockStoreWithMediaConstraintString:nil];
  [[storeMock expect] setVideoResolutionConstraints:@"640x480"];

  NSString *string = [_model currentVideoResoultionConstraintFromStore];

  XCTAssertEqualObjects(string, @"640x480");
  [storeMock verify];
}

- (void)testStoringInvalidConstraintReturnsNo {
  __unused id storeMock = [self setupMockStoreWithMediaConstraintString:@"960x480"];
  XCTAssertFalse([_model storeVideoResoultionConstraint:@"960x480"]);
}

- (void)testWidthConstraintFromStore {
  [self setupMockStoreWithMediaConstraintString:@"1270x480"];
  NSString *width = [_model currentVideoResolutionWidthFromStore];

  XCTAssertEqualObjects(width, @"1270");
}

- (void)testHeightConstraintFromStore {
  [self setupMockStoreWithMediaConstraintString:@"960x540"];
  NSString *height = [_model currentVideoResolutionHeightFromStore];

  XCTAssertEqualObjects(height, @"540");
}

- (void)testConstraintComponentIsNilWhenInvalidConstraintString {
  [self setupMockStoreWithMediaConstraintString:@"invalid"];
  NSString *width = [_model currentVideoResolutionWidthFromStore];

  XCTAssertNil(width);
}

- (void)testConstraintsDictionaryIsNilWhenInvalidConstraintString {
  [self setupMockStoreWithMediaConstraintString:@"invalid"];
  NSDictionary *constraintsDictionary = [_model currentMediaConstraintFromStoreAsRTCDictionary];

  XCTAssertNil(constraintsDictionary);
}
@end
