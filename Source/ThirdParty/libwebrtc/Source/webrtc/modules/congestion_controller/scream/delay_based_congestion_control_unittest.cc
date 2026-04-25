/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/scream/delay_based_congestion_control.h"

#include "api/environment/environment.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/scream/scream_v2_parameters.h"
#include "modules/congestion_controller/scream/test/cc_feedback_generator.h"
#include "system_wrappers/include/clock.h"
#include "test/create_test_environment.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(DelayBasedCongestionControlTest, InfiniteQueueBeforeFirstFeedback) {
  Environment env = CreateTestEnvironment();
  DelayBasedCongestionControl delay_controller(
      ScreamV2Parameters(env.field_trials()));
  EXPECT_EQ(delay_controller.queue_delay(), TimeDelta::PlusInfinity());
}

TEST(DelayBasedCongestionControlTest,
     QueueDelayDoesNotIncreaseIfSendRateIsLow) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  DelayBasedCongestionControl delay_controller(
      ScreamV2Parameters(env.field_trials()));

  // Note, network_config.queue_delay_ms is the one way delay in the simulation,
  // not a delay caused by queues.
  CcFeedbackGenerator feedback_generator({
      .network_config = {.queue_delay_ms = 25,
                         .link_capacity = DataRate::KilobitsPerSec(1000)},
  });

  // Sending at a rate below link capacity should not cause queue delay to
  // increase.
  for (int i = 0; i < 10; ++i) {
    DataRate send_rate = DataRate::KilobitsPerSec(100);
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(send_rate, clock);

    delay_controller.OnTransportPacketsFeedback(feedback);
    EXPECT_EQ(delay_controller.rtt(), TimeDelta::Millis(58));
    EXPECT_EQ(delay_controller.queue_delay(), TimeDelta::Millis(0));
    EXPECT_FALSE(delay_controller.IsQueueDelayDetected());
  }
}

TEST(DelayBasedCongestionControlTest, QueueDelayIncreaseIfSendRateIsHigh) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  DelayBasedCongestionControl delay_controller(
      ScreamV2Parameters(env.field_trials()));
  // Note, network_config.queue_delay_ms is the one way delay in the simulation,
  // not a delay caused by queues.
  CcFeedbackGenerator feedback_generator({
      .network_config = {.queue_delay_ms = 25,
                         .link_capacity = DataRate::KilobitsPerSec(1000)},
  });

  for (int i = 0; i < 10; ++i) {
    // Send faster than link capacity to build a queue.
    DataRate send_rate = DataRate::KilobitsPerSec(2000);
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(send_rate, clock);
    delay_controller.OnTransportPacketsFeedback(feedback);
  }
  EXPECT_GT(delay_controller.queue_delay(), TimeDelta::Millis(50));
  EXPECT_TRUE(delay_controller.IsQueueDelayDetected());
}

TEST(DelayBasedCongestionControlTest, ReferenceWindowNotChangedOnLowDelay) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  DelayBasedCongestionControl delay_controller(
      ScreamV2Parameters(env.field_trials()));

  // Note, network_config.queue_delay_ms is the one way delay in the simulation,
  // not a delay caused by queues.
  CcFeedbackGenerator feedback_generator({
      .network_config = {.queue_delay_ms = 25,
                         .link_capacity = DataRate::KilobitsPerSec(1000)},
  });

  DataRate send_rate = DataRate::KilobitsPerSec(500);
  DataSize ref_window = send_rate * TimeDelta::Millis(50);
  TransportPacketsFeedback feedback =
      feedback_generator.ProcessUntilNextFeedback(send_rate, clock);
  delay_controller.OnTransportPacketsFeedback(feedback);

  ASSERT_EQ(delay_controller.queue_delay(), TimeDelta::Millis(0));
  EXPECT_EQ(delay_controller.UpdateReferenceWindow(
                ref_window, /*ref_window_mss_ratio=*/1.0),
            ref_window);
}

TEST(DelayBasedCongestionControlTest, ReferenceWindowDecreasedOnHighDelay) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  DelayBasedCongestionControl delay_controller(
      ScreamV2Parameters(env.field_trials()));

  // Note, network_config.queue_delay_ms is the one way delay in the simulation,
  // not a delay caused by queues.
  CcFeedbackGenerator feedback_generator({
      .network_config = {.queue_delay_ms = 25,
                         .link_capacity = DataRate::KilobitsPerSec(1000)},
  });

  DataRate send_rate = DataRate::KilobitsPerSec(2000);
  TimeDelta smoothed_rtt;
  for (int i = 0; i < 10; ++i) {
    // Send faster than link capacity to build a queue.
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(send_rate, clock);
    delay_controller.OnTransportPacketsFeedback(feedback);
    smoothed_rtt = delay_controller.rtt();
  }
  DataSize ref_window = send_rate * smoothed_rtt;
  DataSize updated_ref_window = delay_controller.UpdateReferenceWindow(
      ref_window, /*ref_window_mss_ratio=*/1.0);
  EXPECT_LT(updated_ref_window, 0.98 * ref_window);
  EXPECT_GE(updated_ref_window, 0.5 * ref_window);
}

