/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "examples/turnserver/read_auth_file.h"

#include <sstream>

#include "test/gtest.h"

namespace webrtc_examples {

TEST(ReadAuthFile, HandlesEmptyFile) {
  std::istringstream empty;
  auto map = ReadAuthFile(&empty);
  EXPECT_TRUE(map.empty());
}

TEST(ReadAuthFile, RecognizesValidUser) {
  std::istringstream file("foo=deadbeaf\n");
  auto map = ReadAuthFile(&file);
  ASSERT_NE(map.find("foo"), map.end());
  EXPECT_EQ(map["foo"], "\xde\xad\xbe\xaf");
}

TEST(ReadAuthFile, EmptyValueForInvalidHex) {
  std::istringstream file(
      "foo=deadbeaf\n"
      "bar=xxxxinvalidhex\n"
      "baz=cafe\n");
  auto map = ReadAuthFile(&file);
  ASSERT_NE(map.find("foo"), map.end());
  EXPECT_EQ(map["foo"], "\xde\xad\xbe\xaf");
  EXPECT_EQ(map.find("bar"), map.end());
  ASSERT_NE(map.find("baz"), map.end());
  EXPECT_EQ(map["baz"], "\xca\xfe");
}

}  // namespace webrtc_examples
