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

@class RTCConfiguration;
@class RTCDataChannel;
@class RTCDataChannelConfiguration;
@class RTCIceCandidate;
@class RTCMediaConstraints;
@class RTCMediaStream;
@class RTCMediaStreamTrack;
@class RTCPeerConnectionFactory;
@class RTCRtpReceiver;
@class RTCRtpSender;
@class RTCSessionDescription;
@class RTCLegacyStatsReport;

NS_ASSUME_NONNULL_BEGIN

extern NSString * const kRTCPeerConnectionErrorDomain;
extern int const kRTCSessionDescriptionErrorCode;

/** Represents the signaling state of the peer connection. */
typedef NS_ENUM(NSInteger, RTCSignalingState) {
  RTCSignalingStateStable,
  RTCSignalingStateHaveLocalOffer,
  RTCSignalingStateHaveLocalPrAnswer,
  RTCSignalingStateHaveRemoteOffer,
  RTCSignalingStateHaveRemotePrAnswer,
  // Not an actual state, represents the total number of states.
  RTCSignalingStateClosed,
};

/** Represents the ice connection state of the peer connection. */
typedef NS_ENUM(NSInteger, RTCIceConnectionState) {
  RTCIceConnectionStateNew,
  RTCIceConnectionStateChecking,
  RTCIceConnectionStateConnected,
  RTCIceConnectionStateCompleted,
  RTCIceConnectionStateFailed,
  RTCIceConnectionStateDisconnected,
  RTCIceConnectionStateClosed,
  RTCIceConnectionStateCount,
};

/** Represents the ice gathering state of the peer connection. */
typedef NS_ENUM(NSInteger, RTCIceGatheringState) {
  RTCIceGatheringStateNew,
  RTCIceGatheringStateGathering,
  RTCIceGatheringStateComplete,
};

/** Represents the stats output level. */
typedef NS_ENUM(NSInteger, RTCStatsOutputLevel) {
  RTCStatsOutputLevelStandard,
  RTCStatsOutputLevelDebug,
};

@class RTCPeerConnection;

RTC_EXPORT
@protocol RTCPeerConnectionDelegate <NSObject>

/** Called when the SignalingState changed. */
- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didChangeSignalingState:(RTCSignalingState)stateChanged;

/** Called when media is received on a new stream from remote peer. */
- (void)peerConnection:(RTCPeerConnection *)peerConnection
          didAddStream:(RTCMediaStream *)stream;

/** Called when a remote peer closes a stream. */
- (void)peerConnection:(RTCPeerConnection *)peerConnection
       didRemoveStream:(RTCMediaStream *)stream;

/** Called when negotiation is needed, for example ICE has restarted. */
- (void)peerConnectionShouldNegotiate:(RTCPeerConnection *)peerConnection;

/** Called any time the IceConnectionState changes. */
- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didChangeIceConnectionState:(RTCIceConnectionState)newState;

/** Called any time the IceGatheringState changes. */
- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didChangeIceGatheringState:(RTCIceGatheringState)newState;

/** New ice candidate has been found. */
- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didGenerateIceCandidate:(RTCIceCandidate *)candidate;

/** Called when a group of local Ice candidates have been removed. */
- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didRemoveIceCandidates:(NSArray<RTCIceCandidate *> *)candidates;

/** New data channel has been opened. */
- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didOpenDataChannel:(RTCDataChannel *)dataChannel;

@end

RTC_EXPORT
@interface RTCPeerConnection : NSObject

/** The object that will be notifed about events such as state changes and
 *  streams being added or removed.
 */
@property(nonatomic, weak, nullable) id<RTCPeerConnectionDelegate> delegate;
@property(nonatomic, readonly) NSArray<RTCMediaStream *> *localStreams;
@property(nonatomic, readonly, nullable)
    RTCSessionDescription *localDescription;
@property(nonatomic, readonly, nullable)
    RTCSessionDescription *remoteDescription;
@property(nonatomic, readonly) RTCSignalingState signalingState;
@property(nonatomic, readonly) RTCIceConnectionState iceConnectionState;
@property(nonatomic, readonly) RTCIceGatheringState iceGatheringState;
@property(nonatomic, readonly, copy) RTCConfiguration *configuration;

/** Gets all RTCRtpSenders associated with this peer connection.
 *  Note: reading this property returns different instances of RTCRtpSender.
 *  Use isEqual: instead of == to compare RTCRtpSender instances.
 */
