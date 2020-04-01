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

#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "rtc_base/null_socket_server.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/strings/string_builder.h"
#include "test/logging/file_log_writer.h"
#include "test/testsupport/file_utils.h"
#include "test/time_controller/real_time_controller.h"
#include "test/time_controller/simulated_time_controller.h"

ABSL_FLAG(bool, peer_logs, false, "Save logs from peer scenario framework.");
ABSL_FLAG(std::string,
          peer_logs_root,
          "",
          "Output root path, based on project root if unset.");

namespace webrtc {
namespace test {
namespace {
std::unique_ptr<FileLogWriterFactory> GetPeerScenarioLogManager(
    std::string file_name) {
  if (absl::GetFlag(FLAGS_peer_logs) && !file_name.empty()) {
    std::string output_root = absl::GetFlag(FLAGS_peer_logs_root);
    if (output_root.empty())
      output_root = OutputPath() + "output_data/";

    auto base_filename = output_root + file_name + ".";
    RTC_LOG(LS_INFO) << "Saving peer scenario logs to: " << base_filename;
    return std::make_unique<FileLogWriterFactory>(base_filename);
  }
  return nullptr;
}
}  // namespace

PeerScenario::PeerScenario(const testing::TestInfo& test_info, TimeMode mode)
    : PeerScenario(
          std::string(test_info.test_suite_name()) + "/" + test_info.name(),
          mode) {}

PeerScenario::PeerScenario(std::string file_name, TimeMode mode)
    : PeerScenario(GetPeerScenarioLogManager(file_name), mode) {}

PeerScenario::PeerScenario(
    std::unique_ptr<LogWriterFactoryInterface> log_writer_manager,
    TimeMode mode)
    : log_writer_manager_(std::move(log_writer_manager)),
      net_(mode),
      signaling_thread_(net_.time_controller()->GetMainThread()) {}

PeerScenarioClient* PeerScenario::CreateClient(
    PeerScenarioClient::Config config) {
  return CreateClient(
      std::string("client_") + rtc::ToString(peer_clients_.size() + 1), config);
}

PeerScenarioClient* PeerScenario::CreateClient(
    std::string name,
    PeerScenarioClient::Config config) {
  peer_clients_.emplace_back(net(), signaling_thread_,
                             GetLogWriterFactory(name), config);
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
  std::atomic<bool> done(false);
  signaling.NegotiateSdp(
      [&](const SessionDescriptionInterface&) { done = true; });
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

bool PeerScenario::WaitAndProcess(std::atomic<bool>* event,
                                  TimeDelta max_duration) {
  return net_.time_controller()->Wait([event] { return event->load(); },
                                      max_duration);
}

void PeerScenario::ProcessMessages(TimeDelta duration) {
  net_.time_controller()->AdvanceTime(duration);
}

std::unique_ptr<LogWriterFactoryInterface> PeerScenario::GetLogWriterFactory(
    std::string name) {
  if (!log_writer_manager_ || name.empty())
    return nullptr;
  return std::make_unique<LogWriterFactoryAddPrefix>(log_writer_manager_.get(),
                                                     name);
}

}  // namespace test
}  // namespace webrtc
