/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/svc/scalability_mode_util.h"

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/video_codecs/scalability_mode.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(ScalabilityModeUtil, ConvertsL1T2) {
  EXPECT_EQ(ScalabilityModeFromString("L1T2"), ScalabilityMode::kL1T2);
  EXPECT_EQ(ScalabilityModeToString(ScalabilityMode::kL1T2), "L1T2");
}

TEST(ScalabilityModeUtil, RejectsUnknownString) {
  EXPECT_EQ(ScalabilityModeFromString(""), absl::nullopt);
  EXPECT_EQ(ScalabilityModeFromString("not-a-mode"), absl::nullopt);
}

// Check roundtrip conversion of all enum values.
TEST(ScalabilityModeUtil, ConvertsAllToAndFromString) {
  const ScalabilityMode kLastEnum = ScalabilityMode::kS3T3;
  for (int numerical_enum = 0; numerical_enum <= static_cast<int>(kLastEnum);
       numerical_enum++) {
    ScalabilityMode scalability_mode =
        static_cast<ScalabilityMode>(numerical_enum);
    absl::string_view scalability_mode_string =
        ScalabilityModeToString(scalability_mode);
    EXPECT_FALSE(scalability_mode_string.empty());
    EXPECT_EQ(ScalabilityModeFromString(scalability_mode_string),
              scalability_mode);
  }
}

}  // namespace
}  // namespace webrtc
