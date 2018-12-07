/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/transport/goog_cc_factory.h"
#include "api/transport/test/network_control_tester.h"
#include "logging/rtc_event_log/mock/mock_rtc_event_log.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/scenario/scenario.h"

using testing::Field;
using testing::Matcher;
using testing::NiceMock;
using testing::Property;
using testing::_;

namespace webrtc {
namespace test {
namespace {

const uint32_t kInitialBitrateKbps = 60;
const DataRate kInitialBitrate = DataRate::kbps(kInitialBitrateKbps);
const float kDefaultPacingRate = 2.5f;
}  // namespace

class GoogCcNetworkControllerTest : public ::testing::Test {
 protected:
  GoogCcNetworkControllerTest()
      : current_time_(Timestamp::ms(123456)), factory_(&event_log_) {}
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

TEST_F(GoogCcNetworkControllerTest, OnNetworkRouteChanged) {
  NetworkControlUpdate update;
  DataRate new_bitrate = DataRate::bps(200000);
  update = controller_->OnNetworkRouteChange(CreateRouteChange(new_bitrate));
  EXPECT_EQ(update.target_rate->target_rate, new_bitrate);
  EXPECT_EQ(update.pacer_config->data_rate(), new_bitrate * kDefaultPacingRate);
  EXPECT_EQ(update.probe_cluster_configs.size(), 2u);

  // If the bitrate is reset to -1, the new starting bitrate will be
  // the minimum default bitrate.
  const DataRate kDefaultMinBitrate = DataRate::bps(10000);
  update = controller_->OnNetworkRouteChange(CreateRouteChange());
  EXPECT_EQ(update.target_rate->target_rate, kDefaultMinBitrate);
  EXPECT_EQ(update.pacer_config->data_rate(),
            kDefaultMinBitrate * kDefaultPacingRate);
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

// Estimated bitrate reduced when the feedbacks arrive with such a long delay,
// that the send-time-history no longer holds the feedbacked packets.
TEST_F(GoogCcNetworkControllerTest, LongFeedbackDelays) {
  TargetBitrateTrackingSetup();
  const webrtc::PacedPacketInfo kPacingInfo0(0, 5, 2000);
  const webrtc::PacedPacketInfo kPacingInfo1(1, 8, 4000);
  const int64_t kFeedbackTimeoutMs = 60001;
  const int kMaxConsecutiveFailedLookups = 5;
  for (int i = 0; i < kMaxConsecutiveFailedLookups; ++i) {
    std::vector<PacketResult> packets;
    packets.push_back(CreateResult(i * 100, 2 * i * 100, 1500, kPacingInfo0));
    packets.push_back(
        CreateResult(i * 100 + 10, 2 * i * 100 + 10, 1500, kPacingInfo0));
    packets.push_back(
        CreateResult(i * 100 + 20, 2 * i * 100 + 20, 1500, kPacingInfo0));
    packets.push_back(
        CreateResult(i * 100 + 30, 2 * i * 100 + 30, 1500, kPacingInfo1));
    packets.push_back(
        CreateResult(i * 100 + 40, 2 * i * 100 + 40, 1500, kPacingInfo1));

    TransportPacketsFeedback feedback;
    for (PacketResult& packet : packets) {
      controller_->OnSentPacket(packet.sent_packet);
      feedback.sendless_arrival_times.push_back(packet.receive_time);
    }

    feedback.feedback_time = packets[0].receive_time;
    AdvanceTimeMilliseconds(kFeedbackTimeoutMs);
    SentPacket later_packet;
    later_packet.send_time = Timestamp::ms(kFeedbackTimeoutMs + i * 200 + 40);
    later_packet.size = DataSize::bytes(1500);
    later_packet.pacing_info = kPacingInfo1;
    controller_->OnSentPacket(later_packet);

    OnUpdate(controller_->OnTransportPacketsFeedback(feedback));
  }
  OnUpdate(controller_->OnProcessInterval(DefaultInterval()));

  EXPECT_EQ(kInitialBitrateKbps / 2, target_bitrate_->kbps());

  // Test with feedback that isn't late enough to time out.
  {
    std::vector<PacketResult> packets;
    packets.push_back(CreateResult(100, 200, 1500, kPacingInfo0));
    packets.push_back(CreateResult(110, 210, 1500, kPacingInfo0));
    packets.push_back(CreateResult(120, 220, 1500, kPacingInfo0));
    packets.push_back(CreateResult(130, 230, 1500, kPacingInfo1));
    packets.push_back(CreateResult(140, 240, 1500, kPacingInfo1));

    for (const PacketResult& packet : packets)
      controller_->OnSentPacket(packet.sent_packet);

    TransportPacketsFeedback feedback;
    feedback.feedback_time = packets[0].receive_time;
    feedback.packet_feedbacks = packets;

    AdvanceTimeMilliseconds(kFeedbackTimeoutMs - 1);

    SentPacket later_packet;
    later_packet.send_time = Timestamp::ms(kFeedbackTimeoutMs + 240);
    later_packet.size = DataSize::bytes(1500);
    later_packet.pacing_info = kPacingInfo1;
    controller_->OnSentPacket(later_packet);

    OnUpdate(controller_->OnTransportPacketsFeedback(feedback));
  }
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
       FeedbackVersionUpdatesTargetSendRateBasedOnFeedback) {
  GoogCcFeedbackNetworkControllerFactory factory(&event_log_);
  NetworkControllerTester tester(&factory, InitialConfig(60, 0, 600));
  auto packet_producer = &SimpleTargetRateProducer::ProduceNext;

  tester.RunSimulation(TimeDelta::seconds(10), TimeDelta::ms(10),
                       DataRate::kbps(300), TimeDelta::ms(100),
                       packet_producer);
  EXPECT_NEAR(tester.GetState().target_rate->target_rate.kbps<double>(), 300,
              50);

  tester.RunSimulation(TimeDelta::seconds(10), TimeDelta::ms(10),
                       DataRate::kbps(500), TimeDelta::ms(100),
                       packet_producer);
  EXPECT_NEAR(tester.GetState().target_rate->target_rate.kbps<double>(), 500,
              100);

  tester.RunSimulation(TimeDelta::seconds(30), TimeDelta::ms(10),
                       DataRate::kbps(100), TimeDelta::ms(200),
                       packet_producer);
  EXPECT_NEAR(tester.GetState().target_rate->target_rate.kbps<double>(), 100,
              20);
}

TEST_F(GoogCcNetworkControllerTest, LimitsToMinRateIfRttIsHighInTrial) {
  // The field trial limits maximum RTT to 2 seconds, higher RTT means that the
  // controller backs off until it reaches the minimum configured bitrate. This
  // allows the RTT to recover faster than the regular control mechanism would
  // achieve.
  ScopedFieldTrials trial("WebRTC-Bwe-MaxRttLimit/limit:2s/");
  // In the test case, we limit the capacity and add a cross traffic packet
  // burst that blocks media from being sent. This causes the RTT to quickly
  // increase above the threshold in the trial.
  const DataRate kLinkCapacity = DataRate::kbps(100);
  const DataRate kMinRate = DataRate::kbps(20);
  const TimeDelta kBufferBloatDuration = TimeDelta::seconds(10);
  Scenario s("googcc_unit/limit_trial", false);
  NetworkNodeConfig net_conf;
  auto send_net = s.CreateSimulationNode([=](NetworkNodeConfig* c) {
    c->simulation.bandwidth = kLinkCapacity;
    c->simulation.delay = TimeDelta::ms(100);
    c->update_frequency = TimeDelta::ms(5);
  });
  auto ret_net = s.CreateSimulationNode([](NetworkNodeConfig* c) {
    c->simulation.delay = TimeDelta::ms(100);
    c->update_frequency = TimeDelta::ms(5);
  });
  SimulatedTimeClientConfig config;
  config.transport.cc =
      TransportControllerConfig::CongestionController::kGoogCc;
  config.transport.rates.min_rate = kMinRate;
  config.transport.rates.start_rate = kLinkCapacity;
  SimulatedTimeClient* client = s.CreateSimulatedTimeClient(
      "send", config, {PacketStreamConfig()}, {send_net}, {ret_net});
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
  EXPECT_NEAR(client->target_rate_kbps(), kMinRate.kbps(), 1);
}

TEST_F(GoogCcNetworkControllerTest, UpdatesTargetRateBasedOnLinkCapacity) {
  Scenario s("googcc_unit/target_capacity", false);
  SimulatedTimeClientConfig config;
  config.transport.cc =
      TransportControllerConfig::CongestionController::kGoogCcFeedback;
  config.transport.rates.min_rate = DataRate::kbps(10);
  config.transport.rates.max_rate = DataRate::kbps(1500);
  config.transport.rates.start_rate = DataRate::kbps(300);
  NetworkNodeConfig net_conf;
  auto send_net = s.CreateSimulationNode([](NetworkNodeConfig* c) {
    c->simulation.bandwidth = DataRate::kbps(500);
    c->simulation.delay = TimeDelta::ms(100);
    c->update_frequency = TimeDelta::ms(5);
  });
  auto ret_net = s.CreateSimulationNode([](NetworkNodeConfig* c) {
    c->simulation.delay = TimeDelta::ms(100);
    c->update_frequency = TimeDelta::ms(5);
  });
  StatesPrinter* truth = s.CreatePrinter(
      "send.truth.txt", TimeDelta::PlusInfinity(), {send_net->ConfigPrinter()});
  SimulatedTimeClient* client = s.CreateSimulatedTimeClient(
      "send", config, {PacketStreamConfig()}, {send_net}, {ret_net});

  truth->PrintRow();
  s.RunFor(TimeDelta::seconds(25));
  truth->PrintRow();
  EXPECT_NEAR(client->target_rate_kbps(), 450, 100);

  send_net->UpdateConfig([](NetworkNodeConfig* c) {
    c->simulation.bandwidth = DataRate::kbps(800);
    c->simulation.delay = TimeDelta::ms(100);
  });

  truth->PrintRow();
  s.RunFor(TimeDelta::seconds(20));
  truth->PrintRow();
  EXPECT_NEAR(client->target_rate_kbps(), 750, 150);

  send_net->UpdateConfig([](NetworkNodeConfig* c) {
    c->simulation.bandwidth = DataRate::kbps(100);
    c->simulation.delay = TimeDelta::ms(200);
  });
  ret_net->UpdateConfig(
      [](NetworkNodeConfig* c) { c->simulation.delay = TimeDelta::ms(200); });

  truth->PrintRow();
  s.RunFor(TimeDelta::seconds(30));
  truth->PrintRow();
  EXPECT_NEAR(client->target_rate_kbps(), 90, 20);
}

TEST_F(GoogCcNetworkControllerTest, DefaultEstimateVariesInSteadyState) {
  ScopedFieldTrials trial("WebRTC-Bwe-StableBandwidthEstimate/Disabled/");
  Scenario s("googcc_unit/no_stable_varies", false);
  SimulatedTimeClientConfig config;
  config.transport.cc =
      TransportControllerConfig::CongestionController::kGoogCcFeedback;
  NetworkNodeConfig net_conf;
  net_conf.simulation.bandwidth = DataRate::kbps(500);
  net_conf.simulation.delay = TimeDelta::ms(100);
  net_conf.update_frequency = TimeDelta::ms(5);
  auto send_net = s.CreateSimulationNode(net_conf);
  auto ret_net = s.CreateSimulationNode(net_conf);
  SimulatedTimeClient* client = s.CreateSimulatedTimeClient(
      "send", config, {PacketStreamConfig()}, {send_net}, {ret_net});
  // Run for a while to allow the estimate to stabilize.
  s.RunFor(TimeDelta::seconds(20));
  DataRate min_estimate = DataRate::PlusInfinity();
  DataRate max_estimate = DataRate::MinusInfinity();
  // Measure variation in steady state.
  for (int i = 0; i < 20; ++i) {
    min_estimate = std::min(min_estimate, client->link_capacity());
    max_estimate = std::max(max_estimate, client->link_capacity());
    s.RunFor(TimeDelta::seconds(1));
  }
  // We should expect drops by at least 15% (default backoff.)
  EXPECT_LT(min_estimate / max_estimate, 0.85);
}

TEST_F(GoogCcNetworkControllerTest, StableEstimateDoesNotVaryInSteadyState) {
  ScopedFieldTrials trial("WebRTC-Bwe-StableBandwidthEstimate/Enabled/");
  Scenario s("googcc_unit/stable_is_stable", false);
  SimulatedTimeClientConfig config;
  config.transport.cc =
      TransportControllerConfig::CongestionController::kGoogCcFeedback;
  NetworkNodeConfig net_conf;
  net_conf.simulation.bandwidth = DataRate::kbps(500);
  net_conf.simulation.delay = TimeDelta::ms(100);
  net_conf.update_frequency = TimeDelta::ms(5);
  auto send_net = s.CreateSimulationNode(net_conf);
  auto ret_net = s.CreateSimulationNode(net_conf);
  SimulatedTimeClient* client = s.CreateSimulatedTimeClient(
      "send", config, {PacketStreamConfig()}, {send_net}, {ret_net});
  // Run for a while to allow the estimate to stabilize.
  s.RunFor(TimeDelta::seconds(20));
  DataRate min_estimate = DataRate::PlusInfinity();
  DataRate max_estimate = DataRate::MinusInfinity();
  // Measure variation in steady state.
  for (int i = 0; i < 20; ++i) {
    min_estimate = std::min(min_estimate, client->link_capacity());
    max_estimate = std::max(max_estimate, client->link_capacity());
    s.RunFor(TimeDelta::seconds(1));
  }
  // We expect no variation under the trial in steady state.
  EXPECT_GT(min_estimate / max_estimate, 0.95);
}

}  // namespace test
}  // namespace webrtc
