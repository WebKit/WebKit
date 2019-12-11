/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <queue>

#include "api/transport/goog_cc_factory.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/scenario/scenario.h"

using ::testing::_;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Property;

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
GoogCcNetworkControllerFactory CreateFeedbackOnlyFactory() {
  GoogCcFactoryConfig config;
  config.feedback_only = true;
  return GoogCcNetworkControllerFactory(std::move(config));
}

const uint32_t kInitialBitrateKbps = 60;
const DataRate kInitialBitrate = DataRate::kbps(kInitialBitrateKbps);
const float kDefaultPacingRate = 2.5f;

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

void UpdatesTargetRateBasedOnLinkCapacity(std::string test_name = "") {
  auto factory = CreateFeedbackOnlyFactory();
  Scenario s("googcc_unit/target_capacity" + test_name, false);
  CallClientConfig config;
  config.transport.cc_factory = &factory;
  config.transport.rates.min_rate = DataRate::kbps(10);
  config.transport.rates.max_rate = DataRate::kbps(1500);
  config.transport.rates.start_rate = DataRate::kbps(300);
  auto send_net = s.CreateMutableSimulationNode([](NetworkSimulationConfig* c) {
    c->bandwidth = DataRate::kbps(500);
    c->delay = TimeDelta::ms(100);
    c->loss_rate = 0.0;
  });
  auto ret_net = s.CreateMutableSimulationNode(
      [](NetworkSimulationConfig* c) { c->delay = TimeDelta::ms(100); });
  StatesPrinter* truth = s.CreatePrinter(
      "send.truth.txt", TimeDelta::PlusInfinity(), {send_net->ConfigPrinter()});

  auto* client = CreateVideoSendingClient(&s, config, {send_net->node()},
                                          {ret_net->node()});

  truth->PrintRow();
  s.RunFor(TimeDelta::seconds(25));
  truth->PrintRow();
  EXPECT_NEAR(client->target_rate().kbps(), 450, 100);

  send_net->UpdateConfig([](NetworkSimulationConfig* c) {
    c->bandwidth = DataRate::kbps(800);
    c->delay = TimeDelta::ms(100);
  });

  truth->PrintRow();
  s.RunFor(TimeDelta::seconds(20));
  truth->PrintRow();
  EXPECT_NEAR(client->target_rate().kbps(), 750, 150);

  send_net->UpdateConfig([](NetworkSimulationConfig* c) {
    c->bandwidth = DataRate::kbps(100);
    c->delay = TimeDelta::ms(200);
  });
  ret_net->UpdateConfig(
      [](NetworkSimulationConfig* c) { c->delay = TimeDelta::ms(200); });

  truth->PrintRow();
  s.RunFor(TimeDelta::seconds(50));
  truth->PrintRow();
  EXPECT_NEAR(client->target_rate().kbps(), 90, 25);
}
}  // namespace

class GoogCcNetworkControllerTest : public ::testing::Test {
 protected:
  GoogCcNetworkControllerTest()
      : current_time_(Timestamp::ms(123456)), factory_() {}
  ~GoogCcNetworkControllerTest() override {}

  void SetUp() override {
    controller_ = factory_.Create(InitialConfig());
    NetworkControlUpdate update =
        controller_->OnProcessInterval(DefaultInterval());
    EXPECT_EQ(update.target_rate->target_rate, kInitialBitrate);
    EXPECT_EQ(update.pacer_config->data_rate(),
              kInitialBitrate * kDefaultPacingRate);

    EXPECT_EQ(update.probe_cluster_configs[0].target_data_rate,
              kInitialBitrate * 3);
    EXPECT_EQ(update.probe_cluster_configs[1].target_data_rate,
              kInitialBitrate * 5);
  }
  // Custom setup - use an observer that tracks the target bitrate, without
  // prescribing on which iterations it must change (like a mock would).
  void TargetBitrateTrackingSetup() {
    controller_ = factory_.Create(InitialConfig());
    controller_->OnProcessInterval(DefaultInterval());
  }

  NetworkControllerConfig InitialConfig(
      int starting_bandwidth_kbps = kInitialBitrateKbps,
      int min_data_rate_kbps = 0,
      int max_data_rate_kbps = 5 * kInitialBitrateKbps) {
    NetworkControllerConfig config;
    config.constraints.at_time = current_time_;
    config.constraints.min_data_rate = DataRate::kbps(min_data_rate_kbps);
    config.constraints.max_data_rate = DataRate::kbps(max_data_rate_kbps);
    config.constraints.starting_rate = DataRate::kbps(starting_bandwidth_kbps);
    config.event_log = &event_log_;
    return config;
  }
  ProcessInterval DefaultInterval() {
    ProcessInterval interval;
    interval.at_time = current_time_;
    return interval;
  }
  RemoteBitrateReport CreateBitrateReport(DataRate rate) {
    RemoteBitrateReport report;
    report.receive_time = current_time_;
    report.bandwidth = rate;
    return report;
  }
  PacketResult CreateResult(int64_t arrival_time_ms,
                            int64_t send_time_ms,
                            size_t payload_size,
                            PacedPacketInfo pacing_info) {
    PacketResult packet_result;
    packet_result.sent_packet = SentPacket();
    packet_result.sent_packet.send_time = Timestamp::ms(send_time_ms);
    packet_result.sent_packet.size = DataSize::bytes(payload_size);
    packet_result.sent_packet.pacing_info = pacing_info;
    packet_result.receive_time = Timestamp::ms(arrival_time_ms);
    return packet_result;
  }

