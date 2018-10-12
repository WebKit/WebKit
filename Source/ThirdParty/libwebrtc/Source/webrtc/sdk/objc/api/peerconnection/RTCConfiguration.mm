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

#import "RTCCertificate.h"
#import "RTCConfiguration+Native.h"
#import "RTCIceServer+Private.h"
#import "RTCIntervalRange+Private.h"
#import "base/RTCLogging.h"

#include "rtc_base/rtccertificategenerator.h"
#include "rtc_base/sslidentity.h"

@implementation RTCConfiguration

@synthesize iceServers = _iceServers;
@synthesize certificate = _certificate;
@synthesize iceTransportPolicy = _iceTransportPolicy;
@synthesize bundlePolicy = _bundlePolicy;
@synthesize rtcpMuxPolicy = _rtcpMuxPolicy;
@synthesize tcpCandidatePolicy = _tcpCandidatePolicy;
@synthesize candidateNetworkPolicy = _candidateNetworkPolicy;
@synthesize continualGatheringPolicy = _continualGatheringPolicy;
@synthesize maxIPv6Networks = _maxIPv6Networks;
@synthesize disableLinkLocalNetworks = _disableLinkLocalNetworks;
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
@synthesize iceRegatherIntervalRange = _iceRegatherIntervalRange;
@synthesize sdpSemantics = _sdpSemantics;
@synthesize turnCustomizer = _turnCustomizer;
@synthesize activeResetSrtpParams = _activeResetSrtpParams;

- (instancetype)init {
  // Copy defaults.
  webrtc::PeerConnectionInterface::RTCConfiguration config;
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
    if (!config.certificates.empty()) {
      rtc::scoped_refptr<rtc::RTCCertificate> native_cert;
      native_cert = config.certificates[0];
      rtc::RTCCertificatePEM native_pem = native_cert->ToPEM();
      _certificate =
          [[RTCCertificate alloc] initWithPrivateKey:@(native_pem.private_key().c_str())
                                         certificate:@(native_pem.certificate().c_str())];
    }
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
    _maxIPv6Networks = config.max_ipv6_networks;
    _disableLinkLocalNetworks = config.disable_link_local_networks;
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
    if (config.ice_regather_interval_range) {
      const rtc::IntervalRange &nativeIntervalRange = config.ice_regather_interval_range.value();
      _iceRegatherIntervalRange =
          [[RTCIntervalRange alloc] initWithNativeIntervalRange:nativeIntervalRange];
    }
    _sdpSemantics = [[self class] sdpSemanticsForNativeSdpSemantics:config.sdp_semantics];
    _turnCustomizer = config.turn_customizer;
    _activeResetSrtpParams = config.active_reset_srtp_params;
  }
  return self;
}

