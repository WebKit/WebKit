/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/bbr/bbr_network_controller.h"

#include <algorithm>
#include <memory>

#include "modules/congestion_controller/bbr/bbr_factory.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/scenario/scenario.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Ge;
using ::testing::Le;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::StrictMock;

namespace webrtc {
namespace test {
namespace {

const DataRate kInitialBitrate = DataRate::kbps(60);
const Timestamp kDefaultStartTime = Timestamp::ms(10000000);

constexpr double kDataRateMargin = 0.3;
constexpr double kMinDataRateFactor = 1 - kDataRateMargin;
constexpr double kMaxDataRateFactor = 1 + kDataRateMargin;
inline Matcher<TargetTransferRate> TargetRateCloseTo(DataRate rate) {
  DataRate min_data_rate = rate * kMinDataRateFactor;
  DataRate max_data_rate = rate * kMaxDataRateFactor;
  return Field(&TargetTransferRate::target_rate,
               AllOf(Ge(min_data_rate), Le(max_data_rate)));
}

NetworkControllerConfig InitialConfig(
    int starting_bandwidth_kbps = kInitialBitrate.kbps(),
    int min_data_rate_kbps = 0,
    int max_data_rate_kbps = 5 * kInitialBitrate.kbps()) {
  NetworkControllerConfig config;
  config.constraints.at_time = kDefaultStartTime;
  config.constraints.min_data_rate = DataRate::kbps(min_data_rate_kbps);
  config.constraints.max_data_rate = DataRate::kbps(max_data_rate_kbps);
  config.constraints.starting_rate = DataRate::kbps(starting_bandwidth_kbps);
  return config;
}

ProcessInterval InitialProcessInterval() {
  ProcessInterval process_interval;
  process_interval.at_time = kDefaultStartTime;
  return process_interval;
}

NetworkRouteChange CreateRouteChange(Timestamp at_time,
                                     DataRate start_rate,
                                     DataRate min_rate = DataRate::Zero(),
                                     DataRate max_rate = DataRate::Infinity()) {
  NetworkRouteChange route_change;
  route_change.at_time = at_time;
  route_change.constraints.at_time = at_time;
  route_change.constraints.min_data_rate = min_rate;
  route_change.constraints.max_data_rate = max_rate;
  route_change.constraints.starting_rate = start_rate;
  return route_change;
}
}  // namespace

class BbrNetworkControllerTest : public ::testing::Test {
 protected:
  BbrNetworkControllerTest() {}
  ~BbrNetworkControllerTest() override {}
};

TEST_F(BbrNetworkControllerTest, SendsConfigurationOnFirstProcess) {
  std::unique_ptr<NetworkControllerInterface> controller_;
  controller_.reset(new bbr::BbrNetworkController(InitialConfig()));

  NetworkControlUpdate update =
      controller_->OnProcessInterval(InitialProcessInterval());
  EXPECT_THAT(*update.target_rate, TargetRateCloseTo(kInitialBitrate));
  EXPECT_THAT(*update.pacer_config,
              Property(&PacerConfig::data_rate, Ge(kInitialBitrate)));
  EXPECT_THAT(*update.congestion_window, Property(&DataSize::IsFinite, true));
}

TEST_F(BbrNetworkControllerTest, SendsConfigurationOnNetworkRouteChanged) {
  std::unique_ptr<NetworkControllerInterface> controller_;
  controller_.reset(new bbr::BbrNetworkController(InitialConfig()));

  NetworkControlUpdate update =
      controller_->OnProcessInterval(InitialProcessInterval());
  EXPECT_TRUE(update.target_rate.has_value());
  EXPECT_TRUE(update.pacer_config.has_value());
  EXPECT_TRUE(update.congestion_window.has_value());

  DataRate new_bitrate = DataRate::bps(200000);
  update = controller_->OnNetworkRouteChange(
      CreateRouteChange(kDefaultStartTime, new_bitrate));
  EXPECT_THAT(*update.target_rate, TargetRateCloseTo(new_bitrate));
  EXPECT_THAT(*update.pacer_config,
              Property(&PacerConfig::data_rate, Ge(kInitialBitrate)));
  EXPECT_TRUE(update.congestion_window.has_value());
}

// Bandwidth estimation is updated when feedbacks are received.
// Feedbacks which show an increasing delay cause the estimation to be reduced.
TEST_F(BbrNetworkControllerTest, UpdatesTargetSendRate) {
  BbrNetworkControllerFactory factory;
  Scenario s("bbr_unit/updates_rate", false);
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
  auto* client = s.CreateClient("send", config);
  auto routes = s.CreateRoutes(client, {send_net->node()},
                               s.CreateClient("recv", CallClientConfig()),
                               {ret_net->node()});
  s.CreateVideoStream(routes->forward(), VideoStreamConfig());

  s.RunFor(TimeDelta::seconds(25));
  EXPECT_NEAR(client->send_bandwidth().kbps(), 450, 100);

  send_net->UpdateConfig([](NetworkSimulationConfig* c) {
    c->bandwidth = DataRate::kbps(800);
    c->delay = TimeDelta::ms(100);
  });

  s.RunFor(TimeDelta::seconds(20));
  EXPECT_NEAR(client->send_bandwidth().kbps(), 750, 150);

  send_net->UpdateConfig([](NetworkSimulationConfig* c) {
    c->bandwidth = DataRate::kbps(200);
    c->delay = TimeDelta::ms(200);
  });
  ret_net->UpdateConfig(
      [](NetworkSimulationConfig* c) { c->delay = TimeDelta::ms(200); });

  s.RunFor(TimeDelta::seconds(40));
  EXPECT_NEAR(client->send_bandwidth().kbps(), 200, 40);
}

}  // namespace test
}  // namespace webrtc
