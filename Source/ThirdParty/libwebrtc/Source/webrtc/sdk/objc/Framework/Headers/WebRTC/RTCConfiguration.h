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

#import <WebRTC/RTCMacros.h>

@class RTCIceServer;
@class RTCIntervalRange;

/**
 * Represents the ice transport policy. This exposes the same states in C++,
 * which include one more state than what exists in the W3C spec.
 */
typedef NS_ENUM(NSInteger, RTCIceTransportPolicy) {
  RTCIceTransportPolicyNone,
  RTCIceTransportPolicyRelay,
  RTCIceTransportPolicyNoHost,
  RTCIceTransportPolicyAll
};

/** Represents the bundle policy. */
typedef NS_ENUM(NSInteger, RTCBundlePolicy) {
  RTCBundlePolicyBalanced,
  RTCBundlePolicyMaxCompat,
  RTCBundlePolicyMaxBundle
};

/** Represents the rtcp mux policy. */
typedef NS_ENUM(NSInteger, RTCRtcpMuxPolicy) {
  RTCRtcpMuxPolicyNegotiate,
  RTCRtcpMuxPolicyRequire
};

/** Represents the tcp candidate policy. */
typedef NS_ENUM(NSInteger, RTCTcpCandidatePolicy) {
  RTCTcpCandidatePolicyEnabled,
  RTCTcpCandidatePolicyDisabled
};

/** Represents the candidate network policy. */
typedef NS_ENUM(NSInteger, RTCCandidateNetworkPolicy) {
  RTCCandidateNetworkPolicyAll,
  RTCCandidateNetworkPolicyLowCost
};

/** Represents the continual gathering policy. */
typedef NS_ENUM(NSInteger, RTCContinualGatheringPolicy) {
  RTCContinualGatheringPolicyGatherOnce,
  RTCContinualGatheringPolicyGatherContinually
};

/** Represents the encryption key type. */
typedef NS_ENUM(NSInteger, RTCEncryptionKeyType) {
  RTCEncryptionKeyTypeRSA,
  RTCEncryptionKeyTypeECDSA,
};

NS_ASSUME_NONNULL_BEGIN

RTC_EXPORT
@interface RTCConfiguration : NSObject

/** An array of Ice Servers available to be used by ICE. */
@property(nonatomic, copy) NSArray<RTCIceServer *> *iceServers;

/** Which candidates the ICE agent is allowed to use. The W3C calls it
 * |iceTransportPolicy|, while in C++ it is called |type|. */
@property(nonatomic, assign) RTCIceTransportPolicy iceTransportPolicy;

/** The media-bundling policy to use when gathering ICE candidates. */
@property(nonatomic, assign) RTCBundlePolicy bundlePolicy;

/** The rtcp-mux policy to use when gathering ICE candidates. */
@property(nonatomic, assign) RTCRtcpMuxPolicy rtcpMuxPolicy;
@property(nonatomic, assign) RTCTcpCandidatePolicy tcpCandidatePolicy;
@property(nonatomic, assign) RTCCandidateNetworkPolicy candidateNetworkPolicy;
@property(nonatomic, assign)
    RTCContinualGatheringPolicy continualGatheringPolicy;

/** By default, the PeerConnection will use a limited number of IPv6 network
 *  interfaces, in order to avoid too many ICE candidate pairs being created
 *  and delaying ICE completion.
 *
 *  Can be set to INT_MAX to effectively disable the limit.
 */
@property(nonatomic, assign) int maxIPv6Networks;

@property(nonatomic, assign) int audioJitterBufferMaxPackets;
@property(nonatomic, assign) BOOL audioJitterBufferFastAccelerate;
@property(nonatomic, assign) int iceConnectionReceivingTimeout;
@property(nonatomic, assign) int iceBackupCandidatePairPingInterval;

/** Key type used to generate SSL identity. Default is ECDSA. */
@property(nonatomic, assign) RTCEncryptionKeyType keyType;

/** ICE candidate pool size as defined in JSEP. Default is 0. */
@property(nonatomic, assign) int iceCandidatePoolSize;

/** Prune turn ports on the same network to the same turn server.
 *  Default is NO.
 */
@property(nonatomic, assign) BOOL shouldPruneTurnPorts;

/** If set to YES, this means the ICE transport should presume TURN-to-TURN
 *  candidate pairs will succeed, even before a binding response is received.
 */
@property(nonatomic, assign) BOOL shouldPresumeWritableWhenFullyRelayed;

/** If set to non-nil, controls the minimal interval between consecutive ICE
 *  check packets.
 */
@property(nonatomic, copy, nullable) NSNumber *iceCheckMinInterval;

/** ICE Periodic Regathering
 *  If set, WebRTC will periodically create and propose candidates without
 *  starting a new ICE generation. The regathering happens continuously with
 *  interval specified in milliseconds by the uniform distribution [a, b].
 */
@property(nonatomic, strong, nullable) RTCIntervalRange *iceRegatherIntervalRange;

- (instancetype)init;

@end

NS_ASSUME_NONNULL_END
