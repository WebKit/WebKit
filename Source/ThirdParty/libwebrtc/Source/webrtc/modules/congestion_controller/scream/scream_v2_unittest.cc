/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/scream/scream_v2.h"

#include <algorithm>

#include "api/environment/environment.h"
#include "api/transport/ecn_marking.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/scream/test/cc_feedback_generator.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/clock.h"
#include "test/create_test_environment.h"
#include "test/gtest.h"
#include "test/network/simulated_network.h"

namespace webrtc {
namespace {

using ::testing::TestWithParam;

constexpr DataSize kPacketSize = DataSize::Bytes(1000);

TransportPacketsFeedback CreateFeedback(Timestamp feedback_time,
                                        TimeDelta rtt,
                                        int number_of_ect1_packets,
                                        int number_of_packets_in_flight) {
  int sequence_number = 0;
  TransportPacketsFeedback feedback;
  feedback.feedback_time = feedback_time;
  Timestamp send_time = feedback_time - rtt;

  feedback.data_in_flight = kPacketSize * number_of_packets_in_flight;

  for (int i = 0; i < number_of_ect1_packets; ++i) {
    PacketResult result;
    result.sent_packet.send_time = send_time;
    result.sent_packet.size = kPacketSize;
    result.ecn = EcnMarking::kEct1;
    result.receive_time = send_time + rtt / 2;
    result.sent_packet.sequence_number = sequence_number++;
    feedback.packet_feedbacks.push_back(result);
  }

  return feedback;
}

TEST(ScreamV2Test, TargetRateIncreaseToMaxOnUnConstrainedNetwork) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  ScreamV2 scream(env);
  const DataRate kMaxDataRate = DataRate::KilobitsPerSec(2000);
  scream.SetTargetBitrateConstraints(DataRate::Zero(), kMaxDataRate,
                                     DataRate::KilobitsPerSec(300));
  DataRate send_rate = DataRate::KilobitsPerSec(100);
  // Configure a feedback generator simulating a network with infinite
  // capacity but 25ms one way delay.
  CcFeedbackGenerator feedback_generator(
      {.network_config = {.queue_delay_ms = 25}});

  for (int i = 0; i < 100; ++i) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(
            send_rate, clock, [&](const SentPacket& packet) {
              scream.OnPacketSent(packet.data_in_flight);
            });
    scream.OnTransportPacketsFeedback(feedback);
    send_rate = scream.target_rate();
  }
  EXPECT_EQ(send_rate, kMaxDataRate);
}

TEST(ScreamV2Test,
     ReferenceWindowDoesNotDecreaseAfterLowerSendRateOnUnconstrainedNetwork) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  ScreamV2 scream(env);
  const DataRate kMaxDataRate = DataRate::KilobitsPerSec(2000);
  scream.SetTargetBitrateConstraints(DataRate::Zero(), kMaxDataRate,
                                     DataRate::KilobitsPerSec(300));
  DataRate send_rate = DataRate::KilobitsPerSec(100);
  // Configure a feedback generator simulating a network with infinite
  // capacity but 25ms one way delay.
  CcFeedbackGenerator feedback_generator(
      {.network_config = {.queue_delay_ms = 25}});

  for (int i = 0; i < 70; ++i) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(
            send_rate, clock, [&](const SentPacket& packet) {
              scream.OnPacketSent(packet.data_in_flight);
            });
    scream.OnTransportPacketsFeedback(feedback);
    send_rate = scream.target_rate();
  }
  DataSize ref_window = scream.ref_window();

  // Half the send rate, but the network is still unconstrained.
  send_rate = send_rate / 2;
  for (int i = 0; i < 20; ++i) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(
            send_rate, clock, [&](const SentPacket& packet) {
              scream.OnPacketSent(packet.data_in_flight);
            });
    scream.OnTransportPacketsFeedback(feedback);
  }
  // Still the same ref_window.
  EXPECT_EQ(ref_window, scream.ref_window());
}

TEST(ScreamV2Test, ReferenceWindowIncreaseLessPerStepOnLowRtt) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  ScreamV2 scream_1(env);
  ScreamV2 scream_2(env);

  TransportPacketsFeedback high_rtt_feedback =
      CreateFeedback(clock.CurrentTime(), /*rtt=*/TimeDelta::Millis(100),
                     /*number_of_ect1_packets=*/20,
                     /*number_of_packets_in_flight=*/20);
  TransportPacketsFeedback low_rtt_feedback =
      CreateFeedback(clock.CurrentTime(), /*rtt=*/TimeDelta::Millis(1),
                     /*number_of_ect1_packets=*/20,
                     /*number_of_packets_in_flight=*/20);

  scream_1.OnTransportPacketsFeedback(high_rtt_feedback);
  scream_2.OnTransportPacketsFeedback(low_rtt_feedback);

  EXPECT_GT(scream_1.ref_window(), scream_2.ref_window());
}

