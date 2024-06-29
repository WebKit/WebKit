/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/strings/str_join.h"

#include <string>
#include <utility>
#include <vector>

#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(StrJoinTest, CanJoinStringsFromVector) {
  std::vector<std::string> strings = {"Hello", "World"};
  std::string s = StrJoin(strings, " ");
  EXPECT_EQ(s, "Hello World");
}

TEST(StrJoinTest, CanJoinNumbersFromArray) {
  std::array<int, 3> numbers = {1, 2, 3};
  std::string s = StrJoin(numbers, ",");
  EXPECT_EQ(s, "1,2,3");
}

TEST(StrJoinTest, CanFormatElementsWhileJoining) {
  std::vector<std::pair<std::string, std::string>> pairs = {
      {"hello", "world"}, {"foo", "bar"}, {"fum", "gazonk"}};
  std::string s = StrJoin(pairs, ",",
                          [&](rtc::StringBuilder& sb,
                              const std::pair<std::string, std::string>& p) {
                            sb << p.first << "=" << p.second;
                          });
  EXPECT_EQ(s, "hello=world,foo=bar,fum=gazonk");
}

}  // namespace
}  // namespace webrtc
