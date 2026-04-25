/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/scream/scream_network_controller.h"

#include "api/environment/environment.h"
#include "api/transport/network_control.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/scream/test/cc_feedback_generator.h"
#include "system_wrappers/include/clock.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Eq;
using ::testing::Field;
using ::testing::Lt;
using ::testing::Optional;

constexpr double kPacingFactor = 1.1;

TEST(ScreamControllerTest, CanConstruct) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  NetworkControllerConfig config(env);
  ScreamNetworkController scream_controller(config);
}

TEST(ScreamControllerTest, OnNetworkAvailabilityUpdatesTargetRateAndPacerRate) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  NetworkControllerConfig config(env);
  config.constraints.starting_rate = DataRate::KilobitsPerSec(123);
  config.stream_based_config.max_total_allocated_bitrate =
      DataRate::KilobitsPerSec(456);
  ScreamNetworkController scream_controller(config);

  NetworkControlUpdate update =
      scream_controller.OnNetworkAvailability({.network_available = true});
  ASSERT_TRUE(update.has_updates());
  ASSERT_TRUE(update.target_rate.has_value());
  EXPECT_EQ(update.target_rate->target_rate, config.constraints.starting_rate);
  ASSERT_TRUE(update.pacer_config);
  EXPECT_EQ(update.pacer_config->data_window,
            *config.constraints.starting_rate * kPacingFactor *
                PacerConfig::kDefaultTimeInterval);
}

TEST(ScreamControllerTest,
     OnTransportPacketsFeedbackUpdatesTargetRateAndPacerRate) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  NetworkControllerConfig config(env);
  ScreamNetworkController scream_controller(config);

  // Simulation with infinite capacity.
  CcFeedbackGenerator feedback_generator({});

  TransportPacketsFeedback feedback =
      feedback_generator.ProcessUntilNextFeedback(DataRate::KilobitsPerSec(100),
                                                  clock);
  NetworkControlUpdate update =
      scream_controller.OnTransportPacketsFeedback(feedback);
  ASSERT_TRUE(update.has_updates());
  ASSERT_TRUE(update.target_rate.has_value());
  EXPECT_GT(update.target_rate->target_rate, DataRate::KilobitsPerSec(100));
  ASSERT_TRUE(update.pacer_config);
  EXPECT_EQ(update.pacer_config->data_window,
            update.target_rate->target_rate * kPacingFactor *
                PacerConfig::kDefaultTimeInterval);
}

TEST(ScreamControllerTest,
     OnNetworkRouteChangeResetsScreamAndUpdatesTargetRate) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  NetworkControllerConfig config(env);
  config.constraints.starting_rate = DataRate::KilobitsPerSec(50);
  config.stream_based_config.max_total_allocated_bitrate =
      DataRate::KilobitsPerSec(1000);
  ScreamNetworkController scream_controller(config);
  scream_controller.OnNetworkAvailability({.network_available = true});

  CcFeedbackGenerator feedback_generator({});
  DataRate send_rate = DataRate::KilobitsPerSec(100);
  for (int i = 0; i < 10; ++i) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(send_rate, clock);
    NetworkControlUpdate update =
        scream_controller.OnTransportPacketsFeedback(feedback);
    if (update.target_rate.has_value()) {
      send_rate = update.target_rate->target_rate;
    }
  }
  ASSERT_GT(send_rate, DataRate::KilobitsPerSec(50));

  NetworkRouteChange route_change;
  route_change.constraints.starting_rate = config.constraints.starting_rate =
      DataRate::KilobitsPerSec(123);
  route_change.at_time = clock.CurrentTime();
  NetworkControlUpdate update =
      scream_controller.OnNetworkRouteChange(route_change);
  ASSERT_TRUE(update.has_updates());
  ASSERT_TRUE(update.target_rate.has_value());
  EXPECT_EQ(update.target_rate->target_rate,
            route_change.constraints.starting_rate);
  ASSERT_TRUE(update.pacer_config);
  EXPECT_EQ(update.pacer_config->data_window,
            *route_change.constraints.starting_rate * kPacingFactor *
                PacerConfig::kDefaultTimeInterval);
}

