/*
 *  Copyright 2021 The WebRTC project authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/loss_based_bwe_v2.h"

#include <string>

#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/strings/string_builder.h"
#include "test/explicit_key_value_config.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

constexpr TimeDelta kObservationDurationLowerBound = TimeDelta::Millis(200);

std::string Config(bool enabled, bool valid) {
  char buffer[1024];
  rtc::SimpleStringBuilder config_string(buffer);

  config_string << "WebRTC-Bwe-LossBasedBweV2/";

  if (enabled) {
    config_string << "Enabled:true";
  } else {
    config_string << "Enabled:false";
  }

  if (valid) {
    config_string << ",BwRampupUpperBoundFactor:1.2";
  } else {
    config_string << ",BwRampupUpperBoundFactor:0.0";
  }

  config_string
      << ",CandidateFactors:0.9|1.1,HigherBwBiasFactor:0.01,"
         "InherentLossLowerBound:0.001,InherentLossUpperBoundBwBalance:14kbps,"
         "InherentLossUpperBoundOffset:0.9,InitialInherentLossEstimate:0.01,"
         "NewtonIterations:2,NewtonStepSize:0.4,ObservationWindowSize:15,"
         "SendingRateSmoothingFactor:0.01,"
         "InstantUpperBoundTemporalWeightFactor:0.97,"
         "InstantUpperBoundBwBalance:90kbps,"
         "InstantUpperBoundLossOffset:0.1,TemporalWeightFactor:0.98";

  config_string.AppendFormat(
      ",ObservationDurationLowerBound:%dms",
      static_cast<int>(kObservationDurationLowerBound.ms()));

  config_string << "/";

  return config_string.str();
}

TEST(LossBasedBweV2Test, EnabledWhenGivenValidConfigurationValues) {
  test::ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  EXPECT_TRUE(loss_based_bandwidth_estimator.IsEnabled());
}

TEST(LossBasedBweV2Test, DisabledWhenGivenDisabledConfiguration) {
  test::ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/false, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  EXPECT_FALSE(loss_based_bandwidth_estimator.IsEnabled());
}

TEST(LossBasedBweV2Test, DisabledWhenGivenNonValidConfigurationValues) {
  test::ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/false));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  EXPECT_FALSE(loss_based_bandwidth_estimator.IsEnabled());
}

TEST(LossBasedBweV2Test, BandwidthEstimateGivenInitializationAndThenFeedback) {
  PacketResult enough_feedback[2];
  enough_feedback[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback[0].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;

  test::ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback, DataRate::PlusInfinity());

  EXPECT_TRUE(loss_based_bandwidth_estimator.IsReady());
  EXPECT_TRUE(loss_based_bandwidth_estimator.GetBandwidthEstimate().IsFinite());
}

TEST(LossBasedBweV2Test, NoBandwidthEstimateGivenNoInitialization) {
  PacketResult enough_feedback[2];
  enough_feedback[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback[0].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;

  test::ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback, DataRate::PlusInfinity());

  EXPECT_FALSE(loss_based_bandwidth_estimator.IsReady());
  EXPECT_TRUE(
      loss_based_bandwidth_estimator.GetBandwidthEstimate().IsPlusInfinity());
}

TEST(LossBasedBweV2Test, NoBandwidthEstimateGivenNotEnoughFeedback) {
  // Create packet results where the observation duration is less than the lower
  // bound.
  PacketResult not_enough_feedback[2];
  not_enough_feedback[0].sent_packet.size = DataSize::Bytes(15'000);
  not_enough_feedback[1].sent_packet.size = DataSize::Bytes(15'000);
  not_enough_feedback[0].sent_packet.send_time = Timestamp::Zero();
  not_enough_feedback[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound / 2;
  not_enough_feedback[0].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound / 2;
  not_enough_feedback[1].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound;

  test::ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));

  EXPECT_FALSE(loss_based_bandwidth_estimator.IsReady());
  EXPECT_TRUE(
      loss_based_bandwidth_estimator.GetBandwidthEstimate().IsPlusInfinity());

  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      not_enough_feedback, DataRate::PlusInfinity());

  EXPECT_FALSE(loss_based_bandwidth_estimator.IsReady());
  EXPECT_TRUE(
      loss_based_bandwidth_estimator.GetBandwidthEstimate().IsPlusInfinity());
}

TEST(LossBasedBweV2Test,
     SetValueIsTheEstimateUntilAdditionalFeedbackHasBeenReceived) {
  PacketResult enough_feedback_1[2];
  PacketResult enough_feedback_2[2];
  enough_feedback_1[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback_1[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_2[0].sent_packet.send_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[1].sent_packet.send_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_1[0].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_1[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[0].receive_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_2[1].receive_time =
      Timestamp::Zero() + 4 * kObservationDurationLowerBound;

  test::ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity());

  EXPECT_NE(loss_based_bandwidth_estimator.GetBandwidthEstimate(),
            DataRate::KilobitsPerSec(600));

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));

  EXPECT_EQ(loss_based_bandwidth_estimator.GetBandwidthEstimate(),
            DataRate::KilobitsPerSec(600));

  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity());

  EXPECT_NE(loss_based_bandwidth_estimator.GetBandwidthEstimate(),
            DataRate::KilobitsPerSec(600));
}

TEST(LossBasedBweV2Test,
     SetAcknowledgedBitrateOnlyAffectsTheBweWhenAdditionalFeedbackIsGiven) {
  PacketResult enough_feedback_1[2];
  PacketResult enough_feedback_2[2];
  enough_feedback_1[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[0].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_2[1].sent_packet.size = DataSize::Bytes(15'000);
  enough_feedback_1[0].sent_packet.send_time = Timestamp::Zero();
  enough_feedback_1[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_2[0].sent_packet.send_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[1].sent_packet.send_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_1[0].receive_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_1[1].receive_time =
      Timestamp::Zero() + 2 * kObservationDurationLowerBound;
  enough_feedback_2[0].receive_time =
      Timestamp::Zero() + 3 * kObservationDurationLowerBound;
  enough_feedback_2[1].receive_time =
      Timestamp::Zero() + 4 * kObservationDurationLowerBound;

  test::ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator_1(&key_value_config);
  LossBasedBweV2 loss_based_bandwidth_estimator_2(&key_value_config);

  loss_based_bandwidth_estimator_1.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator_2.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator_1.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity());
  loss_based_bandwidth_estimator_2.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity());

  EXPECT_EQ(loss_based_bandwidth_estimator_1.GetBandwidthEstimate(),
            DataRate::KilobitsPerSec(660));

  loss_based_bandwidth_estimator_1.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(600));

  EXPECT_EQ(loss_based_bandwidth_estimator_1.GetBandwidthEstimate(),
            DataRate::KilobitsPerSec(660));

  loss_based_bandwidth_estimator_1.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity());
  loss_based_bandwidth_estimator_2.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity());

  EXPECT_NE(loss_based_bandwidth_estimator_1.GetBandwidthEstimate(),
            loss_based_bandwidth_estimator_2.GetBandwidthEstimate());
}

TEST(LossBasedBweV2Test,
     BandwidthEstimateIsCappedToBeTcpFairGivenTooHighLossRate) {
  PacketResult enough_feedback_no_received_packets[2];
  enough_feedback_no_received_packets[0].sent_packet.size =
      DataSize::Bytes(15'000);
  enough_feedback_no_received_packets[1].sent_packet.size =
      DataSize::Bytes(15'000);
  enough_feedback_no_received_packets[0].sent_packet.send_time =
      Timestamp::Zero();
  enough_feedback_no_received_packets[1].sent_packet.send_time =
      Timestamp::Zero() + kObservationDurationLowerBound;
  enough_feedback_no_received_packets[0].receive_time =
      Timestamp::PlusInfinity();
  enough_feedback_no_received_packets[1].receive_time =
      Timestamp::PlusInfinity();

  test::ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_no_received_packets, DataRate::PlusInfinity());

  EXPECT_EQ(loss_based_bandwidth_estimator.GetBandwidthEstimate(),
            DataRate::KilobitsPerSec(100));
}

}  // namespace

}  // namespace webrtc
