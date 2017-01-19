/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_TEST_PEERCONNECTIONTESTWRAPPER_H_
#define WEBRTC_API_TEST_PEERCONNECTIONTESTWRAPPER_H_

#include <memory>

#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/api/test/fakeaudiocapturemodule.h"
#include "webrtc/api/test/fakeconstraints.h"
#include "webrtc/api/test/fakevideotrackrenderer.h"
#include "webrtc/base/sigslot.h"

class PeerConnectionTestWrapper
    : public webrtc::PeerConnectionObserver,
      public webrtc::CreateSessionDescriptionObserver,
      public sigslot::has_slots<> {
 public:
  // We need these using declarations because there are two versions of each of
  // the below methods and we only override one of them.
  // TODO(deadbeef): Remove once there's only one version of the methods.
  using PeerConnectionObserver::OnAddStream;
  using PeerConnectionObserver::OnRemoveStream;
  using PeerConnectionObserver::OnDataChannel;

  static void Connect(PeerConnectionTestWrapper* caller,
                      PeerConnectionTestWrapper* callee);

  PeerConnectionTestWrapper(const std::string& name,
                            rtc::Thread* network_thread,
                            rtc::Thread* worker_thread);
  virtual ~PeerConnectionTestWrapper();

  bool CreatePc(
      const webrtc::MediaConstraintsInterface* constraints,
      const webrtc::PeerConnectionInterface::RTCConfiguration& config);

  rtc::scoped_refptr<webrtc::DataChannelInterface> CreateDataChannel(
      const std::string& label,
      const webrtc::DataChannelInit& init);

  // Implements PeerConnectionObserver.
  virtual void OnSignalingChange(
     webrtc::PeerConnectionInterface::SignalingState new_state) {}
  virtual void OnStateChange(
      webrtc::PeerConnectionObserver::StateType state_changed) {}
  virtual void OnAddStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream);
  virtual void OnRemoveStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {}
  virtual void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);
  virtual void OnRenegotiationNeeded() {}
  virtual void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) {}
  virtual void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) {}
  virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
  virtual void OnIceComplete() {}

  // Implements CreateSessionDescriptionObserver.
  virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc);
  virtual void OnFailure(const std::string& error) {}

  void CreateOffer(const webrtc::MediaConstraintsInterface* constraints);
  void CreateAnswer(const webrtc::MediaConstraintsInterface* constraints);
  void ReceiveOfferSdp(const std::string& sdp);
  void ReceiveAnswerSdp(const std::string& sdp);
  void AddIceCandidate(const std::string& sdp_mid, int sdp_mline_index,
                       const std::string& candidate);
  void WaitForCallEstablished();
  void WaitForConnection();
  void WaitForAudio();
  void WaitForVideo();
  void GetAndAddUserMedia(
    bool audio, const webrtc::FakeConstraints& audio_constraints,
    bool video, const webrtc::FakeConstraints& video_constraints);

  // sigslots
  sigslot::signal1<std::string*> SignalOnIceCandidateCreated;
  sigslot::signal3<const std::string&,
                   int,
                   const std::string&> SignalOnIceCandidateReady;
  sigslot::signal1<std::string*> SignalOnSdpCreated;
  sigslot::signal1<const std::string&> SignalOnSdpReady;
  sigslot::signal1<webrtc::DataChannelInterface*> SignalOnDataChannel;

 private:
  void SetLocalDescription(const std::string& type, const std::string& sdp);
  void SetRemoteDescription(const std::string& type, const std::string& sdp);
  bool CheckForConnection();
  bool CheckForAudio();
  bool CheckForVideo();
  rtc::scoped_refptr<webrtc::MediaStreamInterface> GetUserMedia(
      bool audio, const webrtc::FakeConstraints& audio_constraints,
      bool video, const webrtc::FakeConstraints& video_constraints);

  std::string name_;
  rtc::Thread* const network_thread_;
  rtc::Thread* const worker_thread_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory_;
  rtc::scoped_refptr<FakeAudioCaptureModule> fake_audio_capture_module_;
  std::unique_ptr<webrtc::FakeVideoTrackRenderer> renderer_;
};

#endif  // WEBRTC_API_TEST_PEERCONNECTIONTESTWRAPPER_H_