TEST(ScreamControllerTest, TargetRateRampsUptoTargetConstraints) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  NetworkControllerConfig config(env);
  config.constraints.max_data_rate = DataRate::KilobitsPerSec(300);
  ScreamNetworkController scream_controller(config);

  // Simulation with infinite capacity.
  CcFeedbackGenerator feedback_generator({});

  DataRate target_rate = DataRate::KilobitsPerSec(100);
  for (int i = 0; i < 10; ++i) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(target_rate, clock);
    NetworkControlUpdate update =
        scream_controller.OnTransportPacketsFeedback(feedback);
    if (update.target_rate.has_value()) {
      target_rate = update.target_rate->target_rate;
    }
  }
  EXPECT_EQ(target_rate, DataRate::KilobitsPerSec(300));

  // Reduce the constraints and expect the next target rate is bound by it.
  TargetRateConstraints constraints;
  constraints.max_data_rate = DataRate::KilobitsPerSec(200);
  scream_controller.OnTargetRateConstraints(constraints);
  TransportPacketsFeedback feedback =
      feedback_generator.ProcessUntilNextFeedback(target_rate, clock);
  NetworkControlUpdate update =
      scream_controller.OnTransportPacketsFeedback(feedback);
  ASSERT_TRUE(update.target_rate.has_value());
  EXPECT_EQ(update.target_rate->target_rate, DataRate::KilobitsPerSec(200));
}

TEST(ScreamControllerTest, TargetRateLimitedByRemoteBitrateReport) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  NetworkControllerConfig config(env);
  config.constraints.max_data_rate = DataRate::KilobitsPerSec(1000);
  ScreamNetworkController scream_controller(config);

  // Simulation with infinite capacity.
  CcFeedbackGenerator feedback_generator({});
  DataRate target_rate = DataRate::KilobitsPerSec(100);
  for (int i = 0; i < 10; ++i) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(
            target_rate, clock, [&](const SentPacket& packet) {
              scream_controller.OnSentPacket(packet);
            });
    NetworkControlUpdate update =
        scream_controller.OnTransportPacketsFeedback(feedback);
    if (update.target_rate.has_value()) {
      target_rate = update.target_rate->target_rate;
    }
  }
  EXPECT_EQ(target_rate, DataRate::KilobitsPerSec(1000));

  RemoteBitrateReport msg;
  msg.bandwidth = DataRate::KilobitsPerSec(500);
  msg.receive_time = clock.CurrentTime();
  NetworkControlUpdate update = scream_controller.OnRemoteBitrateReport(msg);

  ASSERT_TRUE(update.target_rate.has_value());
  EXPECT_EQ(update.target_rate->target_rate, DataRate::KilobitsPerSec(500));

  for (int i = 0; i < 2; ++i) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(
            target_rate, clock, [&](const SentPacket& packet) {
              scream_controller.OnSentPacket(packet);
            });
    update = scream_controller.OnTransportPacketsFeedback(feedback);
    if (update.target_rate.has_value()) {
      EXPECT_EQ(update.target_rate->target_rate, DataRate::KilobitsPerSec(500));
    }
  }
}

TEST(ScreamControllerTest, PacingWindowReducedIfCeCongestedStreamsConfigured) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  CcFeedbackGenerator feedback_generator({
      .network_config = {.link_capacity = DataRate::KilobitsPerSec(900)},
      .send_as_ect1 = true,
  });

  NetworkControllerConfig config(env);
  ScreamNetworkController scream_controller(config);

  StreamsConfig streams_config;
  streams_config.max_total_allocated_bitrate = DataRate::KilobitsPerSec(1000);
  scream_controller.OnStreamsConfig(streams_config);

  NetworkControlUpdate update;
  DataRate send_rate = DataRate::KilobitsPerSec(500);
  for (int i = 0; i < 20; ++i) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(send_rate, clock);
    update = scream_controller.OnTransportPacketsFeedback(feedback);
    if (update.target_rate.has_value()) {
      send_rate = update.target_rate->target_rate;
    }
  }
  EXPECT_THAT(update.pacer_config,
              Optional(Field(&PacerConfig::time_window,
                             Lt(PacerConfig::kDefaultTimeInterval))));
}

TEST(ScreamControllerTest,
     PacingWindowNotReducedIfDelayCongestedStreamsConfigured) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  CcFeedbackGenerator feedback_generator(
      {.network_config = {.link_capacity = DataRate::KilobitsPerSec(900)},
       .send_as_ect1 = false});  // Scream will react to delay increase, not CE.

  NetworkControllerConfig config(env);
  ScreamNetworkController scream_controller(config);

  StreamsConfig streams_config;
  streams_config.max_total_allocated_bitrate = DataRate::KilobitsPerSec(1000);
  scream_controller.OnStreamsConfig(streams_config);

  NetworkControlUpdate update;
  DataRate send_rate = DataRate::KilobitsPerSec(500);
  for (int i = 0; i < 20; ++i) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(send_rate, clock);
    update = scream_controller.OnTransportPacketsFeedback(feedback);
    if (update.target_rate.has_value()) {
      send_rate = update.target_rate->target_rate;
    }
  }
  EXPECT_THAT(update.pacer_config,
              Optional(Field(&PacerConfig::time_window,
                             Eq(PacerConfig::kDefaultTimeInterval))));
}

