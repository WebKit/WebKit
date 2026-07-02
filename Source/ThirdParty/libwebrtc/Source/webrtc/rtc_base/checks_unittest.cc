/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/checks.h"

#include "test/gmock.h"
#include "test/gtest.h"

namespace {

using ::testing::HasSubstr;
using ::testing::Not;

TEST(ChecksTest, ExpressionNotEvaluatedWhenCheckPassing) {
  int i = 0;
  RTC_CHECK(true) << "i=" << ++i;
  RTC_CHECK_EQ(i, 0) << "Previous check passed, but i was incremented!";
}

#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

struct StructWithStringfy {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const StructWithStringfy& /*self*/) {
    sink.Append("absl-stringify");
  }
};

TEST(ChecksDeathTest, Checks) {
#if RTC_CHECK_MSG_ENABLED
  EXPECT_DEATH(RTC_FATAL() << "message",
               "\n\n#\n"
               "# Fatal error in: \\S+, line \\w+\n"
               "# last system error: \\w+\n"
               "# Check failed: FATAL\\(\\)\n"
               "# message");

  int a = 1, b = 2;
  EXPECT_DEATH(RTC_CHECK_EQ(a, b) << 1 << 2u,
               "\n\n#\n"
               "# Fatal error in: \\S+, line \\w+\n"
               "# last system error: \\w+\n"
               "# Check failed: a == b \\(1 vs. 2\\)\n"
               "# 12");
  RTC_CHECK_EQ(5, 5);

  RTC_CHECK(true) << "Shouldn't crash" << 1;
  EXPECT_DEATH(RTC_CHECK(false) << "Hi there!",
               "\n\n#\n"
               "# Fatal error in: \\S+, line \\w+\n"
               "# last system error: \\w+\n"
               "# Check failed: false\n"
               "# Hi there!");

  StructWithStringfy t;
  EXPECT_DEATH(RTC_CHECK(false) << t, HasSubstr("absl-stringify"));
#else
  EXPECT_DEATH(RTC_FATAL() << "message",
               "\n\n#\n"
               "# Fatal error in: \\S+, line \\w+\n"
               "# last system error: \\w+\n"
               "# Check failed.\n"
               "# ");

  int a = 1, b = 2;
  EXPECT_DEATH(RTC_CHECK_EQ(a, b) << 1 << 2u,
               "\n\n#\n"
               "# Fatal error in: \\S+, line \\w+\n"
               "# last system error: \\w+\n"
               "# Check failed.\n"
               "# ");
  RTC_CHECK_EQ(5, 5);

  RTC_CHECK(true) << "Shouldn't crash" << 1;
  EXPECT_DEATH(RTC_CHECK(false) << "Hi there!",
               "\n\n#\n"
               "# Fatal error in: \\S+, line \\w+\n"
               "# last system error: \\w+\n"
               "# Check failed.\n"
               "# ");

  // Should compile, but shouldn't try to stringify 't'
  StructWithStringfy t;
  EXPECT_DEATH(RTC_CHECK(false) << t, Not(HasSubstr("absl-stringify")));
#endif  // RTC_CHECK_MSG_ENABLED
}
#endif  // GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

}  // namespace
