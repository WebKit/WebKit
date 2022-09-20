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
#include "absl/strings/string_view.h"
#include "api/function_view.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/set_remote_description_observer_interface.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/test/frame_generator_interface.h"
#include "api/test/peerconnection_quality_test_fixture.h"
#include "pc/peer_connection_wrapper.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"
#include "test/pc/e2e/peer_configurer.h"
#include "test/pc/e2e/peer_connection_quality_test_params.h"
#include "test/pc/e2e/stats_provider.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// Describes a single participant in the call.
class TestPeer final : public StatsProvider {
 public:
  ~TestPeer() override = default;

  const Params& params() const { return params_; }

  ConfigurableParams configurable_params() const;
  void AddVideoConfig(PeerConnectionE2EQualityTestFixture::VideoConfig config);
  // Removes video config with specified name. Crashes if the config with
  // specified name isn't found.
  void RemoveVideoConfig(absl::string_view stream_label);
  void SetVideoSubscription(
      PeerConnectionE2EQualityTestFixture::VideoSubscription subscription);

  void GetStats(RTCStatsCollectorCallback* callback) override;

  PeerConfigurerImpl::VideoSource ReleaseVideoSource(size_t i) {
    RTC_CHECK(wrapper_) << "TestPeer is already closed";
    return std::move(video_sources_[i]);
  }

  PeerConnectionFactoryInterface* pc_factory() {
    RTC_CHECK(wrapper_) << "TestPeer is already closed";
    return wrapper_->pc_factory();
  }
  PeerConnectionInterface* pc() {
    RTC_CHECK(wrapper_) << "TestPeer is already closed";
    return wrapper_->pc();
  }
  MockPeerConnectionObserver* observer() {
    RTC_CHECK(wrapper_) << "TestPeer is already closed";
    return wrapper_->observer();
  }

  // Tell underlying `PeerConnection` to create an Offer.
  // `observer` will be invoked on the signaling thread when offer is created.
  void CreateOffer(
      rtc::scoped_refptr<CreateSessionDescriptionObserver> observer) {
    RTC_CHECK(wrapper_) << "TestPeer is already closed";
    pc()->CreateOffer(observer.get(), params_.rtc_offer_answer_options);
  }
  std::unique_ptr<SessionDescriptionInterface> CreateOffer() {
    RTC_CHECK(wrapper_) << "TestPeer is already closed";
    return wrapper_->CreateOffer(params_.rtc_offer_answer_options);
  }

  std::unique_ptr<SessionDescriptionInterface> CreateAnswer() {
    RTC_CHECK(wrapper_) << "TestPeer is already closed";
    return wrapper_->CreateAnswer();
  }

  bool SetLocalDescription(std::unique_ptr<SessionDescriptionInterface> desc,
                           std::string* error_out = nullptr) {
    RTC_CHECK(wrapper_) << "TestPeer is already closed";
    return wrapper_->SetLocalDescription(std::move(desc), error_out);
  }

  // `error_out` will be set only if returned value is false.
  bool SetRemoteDescription(std::unique_ptr<SessionDescriptionInterface> desc,
                            std::string* error_out = nullptr);

  rtc::scoped_refptr<RtpTransceiverInterface> AddTransceiver(
      cricket::MediaType media_type,
      const RtpTransceiverInit& init) {
    RTC_CHECK(wrapper_) << "TestPeer is already closed";
    return wrapper_->AddTransceiver(media_type, init);
  }

  rtc::scoped_refptr<RtpSenderInterface> AddTrack(
      rtc::scoped_refptr<MediaStreamTrackInterface> track,
      const std::vector<std::string>& stream_ids = {}) {
    RTC_CHECK(wrapper_) << "TestPeer is already closed";
    return wrapper_->AddTrack(track, stream_ids);
  }

  rtc::scoped_refptr<DataChannelInterface> CreateDataChannel(
      const std::string& label) {
    RTC_CHECK(wrapper_) << "TestPeer is already closed";
    return wrapper_->CreateDataChannel(label);
  }

  PeerConnectionInterface::SignalingState signaling_state() {
    RTC_CHECK(wrapper_) << "TestPeer is already closed";
    return wrapper_->signaling_state();
  }

  bool IsIceGatheringDone() {
    RTC_CHECK(wrapper_) << "TestPeer is already closed";
    return wrapper_->IsIceGatheringDone();
  }

  bool IsIceConnected() {
    RTC_CHECK(wrapper_) << "TestPeer is already closed";
    return wrapper_->IsIceConnected();
  }

  rtc::scoped_refptr<const RTCStatsReport> GetStats() {
    RTC_CHECK(wrapper_) << "TestPeer is already closed";
    return wrapper_->GetStats();
  }

  void DetachAecDump() {
    RTC_CHECK(wrapper_) << "TestPeer is already closed";
    if (audio_processing_) {
      audio_processing_->DetachAecDump();
    }
  }

  // Adds provided |candidates| to the owned peer connection.
  bool AddIceCandidates(
      std::vector<std::unique_ptr<IceCandidateInterface>> candidates);

  // Closes underlying peer connection and destroys all related objects freeing
  // up related resources.
  void Close();

 protected:
  friend class TestPeerFactory;
  TestPeer(rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory,
           rtc::scoped_refptr<PeerConnectionInterface> pc,
           std::unique_ptr<MockPeerConnectionObserver> observer,
           Params params,
           ConfigurableParams configurable_params,
           std::vector<PeerConfigurerImpl::VideoSource> video_sources,
           rtc::scoped_refptr<AudioProcessing> audio_processing,
           std::unique_ptr<rtc::Thread> worker_thread);

 private:
<<<<<<< HEAD
  const Params params_;

  mutable Mutex mutex_;
  ConfigurableParams configurable_params_ RTC_GUARDED_BY(mutex_);

  // Safety flag to protect all tasks posted on the signaling thread to not be
  // executed after `wrapper_` object is destructed.
  rtc::scoped_refptr<PendingTaskSafetyFlag> signaling_thread_task_safety_ =
      nullptr;

  // Keeps ownership of worker thread. It has to be destroyed after `wrapper_`.
=======
  // Keeps ownership of worker thread. It has to be destroyed after |wrapper_|.
>>>>>>> parent of 8e32ad0e8387 (revert libwebrtc changes to help bump)
  std::unique_ptr<rtc::Thread> worker_thread_;
  std::unique_ptr<PeerConnectionWrapper> wrapper_;
  std::vector<PeerConfigurerImpl::VideoSource> video_sources_;
  rtc::scoped_refptr<AudioProcessing> audio_processing_;

  std::vector<std::unique_ptr<IceCandidateInterface>> remote_ice_candidates_;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_TEST_PEER_H_