  NetworkRouteChange CreateRouteChange(
      absl::optional<DataRate> start_rate = absl::nullopt,
      absl::optional<DataRate> min_rate = absl::nullopt,
      absl::optional<DataRate> max_rate = absl::nullopt) {
    NetworkRouteChange route_change;
    route_change.at_time = current_time_;
    route_change.constraints.at_time = current_time_;
    route_change.constraints.min_data_rate = min_rate;
    route_change.constraints.max_data_rate = max_rate;
    route_change.constraints.starting_rate = start_rate;
    return route_change;
  }

  void AdvanceTimeMilliseconds(int timedelta_ms) {
    current_time_ += TimeDelta::ms(timedelta_ms);
  }

  void OnUpdate(NetworkControlUpdate update) {
    if (update.target_rate)
      target_bitrate_ = update.target_rate->target_rate;
  }

  void PacketTransmissionAndFeedbackBlock(int64_t runtime_ms, int64_t delay) {
    int64_t delay_buildup = 0;
    int64_t start_time_ms = current_time_.ms();
    while (current_time_.ms() - start_time_ms < runtime_ms) {
      constexpr size_t kPayloadSize = 1000;
      PacketResult packet =
          CreateResult(current_time_.ms() + delay_buildup, current_time_.ms(),
                       kPayloadSize, PacedPacketInfo());
      delay_buildup += delay;
      controller_->OnSentPacket(packet.sent_packet);
      TransportPacketsFeedback feedback;
      feedback.feedback_time = packet.receive_time;
      feedback.packet_feedbacks.push_back(packet);
      OnUpdate(controller_->OnTransportPacketsFeedback(feedback));
      AdvanceTimeMilliseconds(50);
      OnUpdate(controller_->OnProcessInterval(DefaultInterval()));
    }
  }
  Timestamp current_time_;
  absl::optional<DataRate> target_bitrate_;
  NiceMock<MockRtcEventLog> event_log_;
  GoogCcNetworkControllerFactory factory_;
  std::unique_ptr<NetworkControllerInterface> controller_;
};

TEST_F(GoogCcNetworkControllerTest, ReactsToChangedNetworkConditions) {
  // Test no change.
  AdvanceTimeMilliseconds(25);
  controller_->OnProcessInterval(DefaultInterval());

  NetworkControlUpdate update;
  controller_->OnRemoteBitrateReport(CreateBitrateReport(kInitialBitrate * 2));
  AdvanceTimeMilliseconds(25);
  update = controller_->OnProcessInterval(DefaultInterval());
  EXPECT_EQ(update.target_rate->target_rate, kInitialBitrate * 2);
  EXPECT_EQ(update.pacer_config->data_rate(),
            kInitialBitrate * 2 * kDefaultPacingRate);

  controller_->OnRemoteBitrateReport(CreateBitrateReport(kInitialBitrate));
  AdvanceTimeMilliseconds(25);
  update = controller_->OnProcessInterval(DefaultInterval());
  EXPECT_EQ(update.target_rate->target_rate, kInitialBitrate);
  EXPECT_EQ(update.pacer_config->data_rate(),
            kInitialBitrate * kDefaultPacingRate);
}

