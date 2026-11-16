/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_TOOLS_DATA_CHANNEL_BENCHMARK_PEER_CONNECTION_CLIENT_H_
#define RTC_TOOLS_DATA_CHANNEL_BENCHMARK_PEER_CONNECTION_CLIENT_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "api/data_channel_interface.h"
#include "api/field_trials_view.h"
#include "api/jsep.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "rtc_tools/data_channel_benchmark/signaling_interface.h"

namespace webrtc {

// Handles all the details for creating a PeerConnection and negotiation using a
// SignalingInterface object.
class PeerConnectionClient : public PeerConnectionObserver {
 public:
  explicit PeerConnectionClient(PeerConnectionFactoryInterface* factory,
                                SignalingInterface* signaling);

  ~PeerConnectionClient() override;

  PeerConnectionClient(const PeerConnectionClient&) = delete;
  PeerConnectionClient& operator=(const PeerConnectionClient&) = delete;

  // Set the local description and send offer using the SignalingInterface,
  // initiating the negotiation process.
  bool StartPeerConnection();

  // Whether the peer connection is connected to the remote peer.
  bool IsConnected();

  // Disconnect from the call.
  void Disconnect();

  scoped_refptr<PeerConnectionInterface> peerConnection() {
    return peer_connection_;
  }

  // Set a callback to run when a DataChannel is created by the remote peer.
  void SetOnDataChannel(
      std::function<void(scoped_refptr<DataChannelInterface>)> callback);

  std::vector<scoped_refptr<DataChannelInterface>>& dataChannels() {
    return data_channels_;
  }

  // Creates a default PeerConnectionFactory object.
  static scoped_refptr<PeerConnectionFactoryInterface> CreateDefaultFactory(
      Thread* signaling_thread,
      std::unique_ptr<FieldTrialsView> field_trials);

 private:
  void AddIceCandidate(std::unique_ptr<IceCandidate> candidate);
  bool SetRemoteDescription(std::unique_ptr<SessionDescriptionInterface> desc);

  // Initialize the PeerConnection with a given PeerConnectionFactory.
  bool InitializePeerConnection(PeerConnectionFactoryInterface* factory);
  void DeletePeerConnection();

  // PeerConnectionObserver implementation.
  void OnSignalingChange(
      PeerConnectionInterface::SignalingState new_state) override {
    RTC_LOG(LS_INFO) << __FUNCTION__ << " new state: " << new_state;
  }
  void OnDataChannel(scoped_refptr<DataChannelInterface> channel) override;
  void OnNegotiationNeededEvent(uint32_t event_id) override;
  void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) override;
  void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) override;
  void OnIceCandidate(const IceCandidate* candidate) override;
  void OnIceConnectionReceivingChange(bool receiving) override {
    RTC_LOG(LS_INFO) << __FUNCTION__ << " receiving? " << receiving;
  }
  void OnIceCandidateRemoved(const IceCandidate* candidate) override {}

  scoped_refptr<PeerConnectionInterface> peer_connection_;
  std::function<void(scoped_refptr<DataChannelInterface>)>
      on_data_channel_callback_;
  std::vector<scoped_refptr<DataChannelInterface>> data_channels_;
  SignalingInterface* signaling_;
};

}  // namespace webrtc

#endif  // RTC_TOOLS_DATA_CHANNEL_BENCHMARK_PEER_CONNECTION_CLIENT_H_
