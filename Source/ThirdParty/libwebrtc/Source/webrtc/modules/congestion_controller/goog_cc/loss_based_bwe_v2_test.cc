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
#include <vector>

#include "api/network_state_predictor.h"
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

using ::webrtc::test::ExplicitKeyValueConfig;

constexpr TimeDelta kObservationDurationLowerBound = TimeDelta::Millis(200);
constexpr TimeDelta kDelayedIncreaseWindow = TimeDelta::Millis(300);
constexpr double kMaxIncreaseFactor = 1.5;

class LossBasedBweV2Test : public ::testing::TestWithParam<bool> {
 protected:
  std::string Config(bool enabled,
                     bool valid,
                     bool trendline_integration_enabled) {
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

    if (trendline_integration_enabled) {
      config_string << ",TrendlineIntegrationEnabled:true";
    } else {
      config_string << ",TrendlineIntegrationEnabled:false";
    }

    config_string
        << ",CandidateFactors:1.1|1.0|0.95,HigherBwBiasFactor:0.01,"
           "DelayBasedCandidate:true,"
           "InherentLossLowerBound:0.001,InherentLossUpperBoundBwBalance:"
           "14kbps,"
           "InherentLossUpperBoundOffset:0.9,InitialInherentLossEstimate:0.01,"
           "NewtonIterations:2,NewtonStepSize:0.4,ObservationWindowSize:15,"
           "SendingRateSmoothingFactor:0.01,"
           "InstantUpperBoundTemporalWeightFactor:0.97,"
           "InstantUpperBoundBwBalance:90kbps,"
           "InstantUpperBoundLossOffset:0.1,TemporalWeightFactor:0.98";

    config_string.AppendFormat(
        ",ObservationDurationLowerBound:%dms",
        static_cast<int>(kObservationDurationLowerBound.ms()));
    config_string.AppendFormat(",MaxIncreaseFactor:%f", kMaxIncreaseFactor);
    config_string.AppendFormat(",DelayedIncreaseWindow:%dms",
                               static_cast<int>(kDelayedIncreaseWindow.ms()));

    config_string << "/";

    return config_string.str();
  }