// Test congestion window pushback on network delay happens.
TEST_F(GoogCcNetworkControllerTest, CongestionWindowPushbackOnNetworkDelay) {
  auto factory = CreateFeedbackOnlyFactory();
  ScopedFieldTrials trial(
      "WebRTC-CongestionWindow/QueueSize:800,MinBitrate:30000/");
  Scenario s("googcc_unit/cwnd_on_delay", false);
  auto send_net =
      s.CreateMutableSimulationNode([=](NetworkSimulationConfig* c) {
        c->bandwidth = DataRate::kbps(1000);
        c->delay = TimeDelta::ms(100);
      });
  auto ret_net = s.CreateSimulationNode(
      [](NetworkSimulationConfig* c) { c->delay = TimeDelta::ms(100); });
  CallClientConfig config;
  config.transport.cc_factory = &factory;
  // Start high so bandwidth drop has max effect.
  config.transport.rates.start_rate = DataRate::kbps(300);
  config.transport.rates.max_rate = DataRate::kbps(2000);
  config.transport.rates.min_rate = DataRate::kbps(10);

  auto* client = CreateVideoSendingClient(&s, std::move(config),
                                          {send_net->node()}, {ret_net});

  s.RunFor(TimeDelta::seconds(10));
  send_net->PauseTransmissionUntil(s.Now() + TimeDelta::seconds(10));
  s.RunFor(TimeDelta::seconds(3));

  // After 3 seconds without feedback from any sent packets, we expect that the
  // target rate is reduced to the minimum pushback threshold
  // kDefaultMinPushbackTargetBitrateBps, which is defined as 30 kbps in
  // congestion_window_pushback_controller.
  EXPECT_LT(client->target_rate().kbps(), 40);
}

TEST_F(GoogCcNetworkControllerTest, OnNetworkRouteChanged) {
  NetworkControlUpdate update;
  DataRate new_bitrate = DataRate::bps(200000);
  update = controller_->OnNetworkRouteChange(CreateRouteChange(new_bitrate));
  EXPECT_EQ(update.target_rate->target_rate, new_bitrate);
  EXPECT_EQ(update.pacer_config->data_rate(), new_bitrate * kDefaultPacingRate);
  EXPECT_EQ(update.probe_cluster_configs.size(), 2u);

  // If the bitrate is reset to -1, the new starting bitrate will be
  // the minimum default bitrate.
  const DataRate kDefaultMinBitrate = DataRate::kbps(5);
  update = controller_->OnNetworkRouteChange(CreateRouteChange());
  EXPECT_EQ(update.target_rate->target_rate, kDefaultMinBitrate);
  EXPECT_NEAR(update.pacer_config->data_rate().bps<double>(),
              kDefaultMinBitrate.bps<double>() * kDefaultPacingRate, 10);
  EXPECT_EQ(update.probe_cluster_configs.size(), 2u);
}

TEST_F(GoogCcNetworkControllerTest, ProbeOnRouteChange) {
  NetworkControlUpdate update;
  update = controller_->OnNetworkRouteChange(CreateRouteChange(
      2 * kInitialBitrate, DataRate::Zero(), 20 * kInitialBitrate));

  EXPECT_TRUE(update.pacer_config.has_value());
  EXPECT_EQ(update.target_rate->target_rate, kInitialBitrate * 2);
  EXPECT_EQ(update.probe_cluster_configs.size(), 2u);
  EXPECT_EQ(update.probe_cluster_configs[0].target_data_rate,
            kInitialBitrate * 6);
  EXPECT_EQ(update.probe_cluster_configs[1].target_data_rate,
            kInitialBitrate * 12);

  update = controller_->OnProcessInterval(DefaultInterval());
}

// Bandwidth estimation is updated when feedbacks are received.
// Feedbacks which show an increasing delay cause the estimation to be reduced.
TEST_F(GoogCcNetworkControllerTest, UpdatesDelayBasedEstimate) {
  TargetBitrateTrackingSetup();
  const int64_t kRunTimeMs = 6000;

  // The test must run and insert packets/feedback long enough that the
  // BWE computes a valid estimate. This is first done in an environment which
  // simulates no bandwidth limitation, and therefore not built-up delay.
  PacketTransmissionAndFeedbackBlock(kRunTimeMs, 0);
  ASSERT_TRUE(target_bitrate_.has_value());

  // Repeat, but this time with a building delay, and make sure that the
  // estimation is adjusted downwards.
  DataRate bitrate_before_delay = *target_bitrate_;
  PacketTransmissionAndFeedbackBlock(kRunTimeMs, 50);
  EXPECT_LT(*target_bitrate_, bitrate_before_delay);
}

TEST_F(GoogCcNetworkControllerTest,
       PaddingRateLimitedByCongestionWindowInTrial) {
  ScopedFieldTrials trial(
      "WebRTC-CongestionWindow/QueueSize:200,MinBitrate:30000/");

  Scenario s("googcc_unit/padding_limited", false);
  auto send_net =
      s.CreateMutableSimulationNode([=](NetworkSimulationConfig* c) {
        c->bandwidth = DataRate::kbps(1000);
        c->delay = TimeDelta::ms(100);
      });
  auto ret_net = s.CreateSimulationNode(
      [](NetworkSimulationConfig* c) { c->delay = TimeDelta::ms(100); });
  CallClientConfig config;
  // Start high so bandwidth drop has max effect.
  config.transport.rates.start_rate = DataRate::kbps(1000);
  config.transport.rates.max_rate = DataRate::kbps(2000);
  auto* client = s.CreateClient("send", config);
  auto* route =
      s.CreateRoutes(client, {send_net->node()},
                     s.CreateClient("return", CallClientConfig()), {ret_net});
  VideoStreamConfig video;
  video.stream.pad_to_rate = config.transport.rates.max_rate;
  s.CreateVideoStream(route->forward(), video);

  // Run for a few seconds to allow the controller to stabilize.
  s.RunFor(TimeDelta::seconds(10));

  // Check that padding rate matches target rate.
  EXPECT_NEAR(client->padding_rate().kbps(), client->target_rate().kbps(), 1);

  // Check this is also the case when congestion window pushback kicks in.
  send_net->PauseTransmissionUntil(s.Now() + TimeDelta::seconds(1));
  EXPECT_NEAR(client->padding_rate().kbps(), client->target_rate().kbps(), 1);
}

