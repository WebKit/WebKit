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

#include <vector>

#include "webrtc/base/gunit.h"

#import "NSString+StdString.h"
#import "RTCConfiguration+Private.h"
#import "WebRTC/RTCConfiguration.h"
#import "WebRTC/RTCPeerConnection.h"
#import "WebRTC/RTCPeerConnectionFactory.h"
#import "WebRTC/RTCIceServer.h"
#import "WebRTC/RTCMediaConstraints.h"

@interface RTCPeerConnectionTest : NSObject
- (void)testConfigurationGetter;
@end

@implementation RTCPeerConnectionTest

- (void)testConfigurationGetter {
  NSArray *urlStrings = @[ @"stun:stun1.example.net" ];
  RTCIceServer *server = [[RTCIceServer alloc] initWithURLStrings:urlStrings];

  RTCConfiguration *config = [[RTCConfiguration alloc] init];
  config.iceServers = @[ server ];
  config.iceTransportPolicy = RTCIceTransportPolicyRelay;
  config.bundlePolicy = RTCBundlePolicyMaxBundle;
  config.rtcpMuxPolicy = RTCRtcpMuxPolicyNegotiate;
  config.tcpCandidatePolicy = RTCTcpCandidatePolicyDisabled;
  config.candidateNetworkPolicy = RTCCandidateNetworkPolicyLowCost;
  const int maxPackets = 60;
  const int timeout = 1;
  const int interval = 2;
  config.audioJitterBufferMaxPackets = maxPackets;
  config.audioJitterBufferFastAccelerate = YES;
  config.iceConnectionReceivingTimeout = timeout;
  config.iceBackupCandidatePairPingInterval = interval;
  config.continualGatheringPolicy =
      RTCContinualGatheringPolicyGatherContinually;
  config.shouldPruneTurnPorts = YES;

  RTCMediaConstraints *contraints = [[RTCMediaConstraints alloc] initWithMandatoryConstraints:@{}
      optionalConstraints:nil];
  RTCPeerConnectionFactory *factory = [[RTCPeerConnectionFactory alloc] init];

  RTCConfiguration *newConfig;
  @autoreleasepool {
    RTCPeerConnection *peerConnection =
        [factory peerConnectionWithConfiguration:config constraints:contraints delegate:nil];
    newConfig = peerConnection.configuration;
  }
  EXPECT_EQ([config.iceServers count], [newConfig.iceServers count]);
  RTCIceServer *newServer = newConfig.iceServers[0];
  RTCIceServer *origServer = config.iceServers[0];
  std::string origUrl = origServer.urlStrings.firstObject.UTF8String;
  std::string url = newServer.urlStrings.firstObject.UTF8String;
  EXPECT_EQ(origUrl, url);

  EXPECT_EQ(config.iceTransportPolicy, newConfig.iceTransportPolicy);
  EXPECT_EQ(config.bundlePolicy, newConfig.bundlePolicy);
  EXPECT_EQ(config.rtcpMuxPolicy, newConfig.rtcpMuxPolicy);
  EXPECT_EQ(config.tcpCandidatePolicy, newConfig.tcpCandidatePolicy);
  EXPECT_EQ(config.candidateNetworkPolicy, newConfig.candidateNetworkPolicy);
  EXPECT_EQ(config.audioJitterBufferMaxPackets, newConfig.audioJitterBufferMaxPackets);
  EXPECT_EQ(config.audioJitterBufferFastAccelerate, newConfig.audioJitterBufferFastAccelerate);
  EXPECT_EQ(config.iceConnectionReceivingTimeout, newConfig.iceConnectionReceivingTimeout);
  EXPECT_EQ(config.iceBackupCandidatePairPingInterval,
            newConfig.iceBackupCandidatePairPingInterval);
  EXPECT_EQ(config.continualGatheringPolicy, newConfig.continualGatheringPolicy);
  EXPECT_EQ(config.shouldPruneTurnPorts, newConfig.shouldPruneTurnPorts);
}

@end

TEST(RTCPeerConnectionTest, ConfigurationGetterTest) {
  @autoreleasepool {
    RTCPeerConnectionTest *test = [[RTCPeerConnectionTest alloc] init];
    [test testConfigurationGetter];
  }
}


