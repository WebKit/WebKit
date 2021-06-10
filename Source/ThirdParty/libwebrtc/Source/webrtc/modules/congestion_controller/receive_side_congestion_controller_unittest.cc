/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/include/receive_side_congestion_controller.h"

#include "api/test/network_emulation/create_cross_traffic.h"
#include "api/test/network_emulation/cross_traffic.h"
#include "modules/pacing/packet_router.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/scenario/scenario.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::MockFunction;

namespace webrtc {

namespace {

// Helper to convert some time format to resolution used in absolute send time
// header extension, rounded upwards. |t| is the time to convert, in some
// resolution. |denom| is the value to divide |t| by to get whole seconds,
// e.g. |denom| = 1000 if |t| is in milliseconds.
uint32_t AbsSendTime(int64_t t, int64_t denom) {
  return (((t << 18) + (denom >> 1)) / denom) & 0x00fffffful;
}

const uint32_t kInitialBitrateBps = 60000;

}  // namespace

namespace test {

TEST(ReceiveSideCongestionControllerTest, SendsRembWithAbsSendTime) {
  MockFunction<void(std::vector<std::unique_ptr<rtcp::RtcpPacket>>)>
      feedback_sender;
  MockFunction<void(uint64_t, std::vector<uint32_t>)> remb_sender;
  SimulatedClock clock_(123456);

  ReceiveSideCongestionController controller(
      &clock_, feedback_sender.AsStdFunction(), remb_sender.AsStdFunction(),
      nullptr);

  size_t payload_size = 1000;
  RTPHeader header;
  header.ssrc = 0x11eb21c;
  header.extension.hasAbsoluteSendTime = true;

  EXPECT_CALL(remb_sender, Call(_, ElementsAre(header.ssrc))).Times(AtLeast(1));

  for (int i = 0; i < 10; ++i) {
    clock_.AdvanceTimeMilliseconds((1000 * payload_size) / kInitialBitrateBps);
    int64_t now_ms = clock_.TimeInMilliseconds();
    header.extension.absoluteSendTime = AbsSendTime(now_ms, 1000);
    controller.OnReceivedPacket(now_ms, payload_size, header);
  }
}

TEST(ReceiveSideCongestionControllerTest,
     SendsRembAfterSetMaxDesiredReceiveBitrate) {
  MockFunction<void(std::vector<std::unique_ptr<rtcp::RtcpPacket>>)>
      feedback_sender;
  MockFunction<void(uint64_t, std::vector<uint32_t>)> remb_sender;
  SimulatedClock clock_(123456);

  ReceiveSideCongestionController controller(
      &clock_, feedback_sender.AsStdFunction(), remb_sender.AsStdFunction(),
      nullptr);
  EXPECT_CALL(remb_sender, Call(123, _));
  controller.SetMaxDesiredReceiveBitrate(DataRate::BitsPerSec(123));
}

TEST(ReceiveSideCongestionControllerTest, ConvergesToCapacity) {
  Scenario s("recieve_cc_unit/converge");
  NetworkSimulationConfig net_conf;
  net_conf.bandwidth = DataRate::KilobitsPerSec(1000);
  net_conf.delay = TimeDelta::Millis(50);
  auto* client = s.CreateClient("send", [&](CallClientConfig* c) {
    c->transport.rates.start_rate = DataRate::KilobitsPerSec(300);
  });

  auto* route = s.CreateRoutes(client, {s.CreateSimulationNode(net_conf)},
                               s.CreateClient("return", CallClientConfig()),
                               {s.CreateSimulationNode(net_conf)});
  VideoStreamConfig video;
  video.stream.packet_feedback = false;
  s.CreateVideoStream(route->forward(), video);
  s.RunFor(TimeDelta::Seconds(30));
  EXPECT_NEAR(client->send_bandwidth().kbps(), 900, 150);
}

TEST(ReceiveSideCongestionControllerTest, IsFairToTCP) {
  Scenario s("recieve_cc_unit/tcp_fairness");
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
  VideoStreamConfig video;
  video.stream.packet_feedback = false;
  s.CreateVideoStream(route->forward(), video);
  s.net()->StartCrossTraffic(CreateFakeTcpCrossTraffic(
      s.net()->CreateRoute(send_net), s.net()->CreateRoute(ret_net),
      FakeTcpConfig()));
  s.RunFor(TimeDelta::Seconds(30));
  // For some reason we get outcompeted by TCP here, this should probably be
  // fixed and a lower bound should be added to the test.
  EXPECT_LT(client->send_bandwidth().kbps(), 750);
}
}  // namespace test
}  // namespace webrtc