TEST_F(GoogCcNetworkControllerTest,
       NoCongestionWindowPushbackWithoutReceiveTraffic) {
  ScopedFieldTrials trial(
      "WebRTC-CongestionWindow/QueueSize:800,MinBitrate:30000/"
      "WebRTC-Bwe-CongestionWindowDownlinkDelay/Enabled/");
  Scenario s("googcc_unit/cwnd_no_downlink", false);
  NetworkSimulationConfig net_conf;
  net_conf.bandwidth = DataRate::kbps(1000);
  net_conf.delay = TimeDelta::ms(100);
  auto send_net = s.CreateSimulationNode(net_conf);
  auto ret_net = s.CreateMutableSimulationNode(net_conf);

  auto* client = s.CreateClient("sender", CallClientConfig());
  auto* route = s.CreateRoutes(client, {send_net},
                               s.CreateClient("return", CallClientConfig()),
                               {ret_net->node()});

  s.CreateVideoStream(route->forward(), VideoStreamConfig());
  // A return video stream ensures we get steady traffic stream,
  // so we can better differentiate between send being down and return
  // being down.
  s.CreateVideoStream(route->reverse(), VideoStreamConfig());

  // Wait to stabilize the bandwidth estimate.
  s.RunFor(TimeDelta::seconds(10));
  // Disabling the return triggers the data window expansion logic
  // which will stop the congestion window from activating.
  ret_net->PauseTransmissionUntil(s.Now() + TimeDelta::seconds(10));
  s.RunFor(TimeDelta::seconds(5));

  // Expect that we never lost send speed because we received no packets.
  // 500kbps is enough to demonstrate that congestion window isn't activated.
  EXPECT_GE(client->target_rate().kbps(), 500);
}

TEST_F(GoogCcNetworkControllerTest, CongestionWindowPushBackOnSendDelaySpike) {
  ScopedFieldTrials trial(
      "WebRTC-CongestionWindow/QueueSize:800,MinBitrate:30000/"
      "WebRTC-Bwe-CongestionWindowDownlinkDelay/Enabled/");
  Scenario s("googcc_unit/cwnd_actives_no_feedback", false);
  NetworkSimulationConfig net_conf;
  net_conf.bandwidth = DataRate::kbps(1000);
  net_conf.delay = TimeDelta::ms(100);
  auto send_net = s.CreateMutableSimulationNode(net_conf);
  auto ret_net = s.CreateSimulationNode(net_conf);

  auto* client = s.CreateClient("sender", CallClientConfig());
  auto* route =
      s.CreateRoutes(client, {send_net->node()},
                     s.CreateClient("return", CallClientConfig()), {ret_net});

  s.CreateVideoStream(route->forward(), VideoStreamConfig());
  // A return video stream ensures we get steady traffic stream,
  // so we can better differentiate between send being down and return
  // being down.
  s.CreateVideoStream(route->reverse(), VideoStreamConfig());

  // Wait to stabilize the bandwidth estimate.
  s.RunFor(TimeDelta::seconds(10));
  // Send being down triggers congestion window pushback.
  send_net->PauseTransmissionUntil(s.Now() + TimeDelta::seconds(10));
  s.RunFor(TimeDelta::seconds(3));

  // Expect the target rate to be reduced rapidly due to congestion.
  // We would expect things to be at 30kbps, the min bitrate. Note
  // that the congestion window still gets activated since we are
  // receiving packets upstream.
  EXPECT_LT(client->target_rate().kbps(), 100);
  EXPECT_GE(client->target_rate().kbps(), 30);
}