TEST(ScreamV2Test, ReferenceWindowIncreaseLessPerStepIfCeDetected) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  ScreamV2 scream_1(env);
  ScreamV2 scream_2(env);

  TransportPacketsFeedback feedback =
      CreateFeedback(clock.CurrentTime(), /*rtt=*/TimeDelta::Millis(10),
                     /*number_of_ect1_packets=*/20,
                     /*number_of_packets_in_flight=*/20);

  TransportPacketsFeedback ce_detected_feedback = feedback;
  ce_detected_feedback.packet_feedbacks[0].ecn = EcnMarking::kCe;

  scream_1.OnTransportPacketsFeedback(feedback);
  scream_2.OnTransportPacketsFeedback(ce_detected_feedback);

  EXPECT_GT(scream_1.ref_window(), scream_2.ref_window());
}

TEST(ScreamV2Test, ReferenceWindowIncreaseToDataInflight) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  ScreamV2 scream(env);

  Timestamp start_time = clock.CurrentTime();
  TimeDelta feedback_interval = TimeDelta::Millis(25);

  TransportPacketsFeedback feedback =
      CreateFeedback(clock.CurrentTime(), /*rtt=*/TimeDelta::Millis(10),
                     /*number_of_ect1_packets=*/20,
                     /*number_of_packets_in_flight=*/10);

  while (clock.CurrentTime() < start_time + TimeDelta::Seconds(2)) {
    feedback.feedback_time = clock.CurrentTime();
    scream.OnTransportPacketsFeedback(feedback);
    clock.AdvanceTime(feedback_interval);
  }
  // Target rate can increase up to 1.1 * data_in_flight + Max Segment Size(
  // default 1000 bytes) when no max target rate has been set.
  EXPECT_EQ(scream.ref_window(),
            1.1 * feedback.data_in_flight + DataSize::Bytes(1000));
}

TEST(ScreamV2Test, CalculatesL4sAlpha) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});

  ScreamV2 scream(env);

  Timestamp start_time = clock.CurrentTime();
  TimeDelta feedback_interval = TimeDelta::Millis(25);

  TransportPacketsFeedback feedback =
      CreateFeedback(clock.CurrentTime(), /*rtt=*/TimeDelta::Millis(10),
                     /*number_of_ect1_packets=*/20,
                     /*number_of_packets_in_flight=*/20);
  // CE mark 20% of packets.
  for (int i = 0; i < 4; ++i) {
    feedback.packet_feedbacks[i].ecn = EcnMarking::kCe;
  }

  double l4s_alpha = scream.l4s_alpha();
  while (clock.CurrentTime() < start_time + TimeDelta::Seconds(2)) {
    feedback.feedback_time = clock.CurrentTime();
    scream.OnTransportPacketsFeedback(feedback);
    EXPECT_GT(scream.l4s_alpha(), l4s_alpha);
    clock.AdvanceTime(feedback_interval);
  }

  EXPECT_NEAR(scream.l4s_alpha(), 0.2, 0.01);
}

struct AdaptsToLinkCapacityParams {
  SimulatedNetwork::Config network_config;
  bool send_as_ect1 = true;
  TimeDelta adaption_time;
  TimeDelta time_to_run_after_adaption_time = TimeDelta::Seconds(5);
};

struct AdaptsToLinkCapacityResult {
  DataRate data_rate;
  DataRate min_rate_after_adaption;
  DataRate max_rate_after_adaption;
  TimeDelta max_smoothed_rtt_after_adaptation = TimeDelta::Zero();
};

AdaptsToLinkCapacityResult RunAdaptToLinkCapacityTest(
    const AdaptsToLinkCapacityParams& params) {
  AdaptsToLinkCapacityResult result;
  const Timestamp kStartTime = Timestamp::Seconds(1'234);
  SimulatedClock clock(kStartTime);
  Environment env = CreateTestEnvironment({.time = &clock});
  ScreamV2 scream(env);
  CcFeedbackGenerator feedback_generator(
      {.network_config = params.network_config,
       .send_as_ect1 = params.send_as_ect1,
       .packet_size = DataSize::Bytes(255)});

  DataRate send_rate = DataRate::KilobitsPerSec(100);
  while (clock.CurrentTime() < kStartTime + params.adaption_time) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(
            send_rate, clock, [&](const SentPacket& packet) {
              scream.OnPacketSent(packet.data_in_flight);
            });
    scream.OnTransportPacketsFeedback(feedback);
    send_rate = scream.target_rate();
  }
  result.data_rate = send_rate;
  result.min_rate_after_adaption = send_rate;
  result.max_rate_after_adaption = send_rate;

  Timestamp time_after_adaption = clock.CurrentTime();
  while (clock.CurrentTime() <
         time_after_adaption + params.time_to_run_after_adaption_time) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(
            send_rate, clock, [&](const SentPacket& packet) {
              scream.OnPacketSent(packet.data_in_flight);
            });
    scream.OnTransportPacketsFeedback(feedback);
    send_rate = scream.target_rate();
    result.min_rate_after_adaption =
        std::min(result.min_rate_after_adaption, send_rate);
    result.max_rate_after_adaption =
        std::max(result.max_rate_after_adaption, send_rate);
    result.max_smoothed_rtt_after_adaptation =
        std::max(result.max_smoothed_rtt_after_adaptation, scream.rtt());
  }
  RTC_LOG(LS_INFO) << " rate_after_adaption " << result.data_rate
                   << " max_rate_after_adaption: "
                   << result.max_rate_after_adaption
                   << " min_rate_after_adaption: "
                   << result.min_rate_after_adaption;
  return result;
}

