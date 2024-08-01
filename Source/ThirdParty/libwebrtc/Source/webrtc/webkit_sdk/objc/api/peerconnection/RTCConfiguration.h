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

#import "RTCCertificate.h"
#import "RTCCryptoOptions.h"
#import "RTCMacros.h"

@class RTCIceServer;

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
typedef NS_ENUM(NSInteger, RTCRtcpMuxPolicy) { RTCRtcpMuxPolicyNegotiate, RTCRtcpMuxPolicyRequire };

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

/** Represents the chosen SDP semantics for the RTCPeerConnection. */
typedef NS_ENUM(NSInteger, RTCSdpSemantics) {
  RTCSdpSemanticsPlanB,
  RTCSdpSemanticsUnifiedPlan,
};

NS_ASSUME_NONNULL_BEGIN

RTC_OBJC_EXPORT
@interface RTCConfiguration : NSObject

/** An array of Ice Servers available to be used by ICE. */
@property(nonatomic, copy) NSArray<RTCIceServer *> *iceServers;

/** An RTCCertificate for 're' use. */
@property(nonatomic, nullable) RTCCertificate *certificate;

/** Which candidates the ICE agent is allowed to use. The W3C calls it
 * |iceTransportPolicy|, while in C++ it is called |type|. */
@property(nonatomic, assign) RTCIceTransportPolicy iceTransportPolicy;

/** The media-bundling policy to use when gathering ICE candidates. */
@property(nonatomic, assign) RTCBundlePolicy bundlePolicy;

/** The rtcp-mux policy to use when gathering ICE candidates. */
@property(nonatomic, assign) RTCRtcpMuxPolicy rtcpMuxPolicy;
@property(nonatomic, assign) RTCTcpCandidatePolicy tcpCandidatePolicy;
@property(nonatomic, assign) RTCCandidateNetworkPolicy candidateNetworkPolicy;
@property(nonatomic, assign) RTCContinualGatheringPolicy continualGatheringPolicy;

/** If set to YES, don't gather IPv6 ICE candidates.
 *  Default is NO.
 */
@property(nonatomic, assign) BOOL disableIPV6;

/** If set to YES, don't gather IPv6 ICE candidates on Wi-Fi.
 *  Only intended to be used on specific devices. Certain phones disable IPv6
 *  when the screen is turned off and it would be better to just disable the
 *  IPv6 ICE candidates on Wi-Fi in those cases.
 *  Default is NO.
 */
@property(nonatomic, assign) BOOL disableIPV6OnWiFi;

/** By default, the PeerConnection will use a limited number of IPv6 network
 *  interfaces, in order to avoid too many ICE candidate pairs being created
 *  and delaying ICE completion.
 *
 *  Can be set to INT_MAX to effectively disable the limit.
 */
@property(nonatomic, assign) int maxIPv6Networks;

/** Exclude link-local network interfaces
 *  from considertaion for gathering ICE candidates.
 *  Defaults to NO.
 */
@property(nonatomic, assign) BOOL disableLinkLocalNetworks;

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

/* This flag is only effective when |continualGatheringPolicy| is
 * RTCContinualGatheringPolicyGatherContinually.
 *
 * If YES, after the ICE transport type is changed such that new types of
 * ICE candidates are allowed by the new transport type, e.g. from
 * RTCIceTransportPolicyRelay to RTCIceTransportPolicyAll, candidates that
 * have been gathered by the ICE transport but not matching the previous
 * transport type and as a result not observed by PeerConnectionDelegateAdapter,
 * will be surfaced to the delegate.
 */
@property(nonatomic, assign) BOOL shouldSurfaceIceCandidatesOnIceTransportTypeChanged;

/** If set to non-nil, controls the minimal interval between consecutive ICE
 *  check packets.
 */
@property(nonatomic, copy, nullable) NSNumber *iceCheckMinInterval;

/** Configure the SDP semantics used by this PeerConnection. Note that the
 *  WebRTC 1.0 specification requires UnifiedPlan semantics. The
 *  RTCRtpTransceiver API is only available with UnifiedPlan semantics.
 *
 *  PlanB will cause RTCPeerConnection to create offers and answers with at
 *  most one audio and one video m= section with multiple RTCRtpSenders and
 *  RTCRtpReceivers specified as multiple a=ssrc lines within the section. This
 *  will also cause RTCPeerConnection to ignore all but the first m= section of
 *  the same media type.
 *
 *  UnifiedPlan will cause RTCPeerConnection to create offers and answers with
 *  multiple m= sections where each m= section maps to one RTCRtpSender and one
 *  RTCRtpReceiver (an RTCRtpTransceiver), either both audio or both video. This
 *  will also cause RTCPeerConnection to ignore all but the first a=ssrc lines
 *  that form a Plan B stream.
 *
 *  For users who wish to send multiple audio/video streams and need to stay
 *  interoperable with legacy WebRTC implementations or use legacy APIs,
 *  specify PlanB.
 *
 *  For all other users, specify UnifiedPlan.
 */
@property(nonatomic, assign) RTCSdpSemantics sdpSemantics;

/** Actively reset the SRTP parameters when the DTLS transports underneath are
 *  changed after offer/answer negotiation. This is only intended to be a
 *  workaround for crbug.com/835958
 */
@property(nonatomic, assign) BOOL activeResetSrtpParams;

/** If the remote side support mid-stream codec switches then allow encoder
 *  switching to be performed.
 */

@property(nonatomic, assign) BOOL allowCodecSwitching;

/**
 * If MediaTransportFactory is provided in PeerConnectionFactory, this flag informs PeerConnection
 * that it should use the MediaTransportInterface.
 */
@property(nonatomic, assign) BOOL useMediaTransport;

/**
 * If MediaTransportFactory is provided in PeerConnectionFactory, this flag informs PeerConnection
 * that it should use the MediaTransportInterface for data channels.
 */
@property(nonatomic, assign) BOOL useMediaTransportForDataChannels;

/**
 * Defines advanced optional cryptographic settings related to SRTP and
 * frame encryption for native WebRTC. Setting this will overwrite any
 * options set through the PeerConnectionFactory (which is deprecated).
 */
@property(nonatomic, nullable) RTCCryptoOptions *cryptoOptions;

/**
 * Time interval between audio RTCP reports.
 */
@property(nonatomic, assign) int rtcpAudioReportIntervalMs;

/**
 * Time interval between video RTCP reports.
 */
@property(nonatomic, assign) int rtcpVideoReportIntervalMs;

- (instancetype)init;

@end

NS_ASSUME_NONNULL_END
