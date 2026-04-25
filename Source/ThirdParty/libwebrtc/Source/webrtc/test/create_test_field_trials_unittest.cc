/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/create_test_field_trials.h"

#include <string>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/string_view.h"
#include "api/field_trials.h"
#include "test/gtest.h"

// Declare but do not explicitly include the definition of this flag as
// `CreateTestFieldTrials` has to depend on the definition of this flag.
ABSL_DECLARE_FLAG(std::string, force_fieldtrials);

namespace webrtc {
namespace {

// Flags are globals. To prevent altering behavior of other tests in the same
// binary when --force_fieldtrials flag is used, ensure flag is reset to the
// previous value after unit tests here runs.
class ScopedSetFlag {
 public:
  explicit ScopedSetFlag(absl::string_view value) {
    old_value_ = absl::GetFlag(FLAGS_force_fieldtrials);
    absl::SetFlag(&FLAGS_force_fieldtrials, value);
  }

  ~ScopedSetFlag() { absl::SetFlag(&FLAGS_force_fieldtrials, old_value_); }

 private:
  std::string old_value_;
};

TEST(CreateTestFieldTrialsTest, UsesCommandLineFlag) {
  ScopedSetFlag override_flag("Trial1/Value1/Trial2/Value2/");
  FieldTrials field_trials = CreateTestFieldTrials();

  field_trials.RegisterKeysForTesting({"Trial1", "Trial2"});
  EXPECT_EQ(field_trials.Lookup("Trial1"), "Value1");
  EXPECT_EQ(field_trials.Lookup("Trial2"), "Value2");
}

TEST(CreateTestFieldTrialsTest, UsesConstructionParameter) {
  FieldTrials field_trials =
      CreateTestFieldTrials("Trial1/Value1/Trial2/Value2/");

  field_trials.RegisterKeysForTesting({"Trial1", "Trial2"});
  EXPECT_EQ(field_trials.Lookup("Trial1"), "Value1");
  EXPECT_EQ(field_trials.Lookup("Trial2"), "Value2");
}

TEST(CreateTestFieldTrialsTest,
     ConstructionParameterTakesPrecedenceOverCommandLine) {
  ScopedSetFlag override_flag("TrialCommon/ValueF/TrialFlag/FlagValue/");
  FieldTrials field_trials = CreateTestFieldTrials(
      "TrialCommon/ValueC/TrialConstructor/ConstructorValue/");

  field_trials.RegisterKeysForTesting(
      {"TrialCommon", "TrialFlag", "TrialConstructor"});
  EXPECT_EQ(field_trials.Lookup("TrialCommon"), "ValueC");
  EXPECT_EQ(field_trials.Lookup("TrialFlag"), "FlagValue");
  EXPECT_EQ(field_trials.Lookup("TrialConstructor"), "ConstructorValue");
}

}  // namespace
}  // namespace webrtc