TEST(ScreamControllerTest, InitiallyPaddingIsAllowedToReachNeededRate) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  NetworkControllerConfig config(env);
  ScreamNetworkController scream_controller(config);
  CcFeedbackGenerator feedback_generator(
      {.network_config = {.queue_delay_ms = 10,
                          .link_capacity = DataRate::KilobitsPerSec(5000)},
       .send_as_ect1 = true});
  StreamsConfig streams_config;
  streams_config.max_total_allocated_bitrate = DataRate::KilobitsPerSec(1000);
  scream_controller.OnStreamsConfig(streams_config);

  DataRate send_rate = DataRate::KilobitsPerSec(50);
  DataRate target_rate = DataRate::Zero();
  bool padding_set = false;
  Timestamp padding_stop = Timestamp::Zero();
  Timestamp start_time = clock.CurrentTime();
  while (clock.CurrentTime() < start_time + TimeDelta::Seconds(1)) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(
            send_rate, clock,
            [&](SentPacket packet) { scream_controller.OnSentPacket(packet); });
    NetworkControlUpdate update =
        scream_controller.OnTransportPacketsFeedback(feedback);
    if (update.pacer_config.has_value()) {
      if (update.pacer_config->pad_rate() != DataRate::Zero()) {
        padding_set = true;
        // Set the send rate equal to the padding rate.
        send_rate = update.pacer_config->pad_rate();
        // Pacing rate is rounded.
        EXPECT_GT(
            update.pacer_config->pad_rate(),
            update.target_rate->target_rate - DataRate::KilobitsPerSec(1));
        EXPECT_LT(
            update.pacer_config->pad_rate(),
            update.target_rate->target_rate + DataRate::KilobitsPerSec(1));
      } else if (padding_set && padding_stop.IsZero()) {
        padding_stop = clock.CurrentTime();
      }
    }
    if (update.target_rate) {
      target_rate = update.target_rate->target_rate;
    }
  }
  EXPECT_TRUE(padding_set);
  // Target rate should reach max needed rate.
  EXPECT_GE(target_rate, (*streams_config.max_total_allocated_bitrate));
  // But not much more, since seen data in flight should limit the target rate
  // increase.
  EXPECT_LE(target_rate, 1.5 * (*streams_config.max_total_allocated_bitrate));
  // Padding should stop when target is reached.
  EXPECT_LT(padding_stop - start_time, TimeDelta::Seconds(1));
}

struct PaddingTestResult {
  DataRate target_rate;
  Timestamp padding_start;
  Timestamp padding_stop;
};

PaddingTestResult ProcessUntilPaddingStartAndStop(
    SimulatedClock& clock,
    ScreamNetworkController& scream_controller,
    CcFeedbackGenerator& feedback_generator,
    bool increase_send_rate = true) {
  DataRate target_rate = DataRate::Zero();
  Timestamp padding_start = Timestamp::Zero();
  Timestamp padding_stop = Timestamp::Zero();
  Timestamp start_time = clock.CurrentTime();
  DataRate send_rate = DataRate::KilobitsPerSec(50);

  while (clock.CurrentTime() < start_time + TimeDelta::Seconds(10)) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(
            send_rate, clock,
            [&](SentPacket packet) { scream_controller.OnSentPacket(packet); });
    NetworkControlUpdate update =
        scream_controller.OnTransportPacketsFeedback(feedback);
    if (update.pacer_config.has_value()) {
      if (update.pacer_config->pad_rate() != DataRate::Zero()) {
        padding_start = clock.CurrentTime();
        if (increase_send_rate) {
          // Set the send rate equal to the padding rate.
          send_rate = update.pacer_config->pad_rate();
        }
      } else if (!padding_start.IsZero() && padding_stop.IsZero()) {
        padding_stop = clock.CurrentTime();
      }
    }
    if (update.target_rate) {
      target_rate = update.target_rate->target_rate;
    }
    if (!padding_stop.IsZero()) {
      break;
    }
  }
  EXPECT_FALSE(padding_start.IsZero());
  EXPECT_FALSE(padding_stop.IsZero());
  return {.target_rate = target_rate,
          .padding_start = padding_start,
          .padding_stop = padding_stop};
}

