/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_TEST_PEER_H_
#define TEST_PC_E2E_TEST_PEER_H_

#include <memory>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/types/variant.h"
#include "api/test/frame_generator_interface.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "pc/peer_connection_wrapper.h"
#include "test/pc/e2e/peer_configurer.h"
#include "test/pc/e2e/peer_connection_quality_test_params.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// Describes a single participant in the call.
class TestPeer final {
 public:
  Params* params() const { return params_.get(); }
  PeerConfigurerImpl::VideoSource ReleaseVideoSource(size_t i) {
    return std::move(video_sources_[i]);
  }

  PeerConnectionFactoryInterface* pc_factory() {
    return wrapper_->pc_factory();
  }
  PeerConnectionInterface* pc() { return wrapper_->pc(); }
  MockPeerConnectionObserver* observer() { return wrapper_->observer(); }

  std::unique_ptr<SessionDescriptionInterface> CreateOffer() {
    return wrapper_->CreateOffer();
  }

  std::unique_ptr<SessionDescriptionInterface> CreateAnswer() {
    return wrapper_->CreateAnswer();
  }

  bool SetLocalDescription(std::unique_ptr<SessionDescriptionInterface> desc,
                           std::string* error_out = nullptr) {
    return wrapper_->SetLocalDescription(std::move(desc), error_out);
  }

  bool SetRemoteDescription(std::unique_ptr<SessionDescriptionInterface> desc,
                            std::string* error_out = nullptr) {
    return wrapper_->SetRemoteDescription(std::move(desc), error_out);
  }

  rtc::scoped_refptr<RtpTransceiverInterface> AddTransceiver(
      cricket::MediaType media_type,
      const RtpTransceiverInit& init) {
    return wrapper_->AddTransceiver(media_type, init);
  }

  rtc::scoped_refptr<RtpSenderInterface> AddTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids = {}) {
    return wrapper_->AddTrack(track, stream_ids);
  }

  rtc::scoped_refptr<DataChannelInterface> CreateDataChannel(
      const std::string& label) {
    return wrapper_->CreateDataChannel(label);
  }

  PeerConnectionInterface::SignalingState signaling_state() {
    return wrapper_->signaling_state();
  }

  bool IsIceGatheringDone() { return wrapper_->IsIceGatheringDone(); }

  bool IsIceConnected() { return wrapper_->IsIceConnected(); }

  rtc::scoped_refptr<const RTCStatsReport> GetStats() {
    return wrapper_->GetStats();
  }

  void DetachAecDump() {
    if (audio_processing_) {
      audio_processing_->DetachAecDump();
    }
  }

  // Adds provided |candidates| to the owned peer connection.
  bool AddIceCandidates(
      std::vector<std::unique_ptr<IceCandidateInterface>> candidates);

 protected:
  friend class TestPeerFactory;
  TestPeer(rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory,
           rtc::scoped_refptr<PeerConnectionInterface> pc,
           std::unique_ptr<MockPeerConnectionObserver> observer,
           std::unique_ptr<Params> params,
           std::vector<PeerConfigurerImpl::VideoSource> video_sources,
           rtc::scoped_refptr<AudioProcessing> audio_processing,
           std::unique_ptr<rtc::Thread> worker_thread);

 private:
  // Keeps ownership of worker thread. It has to be destroyed after |wrapper_|.
  std::unique_ptr<rtc::Thread> worker_thread_;
  std::unique_ptr<PeerConnectionWrapper> wrapper_;
  std::unique_ptr<Params> params_;
  std::vector<PeerConfigurerImpl::VideoSource> video_sources_;
  rtc::scoped_refptr<AudioProcessing> audio_processing_;

  std::vector<std::unique_ptr<IceCandidateInterface>> remote_ice_candidates_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_TEST_PEER_H_
