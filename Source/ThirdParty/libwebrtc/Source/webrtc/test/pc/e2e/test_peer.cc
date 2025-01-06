/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/test_peer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/jsep.h"
#include "api/make_ref_counted.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/set_remote_description_observer_interface.h"
#include "api/stats/rtc_stats_collector_callback.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/test/pclf/media_configuration.h"
#include "api/test/pclf/media_quality_test_params.h"
#include "api/test/pclf/peer_configurer.h"
#include "pc/peer_connection_wrapper.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

class SetRemoteDescriptionCallback
    : public webrtc::SetRemoteDescriptionObserverInterface {
 public:
  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
    is_called_ = true;
    error_ = error;
  }

  bool is_called() const { return is_called_; }

  webrtc::RTCError error() const { return error_; }

 private:
  bool is_called_ = false;
  webrtc::RTCError error_;
};

}  // namespace

ConfigurableParams TestPeer::configurable_params() const {
  MutexLock lock(&mutex_);
  return configurable_params_;
}

void TestPeer::AddVideoConfig(VideoConfig config) {
  MutexLock lock(&mutex_);
  configurable_params_.video_configs.push_back(std::move(config));
}

void TestPeer::RemoveVideoConfig(absl::string_view stream_label) {
  MutexLock lock(&mutex_);
  bool config_removed = false;
  for (auto it = configurable_params_.video_configs.begin();
       it != configurable_params_.video_configs.end(); ++it) {
    if (*it->stream_label == stream_label) {
      configurable_params_.video_configs.erase(it);
      config_removed = true;
      break;
    }
  }
  RTC_CHECK(config_removed) << *params_.name << ": No video config with label ["
                            << stream_label << "] was found";
}

void TestPeer::SetVideoSubscription(VideoSubscription subscription) {
  MutexLock lock(&mutex_);
  configurable_params_.video_subscription = std::move(subscription);
}

void TestPeer::GetStats(RTCStatsCollectorCallback* callback) {
  pc()->signaling_thread()->PostTask(
      SafeTask(signaling_thread_task_safety_,
               [this, callback]() { pc()->GetStats(callback); }));
}

bool TestPeer::SetRemoteDescription(
    std::unique_ptr<SessionDescriptionInterface> desc,
    std::string* error_out) {
  RTC_CHECK(wrapper_) << "TestPeer is already closed";

  auto observer = rtc::make_ref_counted<SetRemoteDescriptionCallback>();
  // We're assuming (and asserting) that the PeerConnection implementation of
  // SetRemoteDescription is synchronous when called on the signaling thread.
  pc()->SetRemoteDescription(std::move(desc), observer);
  RTC_CHECK(observer->is_called());
  if (!observer->error().ok()) {
    RTC_LOG(LS_ERROR) << *params_.name << ": Failed to set remote description: "
                      << observer->error().message();
    if (error_out) {
      *error_out = observer->error().message();
    }
  }
  return observer->error().ok();
}

bool TestPeer::AddIceCandidates(
    std::vector<std::unique_ptr<IceCandidateInterface>> candidates) {
  RTC_CHECK(wrapper_) << "TestPeer is already closed";
  bool success = true;
  for (auto& candidate : candidates) {
    if (!pc()->AddIceCandidate(candidate.get())) {
      std::string candidate_str;
      bool res = candidate->ToString(&candidate_str);
      RTC_CHECK(res);
      RTC_LOG(LS_ERROR) << "Failed to add ICE candidate, candidate_str="
                        << candidate_str;
      success = false;
    } else {
      remote_ice_candidates_.push_back(std::move(candidate));
    }
  }
  return success;
}

void TestPeer::Close() {
  signaling_thread_task_safety_->SetNotAlive();
  wrapper_->pc()->Close();
  remote_ice_candidates_.clear();
  video_sources_.clear();
  wrapper_ = nullptr;
  worker_thread_ = nullptr;
}

TestPeer::TestPeer(
    rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory,
    rtc::scoped_refptr<PeerConnectionInterface> pc,
    std::unique_ptr<MockPeerConnectionObserver> observer,
    Params params,
    ConfigurableParams configurable_params,
    std::vector<PeerConfigurer::VideoSource> video_sources,
    std::unique_ptr<rtc::Thread> worker_thread)
    : params_(std::move(params)),
      configurable_params_(std::move(configurable_params)),
      worker_thread_(std::move(worker_thread)),
      wrapper_(std::make_unique<PeerConnectionWrapper>(std::move(pc_factory),
                                                       std::move(pc),
                                                       std::move(observer))),
      video_sources_(std::move(video_sources)) {
  signaling_thread_task_safety_ = PendingTaskSafetyFlag::CreateDetached();
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
