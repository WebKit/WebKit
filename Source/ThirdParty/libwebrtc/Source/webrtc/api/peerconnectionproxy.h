/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_PEERCONNECTIONPROXY_H_
#define WEBRTC_API_PEERCONNECTIONPROXY_H_

#include <string>
#include <vector>

#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/api/proxy.h"

namespace webrtc {

// TODO(deadbeef): Move this to .cc file and out of api/. What threads methods
// are called on is an implementation detail.
BEGIN_SIGNALING_PROXY_MAP(PeerConnection)
  PROXY_SIGNALING_THREAD_DESTRUCTOR()
  PROXY_METHOD0(rtc::scoped_refptr<StreamCollectionInterface>, local_streams)
  PROXY_METHOD0(rtc::scoped_refptr<StreamCollectionInterface>, remote_streams)
  PROXY_METHOD1(bool, AddStream, MediaStreamInterface*)
  PROXY_METHOD1(void, RemoveStream, MediaStreamInterface*)
  PROXY_METHOD2(rtc::scoped_refptr<RtpSenderInterface>,
                AddTrack,
                MediaStreamTrackInterface*,
                std::vector<MediaStreamInterface*>)
  PROXY_METHOD1(bool, RemoveTrack, RtpSenderInterface*)
  PROXY_METHOD1(rtc::scoped_refptr<DtmfSenderInterface>,
                CreateDtmfSender,
                AudioTrackInterface*)
  PROXY_METHOD2(rtc::scoped_refptr<RtpSenderInterface>,
                CreateSender,
                const std::string&,
                const std::string&)
  PROXY_CONSTMETHOD0(std::vector<rtc::scoped_refptr<RtpSenderInterface>>,
                     GetSenders)
  PROXY_CONSTMETHOD0(std::vector<rtc::scoped_refptr<RtpReceiverInterface>>,
                     GetReceivers)
  PROXY_METHOD3(bool,
                GetStats,
                StatsObserver*,
                MediaStreamTrackInterface*,
                StatsOutputLevel)
  PROXY_METHOD1(void, GetStats, RTCStatsCollectorCallback*)
  PROXY_METHOD2(rtc::scoped_refptr<DataChannelInterface>,
                CreateDataChannel,
                const std::string&,
                const DataChannelInit*)
  PROXY_CONSTMETHOD0(const SessionDescriptionInterface*, local_description)
  PROXY_CONSTMETHOD0(const SessionDescriptionInterface*, remote_description)
  PROXY_CONSTMETHOD0(const SessionDescriptionInterface*,
                     pending_local_description)
  PROXY_CONSTMETHOD0(const SessionDescriptionInterface*,
                     pending_remote_description)
  PROXY_CONSTMETHOD0(const SessionDescriptionInterface*,
                     current_local_description)
  PROXY_CONSTMETHOD0(const SessionDescriptionInterface*,
                     current_remote_description)
  PROXY_METHOD2(void,
                CreateOffer,
                CreateSessionDescriptionObserver*,
                const MediaConstraintsInterface*)
  PROXY_METHOD2(void,
                CreateAnswer,
                CreateSessionDescriptionObserver*,
                const MediaConstraintsInterface*)
  PROXY_METHOD2(void,
                CreateOffer,
                CreateSessionDescriptionObserver*,
                const RTCOfferAnswerOptions&)
  PROXY_METHOD2(void,
                CreateAnswer,
                CreateSessionDescriptionObserver*,
                const RTCOfferAnswerOptions&)
  PROXY_METHOD2(void,
                SetLocalDescription,
                SetSessionDescriptionObserver*,
                SessionDescriptionInterface*)
  PROXY_METHOD2(void,
                SetRemoteDescription,
                SetSessionDescriptionObserver*,
                SessionDescriptionInterface*)
  PROXY_METHOD0(PeerConnectionInterface::RTCConfiguration, GetConfiguration);
  PROXY_METHOD2(bool,
                SetConfiguration,
                const PeerConnectionInterface::RTCConfiguration&,
                RTCError*);
  PROXY_METHOD1(bool,
                SetConfiguration,
                const PeerConnectionInterface::RTCConfiguration&);
  PROXY_METHOD1(bool, AddIceCandidate, const IceCandidateInterface*)
  PROXY_METHOD1(bool,
                RemoveIceCandidates,
                const std::vector<cricket::Candidate>&);
  PROXY_METHOD1(void, RegisterUMAObserver, UMAObserver*)
  PROXY_METHOD1(RTCError, SetBitrate, const BitrateParameters&);
  PROXY_METHOD0(SignalingState, signaling_state)
  PROXY_METHOD0(IceConnectionState, ice_connection_state)
  PROXY_METHOD0(IceGatheringState, ice_gathering_state)
  PROXY_METHOD2(bool, StartRtcEventLog, rtc::PlatformFile, int64_t)
  PROXY_METHOD0(void, StopRtcEventLog)
  PROXY_METHOD0(void, Close)
END_PROXY_MAP()

}  // namespace webrtc

#endif  // WEBRTC_API_PEERCONNECTIONPROXY_H_