TEST_F(GoogCcNetworkControllerTest, LimitsToFloorIfRttIsHighInTrial) {
  // The field trial limits maximum RTT to 2 seconds, higher RTT means that the
  // controller backs off until it reaches the minimum configured bitrate. This
  // allows the RTT to recover faster than the regular control mechanism would
  // achieve.
  const DataRate kBandwidthFloor = DataRate::kbps(50);
  ScopedFieldTrials trial("WebRTC-Bwe-MaxRttLimit/limit:2s,floor:" +
                          std::to_string(kBandwidthFloor.kbps()) + "kbps/");
  // In the test case, we limit the capacity and add a cross traffic packet
  // burst that blocks media from being sent. This causes the RTT to quickly
  // increase above the threshold in the trial.
  const DataRate kLinkCapacity = DataRate::kbps(100);
  const TimeDelta kBufferBloatDuration = TimeDelta::seconds(10);
  Scenario s("googcc_unit/limit_trial", false);
  auto send_net = s.CreateSimulationNode([=](NetworkSimulationConfig* c) {
    c->bandwidth = kLinkCapacity;
    c->delay = TimeDelta::ms(100);
  });
  auto ret_net = s.CreateSimulationNode(
      [](NetworkSimulationConfig* c) { c->delay = TimeDelta::ms(100); });
  CallClientConfig config;
  config.transport.rates.start_rate = kLinkCapacity;

  auto* client = CreateVideoSendingClient(&s, config, {send_net}, {ret_net});
  // Run for a few seconds to allow the controller to stabilize.
  s.RunFor(TimeDelta::seconds(10));
  const DataSize kBloatPacketSize = DataSize::bytes(1000);
  const int kBloatPacketCount =
      static_cast<int>(kBufferBloatDuration * kLinkCapacity / kBloatPacketSize);
  // This will cause the RTT to be large for a while.
  s.TriggerPacketBurst({send_net}, kBloatPacketCount, kBloatPacketSize.bytes());
  // Wait to allow the high RTT to be detected and acted upon.
  s.RunFor(TimeDelta::seconds(4));
  // By now the target rate should have dropped to the minimum configured rate.
  EXPECT_NEAR(client->target_rate().kbps(), kBandwidthFloor.kbps(), 1);
}

TEST_F(GoogCcNetworkControllerTest, UpdatesTargetRateBasedOnLinkCapacity) {
  UpdatesTargetRateBasedOnLinkCapacity();
}

TEST_F(GoogCcNetworkControllerTest, StableEstimateDoesNotVaryInSteadyState) {
  auto factory = CreateFeedbackOnlyFactory();
  Scenario s("googcc_unit/stable_target", false);
  CallClientConfig config;
  config.transport.cc_factory = &factory;
  NetworkSimulationConfig net_conf;
  net_conf.bandwidth = DataRate::kbps(500);
  net_conf.delay = TimeDelta::ms(100);
  auto send_net = s.CreateSimulationNode(net_conf);
  auto ret_net = s.CreateSimulationNode(net_conf);

  auto* client = CreateVideoSendingClient(&s, config, {send_net}, {ret_net});
  // Run for a while to allow the estimate to stabilize.
  s.RunFor(TimeDelta::seconds(30));
  DataRate min_stable_target = DataRate::PlusInfinity();
  DataRate max_stable_target = DataRate::MinusInfinity();
  DataRate min_target = DataRate::PlusInfinity();
  DataRate max_target = DataRate::MinusInfinity();

  // Measure variation in steady state.
  for (int i = 0; i < 20; ++i) {
    auto stable_target_rate = client->stable_target_rate();
    auto target_rate = client->link_capacity();
    EXPECT_LE(stable_target_rate, target_rate);

    min_stable_target = std::min(min_stable_target, stable_target_rate);
    max_stable_target = std::max(max_stable_target, stable_target_rate);
    min_target = std::min(min_target, target_rate);
    max_target = std::max(max_target, target_rate);
    s.RunFor(TimeDelta::seconds(1));
  }
  // We should expect drops by at least 15% (default backoff.)
  EXPECT_LT(min_target / max_target, 0.85);
  // We should expect the stable target to be more stable than the immediate one
  EXPECT_GE(min_stable_target / max_stable_target, min_target / max_target);
}

TEST_F(GoogCcNetworkControllerTest,
       LossBasedControlUpdatesTargetRateBasedOnLinkCapacity) {
  ScopedFieldTrials trial("WebRTC-Bwe-LossBasedControl/Enabled/");
  // TODO(srte): Should the behavior be unaffected at low loss rates?
  UpdatesTargetRateBasedOnLinkCapacity("_loss_based");
}

TEST_F(GoogCcNetworkControllerTest,
       LossBasedControlDoesModestBackoffToHighLoss) {
  ScopedFieldTrials trial("WebRTC-Bwe-LossBasedControl/Enabled/");
  Scenario s("googcc_unit/high_loss_channel", false);
  CallClientConfig config;
  config.transport.rates.min_rate = DataRate::kbps(10);
  config.transport.rates.max_rate = DataRate::kbps(1500);
  config.transport.rates.start_rate = DataRate::kbps(300);
  auto send_net = s.CreateSimulationNode([](NetworkSimulationConfig* c) {
    c->bandwidth = DataRate::kbps(2000);
    c->delay = TimeDelta::ms(200);
    c->loss_rate = 0.1;
  });
  auto ret_net = s.CreateSimulationNode(
      [](NetworkSimulationConfig* c) { c->delay = TimeDelta::ms(200); });

  auto* client = CreateVideoSendingClient(&s, config, {send_net}, {ret_net});

  s.RunFor(TimeDelta::seconds(120));
  // Without LossBasedControl trial, bandwidth drops to ~10 kbps.
  EXPECT_GT(client->target_rate().kbps(), 100);
}

