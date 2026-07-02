/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/field_trials.h"

#include <memory>

#include "absl/strings/str_cat.h"
#include "api/field_trials_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_set.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::NotNull;

TEST(FieldTrialsTest, EmptyStringHasNoEffect) {
  FieldTrials f("");
  f.RegisterKeysForTesting({"MyCoolTrial"});

  EXPECT_FALSE(f.IsEnabled("MyCoolTrial"));
  EXPECT_FALSE(f.IsDisabled("MyCoolTrial"));
}

TEST(FieldTrialsTest, EnabledDisabledMustBeFirstInValue) {
  FieldTrials f(
      "MyCoolTrial/EnabledFoo/"
      "MyUncoolTrial/DisabledBar/"
      "AnotherTrial/BazEnabled/");
  f.RegisterKeysForTesting({"MyCoolTrial", "MyUncoolTrial", "AnotherTrial"});

  EXPECT_TRUE(f.IsEnabled("MyCoolTrial"));
  EXPECT_TRUE(f.IsDisabled("MyUncoolTrial"));
  EXPECT_FALSE(f.IsEnabled("AnotherTrial"));
}

TEST(FieldTrialsTest, FieldTrialsSupportSimultaneousInstances) {
  FieldTrials f1("SomeString/Enabled/");
  FieldTrials f2("SomeOtherString/Enabled/");
  f1.RegisterKeysForTesting({"SomeString", "SomeOtherString"});
  f2.RegisterKeysForTesting({"SomeString", "SomeOtherString"});

  EXPECT_TRUE(f1.IsEnabled("SomeString"));
  EXPECT_FALSE(f1.IsEnabled("SomeOtherString"));

  EXPECT_FALSE(f2.IsEnabled("SomeString"));
  EXPECT_TRUE(f2.IsEnabled("SomeOtherString"));
}

TEST(FieldTrialsTest, CreateAcceptsValidInputs) {
  EXPECT_THAT(FieldTrials::Create(""), NotNull());
  EXPECT_THAT(FieldTrials::Create("Audio/Enabled/"), NotNull());
  EXPECT_THAT(FieldTrials::Create("Audio/Enabled/Video/Disabled/"), NotNull());

  // Duplicate trials with the same value is fine
  EXPECT_THAT(FieldTrials::Create("Audio/Enabled/Audio/Enabled/"), NotNull());
  EXPECT_THAT(FieldTrials::Create("Audio/Enabled/B/C/Audio/Enabled/"),
              NotNull());
}

TEST(FieldTrialsTest, CreateRejectsBadInputs) {
  // Bad delimiters
  EXPECT_THAT(FieldTrials::Create("Audio/EnabledVideo/Disabled/"), IsNull());
  EXPECT_THAT(FieldTrials::Create("Audio/Enabled//Video/Disabled/"), IsNull());
  EXPECT_THAT(FieldTrials::Create("/Audio/Enabled/Video/Disabled/"), IsNull());
  EXPECT_THAT(FieldTrials::Create("Audio/Enabled/Video/Disabled"), IsNull());
  EXPECT_THAT(FieldTrials::Create("Audio/Enabled/Video/Disabled/garbage"),
              IsNull());

  // Empty trial or group
  EXPECT_THAT(FieldTrials::Create("Audio//"), IsNull());
  EXPECT_THAT(FieldTrials::Create("/Enabled/"), IsNull());
  EXPECT_THAT(FieldTrials::Create("//"), IsNull());
  EXPECT_THAT(FieldTrials::Create("//Enabled"), IsNull());

  // Duplicate trials with different values is not fine
  EXPECT_THAT(FieldTrials::Create("Audio/Enabled/Audio/Disabled/"), IsNull());
  EXPECT_THAT(FieldTrials::Create("Audio/Enabled/B/C/Audio/Disabled/"),
              IsNull());
  EXPECT_THAT(FieldTrials::Create("Audio/Enabled/Audio/Disabled/"), IsNull());
  EXPECT_THAT(FieldTrials::Create("Audio/Enabled/B/C/Audio/Disabled/"),
              IsNull());
}

TEST(FieldTrialsTest, StringfiyMentionsKeysAndValues) {
  // Exact format of the stringification is undefined.
  EXPECT_THAT(absl::StrCat(FieldTrials("Audio/Enabled/Video/Value/")),
              AllOf(HasSubstr("Audio"), HasSubstr("Enabled"),
                    HasSubstr("Video"), HasSubstr("Value")));
}

TEST(FieldTrialsTest, MergeCombinesFieldTrials) {
  FieldTrials f("Video/Value1/");
  FieldTrials other("Audio/Value2/");

  f.Merge(other);

  f.RegisterKeysForTesting({"Audio", "Video"});
  EXPECT_EQ(f.Lookup("Video"), "Value1");
  EXPECT_EQ(f.Lookup("Audio"), "Value2");
}

TEST(FieldTrialsTest, MergeGivesPrecedenceToOther) {
  FieldTrials f("Audio/Disabled/Video/Enabled/");
  FieldTrials other("Audio/Enabled/");

  f.Merge(other);

  f.RegisterKeysForTesting({"Audio"});
  EXPECT_EQ(f.Lookup("Audio"), "Enabled");
}

TEST(FieldTrialsTest, MergeDoesntChangeTrialAbsentInOther) {
  FieldTrials f("Audio/Enabled/Video/Enabled/");
  FieldTrials other("Audio/Enabled/");

  f.Merge(other);

  f.RegisterKeysForTesting({"Video"});
  EXPECT_EQ(f.Lookup("Video"), "Enabled");
}

TEST(FieldTrialsTest, SetUpdatesTrial) {
  FieldTrials f("Audio/Enabled/Video/Enabled/");

  f.Set("Audio", "Disabled");

  f.RegisterKeysForTesting({"Audio"});
  EXPECT_EQ(f.Lookup("Audio"), "Disabled");
}

TEST(FieldTrialsTest, SettingEmptyValueRemovesFieldTrial) {
  FieldTrials f("Audio/Enabled/Video/Enabled/");

  f.Set("Audio", "");

  f.RegisterKeysForTesting({"Audio"});
  EXPECT_EQ(f.Lookup("Audio"), "");
  EXPECT_THAT(absl::StrCat(f), Not(HasSubstr("Audio")));

  // Absent field trials shouldn't override previous value during merge.
  FieldTrials f2("Audio/Disabled/");
  f2.Merge(f);
  f2.RegisterKeysForTesting({"Audio"});
  EXPECT_EQ(f2.Lookup("Audio"), "Disabled");
}

TEST(FieldTrialsTest, CreateCopy) {
  auto f = std::make_unique<FieldTrials>("Audio/Enabled/");
  f->RegisterKeysForTesting({"Audio"});

  FieldTrialsView* view = f.get();
  auto copy = view->CreateCopy();
  f.reset();
  EXPECT_EQ(copy->Lookup("Audio"), "Enabled");
}

TEST(FieldTrials, Immutable) {
  FieldTrials f("Audio/Enabled/");
  f.RegisterKeysForTesting({"Audio"});

  // Has never been read, modifyable
  f.Set("Audio", "Disabled");
  EXPECT_EQ(f.Lookup("Audio"), "Disabled");

  // A copy can be modified.
  FieldTrials c(f);
  c.Set("Audio", "Enabled");

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
  // But FieldTrials that have been read from,
  // must not be modified (as documented in FieldTrialsView).
  EXPECT_DEATH(f.Set("Audio", "Enabled"), "");
#endif
}

}  // namespace
}  // namespace webrtc
