/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_PEERCONNECTIONTESTWRAPPER_H_
#define PC_TEST_PEERCONNECTIONTESTWRAPPER_H_

#include <memory>
#include <string>

#include "api/peerconnectioninterface.h"
#include "api/test/fakeconstraints.h"
#include "pc/test/fakeaudiocapturemodule.h"
#include "pc/test/fakevideotrackrenderer.h"
#include "rtc_base/sigslot.h"

class PeerConnectionTestWrapper
    : public webrtc::PeerConnectionObserver,
      public webrtc::CreateSessionDescriptionObserver,
      public sigslot::has_slots<> {
 public:
  static void Connect(PeerConnectionTestWrapper* caller,
                      PeerConnectionTestWrapper* callee);

  PeerConnectionTestWrapper(const std::string& name,
                            rtc::Thread* network_thread,
                            rtc::Thread* worker_thread);
  virtual ~PeerConnectionTestWrapper();

  bool CreatePc(
      const webrtc::MediaConstraintsInterface* constraints,
      const webrtc::PeerConnectionInterface::RTCConfiguration& config,
      rtc::scoped_refptr<webrtc::AudioEncoderFactory> audio_encoder_factory,
      rtc::scoped_refptr<webrtc::AudioDecoderFactory> audio_decoder_factory);

  webrtc::PeerConnectionInterface* pc() { return peer_connection_.get(); }

  rtc::scoped_refptr<webrtc::DataChannelInterface> CreateDataChannel(
      const std::string& label,
      const webrtc::DataChannelInit& init);

  // Implements PeerConnectionObserver.
  void OnSignalingChange(
     webrtc::PeerConnectionInterface::SignalingState new_state) override {}
  void OnAddStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  void OnRemoveStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {}
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;
  void OnRenegotiationNeeded() override {}
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override {}
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override {}
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;

  // Implements CreateSessionDescriptionObserver.
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
  void OnFailure(const std::string& error) override {}

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

#endif  // PC_TEST_PEERCONNECTIONTESTWRAPPER_H_