TEST_F(GoogCcNetworkControllerTest, LossBasedEstimatorCapsRateAtModerateLoss) {
  ScopedFieldTrials trial("WebRTC-Bwe-LossBasedControl/Enabled/");
  Scenario s("googcc_unit/moderate_loss_channel", false);
  CallClientConfig config;
  config.transport.rates.min_rate = DataRate::kbps(10);
  config.transport.rates.max_rate = DataRate::kbps(5000);
  config.transport.rates.start_rate = DataRate::kbps(1000);

  NetworkSimulationConfig network;
  network.bandwidth = DataRate::kbps(2000);
  network.delay = TimeDelta::ms(100);
  // 3% loss rate is in the moderate loss rate region at 2000 kbps, limiting the
  // bitrate increase.
  network.loss_rate = 0.03;
  auto send_net = s.CreateMutableSimulationNode(network);
  auto* client = s.CreateClient("send", std::move(config));
  auto* route = s.CreateRoutes(client, {send_net->node()},
                               s.CreateClient("return", CallClientConfig()),
                               {s.CreateSimulationNode(network)});
  s.CreateVideoStream(route->forward(), VideoStreamConfig());
  // Allow the controller to stabilize at the lower bitrate.
  s.RunFor(TimeDelta::seconds(1));
  // This increase in capacity would cause the target bitrate to increase to
  // over 4000 kbps without LossBasedControl.
  send_net->UpdateConfig(
      [](NetworkSimulationConfig* c) { c->bandwidth = DataRate::kbps(5000); });
  s.RunFor(TimeDelta::seconds(20));
  // Using LossBasedControl, the bitrate will not increase over 2500 kbps since
  // we have detected moderate loss.
  EXPECT_LT(client->target_rate().kbps(), 2500);
}

TEST_F(GoogCcNetworkControllerTest, MaintainsLowRateInSafeResetTrial) {
  const DataRate kLinkCapacity = DataRate::kbps(200);
  const DataRate kStartRate = DataRate::kbps(300);

  ScopedFieldTrials trial("WebRTC-Bwe-SafeResetOnRouteChange/Enabled/");
  Scenario s("googcc_unit/safe_reset_low");
  auto* send_net = s.CreateSimulationNode([&](NetworkSimulationConfig* c) {
    c->bandwidth = kLinkCapacity;
    c->delay = TimeDelta::ms(10);
  });
  auto* client = s.CreateClient("send", [&](CallClientConfig* c) {
    c->transport.rates.start_rate = kStartRate;
  });
  auto* route = s.CreateRoutes(
      client, {send_net}, s.CreateClient("return", CallClientConfig()),
      {s.CreateSimulationNode(NetworkSimulationConfig())});
  s.CreateVideoStream(route->forward(), VideoStreamConfig());
  // Allow the controller to stabilize.
  s.RunFor(TimeDelta::ms(500));
  EXPECT_NEAR(client->send_bandwidth().kbps(), kLinkCapacity.kbps(), 50);
  s.ChangeRoute(route->forward(), {send_net});
  // Allow new settings to propagate.
  s.RunFor(TimeDelta::ms(100));
  // Under the trial, the target should be unchanged for low rates.
  EXPECT_NEAR(client->send_bandwidth().kbps(), kLinkCapacity.kbps(), 50);
}

TEST_F(GoogCcNetworkControllerTest, CutsHighRateInSafeResetTrial) {
  const DataRate kLinkCapacity = DataRate::kbps(1000);
  const DataRate kStartRate = DataRate::kbps(300);

  ScopedFieldTrials trial("WebRTC-Bwe-SafeResetOnRouteChange/Enabled/");
  Scenario s("googcc_unit/safe_reset_high_cut");
  auto send_net = s.CreateSimulationNode([&](NetworkSimulationConfig* c) {
    c->bandwidth = kLinkCapacity;
    c->delay = TimeDelta::ms(50);
  });
  auto* client = s.CreateClient("send", [&](CallClientConfig* c) {
    c->transport.rates.start_rate = kStartRate;
  });
  auto* route = s.CreateRoutes(
      client, {send_net}, s.CreateClient("return", CallClientConfig()),
      {s.CreateSimulationNode(NetworkSimulationConfig())});
  s.CreateVideoStream(route->forward(), VideoStreamConfig());
  // Allow the controller to stabilize.
  s.RunFor(TimeDelta::ms(500));
  EXPECT_NEAR(client->send_bandwidth().kbps(), kLinkCapacity.kbps(), 300);
  s.ChangeRoute(route->forward(), {send_net});
  // Allow new settings to propagate.
  s.RunFor(TimeDelta::ms(50));
  // Under the trial, the target should be reset from high values.
  EXPECT_NEAR(client->send_bandwidth().kbps(), kStartRate.kbps(), 30);
}

