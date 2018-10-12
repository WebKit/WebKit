/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/transport/test/network_control_tester.h"
#include "modules/congestion_controller/pcc/pcc_factory.h"
#include "modules/congestion_controller/pcc/pcc_network_controller.h"

#include "test/gmock.h"
#include "test/gtest.h"

using testing::Field;
using testing::Matcher;
using testing::AllOf;
using testing::Ge;
using testing::Le;
using testing::Property;

namespace webrtc {
namespace pcc {
namespace test {
namespace {

const DataRate kInitialBitrate = DataRate::kbps(60);
const Timestamp kDefaultStartTime = Timestamp::ms(10000000);

constexpr double kDataRateMargin = 0.20;
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

}  // namespace

TEST(PccNetworkControllerTest, SendsConfigurationOnFirstProcess) {
  std::unique_ptr<NetworkControllerInterface> controller_;
  controller_.reset(new PccNetworkController(InitialConfig()));

  NetworkControlUpdate update =
      controller_->OnProcessInterval(InitialProcessInterval());
  EXPECT_THAT(*update.target_rate, TargetRateCloseTo(kInitialBitrate));
  EXPECT_THAT(*update.pacer_config,
              Property(&PacerConfig::data_rate, Ge(kInitialBitrate)));
}

TEST(PccNetworkControllerTest, UpdatesTargetSendRate) {
  PccNetworkControllerFactory factory;
  webrtc::test::NetworkControllerTester tester(&factory,
                                               InitialConfig(60, 0, 600));
  auto packet_producer = &webrtc::test::SimpleTargetRateProducer::ProduceNext;

  tester.RunSimulation(TimeDelta::seconds(10), TimeDelta::ms(10),
                       DataRate::kbps(300), TimeDelta::ms(100),
                       packet_producer);
  EXPECT_GE(tester.GetState().target_rate->target_rate.kbps(),
            300 * kMinDataRateFactor);
  EXPECT_LE(tester.GetState().target_rate->target_rate.kbps(),
            300 * kMaxDataRateFactor);

  tester.RunSimulation(TimeDelta::seconds(30), TimeDelta::ms(10),
                       DataRate::kbps(500), TimeDelta::ms(100),
                       packet_producer);
  EXPECT_GE(tester.GetState().target_rate->target_rate.kbps(),
            500 * kMinDataRateFactor);
  EXPECT_LE(tester.GetState().target_rate->target_rate.kbps(),
            500 * kMaxDataRateFactor);

  tester.RunSimulation(TimeDelta::seconds(2), TimeDelta::ms(10),
                       DataRate::kbps(200), TimeDelta::ms(200),
                       packet_producer);
  EXPECT_LE(tester.GetState().target_rate->target_rate.kbps(),
            200 * kMaxDataRateFactor);

  tester.RunSimulation(TimeDelta::seconds(18), TimeDelta::ms(10),
                       DataRate::kbps(200), TimeDelta::ms(200),
                       packet_producer);
  EXPECT_GE(tester.GetState().target_rate->target_rate.kbps(),
            200 * kMinDataRateFactor);
  EXPECT_LE(tester.GetState().target_rate->target_rate.kbps(),
            200 * kMaxDataRateFactor);
}

}  // namespace test
}  // namespace pcc
}  // namespace webrtc
