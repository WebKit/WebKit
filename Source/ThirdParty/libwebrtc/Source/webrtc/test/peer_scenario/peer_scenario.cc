/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/peer_scenario/peer_scenario.h"

#include "absl/memory/memory.h"

namespace webrtc {
namespace test {

PeerScenario::PeerScenario() : signaling_thread_(rtc::Thread::Current()) {}

PeerScenarioClient* PeerScenario::CreateClient(
    PeerScenarioClient::Config config) {
  peer_clients_.emplace_back(net(), thread(), config);
  return &peer_clients_.back();
}

SignalingRoute PeerScenario::ConnectSignaling(
    PeerScenarioClient* caller,
    PeerScenarioClient* callee,
    std::vector<EmulatedNetworkNode*> send_link,
    std::vector<EmulatedNetworkNode*> ret_link) {
  return SignalingRoute(caller, callee, net_.CreateTrafficRoute(send_link),
                        net_.CreateTrafficRoute(ret_link));
}

void PeerScenario::SimpleConnection(
    PeerScenarioClient* caller,
    PeerScenarioClient* callee,
    std::vector<EmulatedNetworkNode*> send_link,
    std::vector<EmulatedNetworkNode*> ret_link) {
  net()->CreateRoute(caller->endpoint(), send_link, callee->endpoint());
  net()->CreateRoute(callee->endpoint(), ret_link, caller->endpoint());
  auto signaling = ConnectSignaling(caller, callee, send_link, ret_link);
  signaling.StartIceSignaling();
  rtc::Event done;
  signaling.NegotiateSdp(
      [&](const SessionDescriptionInterface&) { done.Set(); });
  RTC_CHECK(WaitAndProcess(&done));
}

void PeerScenario::AttachVideoQualityAnalyzer(VideoQualityAnalyzer* analyzer,
                                              VideoTrackInterface* send_track,
                                              PeerScenarioClient* receiver) {
  video_quality_pairs_.emplace_back(clock(), analyzer);
  auto pair = &video_quality_pairs_.back();
  send_track->AddOrUpdateSink(&pair->capture_tap_, rtc::VideoSinkWants());
  receiver->AddVideoReceiveSink(send_track->id(), &pair->decode_tap_);
}

bool PeerScenario::WaitAndProcess(rtc::Event* event, TimeDelta max_duration) {
  constexpr int kStepMs = 5;
  if (event->Wait(0))
    return true;
  for (int elapsed = 0; elapsed < max_duration.ms(); elapsed += kStepMs) {
    thread()->ProcessMessages(kStepMs);
    if (event->Wait(0))
      return true;
  }
  return false;
}

void PeerScenario::ProcessMessages(TimeDelta duration) {
  thread()->ProcessMessages(duration.ms());
}

}  // namespace test
}  // namespace webrtc
