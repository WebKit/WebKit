/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCConfiguration+Private.h"

#include <memory>

#import "RTCIceServer+Private.h"
#import "WebRTC/RTCLogging.h"

#include "webrtc/base/rtccertificategenerator.h"
#include "webrtc/base/sslidentity.h"

@implementation RTCConfiguration

@synthesize iceServers = _iceServers;
@synthesize iceTransportPolicy = _iceTransportPolicy;
@synthesize bundlePolicy = _bundlePolicy;
@synthesize rtcpMuxPolicy = _rtcpMuxPolicy;
@synthesize tcpCandidatePolicy = _tcpCandidatePolicy;
@synthesize candidateNetworkPolicy = _candidateNetworkPolicy;
@synthesize continualGatheringPolicy = _continualGatheringPolicy;
@synthesize audioJitterBufferMaxPackets = _audioJitterBufferMaxPackets;
@synthesize audioJitterBufferFastAccelerate = _audioJitterBufferFastAccelerate;
@synthesize iceConnectionReceivingTimeout = _iceConnectionReceivingTimeout;
@synthesize iceBackupCandidatePairPingInterval =
    _iceBackupCandidatePairPingInterval;
@synthesize keyType = _keyType;
@synthesize iceCandidatePoolSize = _iceCandidatePoolSize;
@synthesize shouldPruneTurnPorts = _shouldPruneTurnPorts;
@synthesize shouldPresumeWritableWhenFullyRelayed =
    _shouldPresumeWritableWhenFullyRelayed;
@synthesize iceCheckMinInterval = _iceCheckMinInterval;

- (instancetype)init {
  // Copy defaults.
  webrtc::PeerConnectionInterface::RTCConfiguration config(
    webrtc::PeerConnectionInterface::RTCConfigurationType::kAggressive);
  return [self initWithNativeConfiguration:config];
}

- (instancetype)initWithNativeConfiguration:
    (const webrtc::PeerConnectionInterface::RTCConfiguration &)config {
  if (self = [super init]) {
    NSMutableArray *iceServers = [NSMutableArray array];
    for (const webrtc::PeerConnectionInterface::IceServer& server : config.servers) {
      RTCIceServer *iceServer = [[RTCIceServer alloc] initWithNativeServer:server];
      [iceServers addObject:iceServer];
    }
    _iceServers = iceServers;
    _iceTransportPolicy =
        [[self class] transportPolicyForTransportsType:config.type];
    _bundlePolicy =
        [[self class] bundlePolicyForNativePolicy:config.bundle_policy];
    _rtcpMuxPolicy =
        [[self class] rtcpMuxPolicyForNativePolicy:config.rtcp_mux_policy];
    _tcpCandidatePolicy = [[self class] tcpCandidatePolicyForNativePolicy:
        config.tcp_candidate_policy];
    _candidateNetworkPolicy = [[self class]
        candidateNetworkPolicyForNativePolicy:config.candidate_network_policy];
    webrtc::PeerConnectionInterface::ContinualGatheringPolicy nativePolicy =
    config.continual_gathering_policy;
    _continualGatheringPolicy =
        [[self class] continualGatheringPolicyForNativePolicy:nativePolicy];
    _audioJitterBufferMaxPackets = config.audio_jitter_buffer_max_packets;
    _audioJitterBufferFastAccelerate = config.audio_jitter_buffer_fast_accelerate;
    _iceConnectionReceivingTimeout = config.ice_connection_receiving_timeout;
    _iceBackupCandidatePairPingInterval =
        config.ice_backup_candidate_pair_ping_interval;
    _keyType = RTCEncryptionKeyTypeECDSA;
    _iceCandidatePoolSize = config.ice_candidate_pool_size;
    _shouldPruneTurnPorts = config.prune_turn_ports;
    _shouldPresumeWritableWhenFullyRelayed =
        config.presume_writable_when_fully_relayed;
    if (config.ice_check_min_interval) {
      _iceCheckMinInterval =
          [NSNumber numberWithInt:*config.ice_check_min_interval];
    }
  }
  return self;
}

