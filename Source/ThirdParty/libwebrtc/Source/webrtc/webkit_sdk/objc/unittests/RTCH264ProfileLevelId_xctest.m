/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "components/video_codec/RTCH264ProfileLevelId.h"

#import <XCTest/XCTest.h>

@interface RTCH264ProfileLevelIdTests : XCTestCase

@end

static NSString *level31ConstrainedHigh = @"640c1f";
static NSString *level31ConstrainedBaseline = @"42e01f";

@implementation RTCH264ProfileLevelIdTests

- (void)testInitWithString {
  RTCH264ProfileLevelId *profileLevelId =
      [[RTCH264ProfileLevelId alloc] initWithHexString:level31ConstrainedHigh];
  XCTAssertEqual(profileLevelId.profile, RTCH264ProfileConstrainedHigh);
  XCTAssertEqual(profileLevelId.level, RTCH264Level3_1);

  profileLevelId = [[RTCH264ProfileLevelId alloc] initWithHexString:level31ConstrainedBaseline];
  XCTAssertEqual(profileLevelId.profile, RTCH264ProfileConstrainedBaseline);
  XCTAssertEqual(profileLevelId.level, RTCH264Level3_1);
}

- (void)testInitWithProfileAndLevel {
  RTCH264ProfileLevelId *profileLevelId =
      [[RTCH264ProfileLevelId alloc] initWithProfile:RTCH264ProfileConstrainedHigh
                                               level:RTCH264Level3_1];
  XCTAssertEqualObjects(profileLevelId.hexString, level31ConstrainedHigh);

  profileLevelId = [[RTCH264ProfileLevelId alloc] initWithProfile:RTCH264ProfileConstrainedBaseline
                                                            level:RTCH264Level3_1];
  XCTAssertEqualObjects(profileLevelId.hexString, level31ConstrainedBaseline);
}

@end
