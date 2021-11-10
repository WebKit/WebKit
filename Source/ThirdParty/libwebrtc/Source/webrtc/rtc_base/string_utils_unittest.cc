/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/string_utils.h"

#include "test/gtest.h"

namespace rtc {

TEST(string_trim_Test, Trimming) {
  EXPECT_EQ("temp", string_trim("\n\r\t temp \n\r\t"));
  EXPECT_EQ("temp\n\r\t temp", string_trim(" temp\n\r\t temp "));
  EXPECT_EQ("temp temp", string_trim("temp temp"));
  EXPECT_EQ("", string_trim(" \r\n\t"));
  EXPECT_EQ("", string_trim(""));
}

TEST(string_toHexTest, ToHex) {
  EXPECT_EQ(ToHex(0), "0");
  EXPECT_EQ(ToHex(0X1243E), "1243e");
  EXPECT_EQ(ToHex(-20), "ffffffec");
}

#if defined(WEBRTC_WIN)

TEST(string_toutf, Empty) {
  char empty_string[] = "";
  EXPECT_TRUE(ToUtf16(empty_string, 0u).empty());
  wchar_t empty_wchar[] = L"";
  EXPECT_TRUE(ToUtf8(empty_wchar, 0u).empty());
}

#endif  // WEBRTC_WIN

TEST(CompileTimeString, MakeActsLikeAString) {
  EXPECT_STREQ(MakeCompileTimeString("abc123"), "abc123");
}

TEST(CompileTimeString, ConvertibleToStdString) {
  EXPECT_EQ(std::string(MakeCompileTimeString("abab")), "abab");
}

namespace detail {
constexpr bool StringEquals(const char* a, const char* b) {
  while (*a && *a == *b)
    a++, b++;
  return *a == *b;
}
}  // namespace detail

static_assert(detail::StringEquals(MakeCompileTimeString("handellm"),
                                   "handellm"),
              "String should initialize.");

static_assert(detail::StringEquals(MakeCompileTimeString("abc123").Concat(
                                       MakeCompileTimeString("def456ghi")),
                                   "abc123def456ghi"),
              "Strings should concatenate.");

}  // namespace rtc
