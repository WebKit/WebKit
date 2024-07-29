/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <XCTest/XCTest.h>

#import <Foundation/Foundation.h>

#import <WebRTC/WebRTC.h>

@interface RTCDoNotPutCPlusPlusInFrameworkHeaders : XCTestCase
@end

@implementation RTCDoNotPutCPlusPlusInFrameworkHeaders

- (void)testNoCPlusPlusInFrameworkHeaders {
  NSString *fullPath = [NSString stringWithFormat:@"%s", __FILE__];
  NSString *extension = fullPath.pathExtension;

  XCTAssertEqualObjects(
      @"m", extension, @"Do not rename %@. It should end with .m.", fullPath.lastPathComponent);
}

@end