TEST(ScreamV2Test, AdaptsToEcnLinkCapacity1Mbps) {
  AdaptsToLinkCapacityParams params{
      .network_config = {.queue_delay_ms = 25,
                         .link_capacity = DataRate::KilobitsPerSec(1000)},
      .send_as_ect1 = true,
      .adaption_time = TimeDelta::Seconds(4)};
  AdaptsToLinkCapacityResult result = RunAdaptToLinkCapacityTest(params);

  EXPECT_LT(result.data_rate, DataRate::KilobitsPerSec(1100));
  EXPECT_GT(result.data_rate, DataRate::KilobitsPerSec(650));
  EXPECT_LT(result.max_rate_after_adaption, DataRate::KilobitsPerSec(1100));
  EXPECT_GT(result.min_rate_after_adaption, DataRate::KilobitsPerSec(650));

  EXPECT_LT(result.max_smoothed_rtt_after_adaptation,
            TimeDelta::Millis(25 * 2 + 25));
}

TEST(ScreamV2Test, AdaptsToLossLinkCapacity5Mbps) {
  AdaptsToLinkCapacityParams params{
      .network_config = {.queue_length_packets = 3,
                         .queue_delay_ms = 10,
                         .link_capacity = DataRate::KilobitsPerSec(5000)},
      .send_as_ect1 = false,  // Adapt only due to loss when queues overflow.
      .adaption_time = TimeDelta::Seconds(10)};

  AdaptsToLinkCapacityResult result = RunAdaptToLinkCapacityTest(params);

  EXPECT_LT(result.data_rate, DataRate::KilobitsPerSec(5400));
  EXPECT_GT(result.data_rate, DataRate::KilobitsPerSec(1500));
  EXPECT_LT(result.max_rate_after_adaption, DataRate::KilobitsPerSec(5400));
  EXPECT_GT(result.min_rate_after_adaption, DataRate::KilobitsPerSec(1500));

  EXPECT_LT(result.max_smoothed_rtt_after_adaptation,
            TimeDelta::Millis(10 * 2 + 40));
}

TEST(ScreamV2Test, AdaptsToDelayLinkCapacity2Mbps) {
  AdaptsToLinkCapacityParams params{
      .network_config = {.queue_delay_ms = 10,
                         .link_capacity = DataRate::KilobitsPerSec(2000)},
      .send_as_ect1 = false,  // Adapt only due to delay increase.
      .adaption_time = TimeDelta::Seconds(10)};

  AdaptsToLinkCapacityResult result = RunAdaptToLinkCapacityTest(params);

  EXPECT_LT(result.data_rate, DataRate::KilobitsPerSec(2500));
  EXPECT_GT(result.data_rate, DataRate::KilobitsPerSec(1500));
  EXPECT_LT(result.max_rate_after_adaption, DataRate::KilobitsPerSec(2500));
  EXPECT_GT(result.min_rate_after_adaption, DataRate::KilobitsPerSec(1500));

  EXPECT_LT(result.max_smoothed_rtt_after_adaptation,
            TimeDelta::Millis(10 * 2 + 50 + 10));
}

TEST(ScreamV2Test, AdaptsToDelayLinkCapacity2MbpsLongRunning) {
  AdaptsToLinkCapacityParams params{
      .network_config = {.queue_delay_ms = 10,
                         .link_capacity = DataRate::KilobitsPerSec(2000)},
      .send_as_ect1 = false,  // Adapt only due to delay increase.
      .adaption_time = TimeDelta::Seconds(5),
      .time_to_run_after_adaption_time = TimeDelta::Minutes(15)};

  AdaptsToLinkCapacityResult result = RunAdaptToLinkCapacityTest(params);
  EXPECT_LT(result.max_smoothed_rtt_after_adaptation,
            TimeDelta::Millis(10 * 2 + 60));
}

}  // namespace
}  // namespace webrtc
