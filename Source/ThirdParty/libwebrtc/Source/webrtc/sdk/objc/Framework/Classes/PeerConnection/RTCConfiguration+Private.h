/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCConfiguration.h"

#include "api/peerconnectioninterface.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTCConfiguration ()

+ (webrtc::PeerConnectionInterface::IceTransportsType)
    nativeTransportsTypeForTransportPolicy:(RTCIceTransportPolicy)policy;

+ (RTCIceTransportPolicy)transportPolicyForTransportsType:
    (webrtc::PeerConnectionInterface::IceTransportsType)nativeType;

+ (NSString *)stringForTransportPolicy:(RTCIceTransportPolicy)policy;

+ (webrtc::PeerConnectionInterface::BundlePolicy)nativeBundlePolicyForPolicy:
    (RTCBundlePolicy)policy;

+ (RTCBundlePolicy)bundlePolicyForNativePolicy:
    (webrtc::PeerConnectionInterface::BundlePolicy)nativePolicy;

+ (NSString *)stringForBundlePolicy:(RTCBundlePolicy)policy;

+ (webrtc::PeerConnectionInterface::RtcpMuxPolicy)nativeRtcpMuxPolicyForPolicy:
    (RTCRtcpMuxPolicy)policy;

+ (RTCRtcpMuxPolicy)rtcpMuxPolicyForNativePolicy:
    (webrtc::PeerConnectionInterface::RtcpMuxPolicy)nativePolicy;

+ (NSString *)stringForRtcpMuxPolicy:(RTCRtcpMuxPolicy)policy;

+ (webrtc::PeerConnectionInterface::TcpCandidatePolicy)
    nativeTcpCandidatePolicyForPolicy:(RTCTcpCandidatePolicy)policy;

+ (RTCTcpCandidatePolicy)tcpCandidatePolicyForNativePolicy:
    (webrtc::PeerConnectionInterface::TcpCandidatePolicy)nativePolicy;

+ (NSString *)stringForTcpCandidatePolicy:(RTCTcpCandidatePolicy)policy;

+ (webrtc::PeerConnectionInterface::CandidateNetworkPolicy)
    nativeCandidateNetworkPolicyForPolicy:(RTCCandidateNetworkPolicy)policy;

+ (RTCCandidateNetworkPolicy)candidateNetworkPolicyForNativePolicy:
    (webrtc::PeerConnectionInterface::CandidateNetworkPolicy)nativePolicy;

+ (NSString *)stringForCandidateNetworkPolicy:(RTCCandidateNetworkPolicy)policy;

+ (rtc::KeyType)nativeEncryptionKeyTypeForKeyType:(RTCEncryptionKeyType)keyType;

/**
 * RTCConfiguration struct representation of this RTCConfiguration. This is
 * needed to pass to the underlying C++ APIs.
 */
- (webrtc::PeerConnectionInterface::RTCConfiguration *)
    createNativeConfiguration;

- (instancetype)initWithNativeConfiguration:
    (const webrtc::PeerConnectionInterface::RTCConfiguration &)config NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