TEST(ScreamControllerTest, PaddingStopIfNetworkCongested) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  NetworkControllerConfig config(env);
  ScreamNetworkController scream_controller(config);
  CcFeedbackGenerator feedback_generator(
      {.network_config = {.queue_delay_ms = 10,
                          .link_capacity = DataRate::KilobitsPerSec(500)},
       .send_as_ect1 = true});
  StreamsConfig streams_config;
  streams_config.max_total_allocated_bitrate = DataRate::KilobitsPerSec(1000);
  scream_controller.OnStreamsConfig(streams_config);

  PaddingTestResult result = ProcessUntilPaddingStartAndStop(
      clock, scream_controller, feedback_generator);

  EXPECT_LT(result.target_rate, DataRate::KilobitsPerSec(600));
  // Padding should stop when congestion is detected.
  EXPECT_LT(result.padding_stop - result.padding_start, TimeDelta::Seconds(1));
}

TEST(ScreamControllerTest, PeriodicallyAllowPadding) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  CcFeedbackGenerator feedback_generator(
      {.network_config = {.queue_delay_ms = 10,
                          .link_capacity = DataRate::KilobitsPerSec(15000)},
       .send_as_ect1 = true});

  NetworkControllerConfig config(env);
  ScreamNetworkController scream_controller(config);

  StreamsConfig streams_config;
  streams_config.max_total_allocated_bitrate = DataRate::KilobitsPerSec(1000);
  scream_controller.OnStreamsConfig(streams_config);

  PaddingTestResult result_1 = ProcessUntilPaddingStartAndStop(
      clock, scream_controller, feedback_generator,
      /*increase_send_rate=*/false);
  PaddingTestResult result_2 = ProcessUntilPaddingStartAndStop(
      clock, scream_controller, feedback_generator);

  TimeDelta padding_duration = result_1.padding_stop - result_1.padding_start;
  TimeDelta time_between_padding =
      result_2.padding_start - result_1.padding_stop;
  EXPECT_GT(padding_duration, TimeDelta::Millis(2900));
  EXPECT_LT(padding_duration, TimeDelta::Millis(3100));
  EXPECT_GT(time_between_padding, TimeDelta::Millis(2900));
  EXPECT_LT(time_between_padding, TimeDelta::Millis(3200));
}

TEST(ScreamControllerTest, PadsToMinOf2xCurrentMaxAndEverSeenMax) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  NetworkControllerConfig config(env);
  ScreamNetworkController scream_controller(config);
  CcFeedbackGenerator feedback_generator(
      {.network_config = {.queue_delay_ms = 50,
                          .link_capacity = DataRate::KilobitsPerSec(5000)},
       .send_as_ect1 = true});
  StreamsConfig streams_config;
  streams_config.max_total_allocated_bitrate = DataRate::KilobitsPerSec(1000);
  scream_controller.OnStreamsConfig(streams_config);
  // Even if max_total_allocated_bitrate is lowered, padding is still allowed up
  // to 2x the new max and previous max.
  streams_config.max_total_allocated_bitrate = DataRate::KilobitsPerSec(300);
  scream_controller.OnStreamsConfig(streams_config);

  PaddingTestResult result_1 = ProcessUntilPaddingStartAndStop(
      clock, scream_controller, feedback_generator);
  EXPECT_LT(result_1.target_rate, DataRate::KilobitsPerSec(700));

  streams_config.max_total_allocated_bitrate = DataRate::KilobitsPerSec(800);
  scream_controller.OnStreamsConfig(streams_config);

  PaddingTestResult result_2 = ProcessUntilPaddingStartAndStop(
      clock, scream_controller, feedback_generator);
  EXPECT_LT(result_2.target_rate, DataRate::KilobitsPerSec(1100));
}

TEST(ScreamControllerTest, CanSetStartBitrate) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  NetworkControllerConfig config(env);

  config.constraints.starting_rate = DataRate::KilobitsPerSec(3000);
  ScreamNetworkController scream_controller(config);
  CcFeedbackGenerator feedback_generator(
      {.network_config = {.queue_delay_ms = 50,
                          .link_capacity = DataRate::KilobitsPerSec(5000)}});

  TransportPacketsFeedback feedback =
      feedback_generator.ProcessUntilNextFeedback(
          /*send_rate=*/DataRate::KilobitsPerSec(100), clock,
          [&](SentPacket packet) { scream_controller.OnSentPacket(packet); });
  NetworkControlUpdate update =
      scream_controller.OnTransportPacketsFeedback(feedback);
  EXPECT_GE(update.target_rate->target_rate, DataRate::KilobitsPerSec(2980));
  EXPECT_LT(update.target_rate->target_rate, DataRate::KilobitsPerSec(3300));
}

}  // namespace
}  // namespace webrtc