  std::vector<PacketResult> CreatePacketResultsWithReceivedPackets(
      Timestamp first_packet_timestamp) {
    std::vector<PacketResult> enough_feedback(2);
    enough_feedback[0].sent_packet.size = DataSize::Bytes(15'000);
    enough_feedback[1].sent_packet.size = DataSize::Bytes(15'000);
    enough_feedback[0].sent_packet.send_time = first_packet_timestamp;
    enough_feedback[1].sent_packet.send_time =
        first_packet_timestamp + kObservationDurationLowerBound;
    enough_feedback[0].receive_time =
        first_packet_timestamp + kObservationDurationLowerBound;
    enough_feedback[1].receive_time =
        first_packet_timestamp + 2 * kObservationDurationLowerBound;
    return enough_feedback;
  }

  std::vector<PacketResult> CreatePacketResultsWith10pLossRate(
      Timestamp first_packet_timestamp) {
    std::vector<PacketResult> enough_feedback(10);
    enough_feedback[0].sent_packet.size = DataSize::Bytes(15'000);
    for (unsigned i = 0; i < enough_feedback.size(); ++i) {
      enough_feedback[i].sent_packet.size = DataSize::Bytes(15'000);
      enough_feedback[i].sent_packet.send_time =
          first_packet_timestamp +
          static_cast<int>(i) * kObservationDurationLowerBound;
      enough_feedback[i].receive_time =
          first_packet_timestamp +
          static_cast<int>(i + 1) * kObservationDurationLowerBound;
    }
    enough_feedback[9].receive_time = Timestamp::PlusInfinity();
    return enough_feedback;
  }

  std::vector<PacketResult> CreatePacketResultsWith50pLossRate(
      Timestamp first_packet_timestamp) {
    std::vector<PacketResult> enough_feedback(2);
    enough_feedback[0].sent_packet.size = DataSize::Bytes(15'000);
    enough_feedback[1].sent_packet.size = DataSize::Bytes(15'000);
    enough_feedback[0].sent_packet.send_time = first_packet_timestamp;
    enough_feedback[1].sent_packet.send_time =
        first_packet_timestamp + kObservationDurationLowerBound;
    enough_feedback[0].receive_time =
        first_packet_timestamp + kObservationDurationLowerBound;
    enough_feedback[1].receive_time = Timestamp::PlusInfinity();
    return enough_feedback;
  }

  std::vector<PacketResult> CreatePacketResultsWith100pLossRate(
      Timestamp first_packet_timestamp) {
    std::vector<PacketResult> enough_feedback(2);
    enough_feedback[0].sent_packet.size = DataSize::Bytes(15'000);
    enough_feedback[1].sent_packet.size = DataSize::Bytes(15'000);
    enough_feedback[0].sent_packet.send_time = first_packet_timestamp;
    enough_feedback[1].sent_packet.send_time =
        first_packet_timestamp + kObservationDurationLowerBound;
    enough_feedback[0].receive_time = Timestamp::PlusInfinity();
    enough_feedback[1].receive_time = Timestamp::PlusInfinity();
    return enough_feedback;
  }
};

TEST_P(LossBasedBweV2Test, EnabledWhenGivenValidConfigurationValues) {
  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  EXPECT_TRUE(loss_based_bandwidth_estimator.IsEnabled());
}

TEST_P(LossBasedBweV2Test, DisabledWhenGivenDisabledConfiguration) {
  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/false, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  EXPECT_FALSE(loss_based_bandwidth_estimator.IsEnabled());
}

TEST_P(LossBasedBweV2Test, DisabledWhenGivenNonValidConfigurationValues) {
  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/false,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  EXPECT_FALSE(loss_based_bandwidth_estimator.IsEnabled());
}

TEST_P(LossBasedBweV2Test, DisabledWhenGivenNonPositiveCandidateFactor) {
  ExplicitKeyValueConfig key_value_config_negative_candidate_factor(
      "WebRTC-Bwe-LossBasedBweV2/Enabled:true,CandidateFactors:-1.3|1.1/");
  LossBasedBweV2 loss_based_bandwidth_estimator_1(
      &key_value_config_negative_candidate_factor);
  EXPECT_FALSE(loss_based_bandwidth_estimator_1.IsEnabled());

  ExplicitKeyValueConfig key_value_config_zero_candidate_factor(
      "WebRTC-Bwe-LossBasedBweV2/Enabled:true,CandidateFactors:0.0|1.1/");
  LossBasedBweV2 loss_based_bandwidth_estimator_2(
      &key_value_config_zero_candidate_factor);
  EXPECT_FALSE(loss_based_bandwidth_estimator_2.IsEnabled());
}

TEST_P(LossBasedBweV2Test,
       DisabledWhenGivenConfigurationThatDoesNotAllowGeneratingCandidates) {
  ExplicitKeyValueConfig key_value_config(
      "WebRTC-Bwe-LossBasedBweV2/"
      "Enabled:true,CandidateFactors:1.0,AckedRateCandidate:false,"
      "DelayBasedCandidate:false/");
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);
  EXPECT_FALSE(loss_based_bandwidth_estimator.IsEnabled());
}

TEST_P(LossBasedBweV2Test, ReturnsDelayBasedEstimateWhenDisabled) {
  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/false, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  EXPECT_EQ(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::KilobitsPerSec(100)),
            DataRate::KilobitsPerSec(100));
}

TEST_P(LossBasedBweV2Test,
       ReturnsDelayBasedEstimateWhenWhenGivenNonValidConfigurationValues) {
  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/false,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  EXPECT_EQ(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::KilobitsPerSec(100)),
            DataRate::KilobitsPerSec(100));
}

TEST_P(LossBasedBweV2Test,
       BandwidthEstimateGivenInitializationAndThenFeedback) {
  std::vector<PacketResult> enough_feedback =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero());

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);

  EXPECT_TRUE(loss_based_bandwidth_estimator.IsReady());
  EXPECT_TRUE(
      loss_based_bandwidth_estimator
          .GetBandwidthEstimate(/*delay_based_limit=*/DataRate::PlusInfinity())
          .IsFinite());
}

TEST_P(LossBasedBweV2Test, NoBandwidthEstimateGivenNoInitialization) {
  std::vector<PacketResult> enough_feedback =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero());
  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);

  EXPECT_FALSE(loss_based_bandwidth_estimator.IsReady());
  EXPECT_TRUE(
      loss_based_bandwidth_estimator
          .GetBandwidthEstimate(/*delay_based_limit=*/DataRate::PlusInfinity())
          .IsPlusInfinity());
}

TEST_P(LossBasedBweV2Test, NoBandwidthEstimateGivenNotEnoughFeedback) {
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

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));

  EXPECT_FALSE(loss_based_bandwidth_estimator.IsReady());
  EXPECT_TRUE(
      loss_based_bandwidth_estimator
          .GetBandwidthEstimate(/*delay_based_limit=*/DataRate::PlusInfinity())
          .IsPlusInfinity());

  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      not_enough_feedback, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);

  EXPECT_FALSE(loss_based_bandwidth_estimator.IsReady());
  EXPECT_TRUE(
      loss_based_bandwidth_estimator
          .GetBandwidthEstimate(/*delay_based_limit=*/DataRate::PlusInfinity())
          .IsPlusInfinity());
}

