/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/field_trials.h"
#include "api/test/network_emulation/create_cross_traffic.h"
#include "api/test/network_emulation/cross_traffic.h"
#include "api/transport/goog_cc_factory.h"
#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "call/video_receive_stream.h"
#include "test/create_test_field_trials.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/network/network_emulation.h"
#include "test/scenario/call_client.h"
#include "test/scenario/column_printer.h"
#include "test/scenario/scenario.h"
#include "test/scenario/scenario_config.h"

using ::testing::IsEmpty;
using ::testing::NiceMock;

namespace webrtc {
namespace test {
namespace {
// Count dips from a constant high bandwidth level within a short window.
int CountBandwidthDips(std::queue<DataRate> bandwidth_history,
                       DataRate threshold) {
  if (bandwidth_history.empty())
    return true;
  DataRate first = bandwidth_history.front();
  bandwidth_history.pop();

  int dips = 0;
  bool state_high = true;
  while (!bandwidth_history.empty()) {
    if (bandwidth_history.front() + threshold < first && state_high) {
      ++dips;
      state_high = false;
    } else if (bandwidth_history.front() == first) {
      state_high = true;
    } else if (bandwidth_history.front() > first) {
      // If this is toggling we will catch it later when front becomes first.
      state_high = false;
    }
    bandwidth_history.pop();
  }
  return dips;
}

constexpr uint32_t kInitialBitrateKbps = 60;
constexpr DataRate kInitialBitrate =
    DataRate::KilobitsPerSec(kInitialBitrateKbps);
constexpr float kDefaultPacingRate = 2.5f;

CallClient* CreateVideoSendingClient(
    Scenario* s,
    CallClientConfig config,
    std::vector<EmulatedNetworkNode*> send_link,
    std::vector<EmulatedNetworkNode*> return_link) {
  auto* client = s->CreateClient("send", std::move(config));
  auto* route = s->CreateRoutes(client, send_link,
                                s->CreateClient("return", CallClientConfig()),
                                return_link);
  s->CreateVideoStream(route->forward(), VideoStreamConfig());
  return client;
}

NetworkRouteChange CreateRouteChange(
    Timestamp time,
    std::optional<DataRate> start_rate = std::nullopt,
    std::optional<DataRate> min_rate = std::nullopt,
    std::optional<DataRate> max_rate = std::nullopt) {
  NetworkRouteChange route_change;
  route_change.at_time = time;
  route_change.constraints.at_time = time;
  route_change.constraints.min_data_rate = min_rate;
  route_change.constraints.max_data_rate = max_rate;
  route_change.constraints.starting_rate = start_rate;
  return route_change;
}

PacketResult CreatePacketResult(Timestamp arrival_time,
                                Timestamp send_time,
                                size_t payload_size,
                                PacedPacketInfo pacing_info) {
  PacketResult packet_result;
  packet_result.sent_packet = SentPacket();
  packet_result.sent_packet.send_time = send_time;
  packet_result.sent_packet.size = DataSize::Bytes(payload_size);
  packet_result.sent_packet.pacing_info = pacing_info;
  packet_result.receive_time = arrival_time;
  return packet_result;
}

// Simulate sending packets and receiving transport feedback during
// `runtime_ms`, then return the final target birate.
std::optional<DataRate> PacketTransmissionAndFeedbackBlock(
    NetworkControllerInterface* controller,
    int64_t runtime_ms,
    int64_t delay,
    Timestamp& current_time) {
  NetworkControlUpdate update;
  std::optional<DataRate> target_bitrate;
  int64_t delay_buildup = 0;
  int64_t start_time_ms = current_time.ms();
  while (current_time.ms() - start_time_ms < runtime_ms) {
    constexpr size_t kPayloadSize = 1000;
    PacketResult packet =
        CreatePacketResult(current_time + TimeDelta::Millis(delay_buildup),
                           current_time, kPayloadSize, PacedPacketInfo());
    delay_buildup += delay;
    update = controller->OnSentPacket(packet.sent_packet);
    if (update.target_rate) {
      target_bitrate = update.target_rate->target_rate;
    }
    TransportPacketsFeedback feedback;
    feedback.feedback_time = packet.receive_time;
    feedback.packet_feedbacks.push_back(packet);
    update = controller->OnTransportPacketsFeedback(feedback);
    if (update.target_rate) {
      target_bitrate = update.target_rate->target_rate;
    }
    current_time += TimeDelta::Millis(50);
    update = controller->OnProcessInterval({.at_time = current_time});
    if (update.target_rate) {
      target_bitrate = update.target_rate->target_rate;
    }
  }
  return target_bitrate;
}

// Scenarios:

void UpdatesTargetRateBasedOnLinkCapacity(absl::string_view test_name = "",
                                          absl::string_view field_trials = "") {
  GoogCcNetworkControllerFactory factory;
  Scenario s("googcc_unit/target_capacity" + std::string(test_name), false);
  CallClientConfig config;
  config.field_trials.Merge(FieldTrials(field_trials));
  config.transport.cc_factory = &factory;
  config.transport.rates.min_rate = DataRate::KilobitsPerSec(10);
  config.transport.rates.max_rate = DataRate::KilobitsPerSec(1500);
  config.transport.rates.start_rate = DataRate::KilobitsPerSec(300);
  auto send_net = s.CreateMutableSimulationNode([](NetworkSimulationConfig* c) {
    c->bandwidth = DataRate::KilobitsPerSec(500);
    c->delay = TimeDelta::Millis(100);
    c->loss_rate = 0.0;
  });
  auto ret_net = s.CreateMutableSimulationNode(
      [](NetworkSimulationConfig* c) { c->delay = TimeDelta::Millis(100); });
  StatesPrinter* truth = s.CreatePrinter(
      "send.truth.txt", TimeDelta::PlusInfinity(), {send_net->ConfigPrinter()});

  auto* client = CreateVideoSendingClient(&s, config, {send_net->node()},
                                          {ret_net->node()});

  truth->PrintRow();
  s.RunFor(TimeDelta::Seconds(25));
  truth->PrintRow();
  EXPECT_NEAR(client->target_rate().kbps(), 450, 100);

  send_net->UpdateConfig([](NetworkSimulationConfig* c) {
    c->bandwidth = DataRate::KilobitsPerSec(800);
    c->delay = TimeDelta::Millis(100);
  });

  truth->PrintRow();
  s.RunFor(TimeDelta::Seconds(20));
  truth->PrintRow();
  EXPECT_NEAR(client->target_rate().kbps(), 750, 150);

  send_net->UpdateConfig([](NetworkSimulationConfig* c) {
    c->bandwidth = DataRate::KilobitsPerSec(100);
    c->delay = TimeDelta::Millis(200);
  });
  ret_net->UpdateConfig(
      [](NetworkSimulationConfig* c) { c->delay = TimeDelta::Millis(200); });

  truth->PrintRow();
  s.RunFor(TimeDelta::Seconds(50));
  truth->PrintRow();
  EXPECT_NEAR(client->target_rate().kbps(), 90, 25);
}

DataRate RunRembDipScenario(absl::string_view test_name) {
  Scenario s(test_name);
  NetworkSimulationConfig net_conf;
  net_conf.bandwidth = DataRate::KilobitsPerSec(2000);
  net_conf.delay = TimeDelta::Millis(50);
  auto* client = s.CreateClient("send", [&](CallClientConfig* c) {
    c->transport.rates.start_rate = DataRate::KilobitsPerSec(1000);
  });
  auto send_net = {s.CreateSimulationNode(net_conf)};
  auto ret_net = {s.CreateSimulationNode(net_conf)};
  auto* route = s.CreateRoutes(
      client, send_net, s.CreateClient("return", CallClientConfig()), ret_net);
  s.CreateVideoStream(route->forward(), VideoStreamConfig());

  s.RunFor(TimeDelta::Seconds(10));
  EXPECT_GT(client->send_bandwidth().kbps(), 1500);

  DataRate RembLimit = DataRate::KilobitsPerSec(250);
  client->SetRemoteBitrate(RembLimit);
  s.RunFor(TimeDelta::Seconds(1));
  EXPECT_EQ(client->send_bandwidth(), RembLimit);

  DataRate RembLimitLifted = DataRate::KilobitsPerSec(10000);
  client->SetRemoteBitrate(RembLimitLifted);
  s.RunFor(TimeDelta::Seconds(10));

  return client->send_bandwidth();
}

}  // namespace

class NetworkControllerTestFixture {
 public:
  explicit NetworkControllerTestFixture(absl::string_view field_trials = "")
      : field_trials_(CreateTestFieldTrials(field_trials)) {}
  explicit NetworkControllerTestFixture(GoogCcFactoryConfig googcc_config)
      : factory_(std::move(googcc_config)) {}

