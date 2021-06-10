/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/socket/state_cookie.h"

#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::SizeIs;

TEST(StateCookieTest, SerializeAndDeserialize) {
  Capabilities capabilities = {/*partial_reliability=*/true,
                               /*message_interleaving=*/false,
                               /*reconfig=*/true};
  StateCookie cookie(VerificationTag(123), TSN(456),
                     /*a_rwnd=*/789, TieTag(101112), capabilities);
  std::vector<uint8_t> serialized = cookie.Serialize();
  EXPECT_THAT(serialized, SizeIs(StateCookie::kCookieSize));
  ASSERT_HAS_VALUE_AND_ASSIGN(StateCookie deserialized,
                              StateCookie::Deserialize(serialized));
  EXPECT_EQ(deserialized.initiate_tag(), VerificationTag(123));
  EXPECT_EQ(deserialized.initial_tsn(), TSN(456));
  EXPECT_EQ(deserialized.a_rwnd(), 789u);
  EXPECT_EQ(deserialized.tie_tag(), TieTag(101112));
  EXPECT_TRUE(deserialized.capabilities().partial_reliability);
  EXPECT_FALSE(deserialized.capabilities().message_interleaving);
  EXPECT_TRUE(deserialized.capabilities().reconfig);
}

}  // namespace
}  // namespace dcsctp