TEST_P(LossBasedBweV2Test,
       SetValueIsTheEstimateUntilAdditionalFeedbackHasBeenReceived) {
  std::vector<PacketResult> enough_feedback_1 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero());
  std::vector<PacketResult> enough_feedback_2 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero() +
          2 * kObservationDurationLowerBound);

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);

  EXPECT_NE(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(600));

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));

  EXPECT_EQ(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(600));

  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);

  EXPECT_NE(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(600));
}

TEST_P(LossBasedBweV2Test,
       SetAcknowledgedBitrateOnlyAffectsTheBweWhenAdditionalFeedbackIsGiven) {
  std::vector<PacketResult> enough_feedback_1 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero());
  std::vector<PacketResult> enough_feedback_2 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero() +
          2 * kObservationDurationLowerBound);

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator_1(&key_value_config);
  LossBasedBweV2 loss_based_bandwidth_estimator_2(&key_value_config);

  loss_based_bandwidth_estimator_1.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator_2.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator_1.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  loss_based_bandwidth_estimator_2.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);

  EXPECT_EQ(loss_based_bandwidth_estimator_1.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(660));

  loss_based_bandwidth_estimator_1.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(600));

  EXPECT_EQ(loss_based_bandwidth_estimator_1.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(660));

  loss_based_bandwidth_estimator_1.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  loss_based_bandwidth_estimator_2.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);

  EXPECT_NE(loss_based_bandwidth_estimator_1.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            loss_based_bandwidth_estimator_2.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()));
}

TEST_P(LossBasedBweV2Test,
       BandwidthEstimateIsCappedToBeTcpFairGivenTooHighLossRate) {
  std::vector<PacketResult> enough_feedback_no_received_packets =
      CreatePacketResultsWith100pLossRate(
          /*first_packet_timestamp=*/Timestamp::Zero());

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_no_received_packets, DataRate::PlusInfinity(),
      BandwidthUsage::kBwNormal);

  EXPECT_EQ(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(100));
}

TEST_P(LossBasedBweV2Test, BandwidthEstimateNotIncreaseWhenNetworkUnderusing) {
  if (!GetParam()) {
    GTEST_SKIP() << "This test should run only if "
                    "trendline_integration_enabled is enabled";
  }
  std::vector<PacketResult> enough_feedback_1 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero());
  std::vector<PacketResult> enough_feedback_2 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero() +
          2 * kObservationDurationLowerBound);

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(),
      BandwidthUsage::kBwUnderusing);
  EXPECT_LE(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  EXPECT_LE(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(600));
}

