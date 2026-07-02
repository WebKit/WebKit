/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/alr_detector.h"

#include <optional>

#include "absl/base/nullability.h"
#include "api/environment/environment_factory.h"
#include "api/field_trials.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/alr_experiment.h"
#include "system_wrappers/include/clock.h"
#include "test/create_test_field_trials.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr DataRate kEstimatedBitrate = DataRate::BitsPerSec(300'000);

class SimulateOutgoingTrafficIn {
 public:
  explicit SimulateOutgoingTrafficIn(AlrDetector* absl_nonnull alr_detector,
                                     SimulatedClock* absl_nonnull clock)
      : alr_detector_(*alr_detector), clock_(*clock) {}

  SimulateOutgoingTrafficIn& ForTime(TimeDelta time) {
    interval_ = time;
    ProduceTraffic();
    return *this;
  }

  SimulateOutgoingTrafficIn& AtPercentOfEstimatedBitrate(int usage_percentage) {
    usage_percentage_.emplace(usage_percentage);
    ProduceTraffic();
    return *this;
  }

 private:
  void ProduceTraffic() {
    if (!interval_ || !usage_percentage_)
      return;
    const TimeDelta kTimeStep = TimeDelta::Millis(10);
    for (TimeDelta t = TimeDelta::Zero(); t < *interval_; t += kTimeStep) {
      clock_.AdvanceTime(kTimeStep);
      alr_detector_.OnBytesSent(
          kEstimatedBitrate * *usage_percentage_ * kTimeStep / 100,
          clock_.CurrentTime());
    }
    // As of now all tests use interval that is a multiple of 10ms.
    RTC_DCHECK_EQ(interval_->ms() % kTimeStep.ms(), 0);
  }
  AlrDetector& alr_detector_;
  SimulatedClock& clock_;
  std::optional<TimeDelta> interval_;
  std::optional<int> usage_percentage_;
};
}  // namespace