- (NSString *)description {
  return [NSString stringWithFormat:
      @"RTCConfiguration: {\n%@\n%@\n%@\n%@\n%@\n%@\n%@\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%@\n}\n",
      _iceServers,
      [[self class] stringForTransportPolicy:_iceTransportPolicy],
      [[self class] stringForBundlePolicy:_bundlePolicy],
      [[self class] stringForRtcpMuxPolicy:_rtcpMuxPolicy],
      [[self class] stringForTcpCandidatePolicy:_tcpCandidatePolicy],
      [[self class] stringForCandidateNetworkPolicy:_candidateNetworkPolicy],
      [[self class]
          stringForContinualGatheringPolicy:_continualGatheringPolicy],
      _audioJitterBufferMaxPackets,
      _audioJitterBufferFastAccelerate,
      _iceConnectionReceivingTimeout,
      _iceBackupCandidatePairPingInterval,
      _iceCandidatePoolSize,
      _shouldPruneTurnPorts,
      _shouldPresumeWritableWhenFullyRelayed,
      _iceCheckMinInterval];
}

#pragma mark - Private

- (webrtc::PeerConnectionInterface::RTCConfiguration *)
    createNativeConfiguration {
  std::unique_ptr<webrtc::PeerConnectionInterface::RTCConfiguration>
      nativeConfig(new webrtc::PeerConnectionInterface::RTCConfiguration(
          webrtc::PeerConnectionInterface::RTCConfigurationType::kAggressive));

  for (RTCIceServer *iceServer in _iceServers) {
    nativeConfig->servers.push_back(iceServer.nativeServer);
  }
  nativeConfig->type =
      [[self class] nativeTransportsTypeForTransportPolicy:_iceTransportPolicy];
  nativeConfig->bundle_policy =
      [[self class] nativeBundlePolicyForPolicy:_bundlePolicy];
  nativeConfig->rtcp_mux_policy =
      [[self class] nativeRtcpMuxPolicyForPolicy:_rtcpMuxPolicy];
  nativeConfig->tcp_candidate_policy =
      [[self class] nativeTcpCandidatePolicyForPolicy:_tcpCandidatePolicy];
  nativeConfig->candidate_network_policy = [[self class]
      nativeCandidateNetworkPolicyForPolicy:_candidateNetworkPolicy];
  nativeConfig->continual_gathering_policy = [[self class]
      nativeContinualGatheringPolicyForPolicy:_continualGatheringPolicy];
  nativeConfig->audio_jitter_buffer_max_packets = _audioJitterBufferMaxPackets;
  nativeConfig->audio_jitter_buffer_fast_accelerate =
      _audioJitterBufferFastAccelerate  ? true : false;
  nativeConfig->ice_connection_receiving_timeout =
      _iceConnectionReceivingTimeout;
  nativeConfig->ice_backup_candidate_pair_ping_interval =
      _iceBackupCandidatePairPingInterval;
  rtc::KeyType keyType =
      [[self class] nativeEncryptionKeyTypeForKeyType:_keyType];
  // Generate non-default certificate.
  if (keyType != rtc::KT_DEFAULT) {
    rtc::scoped_refptr<rtc::RTCCertificate> certificate =
        rtc::RTCCertificateGenerator::GenerateCertificate(
            rtc::KeyParams(keyType), rtc::Optional<uint64_t>());
    if (!certificate) {
      RTCLogError(@"Failed to generate certificate.");
      return nullptr;
    }
    nativeConfig->certificates.push_back(certificate);
  }
  nativeConfig->ice_candidate_pool_size = _iceCandidatePoolSize;
  nativeConfig->prune_turn_ports = _shouldPruneTurnPorts ? true : false;
  nativeConfig->presume_writable_when_fully_relayed =
      _shouldPresumeWritableWhenFullyRelayed ? true : false;
  if (_iceCheckMinInterval != nil) {
    nativeConfig->ice_check_min_interval =
        rtc::Optional<int>(_iceCheckMinInterval.intValue);
  }

  return nativeConfig.release();
}

