/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCPeerConnection.h"

#include "webrtc/api/peerconnectioninterface.h"

NS_ASSUME_NONNULL_BEGIN

namespace webrtc {

/**
 * These objects are created by RTCPeerConnectionFactory to wrap an
 * id<RTCPeerConnectionDelegate> and call methods on that interface.
 */
class PeerConnectionDelegateAdapter : public PeerConnectionObserver {

 public:
  PeerConnectionDelegateAdapter(RTCPeerConnection *peerConnection);
  virtual ~PeerConnectionDelegateAdapter();

  void OnSignalingChange(
      PeerConnectionInterface::SignalingState new_state) override;

  void OnAddStream(rtc::scoped_refptr<MediaStreamInterface> stream) override;

  void OnRemoveStream(rtc::scoped_refptr<MediaStreamInterface> stream) override;

  void OnDataChannel(
      rtc::scoped_refptr<DataChannelInterface> data_channel) override;

  void OnRenegotiationNeeded() override;

  void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) override;

  void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) override;

  void OnIceCandidate(const IceCandidateInterface *candidate) override;

  void OnIceCandidatesRemoved(
      const std::vector<cricket::Candidate>& candidates) override;

 private:
  __weak RTCPeerConnection *peer_connection_;
};

} // namespace webrtc


@interface RTCPeerConnection ()

/** The native PeerConnectionInterface created during construction. */
@property(nonatomic, readonly)
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> nativePeerConnection;

/** Initialize an RTCPeerConnection with a configuration, constraints, and
 *  delegate.
 */
- (instancetype)initWithFactory:
    (RTCPeerConnectionFactory *)factory
                  configuration:
    (RTCConfiguration *)configuration
                    constraints:
    (RTCMediaConstraints *)constraints
                       delegate:
    (nullable id<RTCPeerConnectionDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

+ (webrtc::PeerConnectionInterface::SignalingState)nativeSignalingStateForState:
    (RTCSignalingState)state;

+ (RTCSignalingState)signalingStateForNativeState:
    (webrtc::PeerConnectionInterface::SignalingState)nativeState;

+ (NSString *)stringForSignalingState:(RTCSignalingState)state;

+ (webrtc::PeerConnectionInterface::IceConnectionState)
    nativeIceConnectionStateForState:(RTCIceConnectionState)state;

+ (RTCIceConnectionState)iceConnectionStateForNativeState:
    (webrtc::PeerConnectionInterface::IceConnectionState)nativeState;

+ (NSString *)stringForIceConnectionState:(RTCIceConnectionState)state;

+ (webrtc::PeerConnectionInterface::IceGatheringState)
    nativeIceGatheringStateForState:(RTCIceGatheringState)state;

+ (RTCIceGatheringState)iceGatheringStateForNativeState:
    (webrtc::PeerConnectionInterface::IceGatheringState)nativeState;

+ (NSString *)stringForIceGatheringState:(RTCIceGatheringState)state;

+ (webrtc::PeerConnectionInterface::StatsOutputLevel)
    nativeStatsOutputLevelForLevel:(RTCStatsOutputLevel)level;

@end

NS_ASSUME_NONNULL_END