TEST_F(GoogCcNetworkControllerTest, DetectsHighRateInSafeResetTrial) {
  ScopedFieldTrials trial(
      "WebRTC-Bwe-SafeResetOnRouteChange/Enabled,ack/"
      "WebRTC-Bwe-ProbeRateFallback/Enabled/");
  const DataRate kInitialLinkCapacity = DataRate::kbps(200);
  const DataRate kNewLinkCapacity = DataRate::kbps(800);
  const DataRate kStartRate = DataRate::kbps(300);

  Scenario s("googcc_unit/safe_reset_high_detect");
  auto* initial_net = s.CreateSimulationNode([&](NetworkSimulationConfig* c) {
    c->bandwidth = kInitialLinkCapacity;
    c->delay = TimeDelta::ms(50);
  });
  auto* new_net = s.CreateSimulationNode([&](NetworkSimulationConfig* c) {
    c->bandwidth = kNewLinkCapacity;
    c->delay = TimeDelta::ms(50);
  });
  auto* client = s.CreateClient("send", [&](CallClientConfig* c) {
    c->transport.rates.start_rate = kStartRate;
  });
  auto* route = s.CreateRoutes(
      client, {initial_net}, s.CreateClient("return", CallClientConfig()),
      {s.CreateSimulationNode(NetworkSimulationConfig())});
  s.CreateVideoStream(route->forward(), VideoStreamConfig());
  // Allow the controller to stabilize.
  s.RunFor(TimeDelta::ms(1000));
  EXPECT_NEAR(client->send_bandwidth().kbps(), kInitialLinkCapacity.kbps(), 50);
  s.ChangeRoute(route->forward(), {new_net});
  // Allow new settings to propagate, but not probes to be received.
  s.RunFor(TimeDelta::ms(50));
  // Under the field trial, the target rate should be unchanged since it's lower
  // than the starting rate.
  EXPECT_NEAR(client->send_bandwidth().kbps(), kInitialLinkCapacity.kbps(), 50);
  // However, probing should have made us detect the higher rate.
  s.RunFor(TimeDelta::ms(2000));
  EXPECT_GT(client->send_bandwidth().kbps(), kNewLinkCapacity.kbps() - 300);
}

TEST_F(GoogCcNetworkControllerTest,
       TargetRateReducedOnPacingBufferBuildupInTrial) {
  // Configure strict pacing to ensure build-up.
  ScopedFieldTrials trial(
      "WebRTC-CongestionWindow/QueueSize:100,MinBitrate:30000/"
      "WebRTC-Video-Pacing/factor:1.0/"
      "WebRTC-AddPacingToCongestionWindowPushback/Enabled/");

  const DataRate kLinkCapacity = DataRate::kbps(1000);
  const DataRate kStartRate = DataRate::kbps(1000);

  Scenario s("googcc_unit/pacing_buffer_buildup");
  auto* net = s.CreateSimulationNode([&](NetworkSimulationConfig* c) {
    c->bandwidth = kLinkCapacity;
    c->delay = TimeDelta::ms(50);
  });
  auto* client = s.CreateClient("send", [&](CallClientConfig* c) {
    c->transport.rates.start_rate = kStartRate;
  });
  auto* route = s.CreateRoutes(
      client, {net}, s.CreateClient("return", CallClientConfig()),
      {s.CreateSimulationNode(NetworkSimulationConfig())});
  s.CreateVideoStream(route->forward(), VideoStreamConfig());
  // Allow some time for the buffer to build up.
  s.RunFor(TimeDelta::seconds(5));

  // Without trial, pacer delay reaches ~250 ms.
  EXPECT_LT(client->GetStats().pacer_delay_ms, 150);
}

