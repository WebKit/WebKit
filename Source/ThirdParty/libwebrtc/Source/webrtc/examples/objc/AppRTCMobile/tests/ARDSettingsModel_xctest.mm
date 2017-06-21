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

- (id)setupMockStore {
  id storeMock = [OCMockObject mockForClass:[ARDSettingsStore class]];

  id partialMock = [OCMockObject partialMockForObject:_model];
  [[[partialMock stub] andReturn:storeMock] settingsStore];
  [[[partialMock stub] andReturn:@[ @"640x480", @"960x540", @"1280x720" ]]
      availableVideoResolutions];

  return storeMock;
}

- (void)setUp {
  _model = [[ARDSettingsModel alloc] init];
}

- (void)testRetrievingSetting {
  id storeMock = [self setupMockStore];
  [[[storeMock expect] andReturn:@"640x480"] videoResolution];
  NSString *string = [_model currentVideoResolutionSettingFromStore];

  XCTAssertEqualObjects(string, @"640x480");
}

- (void)testStoringInvalidConstraintReturnsNo {
  id storeMock = [self setupMockStore];
  [([[storeMock stub] andReturn:@"960x480"])videoResolution];
  XCTAssertFalse([_model storeVideoResolutionSetting:@"960x480"]);
}

- (void)testWidthConstraintFromStore {
  id storeMock = [self setupMockStore];
  [([[storeMock stub] andReturn:@"1270x480"])videoResolution];
  int width = [_model currentVideoResolutionWidthFromStore];

  XCTAssertEqual(width, 1270);
}

- (void)testHeightConstraintFromStore {
  id storeMock = [self setupMockStore];
  [([[storeMock stub] andReturn:@"960x540"])videoResolution];
  int height = [_model currentVideoResolutionHeightFromStore];

  XCTAssertEqual(height, 540);
}

- (void)testConstraintComponentIsNilWhenInvalidConstraintString {
  id storeMock = [self setupMockStore];
  [([[storeMock stub] andReturn:@"invalid"])videoResolution];
  int width = [_model currentVideoResolutionWidthFromStore];

  XCTAssertEqual(width, 0);
}

- (void)testStoringAudioSetting {
  id storeMock = [self setupMockStore];
  [[storeMock expect] setAudioOnly:YES];

  [_model storeAudioOnlySetting:YES];
  [storeMock verify];
}

- (void)testReturningDefaultCallOption {
  id storeMock = [self setupMockStore];
  [[[storeMock stub] andReturnValue:@YES] useManualAudioConfig];

  XCTAssertTrue([_model currentUseManualAudioConfigSettingFromStore]);
}

@end