// When network is normal, estimate can increase but never be higher than
// the delay based estimate.
TEST_P(LossBasedBweV2Test,
       BandwidthEstimateCappedByDelayBasedEstimateWhenNetworkNormal) {
  // Create two packet results, network is in normal state, 100% packets are
  // received, and no delay increase.
  std::vector<PacketResult> enough_feedback_1 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero());
  std::vector<PacketResult> enough_feedback_2 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero() +
          2 * kObservationDurationLowerBound);
  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  // If the delay based estimate is infinity, then loss based estimate increases
  // and not bounded by delay based estimate.
  EXPECT_GT(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  // If the delay based estimate is not infinity, then loss based estimate is
  // bounded by delay based estimate.
  EXPECT_EQ(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::KilobitsPerSec(500)),
            DataRate::KilobitsPerSec(500));
}

// When loss based bwe receives a strong signal of overusing and an increase in
// loss rate, it should acked bitrate for emegency backoff.
TEST_P(LossBasedBweV2Test, UseAckedBitrateForEmegencyBackOff) {
  // Create two packet results, first packet has 50% loss rate, second packet
  // has 100% loss rate.
  std::vector<PacketResult> enough_feedback_1 =
      CreatePacketResultsWith50pLossRate(
          /*first_packet_timestamp=*/Timestamp::Zero());
  std::vector<PacketResult> enough_feedback_2 =
      CreatePacketResultsWith100pLossRate(
          /*first_packet_timestamp=*/Timestamp::Zero() +
          2 * kObservationDurationLowerBound);

  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  DataRate acked_bitrate = DataRate::KilobitsPerSec(300);
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(acked_bitrate);
  // Update estimate when network is overusing, and 50% loss rate.
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(),
      BandwidthUsage::kBwOverusing);
  // Update estimate again when network is continuously overusing, and 100%
  // loss rate.
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(),
      BandwidthUsage::kBwOverusing);
  // The estimate bitrate now is backed off based on acked bitrate.
  EXPECT_LE(loss_based_bandwidth_estimator.GetBandwidthEstimate(
                /*delay_based_limit=*/DataRate::PlusInfinity()),
            acked_bitrate);
}

// When receiving the same packet feedback, loss based bwe ignores the feedback
// and returns the current estimate.
TEST_P(LossBasedBweV2Test, NoBweChangeIfObservationDurationUnchanged) {
  std::vector<PacketResult> enough_feedback_1 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero());
  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);
  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(300));

  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  DataRate estimate_1 = loss_based_bandwidth_estimator.GetBandwidthEstimate(
      /*delay_based_limit=*/DataRate::PlusInfinity());

  // Use the same feedback and check if the estimate is unchanged.
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  DataRate estimate_2 = loss_based_bandwidth_estimator.GetBandwidthEstimate(
      /*delay_based_limit=*/DataRate::PlusInfinity());
  EXPECT_EQ(estimate_2, estimate_1);
}

// When receiving feedback of packets that were sent within an observation
// duration, and network is in the normal state, loss based bwe returns the
// current estimate.
TEST_P(LossBasedBweV2Test,
       NoBweChangeIfObservationDurationIsSmallAndNetworkNormal) {
  std::vector<PacketResult> enough_feedback_1 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero());
  std::vector<PacketResult> enough_feedback_2 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero() +
          kObservationDurationLowerBound - TimeDelta::Millis(1));
  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);
  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  DataRate estimate_1 = loss_based_bandwidth_estimator.GetBandwidthEstimate(
      /*delay_based_limit=*/DataRate::PlusInfinity());

  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  DataRate estimate_2 = loss_based_bandwidth_estimator.GetBandwidthEstimate(
      /*delay_based_limit=*/DataRate::PlusInfinity());
  EXPECT_EQ(estimate_2, estimate_1);
}