+ (webrtc::PeerConnectionInterface::IceTransportsType)
    nativeTransportsTypeForTransportPolicy:(RTCIceTransportPolicy)policy {
  switch (policy) {
    case RTCIceTransportPolicyNone:
      return webrtc::PeerConnectionInterface::kNone;
    case RTCIceTransportPolicyRelay:
      return webrtc::PeerConnectionInterface::kRelay;
    case RTCIceTransportPolicyNoHost:
      return webrtc::PeerConnectionInterface::kNoHost;
    case RTCIceTransportPolicyAll:
      return webrtc::PeerConnectionInterface::kAll;
  }
}

+ (RTCIceTransportPolicy)transportPolicyForTransportsType:
    (webrtc::PeerConnectionInterface::IceTransportsType)nativeType {
  switch (nativeType) {
    case webrtc::PeerConnectionInterface::kNone:
      return RTCIceTransportPolicyNone;
    case webrtc::PeerConnectionInterface::kRelay:
      return RTCIceTransportPolicyRelay;
    case webrtc::PeerConnectionInterface::kNoHost:
      return RTCIceTransportPolicyNoHost;
    case webrtc::PeerConnectionInterface::kAll:
      return RTCIceTransportPolicyAll;
  }
}

+ (NSString *)stringForTransportPolicy:(RTCIceTransportPolicy)policy {
  switch (policy) {
    case RTCIceTransportPolicyNone:
      return @"NONE";
    case RTCIceTransportPolicyRelay:
      return @"RELAY";
    case RTCIceTransportPolicyNoHost:
      return @"NO_HOST";
    case RTCIceTransportPolicyAll:
      return @"ALL";
  }
}

+ (webrtc::PeerConnectionInterface::BundlePolicy)nativeBundlePolicyForPolicy:
    (RTCBundlePolicy)policy {
  switch (policy) {
    case RTCBundlePolicyBalanced:
      return webrtc::PeerConnectionInterface::kBundlePolicyBalanced;
    case RTCBundlePolicyMaxCompat:
      return webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat;
    case RTCBundlePolicyMaxBundle:
      return webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle;
  }
}

+ (RTCBundlePolicy)bundlePolicyForNativePolicy:
    (webrtc::PeerConnectionInterface::BundlePolicy)nativePolicy {
  switch (nativePolicy) {
    case webrtc::PeerConnectionInterface::kBundlePolicyBalanced:
      return RTCBundlePolicyBalanced;
    case webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat:
      return RTCBundlePolicyMaxCompat;
    case webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle:
      return RTCBundlePolicyMaxBundle;
  }
}

+ (NSString *)stringForBundlePolicy:(RTCBundlePolicy)policy {
  switch (policy) {
    case RTCBundlePolicyBalanced:
      return @"BALANCED";
    case RTCBundlePolicyMaxCompat:
      return @"MAX_COMPAT";
    case RTCBundlePolicyMaxBundle:
      return @"MAX_BUNDLE";
  }
}

+ (webrtc::PeerConnectionInterface::RtcpMuxPolicy)nativeRtcpMuxPolicyForPolicy:
    (RTCRtcpMuxPolicy)policy {
  switch (policy) {
    case RTCRtcpMuxPolicyNegotiate:
      return webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
    case RTCRtcpMuxPolicyRequire:
      return webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire;
  }
}

+ (RTCRtcpMuxPolicy)rtcpMuxPolicyForNativePolicy:
    (webrtc::PeerConnectionInterface::RtcpMuxPolicy)nativePolicy {
  switch (nativePolicy) {
    case webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate:
      return RTCRtcpMuxPolicyNegotiate;
    case webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire:
      return RTCRtcpMuxPolicyRequire;
  }
}

+ (NSString *)stringForRtcpMuxPolicy:(RTCRtcpMuxPolicy)policy {
  switch (policy) {
    case RTCRtcpMuxPolicyNegotiate:
      return @"NEGOTIATE";
    case RTCRtcpMuxPolicyRequire:
      return @"REQUIRE";
  }
}

+ (webrtc::PeerConnectionInterface::TcpCandidatePolicy)
    nativeTcpCandidatePolicyForPolicy:(RTCTcpCandidatePolicy)policy {
  switch (policy) {
    case RTCTcpCandidatePolicyEnabled:
      return webrtc::PeerConnectionInterface::kTcpCandidatePolicyEnabled;
    case RTCTcpCandidatePolicyDisabled:
      return webrtc::PeerConnectionInterface::kTcpCandidatePolicyDisabled;
  }
}