@property(nonatomic, readonly) NSArray<RTCRtpSender *> *senders;

/** Gets all RTCRtpReceivers associated with this peer connection.
 *  Note: reading this property returns different instances of RTCRtpReceiver.
 *  Use isEqual: instead of == to compare RTCRtpReceiver instances.
 */
@property(nonatomic, readonly) NSArray<RTCRtpReceiver *> *receivers;

- (instancetype)init NS_UNAVAILABLE;

/** Sets the PeerConnection's global configuration to |configuration|.
 *  Any changes to STUN/TURN servers or ICE candidate policy will affect the
 *  next gathering phase, and cause the next call to createOffer to generate
 *  new ICE credentials. Note that the BUNDLE and RTCP-multiplexing policies
 *  cannot be changed with this method.
 */
- (BOOL)setConfiguration:(RTCConfiguration *)configuration;

/** Terminate all media and close the transport. */
- (void)close;

/** Provide a remote candidate to the ICE Agent. */
- (void)addIceCandidate:(RTCIceCandidate *)candidate;

/** Remove a group of remote candidates from the ICE Agent. */
- (void)removeIceCandidates:(NSArray<RTCIceCandidate *> *)candidates;

/** Add a new media stream to be sent on this peer connection. */
- (void)addStream:(RTCMediaStream *)stream;

/** Remove the given media stream from this peer connection. */
- (void)removeStream:(RTCMediaStream *)stream;

/** Generate an SDP offer. */
- (void)offerForConstraints:(RTCMediaConstraints *)constraints
          completionHandler:(nullable void (^)
    (RTCSessionDescription * _Nullable sdp,
     NSError * _Nullable error))completionHandler;

/** Generate an SDP answer. */
- (void)answerForConstraints:(RTCMediaConstraints *)constraints
           completionHandler:(nullable void (^)
    (RTCSessionDescription * _Nullable sdp,
     NSError * _Nullable error))completionHandler;

/** Apply the supplied RTCSessionDescription as the local description. */
- (void)setLocalDescription:(RTCSessionDescription *)sdp
          completionHandler:
    (nullable void (^)(NSError * _Nullable error))completionHandler;

/** Apply the supplied RTCSessionDescription as the remote description. */
- (void)setRemoteDescription:(RTCSessionDescription *)sdp
           completionHandler:
    (nullable void (^)(NSError * _Nullable error))completionHandler;

/** Limits the bandwidth allocated for all RTP streams sent by this
 *  PeerConnection. Nil parameters will be unchanged. Setting
 * |currentBitrateBps| will force the available bitrate estimate to the given
 *  value. Returns YES if the parameters were successfully updated.
 */
- (BOOL)setBweMinBitrateBps:(nullable NSNumber *)minBitrateBps
          currentBitrateBps:(nullable NSNumber *)currentBitrateBps
              maxBitrateBps:(nullable NSNumber *)maxBitrateBps;

/** Start or stop recording an Rtc EventLog. */
- (BOOL)startRtcEventLogWithFilePath:(NSString *)filePath
                      maxSizeInBytes:(int64_t)maxSizeInBytes;
- (void)stopRtcEventLog;

@end

@interface RTCPeerConnection (Media)

/** Create an RTCRtpSender with the specified kind and media stream ID.
 *  See RTCMediaStreamTrack.h for available kinds.
 */
- (RTCRtpSender *)senderWithKind:(NSString *)kind streamId:(NSString *)streamId;

@end

@interface RTCPeerConnection (DataChannel)

/** Create a new data channel with the given label and configuration. */
- (RTCDataChannel *)dataChannelForLabel:(NSString *)label
    configuration:(RTCDataChannelConfiguration *)configuration;

@end

@interface RTCPeerConnection (Stats)

/** Gather stats for the given RTCMediaStreamTrack. If |mediaStreamTrack| is nil
 *  statistics are gathered for all tracks.
 */
- (void)statsForTrack:
    (nullable RTCMediaStreamTrack *)mediaStreamTrack
     statsOutputLevel:(RTCStatsOutputLevel)statsOutputLevel
    completionHandler:
    (nullable void (^)(NSArray<RTCLegacyStatsReport *> *stats))completionHandler;

@end

NS_ASSUME_NONNULL_END