// When receiving feedback of packets that were sent within an observation
// duration, and network is in the underusing state, loss based bwe returns the
// current estimate.
TEST_P(LossBasedBweV2Test,
       NoBweIncreaseIfObservationDurationIsSmallAndNetworkUnderusing) {
  std::vector<PacketResult> enough_feedback_1 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero());
  std::vector<PacketResult> enough_feedback_2 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero() +
          kObservationDurationLowerBound - TimeDelta::Millis(1));
  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);
  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  DataRate estimate_1 = loss_based_bandwidth_estimator.GetBandwidthEstimate(
      /*delay_based_limit=*/DataRate::PlusInfinity());

  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(),
      BandwidthUsage::kBwUnderusing);
  DataRate estimate_2 = loss_based_bandwidth_estimator.GetBandwidthEstimate(
      /*delay_based_limit=*/DataRate::PlusInfinity());
  EXPECT_LE(estimate_2, estimate_1);
}

// When receiving feedback of packets that were sent within an observation
// duration, network is overusing, and trendline integration is enabled, loss
// based bwe updates its estimate.
TEST_P(LossBasedBweV2Test,
       UpdateEstimateIfObservationDurationIsSmallAndNetworkOverusing) {
  if (!GetParam()) {
    GTEST_SKIP() << "This test should run only if "
                    "trendline_integration_enabled is enabled";
  }
  std::vector<PacketResult> enough_feedback_1 =
      CreatePacketResultsWith50pLossRate(
          /*first_packet_timestamp=*/Timestamp::Zero());
  std::vector<PacketResult> enough_feedback_2 =
      CreatePacketResultsWith100pLossRate(
          /*first_packet_timestamp=*/Timestamp::Zero() +
          kObservationDurationLowerBound - TimeDelta::Millis(1));
  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(300));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, DataRate::PlusInfinity(), BandwidthUsage::kBwNormal);
  DataRate estimate_1 = loss_based_bandwidth_estimator.GetBandwidthEstimate(
      /*delay_based_limit=*/DataRate::PlusInfinity());

  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, DataRate::PlusInfinity(),
      BandwidthUsage::kBwOverusing);
  DataRate estimate_2 = loss_based_bandwidth_estimator.GetBandwidthEstimate(
      /*delay_based_limit=*/DataRate::PlusInfinity());
  EXPECT_LT(estimate_2, estimate_1);
}

TEST_P(LossBasedBweV2Test,
       IncreaseToDelayBasedEstimateIfNoLossOrDelayIncrease) {
  std::vector<PacketResult> enough_feedback_1 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero());
  std::vector<PacketResult> enough_feedback_2 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero() +
          2 * kObservationDurationLowerBound);
  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);
  DataRate delay_based_estimate = DataRate::KilobitsPerSec(5000);
  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));

  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, delay_based_estimate, BandwidthUsage::kBwNormal);
  EXPECT_EQ(
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate),
      delay_based_estimate);
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, delay_based_estimate, BandwidthUsage::kBwNormal);
  EXPECT_EQ(
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate),
      delay_based_estimate);
}

// After loss based bwe backs off, the next estimate is capped by
// MaxIncreaseFactor * current estimate.
TEST_P(LossBasedBweV2Test,
       IncreaseByMaxIncreaseFactorAfterLossBasedBweBacksOff) {
  std::vector<PacketResult> enough_feedback_1 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero());
  std::vector<PacketResult> enough_feedback_2 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero() +
          kObservationDurationLowerBound);
  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);
  DataRate delay_based_estimate = DataRate::KilobitsPerSec(5000);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(300));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, delay_based_estimate, BandwidthUsage::kBwNormal);
  DataRate estimate_1 =
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate);
  // Increase the acknowledged bitrate to make sure that the estimate is not
  // capped too low.
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(5000));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, delay_based_estimate, BandwidthUsage::kBwNormal);

  // The estimate is capped by current_estimate * kMaxIncreaseFactor because it
  // recently backed off.
  DataRate estimate_2 =
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate);
  EXPECT_EQ(estimate_2, estimate_1 * kMaxIncreaseFactor);
  EXPECT_LE(estimate_2, delay_based_estimate);
}

