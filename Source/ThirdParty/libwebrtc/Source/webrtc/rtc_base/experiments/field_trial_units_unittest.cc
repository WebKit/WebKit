/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <string>

#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/experiments/field_trial_units.h"
#include "rtc_base/gunit.h"

namespace webrtc {
namespace {
struct DummyExperiment {
  FieldTrialParameter<DataRate> target_rate =
      FieldTrialParameter<DataRate>("t", DataRate::kbps(100));
  FieldTrialParameter<TimeDelta> period =
      FieldTrialParameter<TimeDelta>("p", TimeDelta::ms(100));
  FieldTrialOptional<DataSize> max_buffer =
      FieldTrialOptional<DataSize>("b", absl::nullopt);

  explicit DummyExperiment(std::string field_trial) {
    ParseFieldTrial({&target_rate, &max_buffer, &period}, field_trial);
  }
};
}  // namespace

TEST(FieldTrialParserUnitsTest, FallsBackToDefaults) {
  DummyExperiment exp("");
  EXPECT_EQ(exp.target_rate.Get(), DataRate::kbps(100));
  EXPECT_FALSE(exp.max_buffer.Get().has_value());
  EXPECT_EQ(exp.period.Get(), TimeDelta::ms(100));
}
TEST(FieldTrialParserUnitsTest, ParsesUnitParameters) {
  DummyExperiment exp("t:300kbps,b:5bytes,p:300ms");
  EXPECT_EQ(exp.target_rate.Get(), DataRate::kbps(300));
  EXPECT_EQ(*exp.max_buffer.Get(), DataSize::bytes(5));
  EXPECT_EQ(exp.period.Get(), TimeDelta::ms(300));
}
TEST(FieldTrialParserUnitsTest, ParsesDefaultUnitParameters) {
  DummyExperiment exp("t:300,b:5,p:300");
  EXPECT_EQ(exp.target_rate.Get(), DataRate::kbps(300));
  EXPECT_EQ(*exp.max_buffer.Get(), DataSize::bytes(5));
  EXPECT_EQ(exp.period.Get(), TimeDelta::ms(300));
}
TEST(FieldTrialParserUnitsTest, ParsesInfinityParameter) {
  DummyExperiment exp("t:inf,p:inf");
  EXPECT_EQ(exp.target_rate.Get(), DataRate::Infinity());
  EXPECT_EQ(exp.period.Get(), TimeDelta::PlusInfinity());
}
TEST(FieldTrialParserUnitsTest, ParsesOtherUnitParameters) {
  DummyExperiment exp("t:300bps,p:0.3 seconds,b:8 bytes");
  EXPECT_EQ(exp.target_rate.Get(), DataRate::bps(300));
  EXPECT_EQ(*exp.max_buffer.Get(), DataSize::bytes(8));
  EXPECT_EQ(exp.period.Get(), TimeDelta::ms(300));
}

}  // namespace webrtc
