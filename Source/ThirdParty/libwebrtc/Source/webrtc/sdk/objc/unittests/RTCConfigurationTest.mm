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

#include "rtc_base/gunit.h"

#import "api/peerconnection/RTCConfiguration+Private.h"
#import "api/peerconnection/RTCConfiguration.h"
#import "api/peerconnection/RTCIceServer.h"
#import "api/peerconnection/RTCIntervalRange.h"
#import "helpers/NSString+StdString.h"

@interface RTCConfigurationTest : NSObject
- (void)testConversionToNativeConfiguration;
- (void)testNativeConversionToConfiguration;
@end

@implementation RTCConfigurationTest

- (void)testConversionToNativeConfiguration {
  NSArray *urlStrings = @[ @"stun:stun1.example.net" ];
  RTCIceServer *server = [[RTCIceServer alloc] initWithURLStrings:urlStrings];
  RTCIntervalRange *range = [[RTCIntervalRange alloc] initWithMin:0 max:100];

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
  config.iceRegatherIntervalRange = range;

  std::unique_ptr<webrtc::PeerConnectionInterface::RTCConfiguration>
      nativeConfig([config createNativeConfiguration]);
  EXPECT_TRUE(nativeConfig.get());
  EXPECT_EQ(1u, nativeConfig->servers.size());
  webrtc::PeerConnectionInterface::IceServer nativeServer =
      nativeConfig->servers.front();
  EXPECT_EQ(1u, nativeServer.urls.size());
  EXPECT_EQ("stun:stun1.example.net", nativeServer.urls.front());

  EXPECT_EQ(webrtc::PeerConnectionInterface::kRelay, nativeConfig->type);
  EXPECT_EQ(webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle,
            nativeConfig->bundle_policy);
  EXPECT_EQ(webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate,
            nativeConfig->rtcp_mux_policy);
  EXPECT_EQ(webrtc::PeerConnectionInterface::kTcpCandidatePolicyDisabled,
            nativeConfig->tcp_candidate_policy);
  EXPECT_EQ(webrtc::PeerConnectionInterface::kCandidateNetworkPolicyLowCost,
            nativeConfig->candidate_network_policy);
  EXPECT_EQ(maxPackets, nativeConfig->audio_jitter_buffer_max_packets);
  EXPECT_EQ(true, nativeConfig->audio_jitter_buffer_fast_accelerate);
  EXPECT_EQ(timeout, nativeConfig->ice_connection_receiving_timeout);
  EXPECT_EQ(interval, nativeConfig->ice_backup_candidate_pair_ping_interval);
  EXPECT_EQ(webrtc::PeerConnectionInterface::GATHER_CONTINUALLY,
            nativeConfig->continual_gathering_policy);
  EXPECT_EQ(true, nativeConfig->prune_turn_ports);
  EXPECT_EQ(range.min, nativeConfig->ice_regather_interval_range->min());
  EXPECT_EQ(range.max, nativeConfig->ice_regather_interval_range->max());
}

- (void)testNativeConversionToConfiguration {
  NSArray *urlStrings = @[ @"stun:stun1.example.net" ];
  RTCIceServer *server = [[RTCIceServer alloc] initWithURLStrings:urlStrings];
  RTCIntervalRange *range = [[RTCIntervalRange alloc] initWithMin:0 max:100];

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
  config.iceRegatherIntervalRange = range;

  webrtc::PeerConnectionInterface::RTCConfiguration *nativeConfig =
      [config createNativeConfiguration];
  RTCConfiguration *newConfig = [[RTCConfiguration alloc]
      initWithNativeConfiguration:*nativeConfig];
  EXPECT_EQ([config.iceServers count], newConfig.iceServers.count);
  RTCIceServer *newServer = newConfig.iceServers[0];
  RTCIceServer *origServer = config.iceServers[0];
  EXPECT_EQ(origServer.urlStrings.count, server.urlStrings.count);
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
  EXPECT_EQ(config.iceRegatherIntervalRange.min, newConfig.iceRegatherIntervalRange.min);
  EXPECT_EQ(config.iceRegatherIntervalRange.max, newConfig.iceRegatherIntervalRange.max);
}

@end

TEST(RTCConfigurationTest, NativeConfigurationConversionTest) {
  @autoreleasepool {
    RTCConfigurationTest *test = [[RTCConfigurationTest alloc] init];
    [test testConversionToNativeConfiguration];
    [test testNativeConversionToConfiguration];
  }
}