TEST_F(GoogCcNetworkControllerTest, NoBandwidthTogglingInLossControlTrial) {
  ScopedFieldTrials trial("WebRTC-Bwe-LossBasedControl/Enabled/");
  Scenario s("googcc_unit/no_toggling");
  auto* send_net = s.CreateSimulationNode([&](NetworkSimulationConfig* c) {
    c->bandwidth = DataRate::kbps(2000);
    c->loss_rate = 0.2;
    c->delay = TimeDelta::ms(10);
  });

  auto* client = s.CreateClient("send", [&](CallClientConfig* c) {
    c->transport.rates.start_rate = DataRate::kbps(300);
  });
  auto* route = s.CreateRoutes(
      client, {send_net}, s.CreateClient("return", CallClientConfig()),
      {s.CreateSimulationNode(NetworkSimulationConfig())});
  s.CreateVideoStream(route->forward(), VideoStreamConfig());
  // Allow the controller to initialize.
  s.RunFor(TimeDelta::ms(250));

  std::queue<DataRate> bandwidth_history;
  const TimeDelta step = TimeDelta::ms(50);
  for (TimeDelta time = TimeDelta::Zero(); time < TimeDelta::ms(2000);
       time += step) {
    s.RunFor(step);
    const TimeDelta window = TimeDelta::ms(500);
    if (bandwidth_history.size() >= window / step)
      bandwidth_history.pop();
    bandwidth_history.push(client->send_bandwidth());
    EXPECT_LT(CountBandwidthDips(bandwidth_history, DataRate::kbps(100)), 2);
  }
}

TEST_F(GoogCcNetworkControllerTest, NoRttBackoffCollapseWhenVideoStops) {
  ScopedFieldTrials trial("WebRTC-Bwe-MaxRttLimit/limit:2s/");
  Scenario s("googcc_unit/rttbackoff_video_stop");
  auto* send_net = s.CreateSimulationNode([&](NetworkSimulationConfig* c) {
    c->bandwidth = DataRate::kbps(2000);
    c->delay = TimeDelta::ms(100);
  });

  auto* client = s.CreateClient("send", [&](CallClientConfig* c) {
    c->transport.rates.start_rate = DataRate::kbps(1000);
  });
  auto* route = s.CreateRoutes(
      client, {send_net}, s.CreateClient("return", CallClientConfig()),
      {s.CreateSimulationNode(NetworkSimulationConfig())});
  auto* video = s.CreateVideoStream(route->forward(), VideoStreamConfig());
  // Allow the controller to initialize, then stop video.
  s.RunFor(TimeDelta::seconds(1));
  video->send()->Stop();
  s.RunFor(TimeDelta::seconds(4));
  EXPECT_GT(client->send_bandwidth().kbps(), 1000);
}

TEST_F(GoogCcNetworkControllerTest, NoCrashOnVeryLateFeedback) {
  Scenario s;
  auto ret_net = s.CreateMutableSimulationNode(NetworkSimulationConfig());
  auto* route = s.CreateRoutes(
      s.CreateClient("send", CallClientConfig()),
      {s.CreateSimulationNode(NetworkSimulationConfig())},
      s.CreateClient("return", CallClientConfig()), {ret_net->node()});
  auto* video = s.CreateVideoStream(route->forward(), VideoStreamConfig());
  s.RunFor(TimeDelta::seconds(5));
  // Delay feedback by several minutes. This will cause removal of the send time
  // history for the packets as long as kSendTimeHistoryWindow is configured for
  // a shorter time span.
  ret_net->PauseTransmissionUntil(s.Now() + TimeDelta::seconds(300));
  // Stopping video stream while waiting to save test execution time.
  video->send()->Stop();
  s.RunFor(TimeDelta::seconds(299));
  // Starting to cause addition of new packet to history, which cause old
  // packets to be removed.
  video->send()->Start();
  // Runs until the lost packets are received. We expect that this will run
  // without causing any runtime failures.
  s.RunFor(TimeDelta::seconds(2));
}

TEST_F(GoogCcNetworkControllerTest, IsFairToTCP) {
  Scenario s("googcc_unit/tcp_fairness");
  NetworkSimulationConfig net_conf;
  net_conf.bandwidth = DataRate::kbps(1000);
  net_conf.delay = TimeDelta::ms(50);
  auto* client = s.CreateClient("send", [&](CallClientConfig* c) {
    c->transport.rates.start_rate = DataRate::kbps(1000);
  });
  auto send_net = {s.CreateSimulationNode(net_conf)};
  auto ret_net = {s.CreateSimulationNode(net_conf)};
  auto* route = s.CreateRoutes(
      client, send_net, s.CreateClient("return", CallClientConfig()), ret_net);
  s.CreateVideoStream(route->forward(), VideoStreamConfig());
  s.net()->StartFakeTcpCrossTraffic(s.net()->CreateRoute(send_net),
                                    s.net()->CreateRoute(ret_net),
                                    FakeTcpConfig());
  s.RunFor(TimeDelta::seconds(10));

  // Currently only testing for the upper limit as we in practice back out
  // quite a lot in this scenario. If this behavior is fixed, we should add a
  // lower bound to ensure it stays fixed.
  EXPECT_LT(client->send_bandwidth().kbps(), 750);
}

}  // namespace test
}  // namespace webrtc