TEST(DelayBasedCongestionControlTest, ReferenceWindowNotLowerThanSetMin) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  DelayBasedCongestionControl delay_controller(
      ScreamV2Parameters(env.field_trials()));

  CcFeedbackGenerator feedback_generator({
      .network_config = {.queue_delay_ms = 25,
                         .link_capacity = DataRate::KilobitsPerSec(1000)},
  });

  DataRate send_rate = DataRate::KilobitsPerSec(2000);
  delay_controller.SetMinDelayBasedBwe(send_rate);
  TimeDelta smoothed_rtt;
  for (int i = 0; i < 10; ++i) {
    // Send faster than link capacity to build a queue.
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(send_rate, clock);
    delay_controller.OnTransportPacketsFeedback(feedback);
    smoothed_rtt = delay_controller.rtt();
  }
  DataSize ref_window = send_rate * smoothed_rtt;
  // Despite the queue delay, the reference window will not be decreased to a
  // value that would cause the target rate to be below the minimum.
  DataSize updated_ref_window = delay_controller.UpdateReferenceWindow(
      ref_window, /*ref_window_mss_ratio=*/1.0);
  EXPECT_EQ(updated_ref_window, ref_window);
}

TEST(DelayBasedCongestionControlTest, ResetQueueDelay) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  DelayBasedCongestionControl delay_controller(
      ScreamV2Parameters(env.field_trials()));
  CcFeedbackGenerator feedback_generator({
      .network_config = {.queue_delay_ms = 25,
                         .link_capacity = DataRate::KilobitsPerSec(100)},
  });

  Timestamp start_time = clock.CurrentTime();
  ASSERT_EQ(delay_controller.queue_delay(), TimeDelta::PlusInfinity());

  // Overuse the link during 1s.
  TimeDelta last_smoothed_rtt;
  while (clock.CurrentTime() < start_time + TimeDelta::Seconds(1)) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(
            DataRate::KilobitsPerSec(150), clock);
    delay_controller.OnTransportPacketsFeedback(feedback);
    last_smoothed_rtt = delay_controller.rtt();
  }
  TimeDelta queue_delay_before_reset = delay_controller.queue_delay();
  ASSERT_GT(queue_delay_before_reset, TimeDelta::Zero());
  ASSERT_LT(queue_delay_before_reset, TimeDelta::PlusInfinity());

  delay_controller.ResetQueueDelay();
  ASSERT_EQ(delay_controller.queue_delay(), TimeDelta::PlusInfinity());

  TransportPacketsFeedback feedback =
      feedback_generator.ProcessUntilNextFeedback(DataRate::KilobitsPerSec(150),
                                                  clock);
  delay_controller.OnTransportPacketsFeedback(feedback);
  // RTT is still increasing or equal to the last feedback.
  EXPECT_GE(delay_controller.rtt(), last_smoothed_rtt);
  // But queue delay should be lower.
  EXPECT_LT(delay_controller.queue_delay(), queue_delay_before_reset);
}

TEST(DelayBasedCongestionControlTest,
     IsQueueDrainedInTimeReturnFalseIfLongOverUse) {
  SimulatedClock clock(Timestamp::Seconds(1'234));
  Environment env = CreateTestEnvironment({.time = &clock});
  DelayBasedCongestionControl delay_controller(
      ScreamV2Parameters(env.field_trials()));
  CcFeedbackGenerator feedback_generator({
      .network_config = {.queue_delay_ms = 25,
                         .link_capacity = DataRate::KilobitsPerSec(100)},
  });

  Timestamp start_time = clock.CurrentTime();
  while (clock.CurrentTime() < start_time + TimeDelta::Seconds(30) &&
         delay_controller.IsQueueDrainedInTime(clock.CurrentTime())) {
    TransportPacketsFeedback feedback =
        feedback_generator.ProcessUntilNextFeedback(
            DataRate::KilobitsPerSec(150), clock);
    delay_controller.OnTransportPacketsFeedback(feedback);
  }
  EXPECT_LT(clock.CurrentTime(), start_time + TimeDelta::Seconds(30));
  EXPECT_GT(clock.CurrentTime(), start_time + TimeDelta::Seconds(10));
  EXPECT_FALSE(delay_controller.IsQueueDrainedInTime(clock.CurrentTime()));
}

// TODO: bugs.webrtc.org/447037083 - add tests for clock drift in feedback NTP
// time.

}  // namespace
}  // namespace webrtc
