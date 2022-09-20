/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/experiments/field_trial_parser.h"

#include "absl/strings/string_view.h"
#include "rtc_base/experiments/field_trial_list.h"
#include "rtc_base/gunit.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"
#include "test/gmock.h"

namespace webrtc {
namespace {
const char kDummyExperiment[] = "WebRTC-DummyExperiment";

struct DummyExperiment {
  FieldTrialFlag enabled = FieldTrialFlag("Enabled");
  FieldTrialParameter<double> factor = FieldTrialParameter<double>("f", 0.5);
  FieldTrialParameter<int> retries = FieldTrialParameter<int>("r", 5);
  FieldTrialParameter<unsigned> size = FieldTrialParameter<unsigned>("s", 3);
  FieldTrialParameter<bool> ping = FieldTrialParameter<bool>("p", 0);
  FieldTrialParameter<std::string> hash =
      FieldTrialParameter<std::string>("h", "a80");

  explicit DummyExperiment(absl::string_view field_trial) {
    ParseFieldTrial({&enabled, &factor, &retries, &size, &ping, &hash},
                    field_trial);
  }
  DummyExperiment() {
    std::string trial_string = field_trial::FindFullName(kDummyExperiment);
    ParseFieldTrial({&enabled, &factor, &retries, &size, &ping, &hash},
                    trial_string);
  }
};

enum class CustomEnum {
  kDefault = 0,
  kRed = 1,
  kBlue = 2,
};

}  // namespace

TEST(FieldTrialParserTest, ParsesValidParameters) {
  DummyExperiment exp("Enabled,f:-1.7,r:2,s:10,p:1,h:x7c");
  EXPECT_TRUE(exp.enabled.Get());
  EXPECT_EQ(exp.factor.Get(), -1.7);
  EXPECT_EQ(exp.retries.Get(), 2);
  EXPECT_EQ(exp.size.Get(), 10u);
  EXPECT_EQ(exp.ping.Get(), true);
  EXPECT_EQ(exp.hash.Get(), "x7c");
}
TEST(FieldTrialParserTest, InitializesFromFieldTrial) {
  test::ScopedFieldTrials field_trials(
      "WebRTC-OtherExperiment/Disabled/"
      "WebRTC-DummyExperiment/Enabled,f:-1.7,r:2,s:10,p:1,h:x7c/"
      "WebRTC-AnotherExperiment/Enabled,f:-3.1,otherstuff:beef/");
  DummyExperiment exp;
  EXPECT_TRUE(exp.enabled.Get());
  EXPECT_EQ(exp.factor.Get(), -1.7);
  EXPECT_EQ(exp.retries.Get(), 2);
  EXPECT_EQ(exp.size.Get(), 10u);
  EXPECT_EQ(exp.ping.Get(), true);
  EXPECT_EQ(exp.hash.Get(), "x7c");
}
TEST(FieldTrialParserTest, UsesDefaults) {
  DummyExperiment exp("");
  EXPECT_FALSE(exp.enabled.Get());
  EXPECT_EQ(exp.factor.Get(), 0.5);
  EXPECT_EQ(exp.retries.Get(), 5);
  EXPECT_EQ(exp.size.Get(), 3u);
  EXPECT_EQ(exp.ping.Get(), false);
  EXPECT_EQ(exp.hash.Get(), "a80");
}
TEST(FieldTrialParserTest, CanHandleMixedInput) {
  DummyExperiment exp("p:true,h:,Enabled");
  EXPECT_TRUE(exp.enabled.Get());
  EXPECT_EQ(exp.factor.Get(), 0.5);
  EXPECT_EQ(exp.retries.Get(), 5);
  EXPECT_EQ(exp.size.Get(), 3u);
  EXPECT_EQ(exp.ping.Get(), true);
  EXPECT_EQ(exp.hash.Get(), "");
}
TEST(FieldTrialParserTest, ParsesDoubleParameter) {
  FieldTrialParameter<double> double_param("f", 0.0);
  ParseFieldTrial({&double_param}, "f:45%");
  EXPECT_EQ(double_param.Get(), 0.45);
  ParseFieldTrial({&double_param}, "f:34 %");
  EXPECT_EQ(double_param.Get(), 0.34);
  ParseFieldTrial({&double_param}, "f:0.67");
  EXPECT_EQ(double_param.Get(), 0.67);
}
TEST(FieldTrialParserTest, IgnoresNewKey) {
  DummyExperiment exp("Disabled,r:-11,foo");
  EXPECT_FALSE(exp.enabled.Get());
  EXPECT_EQ(exp.factor.Get(), 0.5);
  EXPECT_EQ(exp.retries.Get(), -11);
}
TEST(FieldTrialParserTest, IgnoresInvalid) {
  DummyExperiment exp("Enabled,f,p:,r:%,,s:-1,:foo,h");
  EXPECT_TRUE(exp.enabled.Get());
  EXPECT_EQ(exp.factor.Get(), 0.5);
  EXPECT_EQ(exp.retries.Get(), 5);
  EXPECT_EQ(exp.size.Get(), 3u);
  EXPECT_EQ(exp.ping.Get(), false);
  EXPECT_EQ(exp.hash.Get(), "a80");
}
TEST(FieldTrialParserTest, IgnoresOutOfRange) {
  FieldTrialConstrained<double> low("low", 10, absl::nullopt, 100);
  FieldTrialConstrained<double> high("high", 10, 5, absl::nullopt);
  ParseFieldTrial({&low, &high}, "low:1000,high:0");
  EXPECT_EQ(low.Get(), 10);
  EXPECT_EQ(high.Get(), 10);
  ParseFieldTrial({&low, &high}, "low:inf,high:nan");
  EXPECT_EQ(low.Get(), 10);
  EXPECT_EQ(high.Get(), 10);
  ParseFieldTrial({&low, &high}, "low:20,high:20");
  EXPECT_EQ(low.Get(), 20);
  EXPECT_EQ(high.Get(), 20);

  FieldTrialConstrained<unsigned> size("size", 5, 1, 10);
  ParseFieldTrial({&size}, "size:0");
  EXPECT_EQ(size.Get(), 5u);
}
TEST(FieldTrialParserTest, ReadsValuesFromFieldWithoutKey) {
  FieldTrialFlag enabled("Enabled");
  FieldTrialParameter<int> req("", 10);
  ParseFieldTrial({&enabled, &req}, "Enabled,20");
  EXPECT_EQ(req.Get(), 20);
  ParseFieldTrial({&req}, "30");
  EXPECT_EQ(req.Get(), 30);
}
TEST(FieldTrialParserTest, ParsesOptionalParameters) {
  FieldTrialOptional<int> max_count("c", absl::nullopt);
  ParseFieldTrial({&max_count}, "");
  EXPECT_FALSE(max_count.GetOptional().has_value());
  ParseFieldTrial({&max_count}, "c:10");
  EXPECT_EQ(max_count.GetOptional().value(), 10);
  ParseFieldTrial({&max_count}, "c");
  EXPECT_FALSE(max_count.GetOptional().has_value());
  ParseFieldTrial({&max_count}, "c:20");
  EXPECT_EQ(max_count.GetOptional().value(), 20);
  ParseFieldTrial({&max_count}, "c:");
  EXPECT_EQ(max_count.GetOptional().value(), 20);

  FieldTrialOptional<unsigned> max_size("c", absl::nullopt);
  ParseFieldTrial({&max_size}, "");
  EXPECT_FALSE(max_size.GetOptional().has_value());
  ParseFieldTrial({&max_size}, "c:10");
  EXPECT_EQ(max_size.GetOptional().value(), 10u);
  ParseFieldTrial({&max_size}, "c");
  EXPECT_FALSE(max_size.GetOptional().has_value());
  ParseFieldTrial({&max_size}, "c:20");
  EXPECT_EQ(max_size.GetOptional().value(), 20u);

  FieldTrialOptional<std::string> optional_string("s", std::string("ab"));
  ParseFieldTrial({&optional_string}, "s:");
  EXPECT_EQ(optional_string.GetOptional().value(), "");
  ParseFieldTrial({&optional_string}, "s");
  EXPECT_FALSE(optional_string.GetOptional().has_value());
}
TEST(FieldTrialParserTest, ParsesCustomEnumParameter) {
  FieldTrialEnum<CustomEnum> my_enum("e", CustomEnum::kDefault,
                                     {{"default", CustomEnum::kDefault},
                                      {"red", CustomEnum::kRed},
                                      {"blue", CustomEnum::kBlue}});
  ParseFieldTrial({&my_enum}, "");
  EXPECT_EQ(my_enum.Get(), CustomEnum::kDefault);
  ParseFieldTrial({&my_enum}, "e:red");
  EXPECT_EQ(my_enum.Get(), CustomEnum::kRed);
  ParseFieldTrial({&my_enum}, "e:2");
  EXPECT_EQ(my_enum.Get(), CustomEnum::kBlue);
  ParseFieldTrial({&my_enum}, "e:5");
  EXPECT_EQ(my_enum.Get(), CustomEnum::kBlue);
}

}  // namespace webrtc