TEST(AlrDetectorTest, AlrDetection) {
  SimulatedClock clock(Timestamp::Seconds(1));
  AlrDetector alr_detector(
      CreateEnvironment(CreateTestFieldTrialsPtr(), &clock));
  alr_detector.SetEstimatedBitrate(kEstimatedBitrate);

  // Start in non-ALR state.
  EXPECT_FALSE(alr_detector.GetApplicationLimitedRegionStartTime());

  // Stay in non-ALR state when usage is close to 100%.
  SimulateOutgoingTrafficIn(&alr_detector, &clock)
      .ForTime(TimeDelta::Seconds(1))
      .AtPercentOfEstimatedBitrate(90);
  EXPECT_FALSE(alr_detector.GetApplicationLimitedRegionStartTime());

  // Verify that we ALR starts when bitrate drops below 20%.
  SimulateOutgoingTrafficIn(&alr_detector, &clock)
      .ForTime(TimeDelta::Millis(1'500))
      .AtPercentOfEstimatedBitrate(20);
  EXPECT_TRUE(alr_detector.GetApplicationLimitedRegionStartTime());

  // Verify that ALR ends when usage is above 65%.
  SimulateOutgoingTrafficIn(&alr_detector, &clock)
      .ForTime(TimeDelta::Seconds(4))
      .AtPercentOfEstimatedBitrate(100);
  EXPECT_FALSE(alr_detector.GetApplicationLimitedRegionStartTime());
}

TEST(AlrDetectorTest, ShortSpike) {
  SimulatedClock clock(Timestamp::Seconds(1));
  AlrDetector alr_detector(
      CreateEnvironment(CreateTestFieldTrialsPtr(), &clock));
  alr_detector.SetEstimatedBitrate(kEstimatedBitrate);
  // Start in non-ALR state.
  EXPECT_FALSE(alr_detector.GetApplicationLimitedRegionStartTime());

  // Verify that we ALR starts when bitrate drops below 20%.
  SimulateOutgoingTrafficIn(&alr_detector, &clock)
      .ForTime(TimeDelta::Seconds(1))
      .AtPercentOfEstimatedBitrate(20);
  EXPECT_TRUE(alr_detector.GetApplicationLimitedRegionStartTime());

  // Verify that we stay in ALR region even after a short bitrate spike.
  SimulateOutgoingTrafficIn(&alr_detector, &clock)
      .ForTime(TimeDelta::Millis(100))
      .AtPercentOfEstimatedBitrate(150);
  EXPECT_TRUE(alr_detector.GetApplicationLimitedRegionStartTime());

  // ALR ends when usage is above 65%.
  SimulateOutgoingTrafficIn(&alr_detector, &clock)
      .ForTime(TimeDelta::Seconds(3))
      .AtPercentOfEstimatedBitrate(100);
  EXPECT_FALSE(alr_detector.GetApplicationLimitedRegionStartTime());
}

TEST(AlrDetectorTest, BandwidthEstimateChanges) {
  SimulatedClock clock(Timestamp::Seconds(1));
  AlrDetector alr_detector(
      CreateEnvironment(CreateTestFieldTrialsPtr(), &clock));
  alr_detector.SetEstimatedBitrate(kEstimatedBitrate);

  // Start in non-ALR state.
  EXPECT_FALSE(alr_detector.GetApplicationLimitedRegionStartTime());

  // ALR starts when bitrate drops below 20%.
  SimulateOutgoingTrafficIn(&alr_detector, &clock)
      .ForTime(TimeDelta::Seconds(1))
      .AtPercentOfEstimatedBitrate(20);
  EXPECT_TRUE(alr_detector.GetApplicationLimitedRegionStartTime());

  // When bandwidth estimate drops the detector should stay in ALR mode and quit
  // it shortly afterwards as the sender continues sending the same amount of
  // traffic. This is necessary to ensure that ProbeController can still react
  // to the BWE drop by initiating a new probe.
  alr_detector.SetEstimatedBitrate(kEstimatedBitrate / 5);
  EXPECT_TRUE(alr_detector.GetApplicationLimitedRegionStartTime());
  SimulateOutgoingTrafficIn(&alr_detector, &clock)
      .ForTime(TimeDelta::Seconds(1))
      .AtPercentOfEstimatedBitrate(50);
  EXPECT_FALSE(alr_detector.GetApplicationLimitedRegionStartTime());
}

TEST(AlrDetectorTest, ParseControlFieldTrial) {
  FieldTrials field_trials =
      CreateTestFieldTrials("WebRTC-ProbingScreenshareBwe/Control/");
  std::optional<AlrExperimentSettings> parsed_params =
      AlrExperimentSettings::CreateFromFieldTrial(
          field_trials, "WebRTC-ProbingScreenshareBwe");
  EXPECT_FALSE(static_cast<bool>(parsed_params));
}

TEST(AlrDetectorTest, ParseActiveFieldTrial) {
  FieldTrials field_trials = CreateTestFieldTrials(
      "WebRTC-ProbingScreenshareBwe/1.1,2875,85,20,-20,1/");
  std::optional<AlrExperimentSettings> parsed_params =
      AlrExperimentSettings::CreateFromFieldTrial(
          field_trials, "WebRTC-ProbingScreenshareBwe");
  ASSERT_TRUE(static_cast<bool>(parsed_params));
  EXPECT_EQ(1.1f, parsed_params->pacing_factor);
  EXPECT_EQ(2875, parsed_params->max_paced_queue_time);
  EXPECT_EQ(85, parsed_params->alr_bandwidth_usage_percent);
  EXPECT_EQ(20, parsed_params->alr_start_budget_level_percent);
  EXPECT_EQ(-20, parsed_params->alr_stop_budget_level_percent);
  EXPECT_EQ(1, parsed_params->group_id);
}

TEST(AlrDetectorTest, ParseAlrSpecificFieldTrial) {
  SimulatedClock clock(Timestamp::Seconds(1));
  AlrDetector alr_detector(CreateEnvironment(
      CreateTestFieldTrialsPtr(
          "WebRTC-AlrDetectorParameters/bw_usage:90%,start:0%,stop:-10%/"),
      &clock));
  alr_detector.SetEstimatedBitrate(kEstimatedBitrate);

  // Start in non-ALR state.
  EXPECT_FALSE(alr_detector.GetApplicationLimitedRegionStartTime());

  // ALR does not start at 100% utilization.
  SimulateOutgoingTrafficIn(&alr_detector, &clock)
      .ForTime(TimeDelta::Seconds(1))
      .AtPercentOfEstimatedBitrate(100);
  EXPECT_FALSE(alr_detector.GetApplicationLimitedRegionStartTime());

  // ALR does start at 85% utilization.
  // Overused 10% above so it should take about 2s to reach a budget level of
  // 0%.
  SimulateOutgoingTrafficIn(&alr_detector, &clock)
      .ForTime(TimeDelta::Millis(2'100))
      .AtPercentOfEstimatedBitrate(85);
  EXPECT_TRUE(alr_detector.GetApplicationLimitedRegionStartTime());
}

}  // namespace webrtc
