/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_EXAMPLES_UNITYPLUGIN_SIMPLE_PEER_CONNECTION_H_
#define WEBRTC_EXAMPLES_UNITYPLUGIN_SIMPLE_PEER_CONNECTION_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "webrtc/api/datachannelinterface.h"
#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/examples/unityplugin/unity_plugin_apis.h"

class SimplePeerConnection : public webrtc::PeerConnectionObserver,
                             public webrtc::CreateSessionDescriptionObserver,
                             public webrtc::DataChannelObserver,
                             public webrtc::AudioTrackSinkInterface {
 public:
  SimplePeerConnection() {}
  ~SimplePeerConnection() {}

  bool InitializePeerConnection(bool is_receiver);
  void DeletePeerConnection();
  void AddStreams(bool audio_only);
  bool CreateDataChannel();
  bool CreateOffer();
  bool CreateAnswer();
  bool SendDataViaDataChannel(const std::string& data);
  void SetAudioControl(bool is_mute, bool is_record);

  // Register callback functions.
  void RegisterOnVideoFramReady(VIDEOFRAMEREADY_CALLBACK callback);
  void RegisterOnLocalDataChannelReady(LOCALDATACHANNELREADY_CALLBACK callback);
  void RegisterOnDataFromDataChannelReady(
      DATAFROMEDATECHANNELREADY_CALLBACK callback);
  void RegisterOnFailure(FAILURE_CALLBACK callback);
  void RegisterOnAudioBusReady(AUDIOBUSREADY_CALLBACK callback);
  void RegisterOnLocalSdpReadytoSend(LOCALSDPREADYTOSEND_CALLBACK callback);
  void RegisterOnIceCandiateReadytoSend(
      ICECANDIDATEREADYTOSEND_CALLBACK callback);
  bool ReceivedSdp(const char* sdp);
  bool ReceivedIceCandidate(const char* ice_candidate);

  bool SetHeadPosition(float x, float y, float z);
  bool SetHeadRotation(float rx, float ry, float rz, float rw);
  bool SetRemoteAudioPosition(float x, float y, float z);
  bool SetRemoteAudioRotation(float rx, float ry, float rz, float rw);

 protected:
  bool CreatePeerConnection(bool receiver);
  void CloseDataChannel();
  std::unique_ptr<cricket::VideoCapturer> OpenVideoCaptureDevice();
  void SetAudioControl();

  // PeerConnectionObserver implementation.
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override {}
  void OnAddStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  void OnRemoveStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {}
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;
  void OnRenegotiationNeeded() override {}
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override {}
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override {}
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void OnIceConnectionReceivingChange(bool receiving) override {}

  // CreateSessionDescriptionObserver implementation.
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
  void OnFailure(const std::string& error) override;

  // DataChannelObserver implementation.
  void OnStateChange() override;
  void OnMessage(const webrtc::DataBuffer& buffer) override;

  // AudioTrackSinkInterface implementation.
  void OnData(const void* audio_data,
              int bits_per_sample,
              int sample_rate,
              size_t number_of_channels,
              size_t number_of_frames) override;

  // Get remote audio tracks ssrcs.
  std::vector<uint32_t> GetRemoteAudioTrackSsrcs();

 private:
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;
  std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >
      active_streams_;

  webrtc::MediaStreamInterface* remote_stream_ = nullptr;

  VIDEOFRAMEREADY_CALLBACK OnVideoFrameReady = nullptr;
  LOCALDATACHANNELREADY_CALLBACK OnLocalDataChannelReady = nullptr;
  DATAFROMEDATECHANNELREADY_CALLBACK OnDataFromDataChannelReady = nullptr;
  FAILURE_CALLBACK OnFailureMessage = nullptr;
  AUDIOBUSREADY_CALLBACK OnAudioReady = nullptr;

  LOCALSDPREADYTOSEND_CALLBACK OnLocalSdpReady = nullptr;
  ICECANDIDATEREADYTOSEND_CALLBACK OnIceCandiateReady = nullptr;

  bool is_mute_audio_ = false;
  bool is_record_audio_ = false;

  // disallow copy-and-assign
  SimplePeerConnection(const SimplePeerConnection&) = delete;
  SimplePeerConnection& operator=(const SimplePeerConnection&) = delete;
};

#endif  // WEBRTC_EXAMPLES_UNITYPLUGIN_SIMPLE_PEER_CONNECTION_H_
