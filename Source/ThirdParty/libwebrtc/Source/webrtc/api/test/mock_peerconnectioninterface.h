/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_MOCK_PEERCONNECTIONINTERFACE_H_
#define API_TEST_MOCK_PEERCONNECTIONINTERFACE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/peerconnectioninterface.h"
#include "test/gmock.h"

namespace webrtc {

class MockPeerConnectionInterface
    : public rtc::RefCountedObject<webrtc::PeerConnectionInterface> {
 public:
  // PeerConnectionInterface
  MOCK_METHOD0(local_streams, rtc::scoped_refptr<StreamCollectionInterface>());
  MOCK_METHOD0(remote_streams, rtc::scoped_refptr<StreamCollectionInterface>());
  MOCK_METHOD1(AddStream, bool(MediaStreamInterface*));
  MOCK_METHOD1(RemoveStream, void(MediaStreamInterface*));
  MOCK_METHOD2(AddTrack,
               RTCErrorOr<rtc::scoped_refptr<RtpSenderInterface>>(
                   rtc::scoped_refptr<MediaStreamTrackInterface>,
                   const std::vector<std::string>&));
  MOCK_METHOD2(AddTrack,
               rtc::scoped_refptr<RtpSenderInterface>(
                   MediaStreamTrackInterface*,
                   std::vector<MediaStreamInterface*>));
  MOCK_METHOD1(RemoveTrack, bool(RtpSenderInterface*));
  MOCK_METHOD1(AddTransceiver,
               RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>(
                   rtc::scoped_refptr<MediaStreamTrackInterface>));
  MOCK_METHOD2(AddTransceiver,
               RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>(
                   rtc::scoped_refptr<MediaStreamTrackInterface>,
                   const RtpTransceiverInit&));
  MOCK_METHOD1(AddTransceiver,
               RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>(
                   cricket::MediaType));
  MOCK_METHOD2(AddTransceiver,
               RTCErrorOr<rtc::scoped_refptr<RtpTransceiverInterface>>(
                   cricket::MediaType,
                   const RtpTransceiverInit&));
  MOCK_METHOD2(CreateSender,
               rtc::scoped_refptr<RtpSenderInterface>(const std::string&,
                                                      const std::string&));
  MOCK_CONST_METHOD0(GetSenders,
                     std::vector<rtc::scoped_refptr<RtpSenderInterface>>());
  MOCK_CONST_METHOD0(GetReceivers,
                     std::vector<rtc::scoped_refptr<RtpReceiverInterface>>());
  MOCK_CONST_METHOD0(
      GetTransceivers,
      std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>());
  MOCK_METHOD3(GetStats,
               bool(StatsObserver*,
                    MediaStreamTrackInterface*,
                    StatsOutputLevel));
  MOCK_METHOD1(GetStats, void(RTCStatsCollectorCallback*));
  MOCK_METHOD2(GetStats,
               void(rtc::scoped_refptr<RtpSenderInterface>,
                    rtc::scoped_refptr<RTCStatsCollectorCallback>));
  MOCK_METHOD2(GetStats,
               void(rtc::scoped_refptr<RtpReceiverInterface>,
                    rtc::scoped_refptr<RTCStatsCollectorCallback>));
  MOCK_METHOD0(ClearStatsCache, void());
  MOCK_METHOD2(
      CreateDataChannel,
      rtc::scoped_refptr<DataChannelInterface>(const std::string&,
                                               const DataChannelInit*));
  MOCK_CONST_METHOD0(local_description, const SessionDescriptionInterface*());
  MOCK_CONST_METHOD0(remote_description, const SessionDescriptionInterface*());
  MOCK_CONST_METHOD0(current_local_description,
                     const SessionDescriptionInterface*());
  MOCK_CONST_METHOD0(current_remote_description,
                     const SessionDescriptionInterface*());
  MOCK_CONST_METHOD0(pending_local_description,
                     const SessionDescriptionInterface*());
  MOCK_CONST_METHOD0(pending_remote_description,
                     const SessionDescriptionInterface*());
  MOCK_METHOD2(CreateOffer,
               void(CreateSessionDescriptionObserver*,
                    const MediaConstraintsInterface*));
  MOCK_METHOD2(CreateOffer,
               void(CreateSessionDescriptionObserver*,
                    const RTCOfferAnswerOptions&));
  MOCK_METHOD2(CreateAnswer,
               void(CreateSessionDescriptionObserver*,
                    const RTCOfferAnswerOptions&));
  MOCK_METHOD2(CreateAnswer,
               void(CreateSessionDescriptionObserver*,
                    const MediaConstraintsInterface*));
  MOCK_METHOD2(SetLocalDescription,
               void(SetSessionDescriptionObserver*,
                    SessionDescriptionInterface*));
  MOCK_METHOD2(SetRemoteDescription,
               void(SetSessionDescriptionObserver*,
                    SessionDescriptionInterface*));
  MOCK_METHOD2(SetRemoteDescription,
               void(std::unique_ptr<SessionDescriptionInterface>,
                    rtc::scoped_refptr<SetRemoteDescriptionObserverInterface>));
  MOCK_METHOD0(GetConfiguration, PeerConnectionInterface::RTCConfiguration());
  MOCK_METHOD2(SetConfiguration,
               bool(const PeerConnectionInterface::RTCConfiguration&,
                    RTCError*));
  MOCK_METHOD1(SetConfiguration,
               bool(const PeerConnectionInterface::RTCConfiguration&));
  MOCK_METHOD1(AddIceCandidate, bool(const IceCandidateInterface*));
  MOCK_METHOD1(RemoveIceCandidates,
               bool(const std::vector<cricket::Candidate>&));
  MOCK_METHOD1(SetBitrate, RTCError(const BitrateSettings&));
  MOCK_METHOD1(SetBitrate, RTCError(const BitrateParameters&));
  MOCK_METHOD1(SetBitrateAllocationStrategy,
               void(std::unique_ptr<rtc::BitrateAllocationStrategy>));
  MOCK_METHOD1(SetAudioPlayout, void(bool));
  MOCK_METHOD1(SetAudioRecording, void(bool));
  MOCK_METHOD0(signaling_state, SignalingState());
  MOCK_METHOD0(ice_connection_state, IceConnectionState());
  MOCK_METHOD0(ice_gathering_state, IceGatheringState());
  MOCK_METHOD2(StartRtcEventLog, bool(rtc::PlatformFile, int64_t));
  MOCK_METHOD2(StartRtcEventLog,
               bool(std::unique_ptr<RtcEventLogOutput>, int64_t));
  MOCK_METHOD0(StopRtcEventLog, void());
  MOCK_METHOD0(Close, void());
};

}  // namespace webrtc

#endif  // API_TEST_MOCK_PEERCONNECTIONINTERFACE_H_