+ (webrtc::PeerConnectionInterface::CandidateNetworkPolicy)
    nativeCandidateNetworkPolicyForPolicy:(RTCCandidateNetworkPolicy)policy {
  switch (policy) {
    case RTCCandidateNetworkPolicyAll:
      return webrtc::PeerConnectionInterface::kCandidateNetworkPolicyAll;
    case RTCCandidateNetworkPolicyLowCost:
      return webrtc::PeerConnectionInterface::kCandidateNetworkPolicyLowCost;
  }
}

+ (RTCTcpCandidatePolicy)tcpCandidatePolicyForNativePolicy:
    (webrtc::PeerConnectionInterface::TcpCandidatePolicy)nativePolicy {
  switch (nativePolicy) {
    case webrtc::PeerConnectionInterface::kTcpCandidatePolicyEnabled:
      return RTCTcpCandidatePolicyEnabled;
    case webrtc::PeerConnectionInterface::kTcpCandidatePolicyDisabled:
      return RTCTcpCandidatePolicyDisabled;
  }
}

+ (NSString *)stringForTcpCandidatePolicy:(RTCTcpCandidatePolicy)policy {
  switch (policy) {
    case RTCTcpCandidatePolicyEnabled:
      return @"TCP_ENABLED";
    case RTCTcpCandidatePolicyDisabled:
      return @"TCP_DISABLED";
  }
}

+ (RTCCandidateNetworkPolicy)candidateNetworkPolicyForNativePolicy:
    (webrtc::PeerConnectionInterface::CandidateNetworkPolicy)nativePolicy {
  switch (nativePolicy) {
    case webrtc::PeerConnectionInterface::kCandidateNetworkPolicyAll:
      return RTCCandidateNetworkPolicyAll;
    case webrtc::PeerConnectionInterface::kCandidateNetworkPolicyLowCost:
      return RTCCandidateNetworkPolicyLowCost;
  }
}

+ (NSString *)stringForCandidateNetworkPolicy:
    (RTCCandidateNetworkPolicy)policy {
  switch (policy) {
    case RTCCandidateNetworkPolicyAll:
      return @"CANDIDATE_ALL_NETWORKS";
    case RTCCandidateNetworkPolicyLowCost:
      return @"CANDIDATE_LOW_COST_NETWORKS";
  }
}

+ (webrtc::PeerConnectionInterface::ContinualGatheringPolicy)
    nativeContinualGatheringPolicyForPolicy:
        (RTCContinualGatheringPolicy)policy {
  switch (policy) {
    case RTCContinualGatheringPolicyGatherOnce:
      return webrtc::PeerConnectionInterface::GATHER_ONCE;
    case RTCContinualGatheringPolicyGatherContinually:
      return webrtc::PeerConnectionInterface::GATHER_CONTINUALLY;
  }
}

+ (RTCContinualGatheringPolicy)continualGatheringPolicyForNativePolicy:
    (webrtc::PeerConnectionInterface::ContinualGatheringPolicy)nativePolicy {
  switch (nativePolicy) {
    case webrtc::PeerConnectionInterface::GATHER_ONCE:
      return RTCContinualGatheringPolicyGatherOnce;
    case webrtc::PeerConnectionInterface::GATHER_CONTINUALLY:
      return RTCContinualGatheringPolicyGatherContinually;
  }
}

+ (NSString *)stringForContinualGatheringPolicy:
    (RTCContinualGatheringPolicy)policy {
  switch (policy) {
    case RTCContinualGatheringPolicyGatherOnce:
      return @"GATHER_ONCE";
    case RTCContinualGatheringPolicyGatherContinually:
      return @"GATHER_CONTINUALLY";
  }
}

+ (rtc::KeyType)nativeEncryptionKeyTypeForKeyType:
    (RTCEncryptionKeyType)keyType {
  switch (keyType) {
    case RTCEncryptionKeyTypeRSA:
      return rtc::KT_RSA;
    case RTCEncryptionKeyTypeECDSA:
      return rtc::KT_ECDSA;
  }
}

@end
