/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/p2p_constants.h"

#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(P2PConstantsTest, IsMdnsHostnameMatchesLocalDomain) {
  EXPECT_TRUE(IsMdnsHostname("example.local"));
  EXPECT_TRUE(IsMdnsHostname("a.b.local"));
  EXPECT_TRUE(IsMdnsHostname("7dfcb6d2-d7af-4ddc-8b88-65d065f608e9.local"));
}

TEST(P2PConstantsTest, IsMdnsHostnameMatchesFullyQualifiedName) {
  EXPECT_TRUE(IsMdnsHostname("example.local."));
}

TEST(P2PConstantsTest, IsMdnsHostnameIsCaseInsensitive) {
  EXPECT_TRUE(IsMdnsHostname("EXAMPLE.LOCAL"));
  EXPECT_TRUE(IsMdnsHostname("example.Local"));
  EXPECT_TRUE(IsMdnsHostname("Example.LoCaL."));
}

TEST(P2PConstantsTest, IsMdnsHostnameDoesNotMatchOtherNames) {
  EXPECT_FALSE(IsMdnsHostname(""));
  EXPECT_FALSE(IsMdnsHostname("."));
  EXPECT_FALSE(IsMdnsHostname("local"));
  EXPECT_FALSE(IsMdnsHostname("local."));
  EXPECT_FALSE(IsMdnsHostname("examplelocal"));
  EXPECT_FALSE(IsMdnsHostname("example.local.."));
  EXPECT_FALSE(IsMdnsHostname("example.localhost"));
  EXPECT_FALSE(IsMdnsHostname("example.local.org"));
  EXPECT_FALSE(IsMdnsHostname("local.example.org"));
  EXPECT_FALSE(IsMdnsHostname("1.2.3.4"));
}

}  // namespace
}  // namespace webrtc
