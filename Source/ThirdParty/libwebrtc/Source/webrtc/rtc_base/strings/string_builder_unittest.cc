/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/strings/string_builder.h"

#include "rtc_base/checks.h"
#include "rtc_base/stringutils.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace rtc {

TEST(SimpleStringBuilder, Limit) {
  char sb_buf[10];
  SimpleStringBuilder sb(sb_buf);
  EXPECT_EQ(0u, strlen(sb.str()));

  // Test that for a SSB with a buffer size of 10, that we can write 9 chars
  // into it.
  sb << "012345678";  // 9 characters + '\0'.
  EXPECT_EQ(0, strcmp(sb.str(), "012345678"));
}

TEST(SimpleStringBuilder, NumbersAndChars) {
  char sb_buf[100];
  SimpleStringBuilder sb(sb_buf);
  sb << 1 << ':' << 2.1 << ":" << 2.2f << ':' << 78187493520ll << ':'
     << 78187493520ul;
  EXPECT_EQ(0, strcmp(sb.str(), "1:2.1:2.2:78187493520:78187493520"));
}

TEST(SimpleStringBuilder, Format) {
  char sb_buf[100];
  SimpleStringBuilder sb(sb_buf);
  sb << "Here we go - ";
  sb.AppendFormat("This is a hex formatted value: 0x%08llx", 3735928559ULL);
  EXPECT_EQ(0,
            strcmp(sb.str(),
                   "Here we go - This is a hex formatted value: 0xdeadbeef"));
}

TEST(SimpleStringBuilder, StdString) {
  char sb_buf[100];
  SimpleStringBuilder sb(sb_buf);
  std::string str = "does this work?";
  sb << str;
  EXPECT_EQ(str, sb.str());
}

// These tests are safe to run if we have death test support or if DCHECKs are
// off.
#if (GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)) || !RTC_DCHECK_IS_ON

TEST(SimpleStringBuilder, BufferOverrunConstCharP) {
  char sb_buf[4];
  SimpleStringBuilder sb(sb_buf);
  const char* const msg = "This is just too much";
#if RTC_DCHECK_IS_ON
  EXPECT_DEATH(sb << msg, "");
#else
  sb << msg;
  EXPECT_THAT(sb.str(), testing::StrEq("Thi"));
#endif
}

TEST(SimpleStringBuilder, BufferOverrunStdString) {
  char sb_buf[4];
  SimpleStringBuilder sb(sb_buf);
  sb << 12;
  const std::string msg = "Aw, come on!";
#if RTC_DCHECK_IS_ON
  EXPECT_DEATH(sb << msg, "");
#else
  sb << msg;
  EXPECT_THAT(sb.str(), testing::StrEq("12A"));
#endif
}

TEST(SimpleStringBuilder, BufferOverrunInt) {
  char sb_buf[4];
  SimpleStringBuilder sb(sb_buf);
  constexpr int num = -12345;
#if RTC_DCHECK_IS_ON
  EXPECT_DEATH(sb << num, "");
#else
  sb << num;
  // If we run into the end of the buffer, resonable results are either that
  // the append has no effect or that it's truncated at the point where the
  // buffer ends.
  EXPECT_THAT(sb.str(),
              testing::AnyOf(testing::StrEq(""), testing::StrEq("-12")));
#endif
}

TEST(SimpleStringBuilder, BufferOverrunDouble) {
  char sb_buf[5];
  SimpleStringBuilder sb(sb_buf);
  constexpr double num = 123.456;
#if RTC_DCHECK_IS_ON
  EXPECT_DEATH(sb << num, "");
#else
  sb << num;
  EXPECT_THAT(sb.str(),
              testing::AnyOf(testing::StrEq(""), testing::StrEq("123.")));
#endif
}

TEST(SimpleStringBuilder, BufferOverrunConstCharPAlreadyFull) {
  char sb_buf[4];
  SimpleStringBuilder sb(sb_buf);
  sb << 123;
  const char* const msg = "This is just too much";
#if RTC_DCHECK_IS_ON
  EXPECT_DEATH(sb << msg, "");
#else
  sb << msg;
  EXPECT_THAT(sb.str(), testing::StrEq("123"));
#endif
}

TEST(SimpleStringBuilder, BufferOverrunIntAlreadyFull) {
  char sb_buf[4];
  SimpleStringBuilder sb(sb_buf);
  sb << "xyz";
  constexpr int num = -12345;
#if RTC_DCHECK_IS_ON
  EXPECT_DEATH(sb << num, "");
#else
  sb << num;
  EXPECT_THAT(sb.str(), testing::StrEq("xyz"));
#endif
}

#endif

}  // namespace rtc
