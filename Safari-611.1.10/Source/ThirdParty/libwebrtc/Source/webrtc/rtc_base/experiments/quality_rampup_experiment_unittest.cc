/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/quality_rampup_experiment.h"

#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

class QualityRampupExperimentTest : public ::testing::Test {
 protected:
  int64_t NowMs() const { return current_ms_; }
  int64_t AdvanceMs(int64_t delta_ms) {
    current_ms_ += delta_ms;
    return current_ms_;
  }
  int64_t current_ms_ = 2345;
};

TEST_F(QualityRampupExperimentTest, ValuesNotSetByDefault) {
  const auto settings = QualityRampupExperiment::ParseSettings();
  EXPECT_FALSE(settings.MinPixels());
  EXPECT_FALSE(settings.MinDurationMs());
  EXPECT_FALSE(settings.MaxBitrateFactor());
}

TEST_F(QualityRampupExperimentTest, ParseMinPixels) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityRampupSettings/min_pixels:10000/");
  EXPECT_EQ(10000, QualityRampupExperiment::ParseSettings().MinPixels());
}

TEST_F(QualityRampupExperimentTest, ParseMinDuration) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityRampupSettings/min_duration_ms:987/");
  EXPECT_EQ(987, QualityRampupExperiment::ParseSettings().MinDurationMs());
}

TEST_F(QualityRampupExperimentTest, ParseMaxBitrateFactor) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityRampupSettings/max_bitrate_factor:1.23/");
  EXPECT_EQ(1.23, QualityRampupExperiment::ParseSettings().MaxBitrateFactor());
}

TEST_F(QualityRampupExperimentTest, ReportsBwHighWhenDurationPassed) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityRampupSettings/"
      "min_pixels:10000,min_duration_ms:2000/");
  auto exp = QualityRampupExperiment::ParseSettings();
  EXPECT_EQ(10000, exp.MinPixels());
  EXPECT_EQ(2000, exp.MinDurationMs());

  const uint32_t kMaxKbps = 800;
  exp.SetMaxBitrate(/*pixels*/ 10000, kMaxKbps);

  const uint32_t kAvailableKbps = kMaxKbps;
  EXPECT_FALSE(exp.BwHigh(NowMs(), kAvailableKbps));
  EXPECT_FALSE(exp.BwHigh(AdvanceMs(2000 - 1), kAvailableKbps));
  EXPECT_TRUE(exp.BwHigh(AdvanceMs(1), kAvailableKbps));
}

TEST_F(QualityRampupExperimentTest, UsesMaxSetBitrate) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityRampupSettings/"
      "min_pixels:10000,min_duration_ms:2000/");
  auto exp = QualityRampupExperiment::ParseSettings();

  const uint32_t kMaxKbps = 800;
  exp.SetMaxBitrate(/*pixels*/ 10000, kMaxKbps);
  exp.SetMaxBitrate(/*pixels*/ 10000, kMaxKbps - 1);

  EXPECT_FALSE(exp.BwHigh(NowMs(), kMaxKbps - 1));
  EXPECT_FALSE(exp.BwHigh(AdvanceMs(2000), kMaxKbps - 1));
  EXPECT_FALSE(exp.BwHigh(AdvanceMs(1), kMaxKbps));
  EXPECT_TRUE(exp.BwHigh(AdvanceMs(2000), kMaxKbps));
}

TEST_F(QualityRampupExperimentTest, DoesNotReportBwHighIfBelowMinPixels) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityRampupSettings/"
      "min_pixels:10000,min_duration_ms:2000/");
  auto exp = QualityRampupExperiment::ParseSettings();

  const uint32_t kMaxKbps = 800;
  exp.SetMaxBitrate(/*pixels*/ 9999, kMaxKbps);

  const uint32_t kAvailableKbps = kMaxKbps;
  EXPECT_FALSE(exp.BwHigh(NowMs(), kAvailableKbps));
  EXPECT_FALSE(exp.BwHigh(AdvanceMs(2000), kAvailableKbps));
}

TEST_F(QualityRampupExperimentTest, ReportsBwHighWithMaxBitrateFactor) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityRampupSettings/"
      "min_pixels:10000,min_duration_ms:2000,max_bitrate_factor:1.5/");
  auto exp = QualityRampupExperiment::ParseSettings();
  EXPECT_EQ(10000, exp.MinPixels());
  EXPECT_EQ(2000, exp.MinDurationMs());
  EXPECT_EQ(1.5, exp.MaxBitrateFactor());

  const uint32_t kMaxKbps = 800;
  exp.SetMaxBitrate(/*pixels*/ 10000, kMaxKbps);

  const uint32_t kAvailableKbps = kMaxKbps * 1.5;
  EXPECT_FALSE(exp.BwHigh(NowMs(), kAvailableKbps - 1));
  EXPECT_FALSE(exp.BwHigh(AdvanceMs(2000), kAvailableKbps - 1));
  EXPECT_FALSE(exp.BwHigh(AdvanceMs(1), kAvailableKbps));
  EXPECT_TRUE(exp.BwHigh(AdvanceMs(2000), kAvailableKbps));
}

TEST_F(QualityRampupExperimentTest, ReportsBwHigh) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-Video-QualityRampupSettings/"
      "min_pixels:10000,min_duration_ms:2000/");
  auto exp = QualityRampupExperiment::ParseSettings();

  const uint32_t kMaxKbps = 800;
  exp.SetMaxBitrate(/*pixels*/ 10000, kMaxKbps);

  const uint32_t kAvailableKbps = kMaxKbps;
  EXPECT_FALSE(exp.BwHigh(NowMs(), kAvailableKbps));
  EXPECT_FALSE(exp.BwHigh(AdvanceMs(2000 - 1), kAvailableKbps));
  EXPECT_FALSE(exp.BwHigh(AdvanceMs(1), kAvailableKbps - 1));  // Below, reset.
  EXPECT_FALSE(exp.BwHigh(AdvanceMs(1), kAvailableKbps));
  EXPECT_FALSE(exp.BwHigh(AdvanceMs(2000 - 1), kAvailableKbps));
  EXPECT_TRUE(exp.BwHigh(AdvanceMs(1), kAvailableKbps));
}

}  // namespace
}  // namespace webrtc