- (NSString *)description {
  static NSString *formatString =
      @"RTCConfiguration: "
      @"{\n%@\n%@\n%@\n%@\n%@\n%@\n%@\n%@\n%d\n%d\n%d\n%d\n%d\n%d\n%d\n%@\n%@\n%d\n%d\n%d\n}\n";

  return [NSString
      stringWithFormat:formatString,
                       _iceServers,
                       [[self class] stringForTransportPolicy:_iceTransportPolicy],
                       [[self class] stringForBundlePolicy:_bundlePolicy],
                       [[self class] stringForRtcpMuxPolicy:_rtcpMuxPolicy],
                       [[self class] stringForTcpCandidatePolicy:_tcpCandidatePolicy],
                       [[self class] stringForCandidateNetworkPolicy:_candidateNetworkPolicy],
                       [[self class] stringForContinualGatheringPolicy:_continualGatheringPolicy],
                       [[self class] stringForSdpSemantics:_sdpSemantics],
                       _audioJitterBufferMaxPackets,
                       _audioJitterBufferFastAccelerate,
                       _iceConnectionReceivingTimeout,
                       _iceBackupCandidatePairPingInterval,
                       _iceCandidatePoolSize,
                       _shouldPruneTurnPorts,
                       _shouldPresumeWritableWhenFullyRelayed,
                       _iceCheckMinInterval,
                       _iceRegatherIntervalRange,
                       _disableLinkLocalNetworks,
                       _maxIPv6Networks,
                       _activeResetSrtpParams];
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
  nativeConfig->max_ipv6_networks = _maxIPv6Networks;
  nativeConfig->disable_link_local_networks = _disableLinkLocalNetworks;
  nativeConfig->audio_jitter_buffer_max_packets = _audioJitterBufferMaxPackets;
  nativeConfig->audio_jitter_buffer_fast_accelerate =
      _audioJitterBufferFastAccelerate  ? true : false;
  nativeConfig->ice_connection_receiving_timeout =
      _iceConnectionReceivingTimeout;
  nativeConfig->ice_backup_candidate_pair_ping_interval =
      _iceBackupCandidatePairPingInterval;
  rtc::KeyType keyType =
      [[self class] nativeEncryptionKeyTypeForKeyType:_keyType];
  if (_certificate != nullptr) {
    // if offered a pemcert use it...
    RTC_LOG(LS_INFO) << "Have configured cert - using it.";
    std::string pem_private_key = [[_certificate private_key] UTF8String];
    std::string pem_certificate = [[_certificate certificate] UTF8String];
    rtc::RTCCertificatePEM pem = rtc::RTCCertificatePEM(pem_private_key, pem_certificate);
    rtc::scoped_refptr<rtc::RTCCertificate> certificate = rtc::RTCCertificate::FromPEM(pem);
    RTC_LOG(LS_INFO) << "Created cert from PEM strings.";
    if (!certificate) {
      RTC_LOG(LS_ERROR) << "Failed to generate certificate from PEM.";
      return nullptr;
    }
    nativeConfig->certificates.push_back(certificate);
  } else {
    RTC_LOG(LS_INFO) << "Don't have configured cert.";
    // Generate non-default certificate.
    if (keyType != rtc::KT_DEFAULT) {
      rtc::scoped_refptr<rtc::RTCCertificate> certificate =
          rtc::RTCCertificateGenerator::GenerateCertificate(rtc::KeyParams(keyType),
                                                            absl::optional<uint64_t>());
      if (!certificate) {
        RTCLogError(@"Failed to generate certificate.");
        return nullptr;
      }
      nativeConfig->certificates.push_back(certificate);
    }
  }
  nativeConfig->ice_candidate_pool_size = _iceCandidatePoolSize;
  nativeConfig->prune_turn_ports = _shouldPruneTurnPorts ? true : false;
  nativeConfig->presume_writable_when_fully_relayed =
      _shouldPresumeWritableWhenFullyRelayed ? true : false;
  if (_iceCheckMinInterval != nil) {
    nativeConfig->ice_check_min_interval = absl::optional<int>(_iceCheckMinInterval.intValue);
  }
  if (_iceRegatherIntervalRange != nil) {
    std::unique_ptr<rtc::IntervalRange> nativeIntervalRange(
        _iceRegatherIntervalRange.nativeIntervalRange);
    nativeConfig->ice_regather_interval_range =
        absl::optional<rtc::IntervalRange>(*nativeIntervalRange);
  }
  nativeConfig->sdp_semantics = [[self class] nativeSdpSemanticsForSdpSemantics:_sdpSemantics];
  if (_turnCustomizer) {
    nativeConfig->turn_customizer = _turnCustomizer;
  }
  nativeConfig->active_reset_srtp_params = _activeResetSrtpParams ? true : false;
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

+ (webrtc::SdpSemantics)nativeSdpSemanticsForSdpSemantics:(RTCSdpSemantics)sdpSemantics {
  switch (sdpSemantics) {
    case RTCSdpSemanticsPlanB:
      return webrtc::SdpSemantics::kPlanB;
    case RTCSdpSemanticsUnifiedPlan:
      return webrtc::SdpSemantics::kUnifiedPlan;
  }
}

+ (RTCSdpSemantics)sdpSemanticsForNativeSdpSemantics:(webrtc::SdpSemantics)sdpSemantics {
  switch (sdpSemantics) {
    case webrtc::SdpSemantics::kPlanB:
      return RTCSdpSemanticsPlanB;
    case webrtc::SdpSemantics::kUnifiedPlan:
      return RTCSdpSemanticsUnifiedPlan;
  }
}

+ (NSString *)stringForSdpSemantics:(RTCSdpSemantics)sdpSemantics {
  switch (sdpSemantics) {
    case RTCSdpSemanticsPlanB:
      return @"PLAN_B";
    case RTCSdpSemanticsUnifiedPlan:
      return @"UNIFIED_PLAN";
  }
}

@end
