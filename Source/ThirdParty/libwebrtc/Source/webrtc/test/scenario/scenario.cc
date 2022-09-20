/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/scenario.h"

#include <algorithm>
#include <memory>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/string_view.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "rtc_base/socket_address.h"
#include "test/logging/file_log_writer.h"
#include "test/network/network_emulation.h"
#include "test/testsupport/file_utils.h"

ABSL_FLAG(bool, scenario_logs, false, "Save logs from scenario framework.");
ABSL_FLAG(std::string,
          scenario_logs_root,
          "",
          "Output root path, based on project root if unset.");

namespace webrtc {
namespace test {
namespace {

std::unique_ptr<FileLogWriterFactory> GetScenarioLogManager(
    absl::string_view file_name) {
  if (absl::GetFlag(FLAGS_scenario_logs) && !file_name.empty()) {
    std::string output_root = absl::GetFlag(FLAGS_scenario_logs_root);
    if (output_root.empty())
      output_root = OutputPath() + "output_data/";

    auto base_filename = output_root + std::string(file_name) + ".";
    RTC_LOG(LS_INFO) << "Saving scenario logs to: " << base_filename;
    return std::make_unique<FileLogWriterFactory>(base_filename);
  }
  return nullptr;
}
}  // namespace

Scenario::Scenario()
    : Scenario(std::unique_ptr<LogWriterFactoryInterface>(),
               /*real_time=*/false) {}

Scenario::Scenario(const testing::TestInfo* test_info)
    : Scenario(std::string(test_info->test_suite_name()) + "/" +
               test_info->name()) {}

Scenario::Scenario(absl::string_view file_name)
    : Scenario(file_name, /*real_time=*/false) {}

Scenario::Scenario(absl::string_view file_name, bool real_time)
    : Scenario(GetScenarioLogManager(file_name), real_time) {}

Scenario::Scenario(
    std::unique_ptr<LogWriterFactoryInterface> log_writer_factory,
    bool real_time)
    : log_writer_factory_(std::move(log_writer_factory)),
      network_manager_(real_time ? TimeMode::kRealTime : TimeMode::kSimulated),
      clock_(network_manager_.time_controller()->GetClock()),
      audio_decoder_factory_(CreateBuiltinAudioDecoderFactory()),
      audio_encoder_factory_(CreateBuiltinAudioEncoderFactory()),
      task_queue_(network_manager_.time_controller()
                      ->GetTaskQueueFactory()
                      ->CreateTaskQueue("Scenario",
                                        TaskQueueFactory::Priority::NORMAL)) {}

Scenario::~Scenario() {
  if (start_time_.IsFinite())
    Stop();
  for (auto& call_client : clients_) {
    call_client->transport_->Disconnect();
    call_client->UnBind();
  }
}

ColumnPrinter Scenario::TimePrinter() {
  return ColumnPrinter::Lambda(
      "time",
      [this](rtc::SimpleStringBuilder& sb) {
        sb.AppendFormat("%.3lf", Now().seconds<double>());
      },
      32);
}

StatesPrinter* Scenario::CreatePrinter(absl::string_view name,
                                       TimeDelta interval,
                                       std::vector<ColumnPrinter> printers) {
  std::vector<ColumnPrinter> all_printers{TimePrinter()};
  for (auto& printer : printers)
    all_printers.push_back(printer);
  StatesPrinter* printer = new StatesPrinter(GetLogWriter(name), all_printers);
  printers_.emplace_back(printer);
  printer->PrintHeaders();
  if (interval.IsFinite())
    Every(interval, [printer] { printer->PrintRow(); });
  return printer;
}

CallClient* Scenario::CreateClient(absl::string_view name,
                                   CallClientConfig config) {
  CallClient* client = new CallClient(network_manager_.time_controller(),
                                      GetLogWriterFactory(name), config);
  if (config.transport.state_log_interval.IsFinite()) {
    Every(config.transport.state_log_interval, [this, client]() {
      client->network_controller_factory_.LogCongestionControllerStats(Now());
    });
  }
  clients_.emplace_back(client);
  return client;
}

CallClient* Scenario::CreateClient(
    absl::string_view name,
    std::function<void(CallClientConfig*)> config_modifier) {
  CallClientConfig config;
  config_modifier(&config);
  return CreateClient(name, config);
}

CallClientPair* Scenario::CreateRoutes(
    CallClient* first,
    std::vector<EmulatedNetworkNode*> send_link,
    CallClient* second,
    std::vector<EmulatedNetworkNode*> return_link) {
  return CreateRoutes(first, send_link,
                      DataSize::Bytes(PacketOverhead::kDefault), second,
                      return_link, DataSize::Bytes(PacketOverhead::kDefault));
}

CallClientPair* Scenario::CreateRoutes(
    CallClient* first,
    std::vector<EmulatedNetworkNode*> send_link,
    DataSize first_overhead,
    CallClient* second,
    std::vector<EmulatedNetworkNode*> return_link,
    DataSize second_overhead) {
  CallClientPair* client_pair = new CallClientPair(first, second);
  ChangeRoute(client_pair->forward(), send_link, first_overhead);
  ChangeRoute(client_pair->reverse(), return_link, second_overhead);
  client_pairs_.emplace_back(client_pair);
  return client_pair;
}

void Scenario::ChangeRoute(std::pair<CallClient*, CallClient*> clients,
                           std::vector<EmulatedNetworkNode*> over_nodes) {
  ChangeRoute(clients, over_nodes, DataSize::Bytes(PacketOverhead::kDefault));
}

void Scenario::ChangeRoute(std::pair<CallClient*, CallClient*> clients,
                           std::vector<EmulatedNetworkNode*> over_nodes,
                           DataSize overhead) {
  EmulatedRoute* route = network_manager_.CreateRoute(over_nodes);
  uint16_t port = clients.second->Bind(route->to);
  auto addr = rtc::SocketAddress(route->to->GetPeerLocalAddress(), port);
  clients.first->transport_->Connect(route->from, addr, overhead);
}

EmulatedNetworkNode* Scenario::CreateSimulationNode(
    std::function<void(NetworkSimulationConfig*)> config_modifier) {
  NetworkSimulationConfig config;
  config_modifier(&config);
  return CreateSimulationNode(config);
}

EmulatedNetworkNode* Scenario::CreateSimulationNode(
    NetworkSimulationConfig config) {
  return network_manager_.CreateEmulatedNode(
      SimulationNode::CreateBehavior(config));
}

SimulationNode* Scenario::CreateMutableSimulationNode(
    std::function<void(NetworkSimulationConfig*)> config_modifier) {
  NetworkSimulationConfig config;
  config_modifier(&config);
  return CreateMutableSimulationNode(config);
}

SimulationNode* Scenario::CreateMutableSimulationNode(
    NetworkSimulationConfig config) {
  std::unique_ptr<SimulatedNetwork> behavior =
      SimulationNode::CreateBehavior(config);
  SimulatedNetwork* behavior_ptr = behavior.get();
  auto* emulated_node =
      network_manager_.CreateEmulatedNode(std::move(behavior));
  simulation_nodes_.emplace_back(
      new SimulationNode(config, behavior_ptr, emulated_node));
  return simulation_nodes_.back().get();
}

void Scenario::TriggerPacketBurst(std::vector<EmulatedNetworkNode*> over_nodes,
                                  size_t num_packets,
                                  size_t packet_size) {
  network_manager_.CreateCrossTrafficRoute(over_nodes)
      ->TriggerPacketBurst(num_packets, packet_size);
}

void Scenario::NetworkDelayedAction(
    std::vector<EmulatedNetworkNode*> over_nodes,
    size_t packet_size,
    std::function<void()> action) {
  network_manager_.CreateCrossTrafficRoute(over_nodes)
      ->NetworkDelayedAction(packet_size, action);
}

VideoStreamPair* Scenario::CreateVideoStream(
    std::pair<CallClient*, CallClient*> clients,
    std::function<void(VideoStreamConfig*)> config_modifier) {
  VideoStreamConfig config;
  config_modifier(&config);
  return CreateVideoStream(clients, config);
}

VideoStreamPair* Scenario::CreateVideoStream(
    std::pair<CallClient*, CallClient*> clients,
    VideoStreamConfig config) {
  video_streams_.emplace_back(
      new VideoStreamPair(clients.first, clients.second, config));
  return video_streams_.back().get();
}

AudioStreamPair* Scenario::CreateAudioStream(
    std::pair<CallClient*, CallClient*> clients,
    std::function<void(AudioStreamConfig*)> config_modifier) {
  AudioStreamConfig config;
  config_modifier(&config);
  return CreateAudioStream(clients, config);
}

AudioStreamPair* Scenario::CreateAudioStream(
    std::pair<CallClient*, CallClient*> clients,
    AudioStreamConfig config) {
  audio_streams_.emplace_back(
      new AudioStreamPair(clients.first, audio_encoder_factory_, clients.second,
                          audio_decoder_factory_, config));
  return audio_streams_.back().get();
}

void Scenario::Every(TimeDelta interval,
                     absl::AnyInvocable<void(TimeDelta)> function) {
  RepeatingTaskHandle::DelayedStart(
      task_queue_.get(), interval,
      [interval, function = std::move(function)]() mutable {
        function(interval);
        return interval;
      });
}

void Scenario::Every(TimeDelta interval, absl::AnyInvocable<void()> function) {
  RepeatingTaskHandle::DelayedStart(
      task_queue_.get(), interval,
      [interval, function = std::move(function)]() mutable {
        function();
        return interval;
      });
}

void Scenario::Post(absl::AnyInvocable<void() &&> function) {
  task_queue_->PostTask(std::move(function));
}

void Scenario::At(TimeDelta offset, absl::AnyInvocable<void() &&> function) {
  RTC_DCHECK_GT(offset, TimeSinceStart());
  task_queue_->PostDelayedTask(std::move(function), TimeUntilTarget(offset));
}

void Scenario::RunFor(TimeDelta duration) {
  if (start_time_.IsInfinite())
    Start();
  network_manager_.time_controller()->AdvanceTime(duration);
}

void Scenario::RunUntil(TimeDelta target_time_since_start) {
  RunFor(TimeUntilTarget(target_time_since_start));
}

void Scenario::RunUntil(TimeDelta target_time_since_start,
                        TimeDelta check_interval,
                        std::function<bool()> exit_function) {
  if (start_time_.IsInfinite())
    Start();
  while (check_interval >= TimeUntilTarget(target_time_since_start)) {
    network_manager_.time_controller()->AdvanceTime(check_interval);
    if (exit_function())
      return;
  }
  network_manager_.time_controller()->AdvanceTime(
      TimeUntilTarget(target_time_since_start));
}

void Scenario::Start() {
  start_time_ = clock_->CurrentTime();
  for (auto& stream_pair : video_streams_)
    stream_pair->receive()->Start();
  for (auto& stream_pair : audio_streams_)
    stream_pair->receive()->Start();
  for (auto& stream_pair : video_streams_) {
    if (stream_pair->config_.autostart) {
      stream_pair->send()->Start();
    }
  }
  for (auto& stream_pair : audio_streams_) {
    if (stream_pair->config_.autostart) {
      stream_pair->send()->Start();
    }
  }
}

void Scenario::Stop() {
  RTC_DCHECK(start_time_.IsFinite());
  for (auto& stream_pair : video_streams_) {
    stream_pair->send()->Stop();
  }
  for (auto& stream_pair : audio_streams_)
    stream_pair->send()->Stop();
  for (auto& stream_pair : video_streams_)
    stream_pair->receive()->Stop();
  for (auto& stream_pair : audio_streams_)
    stream_pair->receive()->Stop();
  start_time_ = Timestamp::PlusInfinity();
}

Timestamp Scenario::Now() {
  return clock_->CurrentTime();
}

TimeDelta Scenario::TimeSinceStart() {
  if (start_time_.IsInfinite())
    return TimeDelta::Zero();
  return Now() - start_time_;
}

TimeDelta Scenario::TimeUntilTarget(TimeDelta target_time_offset) {
  return target_time_offset - TimeSinceStart();
}

}  // namespace test
}  // namespace webrtc
