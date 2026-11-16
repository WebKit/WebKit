/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/transport/ecn_marking.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::AnyOfArray;
using ::testing::Not;

TEST(EcnMarkingTest, StringifyProducesNonTrivialUniqueValues) {
  std::vector<std::string> all;
  for (uint8_t i = 0; i < 4; ++i) {
    std::string name = absl::StrCat(static_cast<EcnMarking>(i));

    // Check name is not trivial - not empty, and not just the number.
    EXPECT_NE(name, "");
    EXPECT_NE(name, absl::StrCat(i));

    // Check that all values are unique.
    EXPECT_THAT(name, Not(AnyOfArray(all)));
    all.push_back(name);
  }
}

}  // namespace
}  // namespace webrtc