// After loss based bwe backs off, the estimate is bounded during the delayed
// window.
TEST_P(LossBasedBweV2Test,
       EstimateBitrateIsBoundedDuringDelayedWindowAfterLossBasedBweBacksOff) {
  std::vector<PacketResult> enough_feedback_1 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero());
  std::vector<PacketResult> enough_feedback_2 =
      CreatePacketResultsWith50pLossRate(
          /*first_packet_timestamp=*/Timestamp::Zero() +
          kDelayedIncreaseWindow - TimeDelta::Millis(2));
  std::vector<PacketResult> enough_feedback_3 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero() +
          kDelayedIncreaseWindow - TimeDelta::Millis(1));
  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);
  DataRate delay_based_estimate = DataRate::KilobitsPerSec(5000);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(300));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, delay_based_estimate, BandwidthUsage::kBwNormal);
  // Increase the acknowledged bitrate to make sure that the estimate is not
  // capped too low.
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(5000));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, delay_based_estimate, BandwidthUsage::kBwNormal);

  // The estimate is capped by current_estimate * kMaxIncreaseFactor because
  // it recently backed off.
  DataRate estimate_2 =
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate);

  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_3, delay_based_estimate, BandwidthUsage::kBwNormal);
  // The latest estimate is the same as the previous estimate since the sent
  // packets were sent within the DelayedIncreaseWindow.
  EXPECT_EQ(
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate),
      estimate_2);
}

// The estimate is not bounded after the delayed increase window.
TEST_P(LossBasedBweV2Test, KeepIncreasingEstimateAfterDelayedIncreaseWindow) {
  std::vector<PacketResult> enough_feedback_1 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero());
  std::vector<PacketResult> enough_feedback_2 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero() +
          kDelayedIncreaseWindow - TimeDelta::Millis(1));
  std::vector<PacketResult> enough_feedback_3 =
      CreatePacketResultsWithReceivedPackets(
          /*first_packet_timestamp=*/Timestamp::Zero() +
          kDelayedIncreaseWindow + TimeDelta::Millis(1));
  ExplicitKeyValueConfig key_value_config(
      Config(/*enabled=*/true, /*valid=*/true,
             /*trendline_integration_enabled=*/GetParam()));
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);
  DataRate delay_based_estimate = DataRate::KilobitsPerSec(5000);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(300));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_1, delay_based_estimate, BandwidthUsage::kBwNormal);
  // Increase the acknowledged bitrate to make sure that the estimate is not
  // capped too low.
  loss_based_bandwidth_estimator.SetAcknowledgedBitrate(
      DataRate::KilobitsPerSec(5000));
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_2, delay_based_estimate, BandwidthUsage::kBwNormal);

  // The estimate is capped by current_estimate * kMaxIncreaseFactor because it
  // recently backed off.
  DataRate estimate_2 =
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate);

  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_3, delay_based_estimate, BandwidthUsage::kBwNormal);
  // The estimate can continue increasing after the DelayedIncreaseWindow.
  EXPECT_GE(
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate),
      estimate_2);
}

TEST_P(LossBasedBweV2Test, NotIncreaseIfInherentLossLessThanAverageLoss) {
  ExplicitKeyValueConfig key_value_config(
      "WebRTC-Bwe-LossBasedBweV2/"
      "Enabled:true,CandidateFactors:1.2,AckedRateCandidate:false,"
      "ObservationWindowSize:2,"
      "DelayBasedCandidate:true,InstantUpperBoundBwBalance:100kbps,"
      "ObservationDurationLowerBound:200ms,"
      "NotIncreaseIfInherentLossLessThanAverageLoss:true/");
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);
  DataRate delay_based_estimate = DataRate::KilobitsPerSec(5000);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));

  std::vector<PacketResult> enough_feedback_10p_loss_1 =
      CreatePacketResultsWith10pLossRate(
          /*first_packet_timestamp=*/Timestamp::Zero());
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_10p_loss_1, delay_based_estimate,
      BandwidthUsage::kBwNormal);

  std::vector<PacketResult> enough_feedback_10p_loss_2 =
      CreatePacketResultsWith10pLossRate(
          /*first_packet_timestamp=*/Timestamp::Zero() +
          kObservationDurationLowerBound);
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_10p_loss_2, delay_based_estimate,
      BandwidthUsage::kBwNormal);

  // Do not increase the bitrate because inherent loss is less than average loss
  EXPECT_EQ(
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate),
      DataRate::KilobitsPerSec(600));
}

