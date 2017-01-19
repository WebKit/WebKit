/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#include <memory>

#include "webrtc/base/gunit.h"

#import "NSString+StdString.h"
#import "RTCMediaConstraints+Private.h"
#import "WebRTC/RTCMediaConstraints.h"

@interface RTCMediaConstraintsTest : NSObject
- (void)testMediaConstraints;
@end

@implementation RTCMediaConstraintsTest

- (void)testMediaConstraints {
  NSDictionary *mandatory = @{@"key1": @"value1", @"key2": @"value2"};
  NSDictionary *optional = @{@"key3": @"value3", @"key4": @"value4"};

  RTCMediaConstraints *constraints = [[RTCMediaConstraints alloc]
      initWithMandatoryConstraints:mandatory
               optionalConstraints:optional];
  std::unique_ptr<webrtc::MediaConstraints> nativeConstraints =
      [constraints nativeConstraints];

  webrtc::MediaConstraintsInterface::Constraints nativeMandatory =
      nativeConstraints->GetMandatory();
  [self expectConstraints:mandatory inNativeConstraints:nativeMandatory];

  webrtc::MediaConstraintsInterface::Constraints nativeOptional =
      nativeConstraints->GetOptional();
  [self expectConstraints:optional inNativeConstraints:nativeOptional];
}

- (void)expectConstraints:(NSDictionary *)constraints
      inNativeConstraints:
    (webrtc::MediaConstraintsInterface::Constraints)nativeConstraints {
  EXPECT_EQ(constraints.count, nativeConstraints.size());

  for (NSString *key in constraints) {
    NSString *value = [constraints objectForKey:key];

    std::string nativeValue;
    bool found = nativeConstraints.FindFirst(key.stdString, &nativeValue);
    EXPECT_TRUE(found);
    EXPECT_EQ(value.stdString, nativeValue);
  }
}

@end

TEST(RTCMediaConstraintsTest, MediaConstraintsTest) {
  @autoreleasepool {
    RTCMediaConstraintsTest *test = [[RTCMediaConstraintsTest alloc] init];
    [test testMediaConstraints];
  }
}