  std::unique_ptr<NetworkControllerInterface> CreateController() {
    NetworkControllerConfig config = InitialConfig();
    std::unique_ptr<NetworkControllerInterface> controller =
        factory_.Create(config);
    return controller;
  }

 private:
  NetworkControllerConfig InitialConfig(
      int starting_bandwidth_kbps = kInitialBitrateKbps,
      int min_data_rate_kbps = 0,
      int max_data_rate_kbps = 5 * kInitialBitrateKbps) {
    NetworkControllerConfig config(env_);
    config.constraints.at_time = Timestamp::Zero();
    config.constraints.min_data_rate =
        DataRate::KilobitsPerSec(min_data_rate_kbps);
    config.constraints.max_data_rate =
        DataRate::KilobitsPerSec(max_data_rate_kbps);
    config.constraints.starting_rate =
        DataRate::KilobitsPerSec(starting_bandwidth_kbps);
    return config;
  }

  FieldTrials field_trials_ = CreateTestFieldTrials();
  const Environment env_ = CreateEnvironment(&field_trials_);
  GoogCcNetworkControllerFactory factory_;
};

TEST(GoogCcNetworkControllerTest,
     InitializeTargetRateOnFirstProcessIntervalAfterNetworkAvailable) {
  NetworkControllerTestFixture fixture;
  std::unique_ptr<NetworkControllerInterface> controller =
      fixture.CreateController();

  NetworkControlUpdate update = controller->OnNetworkAvailability(
      {.at_time = Timestamp::Millis(123456), .network_available = true});
  update =
      controller->OnProcessInterval({.at_time = Timestamp::Millis(123456)});

  EXPECT_EQ(update.target_rate->target_rate, kInitialBitrate);
  EXPECT_EQ(update.pacer_config->data_rate(),
            kInitialBitrate * kDefaultPacingRate);
  EXPECT_EQ(update.probe_cluster_configs[0].target_data_rate,
            kInitialBitrate * 3);
  EXPECT_EQ(update.probe_cluster_configs[1].target_data_rate,
            kInitialBitrate * 5);
}

TEST(GoogCcNetworkControllerTest, ReactsToChangedNetworkConditions) {
  NetworkControllerTestFixture fixture;
  std::unique_ptr<NetworkControllerInterface> controller =
      fixture.CreateController();
  Timestamp current_time = Timestamp::Millis(123);
  NetworkControlUpdate update = controller->OnNetworkAvailability(
      {.at_time = current_time, .network_available = true});
  update = controller->OnProcessInterval({.at_time = current_time});
  update = controller->OnRemoteBitrateReport(
      {.receive_time = current_time, .bandwidth = kInitialBitrate * 2});

  current_time += TimeDelta::Millis(25);
  update = controller->OnProcessInterval({.at_time = current_time});
  EXPECT_EQ(update.target_rate->target_rate, kInitialBitrate * 2);
  EXPECT_EQ(update.pacer_config->data_rate(),
            kInitialBitrate * 2 * kDefaultPacingRate);

  update = controller->OnRemoteBitrateReport(
      {.receive_time = current_time, .bandwidth = kInitialBitrate});
  current_time += TimeDelta::Millis(25);
  update = controller->OnProcessInterval({.at_time = current_time});
  EXPECT_EQ(update.target_rate->target_rate, kInitialBitrate);
  EXPECT_EQ(update.pacer_config->data_rate(),
            kInitialBitrate * kDefaultPacingRate);
}

TEST(GoogCcNetworkControllerTest, OnNetworkRouteChanged) {
  NetworkControllerTestFixture fixture;
  std::unique_ptr<NetworkControllerInterface> controller =
      fixture.CreateController();
  Timestamp current_time = Timestamp::Millis(123);
  NetworkControlUpdate update = controller->OnNetworkAvailability(
      {.at_time = current_time, .network_available = true});
  DataRate new_bitrate = DataRate::BitsPerSec(200000);

  update = controller->OnNetworkRouteChange(
      CreateRouteChange(current_time, new_bitrate));
  EXPECT_EQ(update.target_rate->target_rate, new_bitrate);
  EXPECT_EQ(update.pacer_config->data_rate(), new_bitrate * kDefaultPacingRate);
  EXPECT_EQ(update.probe_cluster_configs.size(), 2u);

  // If the bitrate is reset to -1, the new starting bitrate will be
  // the minimum default bitrate.
  const DataRate kDefaultMinBitrate = DataRate::KilobitsPerSec(5);
  update = controller->OnNetworkRouteChange(CreateRouteChange(current_time));
  EXPECT_EQ(update.target_rate->target_rate, kDefaultMinBitrate);
  EXPECT_EQ(update.pacer_config->data_window,
            kDefaultMinBitrate * kDefaultPacingRate *
                PacerConfig::kDefaultTimeInterval);
  EXPECT_EQ(update.probe_cluster_configs.size(), 2u);
}

TEST(GoogCcNetworkControllerTest, ProbeOnRouteChange) {
  NetworkControllerTestFixture fixture;
  std::unique_ptr<NetworkControllerInterface> controller =
      fixture.CreateController();
  Timestamp current_time = Timestamp::Millis(123);
  NetworkControlUpdate update = controller->OnNetworkAvailability(
      {.at_time = current_time, .network_available = true});
  current_time += TimeDelta::Seconds(3);

  update = controller->OnNetworkRouteChange(
      CreateRouteChange(current_time, 2 * kInitialBitrate, DataRate::Zero(),
                        20 * kInitialBitrate));

  EXPECT_TRUE(update.pacer_config.has_value());
  EXPECT_EQ(update.target_rate->target_rate, kInitialBitrate * 2);
  EXPECT_EQ(update.probe_cluster_configs.size(), 2u);
  EXPECT_EQ(update.probe_cluster_configs[0].target_data_rate,
            kInitialBitrate * 6);
  EXPECT_EQ(update.probe_cluster_configs[1].target_data_rate,
            kInitialBitrate * 12);

  update = controller->OnProcessInterval({.at_time = current_time});
}

TEST(GoogCcNetworkControllerTest, ProbeAfterRouteChangeWhenTransportWritable) {
  NetworkControllerTestFixture fixture;
  std::unique_ptr<NetworkControllerInterface> controller =
      fixture.CreateController();
  Timestamp current_time = Timestamp::Millis(123);

  NetworkControlUpdate update = controller->OnNetworkAvailability(
      {.at_time = current_time, .network_available = false});
  EXPECT_THAT(update.probe_cluster_configs, IsEmpty());

  update = controller->OnNetworkRouteChange(
      CreateRouteChange(current_time, 2 * kInitialBitrate, DataRate::Zero(),
                        20 * kInitialBitrate));
  // Transport is not writable. So not point in sending a probe.
  EXPECT_THAT(update.probe_cluster_configs, IsEmpty());

  // Probe is sent when transport becomes writable.
  update = controller->OnNetworkAvailability(
      {.at_time = current_time, .network_available = true});
  EXPECT_THAT(update.probe_cluster_configs, Not(IsEmpty()));
}

// Bandwidth estimation is updated when feedbacks are received.
// Feedbacks which show an increasing delay cause the estimation to be reduced.
TEST(GoogCcNetworkControllerTest, UpdatesDelayBasedEstimate) {
  NetworkControllerTestFixture fixture;
  std::unique_ptr<NetworkControllerInterface> controller =
      fixture.CreateController();
  const int64_t kRunTimeMs = 6000;
  Timestamp current_time = Timestamp::Millis(123);
  NetworkControlUpdate update = controller->OnNetworkAvailability(
      {.at_time = current_time, .network_available = true});

  // The test must run and insert packets/feedback long enough that the
  // BWE computes a valid estimate. This is first done in an environment which
  // simulates no bandwidth limitation, and therefore not built-up delay.
  std::optional<DataRate> target_bitrate_before_delay =
      PacketTransmissionAndFeedbackBlock(controller.get(), kRunTimeMs, 0,
                                         current_time);
  ASSERT_TRUE(target_bitrate_before_delay.has_value());

  // Repeat, but this time with a building delay, and make sure that the
  // estimation is adjusted downwards.
  std::optional<DataRate> target_bitrate_after_delay =
      PacketTransmissionAndFeedbackBlock(controller.get(), kRunTimeMs, 50,
                                         current_time);
  EXPECT_LT(*target_bitrate_after_delay, *target_bitrate_before_delay);
}

TEST(GoogCcNetworkControllerTest, LimitPacingFactorToUpperLinkCapacity) {
  NetworkControllerTestFixture fixture(
      /*field_trials=*/
      "WebRTC-Bwe-LimitPacingFactorByUpperLinkCapacityEstimate/Enabled/");
  std::unique_ptr<NetworkControllerInterface> controller =
      fixture.CreateController();
  Timestamp current_time = Timestamp::Millis(123);
  NetworkControlUpdate update = controller->OnNetworkAvailability(
      {.at_time = current_time, .network_available = true});
  update = controller->OnProcessInterval({.at_time = current_time});
  current_time += TimeDelta::Millis(100);
  NetworkStateEstimate network_estimate = {
      .link_capacity_upper = kInitialBitrate * kDefaultPacingRate / 2};
  update = controller->OnNetworkStateEstimate(network_estimate);
  // OnNetworkStateEstimate does not trigger processing a new estimate. So add a
  // dummy loss report to trigger a BWE update in the next process interval.
  TransportLossReport loss_report;
  loss_report.start_time = current_time;
  loss_report.end_time = current_time;
  loss_report.receive_time = current_time;
  loss_report.packets_received_delta = 50;
  loss_report.packets_lost_delta = 1;
  update = controller->OnTransportLossReport(loss_report);
  update = controller->OnProcessInterval({.at_time = current_time});
  ASSERT_TRUE(update.pacer_config);
  ASSERT_TRUE(update.target_rate);
  EXPECT_GE(update.target_rate->target_rate, kInitialBitrate);
  EXPECT_EQ(update.pacer_config->data_rate(),
            network_estimate.link_capacity_upper);
}

// Test congestion window pushback on network delay happens.
TEST(GoogCcScenario, CongestionWindowPushbackOnNetworkDelay) {
  GoogCcNetworkControllerFactory factory;
  Scenario s("googcc_unit/cwnd_on_delay", false);
  auto send_net =
      s.CreateMutableSimulationNode([=](NetworkSimulationConfig* c) {
        c->bandwidth = DataRate::KilobitsPerSec(1000);
        c->delay = TimeDelta::Millis(100);
      });
  auto ret_net = s.CreateSimulationNode(
      [](NetworkSimulationConfig* c) { c->delay = TimeDelta::Millis(100); });
  CallClientConfig config;
  config.field_trials.Set("WebRTC-CongestionWindow",
                          "QueueSize:800,MinBitrate:30000");
  config.transport.cc_factory = &factory;
  // Start high so bandwidth drop has max effect.
  config.transport.rates.start_rate = DataRate::KilobitsPerSec(300);
  config.transport.rates.max_rate = DataRate::KilobitsPerSec(2000);
  config.transport.rates.min_rate = DataRate::KilobitsPerSec(10);

  auto* client = CreateVideoSendingClient(&s, std::move(config),
                                          {send_net->node()}, {ret_net});

  s.RunFor(TimeDelta::Seconds(10));
  send_net->PauseTransmissionUntil(s.Now() + TimeDelta::Seconds(10));
  s.RunFor(TimeDelta::Seconds(3));

  // After 3 seconds without feedback from any sent packets, we expect that the
  // target rate is reduced to the minimum pushback threshold
  // kDefaultMinPushbackTargetBitrateBps, which is defined as 30 kbps in
  // congestion_window_pushback_controller.
  EXPECT_LT(client->target_rate().kbps(), 40);
}

// Test congestion window pushback on network delay happens.
TEST(GoogCcScenario, CongestionWindowPushbackDropFrameOnNetworkDelay) {
  GoogCcNetworkControllerFactory factory;
  Scenario s("googcc_unit/cwnd_on_delay", false);
  auto send_net =
      s.CreateMutableSimulationNode([=](NetworkSimulationConfig* c) {
        c->bandwidth = DataRate::KilobitsPerSec(1000);
        c->delay = TimeDelta::Millis(100);
      });
  auto ret_net = s.CreateSimulationNode(
      [](NetworkSimulationConfig* c) { c->delay = TimeDelta::Millis(100); });
  CallClientConfig config;
  config.field_trials.Set("WebRTC-CongestionWindow",
                          "QueueSize:800,MinBitrate:30000,DropFrame:true");
  config.transport.cc_factory = &factory;
  // Start high so bandwidth drop has max effect.
  config.transport.rates.start_rate = DataRate::KilobitsPerSec(300);
  config.transport.rates.max_rate = DataRate::KilobitsPerSec(2000);
  config.transport.rates.min_rate = DataRate::KilobitsPerSec(10);

  auto* client = CreateVideoSendingClient(&s, std::move(config),
                                          {send_net->node()}, {ret_net});

  s.RunFor(TimeDelta::Seconds(10));
  send_net->PauseTransmissionUntil(s.Now() + TimeDelta::Seconds(10));
  s.RunFor(TimeDelta::Seconds(3));

  // As the dropframe is set, after 3 seconds without feedback from any sent
  // packets, we expect that the target rate is not reduced by congestion
  // window.
  EXPECT_GT(client->target_rate().kbps(), 300);
}

TEST(GoogCcScenario, PaddingRateLimitedByCongestionWindowInTrial) {
  Scenario s("googcc_unit/padding_limited", false);
  auto send_net =
      s.CreateMutableSimulationNode([=](NetworkSimulationConfig* c) {
        c->bandwidth = DataRate::KilobitsPerSec(1000);
        c->delay = TimeDelta::Millis(100);
      });
  auto ret_net = s.CreateSimulationNode(
      [](NetworkSimulationConfig* c) { c->delay = TimeDelta::Millis(100); });
  CallClientConfig config;
  config.field_trials.Set("WebRTC-CongestionWindow",
                          "QueueSize:200,MinBitrate:30000");
  // Start high so bandwidth drop has max effect.
  config.transport.rates.start_rate = DataRate::KilobitsPerSec(1000);
  config.transport.rates.max_rate = DataRate::KilobitsPerSec(2000);
  auto* client = s.CreateClient("send", config);
  auto* route =
      s.CreateRoutes(client, {send_net->node()},
                     s.CreateClient("return", CallClientConfig()), {ret_net});
  VideoStreamConfig video;
  video.stream.pad_to_rate = config.transport.rates.max_rate;
  s.CreateVideoStream(route->forward(), video);

  // Run for a few seconds to allow the controller to stabilize.
  s.RunFor(TimeDelta::Seconds(10));

  // Check that padding rate matches target rate.
  EXPECT_NEAR(client->padding_rate().kbps(), client->target_rate().kbps(), 1);

  // Check this is also the case when congestion window pushback kicks in.
  send_net->PauseTransmissionUntil(s.Now() + TimeDelta::Seconds(1));
  EXPECT_NEAR(client->padding_rate().kbps(), client->target_rate().kbps(), 1);
}

TEST(GoogCcScenario, LimitsToFloorIfRttIsHighInTrial) {
  // The field trial limits maximum RTT to 2 seconds, higher RTT means that the
  // controller backs off until it reaches the minimum configured bitrate. This
  // allows the RTT to recover faster than the regular control mechanism would
  // achieve.
  const DataRate kBandwidthFloor = DataRate::KilobitsPerSec(50);
  // In the test case, we limit the capacity and add a cross traffic packet
  // burst that blocks media from being sent. This causes the RTT to quickly
  // increase above the threshold in the trial.
  const DataRate kLinkCapacity = DataRate::KilobitsPerSec(100);
  const TimeDelta kBufferBloatDuration = TimeDelta::Seconds(10);
  Scenario s("googcc_unit/limit_trial", false);
  auto send_net = s.CreateSimulationNode([=](NetworkSimulationConfig* c) {
    c->bandwidth = kLinkCapacity;
    c->delay = TimeDelta::Millis(100);
  });
  auto ret_net = s.CreateSimulationNode(
      [](NetworkSimulationConfig* c) { c->delay = TimeDelta::Millis(100); });
  CallClientConfig config;
  config.field_trials.Set(
      "WebRTC-Bwe-MaxRttLimit",
      "limit:2s,floor:" + std::to_string(kBandwidthFloor.kbps()) + "kbps");
  config.transport.rates.start_rate = kLinkCapacity;

  auto* client = CreateVideoSendingClient(&s, config, {send_net}, {ret_net});
  // Run for a few seconds to allow the controller to stabilize.
  s.RunFor(TimeDelta::Seconds(10));
  const DataSize kBloatPacketSize = DataSize::Bytes(1000);
  const int kBloatPacketCount =
      static_cast<int>(kBufferBloatDuration * kLinkCapacity / kBloatPacketSize);
  // This will cause the RTT to be large for a while.
  s.TriggerPacketBurst({send_net}, kBloatPacketCount, kBloatPacketSize.bytes());
  // Wait to allow the high RTT to be detected and acted upon.
  s.RunFor(TimeDelta::Seconds(6));
  // By now the target rate should have dropped to the minimum configured rate.
  EXPECT_NEAR(client->target_rate().kbps(), kBandwidthFloor.kbps(), 5);
}

TEST(GoogCcScenario, UpdatesTargetRateBasedOnLinkCapacity) {
  UpdatesTargetRateBasedOnLinkCapacity();
}

TEST(GoogCcScenario, LossBasedControlDoesModestBackoffToHighLoss) {
  Scenario s("googcc_unit/high_loss_channel", false);
  CallClientConfig config;
  config.transport.rates.min_rate = DataRate::KilobitsPerSec(10);
  config.transport.rates.max_rate = DataRate::KilobitsPerSec(1500);
  config.transport.rates.start_rate = DataRate::KilobitsPerSec(300);
  auto send_net = s.CreateSimulationNode([](NetworkSimulationConfig* c) {
    c->bandwidth = DataRate::KilobitsPerSec(2000);
    c->delay = TimeDelta::Millis(200);
    c->loss_rate = 0.1;
  });
  auto ret_net = s.CreateSimulationNode(
      [](NetworkSimulationConfig* c) { c->delay = TimeDelta::Millis(200); });

  auto* client = CreateVideoSendingClient(&s, config, {send_net}, {ret_net});

  s.RunFor(TimeDelta::Seconds(120));
  EXPECT_GT(client->target_rate().kbps(), 100);
}

DataRate AverageBitrateAfterCrossInducedLoss(absl::string_view name) {
  Scenario s(name, false);
  NetworkSimulationConfig net_conf;
  net_conf.bandwidth = DataRate::KilobitsPerSec(1000);
  net_conf.delay = TimeDelta::Millis(100);
  // Short queue length means that we'll induce loss when sudden TCP traffic
  // spikes are induced. This corresponds to ca 200 ms for a packet size of 1000
  // bytes. Such limited buffers are common on for instance wifi routers.
  net_conf.packet_queue_length_limit = 25;

  auto send_net = {s.CreateSimulationNode(net_conf)};
  auto ret_net = {s.CreateSimulationNode(net_conf)};

  auto* client = s.CreateClient("send", CallClientConfig());
  auto* callee = s.CreateClient("return", CallClientConfig());
  auto* route = s.CreateRoutes(client, send_net, callee, ret_net);
  // TODO(srte): Make this work with RTX enabled or remove it.
  auto* video = s.CreateVideoStream(route->forward(), [](VideoStreamConfig* c) {
    c->stream.use_rtx = false;
  });
  s.RunFor(TimeDelta::Seconds(10));
  for (int i = 0; i < 4; ++i) {
    // Sends TCP cross traffic inducing loss.
    auto* tcp_traffic = s.net()->StartCrossTraffic(CreateFakeTcpCrossTraffic(
        s.net()->CreateRoute(send_net), s.net()->CreateRoute(ret_net),
        FakeTcpConfig()));
    s.RunFor(TimeDelta::Seconds(2));
    // Allow the ccongestion controller to recover.
    s.net()->StopCrossTraffic(tcp_traffic);
    s.RunFor(TimeDelta::Seconds(20));
  }

  // Querying the video stats from within the expected runtime environment
  // (i.e. the TQ that belongs to the CallClient, not the Scenario TQ that
  // we're currently on).
  VideoReceiveStreamInterface::Stats video_receive_stats;
  auto* video_stream = video->receive();
  callee->SendTask([&video_stream, &video_receive_stats]() {
    video_receive_stats = video_stream->GetStats();
  });
  return DataSize::Bytes(
             video_receive_stats.rtp_stats.packet_counter.TotalBytes()) /
         s.TimeSinceStart();
}

TEST(GoogCcScenario, MaintainsLowRateInSafeResetTrial) {
  const DataRate kLinkCapacity = DataRate::KilobitsPerSec(200);
  const DataRate kStartRate = DataRate::KilobitsPerSec(300);

  Scenario s("googcc_unit/safe_reset_low");
  auto* send_net = s.CreateSimulationNode([&](NetworkSimulationConfig* c) {
    c->bandwidth = kLinkCapacity;
    c->delay = TimeDelta::Millis(10);
  });
  auto* client = s.CreateClient("send", [&](CallClientConfig* c) {
    c->field_trials.Set("WebRTC-Bwe-SafeResetOnRouteChange", "Enabled");
    c->transport.rates.start_rate = kStartRate;
  });
  auto* route = s.CreateRoutes(
      client, {send_net}, s.CreateClient("return", CallClientConfig()),
      {s.CreateSimulationNode(NetworkSimulationConfig())});
  s.CreateVideoStream(route->forward(), VideoStreamConfig());
  // Allow the controller to stabilize.
  s.RunFor(TimeDelta::Millis(500));
  EXPECT_NEAR(client->send_bandwidth().kbps(), kLinkCapacity.kbps(), 50);
  s.ChangeRoute(route->forward(), {send_net});
  // Allow new settings to propagate.
  s.RunFor(TimeDelta::Millis(100));
  // Under the trial, the target should be unchanged for low rates.
  EXPECT_NEAR(client->send_bandwidth().kbps(), kLinkCapacity.kbps(), 50);
}

TEST(GoogCcScenario, DoNotResetBweUnlessNetworkAdapterChangeOnRoutChange) {
  Scenario s("googcc_unit/do_not_reset_bwe_unless_adapter_change");

  const DataRate kLinkCapacity = DataRate::KilobitsPerSec(1000);
  const DataRate kStartRate = DataRate::KilobitsPerSec(300);

  auto send_net = s.CreateSimulationNode([&](NetworkSimulationConfig* c) {
    c->bandwidth = kLinkCapacity;
    c->delay = TimeDelta::Millis(50);
  });
  auto* client = s.CreateClient("send", [&](CallClientConfig* c) {
    c->field_trials.Set("WebRTC-Bwe-ResetOnAdapterIdChange", "Enabled");
    c->transport.rates.start_rate = kStartRate;
  });
  client->UpdateNetworkAdapterId(0);
  auto* route = s.CreateRoutes(
      client, {send_net}, s.CreateClient("return", CallClientConfig()),
      {s.CreateSimulationNode(NetworkSimulationConfig())});
  s.CreateVideoStream(route->forward(), VideoStreamConfig());
  // Allow the controller to stabilize.
  s.RunFor(TimeDelta::Millis(500));
  EXPECT_NEAR(client->send_bandwidth().kbps(), kLinkCapacity.kbps(), 300);
  s.ChangeRoute(route->forward(), {send_net});
  // Allow new settings to propagate.
  s.RunFor(TimeDelta::Millis(50));
  // Under the trial, the target should not drop.
  EXPECT_NEAR(client->send_bandwidth().kbps(), kLinkCapacity.kbps(), 300);

  s.RunFor(TimeDelta::Millis(500));
  // But if adapter id change, BWE should reset and start from the beginning if
  // the network route changes.
  client->UpdateNetworkAdapterId(1);
  s.ChangeRoute(route->forward(), {send_net});
  // Allow new settings to propagate.
  s.RunFor(TimeDelta::Millis(50));
  EXPECT_NEAR(client->send_bandwidth().kbps(), kStartRate.kbps(), 30);
}

TEST(GoogCcScenario, CutsHighRateInSafeResetTrial) {
  const DataRate kLinkCapacity = DataRate::KilobitsPerSec(1000);
  const DataRate kStartRate = DataRate::KilobitsPerSec(300);

  Scenario s("googcc_unit/safe_reset_high_cut");
  auto send_net = s.CreateSimulationNode([&](NetworkSimulationConfig* c) {
    c->bandwidth = kLinkCapacity;
    c->delay = TimeDelta::Millis(50);
  });
  auto* client = s.CreateClient("send", [&](CallClientConfig* c) {
    c->field_trials.Set("WebRTC-Bwe-SafeResetOnRouteChange", "Enabled");
    c->transport.rates.start_rate = kStartRate;
  });
  auto* route = s.CreateRoutes(
      client, {send_net}, s.CreateClient("return", CallClientConfig()),
      {s.CreateSimulationNode(NetworkSimulationConfig())});
  s.CreateVideoStream(route->forward(), VideoStreamConfig());
  // Allow the controller to stabilize.
  s.RunFor(TimeDelta::Millis(500));
  EXPECT_NEAR(client->send_bandwidth().kbps(), kLinkCapacity.kbps(), 300);
  client->UpdateNetworkAdapterId(1);
  s.ChangeRoute(route->forward(), {send_net});
  // Allow new settings to propagate.
  s.RunFor(TimeDelta::Millis(50));
  // Under the trial, the target should be reset from high values.
  EXPECT_NEAR(client->send_bandwidth().kbps(), kStartRate.kbps(), 30);
}

TEST(GoogCcScenario, DetectsHighRateInSafeResetTrial) {
  const DataRate kInitialLinkCapacity = DataRate::KilobitsPerSec(200);
  const DataRate kNewLinkCapacity = DataRate::KilobitsPerSec(800);
  const DataRate kStartRate = DataRate::KilobitsPerSec(300);

  Scenario s("googcc_unit/safe_reset_high_detect");
  auto* initial_net = s.CreateSimulationNode([&](NetworkSimulationConfig* c) {
    c->bandwidth = kInitialLinkCapacity;
    c->delay = TimeDelta::Millis(50);
  });
  auto* new_net = s.CreateSimulationNode([&](NetworkSimulationConfig* c) {
    c->bandwidth = kNewLinkCapacity;
    c->delay = TimeDelta::Millis(50);
  });
  auto* client = s.CreateClient("send", [&](CallClientConfig* c) {
    c->field_trials.Set("WebRTC-Bwe-SafeResetOnRouteChange", "Enabled,ack");
    c->transport.rates.start_rate = kStartRate;
  });
  auto* route = s.CreateRoutes(
      client, {initial_net}, s.CreateClient("return", CallClientConfig()),
      {s.CreateSimulationNode(NetworkSimulationConfig())});
  s.CreateVideoStream(route->forward(), VideoStreamConfig());
  // Allow the controller to stabilize.
  s.RunFor(TimeDelta::Millis(2000));
  EXPECT_NEAR(client->send_bandwidth().kbps(), kInitialLinkCapacity.kbps(), 50);
  client->UpdateNetworkAdapterId(1);
  s.ChangeRoute(route->forward(), {new_net});
  // Allow new settings to propagate, but not probes to be received.
  s.RunFor(TimeDelta::Millis(50));
  // Under the field trial, the target rate should be unchanged since it's lower
  // than the starting rate.
  EXPECT_NEAR(client->send_bandwidth().kbps(), kInitialLinkCapacity.kbps(), 50);
  // However, probing should have made us detect the higher rate.
  // NOTE: This test causes high loss rate, and the loss-based estimator reduces
  // the bitrate, making the test fail if we wait longer than one second here.
  s.RunFor(TimeDelta::Millis(1000));
  EXPECT_GT(client->send_bandwidth().kbps(), kNewLinkCapacity.kbps() - 300);
}

TEST(GoogCcScenario, TargetRateReducedOnPacingBufferBuildupInTrial) {
  const DataRate kLinkCapacity = DataRate::KilobitsPerSec(1000);
  const DataRate kStartRate = DataRate::KilobitsPerSec(1000);

  Scenario s("googcc_unit/pacing_buffer_buildup");
  auto* net = s.CreateSimulationNode([&](NetworkSimulationConfig* c) {
    c->bandwidth = kLinkCapacity;
    c->delay = TimeDelta::Millis(50);
  });
  auto* client = s.CreateClient("send", [&](CallClientConfig* c) {
    c->field_trials.Set("WebRTC-CongestionWindow",
                        "QueueSize:100,MinBitrate:30000");
    c->field_trials.Set("WebRTC-Video-Pacing", "factor:1.0");
    c->field_trials.Set("WebRTC-AddPacingToCongestionWindowPushback",
                        "Enabled");
    c->transport.rates.start_rate = kStartRate;
  });
  auto* route = s.CreateRoutes(
      client, {net}, s.CreateClient("return", CallClientConfig()),
      {s.CreateSimulationNode(NetworkSimulationConfig())});
  s.CreateVideoStream(route->forward(), VideoStreamConfig());
  // Allow some time for the buffer to build up.
  s.RunFor(TimeDelta::Seconds(5));

  // Without trial, pacer delay reaches ~250 ms.
  EXPECT_LT(client->GetStats().pacer_delay_ms, 150);
}

TEST(GoogCcScenario, NoBandwidthTogglingInLossControlTrial) {
  Scenario s("googcc_unit/no_toggling");
  auto* send_net = s.CreateSimulationNode([&](NetworkSimulationConfig* c) {
    c->bandwidth = DataRate::KilobitsPerSec(2000);
    c->loss_rate = 0.2;
    c->delay = TimeDelta::Millis(10);
  });

  auto* client = s.CreateClient("send", [&](CallClientConfig* c) {
    c->field_trials.Set("WebRTC-Bwe-LossBasedControl", "Enabled");
    c->transport.rates.start_rate = DataRate::KilobitsPerSec(300);
  });
  auto* route = s.CreateRoutes(
      client, {send_net}, s.CreateClient("return", CallClientConfig()),
      {s.CreateSimulationNode(NetworkSimulationConfig())});
  s.CreateVideoStream(route->forward(), VideoStreamConfig());
  // Allow the controller to initialize.
  s.RunFor(TimeDelta::Millis(250));

  std::queue<DataRate> bandwidth_history;
  const TimeDelta step = TimeDelta::Millis(50);
  for (TimeDelta time = TimeDelta::Zero(); time < TimeDelta::Millis(2000);
       time += step) {
    s.RunFor(step);
    const TimeDelta window = TimeDelta::Millis(500);
    if (bandwidth_history.size() >= window / step)
      bandwidth_history.pop();
    bandwidth_history.push(client->send_bandwidth());
    EXPECT_LT(
        CountBandwidthDips(bandwidth_history, DataRate::KilobitsPerSec(100)),
        2);
  }
}

TEST(GoogCcScenario, NoRttBackoffCollapseWhenVideoStops) {
  Scenario s("googcc_unit/rttbackoff_video_stop");
  auto* send_net = s.CreateSimulationNode([&](NetworkSimulationConfig* c) {
    c->bandwidth = DataRate::KilobitsPerSec(2000);
    c->delay = TimeDelta::Millis(100);
  });

  auto* client = s.CreateClient("send", [&](CallClientConfig* c) {
    c->field_trials.Set("WebRTC-Bwe-MaxRttLimit", "limit:2s");
    c->transport.rates.start_rate = DataRate::KilobitsPerSec(1000);
  });
  auto* route = s.CreateRoutes(
      client, {send_net}, s.CreateClient("return", CallClientConfig()),
      {s.CreateSimulationNode(NetworkSimulationConfig())});
  auto* video = s.CreateVideoStream(route->forward(), VideoStreamConfig());
  // Allow the controller to initialize, then stop video.
  s.RunFor(TimeDelta::Seconds(1));
  video->send()->Stop();
  s.RunFor(TimeDelta::Seconds(4));
  EXPECT_GT(client->send_bandwidth().kbps(), 1000);
}

TEST(GoogCcScenario, NoCrashOnVeryLateFeedback) {
  Scenario s;
  auto ret_net = s.CreateMutableSimulationNode(NetworkSimulationConfig());
  auto* route = s.CreateRoutes(
      s.CreateClient("send", CallClientConfig()),
      {s.CreateSimulationNode(NetworkSimulationConfig())},
      s.CreateClient("return", CallClientConfig()), {ret_net->node()});
  auto* video = s.CreateVideoStream(route->forward(), VideoStreamConfig());
  s.RunFor(TimeDelta::Seconds(5));
  // Delay feedback by several minutes. This will cause removal of the send time
  // history for the packets as long as kSendTimeHistoryWindow is configured for
  // a shorter time span.
  ret_net->PauseTransmissionUntil(s.Now() + TimeDelta::Seconds(300));
  // Stopping video stream while waiting to save test execution time.
  video->send()->Stop();
  s.RunFor(TimeDelta::Seconds(299));
  // Starting to cause addition of new packet to history, which cause old
  // packets to be removed.
  video->send()->Start();
  // Runs until the lost packets are received. We expect that this will run
  // without causing any runtime failures.
  s.RunFor(TimeDelta::Seconds(2));
}

TEST(GoogCcScenario, IsFairToTCP) {
  Scenario s("googcc_unit/tcp_fairness");
  NetworkSimulationConfig net_conf;
  net_conf.bandwidth = DataRate::KilobitsPerSec(1000);
  net_conf.delay = TimeDelta::Millis(50);
  auto* client = s.CreateClient("send", [&](CallClientConfig* c) {
    c->transport.rates.start_rate = DataRate::KilobitsPerSec(1000);
  });
  auto send_net = {s.CreateSimulationNode(net_conf)};
  auto ret_net = {s.CreateSimulationNode(net_conf)};
  auto* route = s.CreateRoutes(
      client, send_net, s.CreateClient("return", CallClientConfig()), ret_net);
  s.CreateVideoStream(route->forward(), VideoStreamConfig());
  s.net()->StartCrossTraffic(CreateFakeTcpCrossTraffic(
      s.net()->CreateRoute(send_net), s.net()->CreateRoute(ret_net),
      FakeTcpConfig()));
  s.RunFor(TimeDelta::Seconds(10));

  // Currently only testing for the upper limit as we in practice back out
  // quite a lot in this scenario. If this behavior is fixed, we should add a
  // lower bound to ensure it stays fixed.
  EXPECT_LT(client->send_bandwidth().kbps(), 750);
}

TEST(GoogCcScenario, FastRampupOnRembCapLifted) {
  DataRate final_estimate =
      RunRembDipScenario("googcc_unit/default_fast_rampup_on_remb_cap_lifted");
  EXPECT_GT(final_estimate.kbps(), 1500);
}

TEST(GoogCcScenario, FallbackToLossBasedBweWithoutPacketFeedback) {
  const DataRate kLinkCapacity = DataRate::KilobitsPerSec(1000);
  const DataRate kStartRate = DataRate::KilobitsPerSec(1000);

  Scenario s("googcc_unit/high_loss_channel", false);
  auto* net = s.CreateMutableSimulationNode([&](NetworkSimulationConfig* c) {
    c->bandwidth = kLinkCapacity;
    c->delay = TimeDelta::Millis(100);
  });
  auto* client = s.CreateClient("send", [&](CallClientConfig* c) {
    c->transport.rates.start_rate = kStartRate;
  });
  auto* route = s.CreateRoutes(
      client, {net->node()}, s.CreateClient("return", CallClientConfig()),
      {s.CreateSimulationNode(NetworkSimulationConfig())});

  // Create a config without packet feedback.
  VideoStreamConfig video_config;
  video_config.stream.packet_feedback = false;
  s.CreateVideoStream(route->forward(), video_config);

  s.RunFor(TimeDelta::Seconds(20));
  // Bandwith does not backoff because network is normal.
  EXPECT_GE(client->target_rate().kbps(), 500);

  // Update the network to create high loss ratio
  net->UpdateConfig([](NetworkSimulationConfig* c) { c->loss_rate = 0.15; });
  s.RunFor(TimeDelta::Seconds(20));

  // Bandwidth decreases thanks to loss based bwe v0.
  EXPECT_LE(client->target_rate().kbps(), 300);
}

}  // namespace test
}  // namespace webrtc
