/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_SCENARIO_H_
#define TEST_SCENARIO_SCENARIO_H_
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "rtc_base/constructormagic.h"
#include "rtc_base/fakeclock.h"
#include "test/scenario/audio_stream.h"
#include "test/scenario/call_client.h"
#include "test/scenario/column_printer.h"
#include "test/scenario/network_node.h"
#include "test/scenario/scenario_config.h"
#include "test/scenario/simulated_time.h"
#include "test/scenario/video_stream.h"

namespace webrtc {
namespace test {
// RepeatedActivity is created by the Scenario class and can be used to stop a
// running activity at runtime.
class RepeatedActivity {
 public:
  void Stop();

 private:
  friend class Scenario;
  RepeatedActivity(TimeDelta interval, std::function<void(TimeDelta)> function);

  void Poll(Timestamp time);
  void SetStartTime(Timestamp time);
  Timestamp NextTime();

  TimeDelta interval_;
  std::function<void(TimeDelta)> function_;
  Timestamp last_update_ = Timestamp::MinusInfinity();
};

struct PendingActivity {
  TimeDelta after_duration;
  std::function<void()> function;
};

// Scenario is a class owning everything for a test scenario. It creates and
// holds network nodes, call clients and media streams. It also provides methods
// for changing behavior at runtime. Since it always keeps ownership of the
// created components, it generally returns non-owning pointers. It maintains
// the life of its objects until it is destroyed.
// For methods accepting configuration structs, a modifier function interface is
// generally provided. This allows simple partial overriding of the default
// configuration.
class Scenario {
 public:
  Scenario();
  explicit Scenario(std::string file_name);
  Scenario(std::string file_name, bool real_time);
  RTC_DISALLOW_COPY_AND_ASSIGN(Scenario);
  ~Scenario();

  SimulationNode* CreateSimulationNode(NetworkNodeConfig config);
  SimulationNode* CreateSimulationNode(
      std::function<void(NetworkNodeConfig*)> config_modifier);
  NetworkNode* CreateNetworkNode(
      NetworkNodeConfig config,
      std::unique_ptr<NetworkBehaviorInterface> behavior);

  CallClient* CreateClient(std::string name, CallClientConfig config);
  CallClient* CreateClient(
      std::string name,
      std::function<void(CallClientConfig*)> config_modifier);

  SimulatedTimeClient* CreateSimulatedTimeClient(
      std::string name,
      SimulatedTimeClientConfig config,
      std::vector<PacketStreamConfig> stream_configs,
      std::vector<NetworkNode*> send_link,
      std::vector<NetworkNode*> return_link);

  VideoStreamPair* CreateVideoStream(
      CallClient* sender,
      std::vector<NetworkNode*> send_link,
      CallClient* receiver,
      std::vector<NetworkNode*> return_link,
      std::function<void(VideoStreamConfig*)> config_modifier);
  VideoStreamPair* CreateVideoStream(CallClient* sender,
                                     std::vector<NetworkNode*> send_link,
                                     CallClient* receiver,
                                     std::vector<NetworkNode*> return_link,
                                     VideoStreamConfig config);

  AudioStreamPair* CreateAudioStream(
      CallClient* sender,
      std::vector<NetworkNode*> send_link,
      CallClient* receiver,
      std::vector<NetworkNode*> return_link,
      std::function<void(AudioStreamConfig*)> config_modifier);
  AudioStreamPair* CreateAudioStream(CallClient* sender,
                                     std::vector<NetworkNode*> send_link,
                                     CallClient* receiver,
                                     std::vector<NetworkNode*> return_link,
                                     AudioStreamConfig config);

  CrossTrafficSource* CreateCrossTraffic(
      std::vector<NetworkNode*> over_nodes,
      std::function<void(CrossTrafficConfig*)> config_modifier);
  CrossTrafficSource* CreateCrossTraffic(std::vector<NetworkNode*> over_nodes,
                                         CrossTrafficConfig config);

  // Runs the provided function with a fixed interval.
  RepeatedActivity* Every(TimeDelta interval,
                          std::function<void(TimeDelta)> function);
  RepeatedActivity* Every(TimeDelta interval, std::function<void()> function);

  // Runs the provided function after given duration has passed in a session.
  void At(TimeDelta offset, std::function<void()> function);

  // Sends a packet over the nodes and runs |action| when it has been delivered.
  void NetworkDelayedAction(std::vector<NetworkNode*> over_nodes,
                            size_t packet_size,
                            std::function<void()> action);

  // Runs the scenario for the given time or until the exit function returns
  // true.
  void RunFor(TimeDelta duration);
  void RunUntil(TimeDelta max_duration,
                TimeDelta probe_interval,
                std::function<bool()> exit_function);

  // Triggers sending of dummy packets over the given nodes.
  void TriggerPacketBurst(std::vector<NetworkNode*> over_nodes,
                          size_t num_packets,
                          size_t packet_size);

  ColumnPrinter TimePrinter();
  StatesPrinter* CreatePrinter(std::string name,
                               TimeDelta interval,
                               std::vector<ColumnPrinter> printers);

  // Returns the current time.
  Timestamp Now();
  // Return the duration of the current session so far.
  TimeDelta Duration();

  std::string GetFullPathOrEmpty(std::string name) const {
    if (base_filename_.empty() || name.empty())
      return std::string();
    return base_filename_ + "." + name;
  }

 private:
  NullReceiver null_receiver_;
  std::string base_filename_;
  const bool real_time_mode_;
  SimulatedClock sim_clock_;
  Clock* clock_;
  // Event logs use a global clock instance, this is used to override that
  // instance when not running in real time.
  rtc::FakeClock event_log_fake_clock_;

  std::vector<std::unique_ptr<CallClient>> clients_;
  std::vector<std::unique_ptr<NetworkNode>> network_nodes_;
  std::vector<std::unique_ptr<CrossTrafficSource>> cross_traffic_sources_;
  std::vector<std::unique_ptr<VideoStreamPair>> video_streams_;
  std::vector<std::unique_ptr<AudioStreamPair>> audio_streams_;

  std::vector<std::unique_ptr<SimulatedTimeClient>> simulated_time_clients_;

  std::vector<std::unique_ptr<RepeatedActivity>> repeated_activities_;
  std::vector<std::unique_ptr<ActionReceiver>> action_receivers_;
  std::vector<std::unique_ptr<PendingActivity>> pending_activities_;
  std::vector<std::unique_ptr<StatesPrinter>> printers_;

  int64_t next_receiver_id_ = 40000;
  rtc::scoped_refptr<AudioDecoderFactory> audio_decoder_factory_;
  rtc::scoped_refptr<AudioEncoderFactory> audio_encoder_factory_;

  Timestamp start_time_ = Timestamp::PlusInfinity();
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_SCENARIO_H_
