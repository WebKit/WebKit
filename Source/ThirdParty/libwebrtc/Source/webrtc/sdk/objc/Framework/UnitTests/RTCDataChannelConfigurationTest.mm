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

#include "webrtc/base/gunit.h"

#import "NSString+StdString.h"
#import "RTCDataChannelConfiguration+Private.h"
#import "WebRTC/RTCDataChannelConfiguration.h"

@interface RTCDataChannelConfigurationTest : NSObject
- (void)testConversionToNativeDataChannelInit;
@end

@implementation RTCDataChannelConfigurationTest

- (void)testConversionToNativeDataChannelInit {
  BOOL isOrdered = NO;
  int maxPacketLifeTime = 5;
  int maxRetransmits = 4;
  BOOL isNegotiated = YES;
  int channelId = 4;
  NSString *protocol = @"protocol";

  RTCDataChannelConfiguration *dataChannelConfig =
      [[RTCDataChannelConfiguration alloc] init];
  dataChannelConfig.isOrdered = isOrdered;
  dataChannelConfig.maxPacketLifeTime = maxPacketLifeTime;
  dataChannelConfig.maxRetransmits = maxRetransmits;
  dataChannelConfig.isNegotiated = isNegotiated;
  dataChannelConfig.channelId = channelId;
  dataChannelConfig.protocol = protocol;

  webrtc::DataChannelInit nativeInit = dataChannelConfig.nativeDataChannelInit;
  EXPECT_EQ(isOrdered, nativeInit.ordered);
  EXPECT_EQ(maxPacketLifeTime, nativeInit.maxRetransmitTime);
  EXPECT_EQ(maxRetransmits, nativeInit.maxRetransmits);
  EXPECT_EQ(isNegotiated, nativeInit.negotiated);
  EXPECT_EQ(channelId, nativeInit.id);
  EXPECT_EQ(protocol.stdString, nativeInit.protocol);
}

@end

TEST(RTCDataChannelConfiguration, NativeDataChannelInitConversionTest) {
  @autoreleasepool {
    RTCDataChannelConfigurationTest *test =
        [[RTCDataChannelConfigurationTest alloc] init];
    [test testConversionToNativeDataChannelInit];
  }
}
