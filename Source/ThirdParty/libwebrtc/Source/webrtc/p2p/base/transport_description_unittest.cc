/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/transport_description.h"
#include "test/gtest.h"

using webrtc::RTCErrorType;

namespace cricket {

TEST(IceParameters, SuccessfulParse) {
  auto result = IceParameters::Parse("ufrag", "22+characters+long+pwd");
  ASSERT_TRUE(result.ok());
  IceParameters parameters = result.MoveValue();
  EXPECT_EQ("ufrag", parameters.ufrag);
  EXPECT_EQ("22+characters+long+pwd", parameters.pwd);
}

TEST(IceParameters, FailedParseShortUfrag) {
  auto result = IceParameters::Parse("3ch", "22+characters+long+pwd");
  EXPECT_EQ(RTCErrorType::SYNTAX_ERROR, result.error().type());
}

TEST(IceParameters, FailedParseLongUfrag) {
  std::string ufrag(257, '+');
  auto result = IceParameters::Parse(ufrag, "22+characters+long+pwd");
  EXPECT_EQ(RTCErrorType::SYNTAX_ERROR, result.error().type());
}

TEST(IceParameters, FailedParseShortPwd) {
  auto result = IceParameters::Parse("ufrag", "21+character+long+pwd");
  EXPECT_EQ(RTCErrorType::SYNTAX_ERROR, result.error().type());
}

TEST(IceParameters, FailedParseLongPwd) {
  std::string pwd(257, '+');
  auto result = IceParameters::Parse("ufrag", pwd);
  EXPECT_EQ(RTCErrorType::SYNTAX_ERROR, result.error().type());
}

TEST(IceParameters, FailedParseBadUfragChar) {
  auto result = IceParameters::Parse("ufrag\r\n", "22+characters+long+pwd");
  EXPECT_EQ(RTCErrorType::SYNTAX_ERROR, result.error().type());
}

TEST(IceParameters, FailedParseBadPwdChar) {
  auto result = IceParameters::Parse("ufrag", "22+characters+long+pwd\r\n");
  EXPECT_EQ(RTCErrorType::SYNTAX_ERROR, result.error().type());
}

}  // namespace cricket
