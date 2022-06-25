/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "system_wrappers/include/field_trial.h"

#include "rtc_base/checks.h"
#include "test/gtest.h"
#include "test/testsupport/rtc_expect_death.h"

namespace webrtc {
namespace field_trial {
#if GTEST_HAS_DEATH_TEST && RTC_DCHECK_IS_ON && !defined(WEBRTC_ANDROID) && \
    !defined(WEBRTC_EXCLUDE_FIELD_TRIAL_DEFAULT)
TEST(FieldTrialValidationTest, AcceptsValidInputs) {
  InitFieldTrialsFromString("");
  InitFieldTrialsFromString("Audio/Enabled/");
  InitFieldTrialsFromString("Audio/Enabled/Video/Disabled/");
  EXPECT_TRUE(FieldTrialsStringIsValid(""));
  EXPECT_TRUE(FieldTrialsStringIsValid("Audio/Enabled/"));
  EXPECT_TRUE(FieldTrialsStringIsValid("Audio/Enabled/Video/Disabled/"));

  // Duplicate trials with the same value is fine
  InitFieldTrialsFromString("Audio/Enabled/Audio/Enabled/");
  InitFieldTrialsFromString("Audio/Enabled/B/C/Audio/Enabled/");
  EXPECT_TRUE(FieldTrialsStringIsValid("Audio/Enabled/Audio/Enabled/"));
  EXPECT_TRUE(FieldTrialsStringIsValid("Audio/Enabled/B/C/Audio/Enabled/"));
}

TEST(FieldTrialValidationDeathTest, RejectsBadInputs) {
  // Bad delimiters
  RTC_EXPECT_DEATH(InitFieldTrialsFromString("Audio/EnabledVideo/Disabled/"),
                   "Invalid field trials string:");
  RTC_EXPECT_DEATH(InitFieldTrialsFromString("Audio/Enabled//Video/Disabled/"),
                   "Invalid field trials string:");
  RTC_EXPECT_DEATH(InitFieldTrialsFromString("/Audio/Enabled/Video/Disabled/"),
                   "Invalid field trials string:");
  RTC_EXPECT_DEATH(InitFieldTrialsFromString("Audio/Enabled/Video/Disabled"),
                   "Invalid field trials string:");
  RTC_EXPECT_DEATH(
      InitFieldTrialsFromString("Audio/Enabled/Video/Disabled/garbage"),
      "Invalid field trials string:");
  EXPECT_FALSE(FieldTrialsStringIsValid("Audio/EnabledVideo/Disabled/"));
  EXPECT_FALSE(FieldTrialsStringIsValid("Audio/Enabled//Video/Disabled/"));
  EXPECT_FALSE(FieldTrialsStringIsValid("/Audio/Enabled/Video/Disabled/"));
  EXPECT_FALSE(FieldTrialsStringIsValid("Audio/Enabled/Video/Disabled"));
  EXPECT_FALSE(
      FieldTrialsStringIsValid("Audio/Enabled/Video/Disabled/garbage"));

  // Empty trial or group
  RTC_EXPECT_DEATH(InitFieldTrialsFromString("Audio//"),
                   "Invalid field trials string:");
  RTC_EXPECT_DEATH(InitFieldTrialsFromString("/Enabled/"),
                   "Invalid field trials string:");
  RTC_EXPECT_DEATH(InitFieldTrialsFromString("//"),
                   "Invalid field trials string:");
  RTC_EXPECT_DEATH(InitFieldTrialsFromString("//Enabled"),
                   "Invalid field trials string:");
  EXPECT_FALSE(FieldTrialsStringIsValid("Audio//"));
  EXPECT_FALSE(FieldTrialsStringIsValid("/Enabled/"));
  EXPECT_FALSE(FieldTrialsStringIsValid("//"));
  EXPECT_FALSE(FieldTrialsStringIsValid("//Enabled"));

  // Duplicate trials with different values is not fine
  RTC_EXPECT_DEATH(InitFieldTrialsFromString("Audio/Enabled/Audio/Disabled/"),
                   "Invalid field trials string:");
  RTC_EXPECT_DEATH(
      InitFieldTrialsFromString("Audio/Enabled/B/C/Audio/Disabled/"),
      "Invalid field trials string:");
  EXPECT_FALSE(FieldTrialsStringIsValid("Audio/Enabled/Audio/Disabled/"));
  EXPECT_FALSE(FieldTrialsStringIsValid("Audio/Enabled/B/C/Audio/Disabled/"));
}

TEST(FieldTrialMergingTest, MergesValidInput) {
  EXPECT_EQ(MergeFieldTrialsStrings("Video/Enabled/", "Audio/Enabled/"),
            "Audio/Enabled/Video/Enabled/");
  EXPECT_EQ(MergeFieldTrialsStrings("Audio/Disabled/Video/Enabled/",
                                    "Audio/Enabled/"),
            "Audio/Enabled/Video/Enabled/");
  EXPECT_EQ(
      MergeFieldTrialsStrings("Audio/Enabled/Video/Enabled/", "Audio/Enabled/"),
      "Audio/Enabled/Video/Enabled/");
  EXPECT_EQ(
      MergeFieldTrialsStrings("Audio/Enabled/Audio/Enabled/", "Video/Enabled/"),
      "Audio/Enabled/Video/Enabled/");
}

TEST(FieldTrialMergingDeathTest, DchecksBadInput) {
  RTC_EXPECT_DEATH(MergeFieldTrialsStrings("Audio/Enabled/", "garbage"),
                   "Invalid field trials string:");
}

TEST(FieldTrialMergingTest, HandlesEmptyInput) {
  EXPECT_EQ(MergeFieldTrialsStrings("", "Audio/Enabled/"), "Audio/Enabled/");
  EXPECT_EQ(MergeFieldTrialsStrings("Audio/Enabled/", ""), "Audio/Enabled/");
  EXPECT_EQ(MergeFieldTrialsStrings("", ""), "");
}
#endif  // GTEST_HAS_DEATH_TEST && RTC_DCHECK_IS_ON && !defined(WEBRTC_ANDROID)
        // && !defined(WEBRTC_EXCLUDE_FIELD_TRIAL_DEFAULT)

}  // namespace field_trial
}  // namespace webrtc