TEST_P(LossBasedBweV2Test,
       SelectHighBandwidthCandidateIfLossRateIsLessThanThreshold) {
  ExplicitKeyValueConfig key_value_config(
      "WebRTC-Bwe-LossBasedBweV2/"
      "Enabled:true,CandidateFactors:1.2|0.8,AckedRateCandidate:false,"
      "ObservationWindowSize:2,"
      "DelayBasedCandidate:true,InstantUpperBoundBwBalance:100kbps,"
      "ObservationDurationLowerBound:200ms,HigherBwBiasFactor:1000,"
      "HigherLogBwBiasFactor:1000,LossThresholdOfHighBandwidthPreference:0."
      "20/");
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);
  DataRate delay_based_estimate = DataRate::KilobitsPerSec(5000);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));

  std::vector<PacketResult> enough_feedback_10p_loss_1 =
      CreatePacketResultsWith10pLossRate(
          /*first_packet_timestamp=*/Timestamp::Zero());
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_10p_loss_1, delay_based_estimate,
      BandwidthUsage::kBwNormal);

  std::vector<PacketResult> enough_feedback_10p_loss_2 =
      CreatePacketResultsWith10pLossRate(
          /*first_packet_timestamp=*/Timestamp::Zero() +
          kObservationDurationLowerBound);
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_10p_loss_2, delay_based_estimate,
      BandwidthUsage::kBwNormal);

  // Because LossThresholdOfHighBandwidthPreference is 20%, the average loss is
  // 10%, bandwidth estimate should increase.
  EXPECT_GT(
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate),
      DataRate::KilobitsPerSec(600));
}

TEST_P(LossBasedBweV2Test,
       SelectLowBandwidthCandidateIfLossRateIsIsHigherThanThreshold) {
  ExplicitKeyValueConfig key_value_config(
      "WebRTC-Bwe-LossBasedBweV2/"
      "Enabled:true,CandidateFactors:1.2|0.8,AckedRateCandidate:false,"
      "ObservationWindowSize:2,"
      "DelayBasedCandidate:true,InstantUpperBoundBwBalance:100kbps,"
      "ObservationDurationLowerBound:200ms,HigherBwBiasFactor:1000,"
      "HigherLogBwBiasFactor:1000,LossThresholdOfHighBandwidthPreference:0."
      "05/");
  LossBasedBweV2 loss_based_bandwidth_estimator(&key_value_config);
  DataRate delay_based_estimate = DataRate::KilobitsPerSec(5000);

  loss_based_bandwidth_estimator.SetBandwidthEstimate(
      DataRate::KilobitsPerSec(600));

  std::vector<PacketResult> enough_feedback_10p_loss_1 =
      CreatePacketResultsWith10pLossRate(
          /*first_packet_timestamp=*/Timestamp::Zero());
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_10p_loss_1, delay_based_estimate,
      BandwidthUsage::kBwNormal);

  std::vector<PacketResult> enough_feedback_10p_loss_2 =
      CreatePacketResultsWith10pLossRate(
          /*first_packet_timestamp=*/Timestamp::Zero() +
          kObservationDurationLowerBound);
  loss_based_bandwidth_estimator.UpdateBandwidthEstimate(
      enough_feedback_10p_loss_2, delay_based_estimate,
      BandwidthUsage::kBwNormal);

  // Because LossThresholdOfHighBandwidthPreference is 5%, the average loss is
  // 10%, bandwidth estimate should decrease.
  EXPECT_LT(
      loss_based_bandwidth_estimator.GetBandwidthEstimate(delay_based_estimate),
      DataRate::KilobitsPerSec(600));
}

INSTANTIATE_TEST_SUITE_P(LossBasedBweV2Tests,
                         LossBasedBweV2Test,
                         ::testing::Bool());

}  // namespace
}  // namespace webrtc
