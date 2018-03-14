/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "rtc_base/gunit.h"
#include "rtc_base/nethelpers.h"
#include "rtc_base/win32.h"

#if !defined(WEBRTC_WIN)
#error Only for Windows
#endif

namespace rtc {

class Win32Test : public testing::Test {
 public:
  Win32Test() {
  }
};

TEST_F(Win32Test, IPv6AddressCompression) {
  IPAddress ipv6;

  // Zero compression should be done on the leftmost 0s when there are
  // multiple longest series.
  ASSERT_TRUE(IPFromString("2a00:8a00:a000:1190:0000:0001:000:252", &ipv6));
  EXPECT_EQ("2a00:8a00:a000:1190::1:0:252", ipv6.ToString());

  // Ensure the zero compression could handle multiple octects.
  ASSERT_TRUE(IPFromString("0:0:0:0:0:0:0:1", &ipv6));
  EXPECT_EQ("::1", ipv6.ToString());

  // Make sure multiple 0 octects compressed.
  ASSERT_TRUE(IPFromString("fe80:0:0:0:2aa:ff:fe9a:4ca2", &ipv6));
  EXPECT_EQ("fe80::2aa:ff:fe9a:4ca2", ipv6.ToString());

  // Test zero compression at the end of string.
  ASSERT_TRUE(IPFromString("2a00:8a00:a000:1190:0000:0001:000:00", &ipv6));
  EXPECT_EQ("2a00:8a00:a000:1190:0:1::", ipv6.ToString());

  // Test zero compression at the beginning of string.
  ASSERT_TRUE(IPFromString("0:0:000:1190:0000:0001:000:00", &ipv6));
  EXPECT_EQ("::1190:0:1:0:0", ipv6.ToString());

  // Test zero compression only done once.
  ASSERT_TRUE(IPFromString("0:1:000:1190:0000:0001:000:01", &ipv6));
  EXPECT_EQ("::1:0:1190:0:1:0:1", ipv6.ToString());

  // Make sure noncompressable IPv6 is the same.
  ASSERT_TRUE(IPFromString("1234:5678:abcd:1234:5678:abcd:1234:5678", &ipv6));
  EXPECT_EQ("1234:5678:abcd:1234:5678:abcd:1234:5678", ipv6.ToString());
}

// Test that invalid IPv6 addresses are recognized and false is returned.
TEST_F(Win32Test, InvalidIPv6AddressParsing) {
  IPAddress ipv6;

  // More than 1 run of "::"s.
  EXPECT_FALSE(IPFromString("1::2::3", &ipv6));

  // More than 1 run of "::"s in a longer address.
  // See: https://bugs.chromium.org/p/webrtc/issues/detail?id=7592
  EXPECT_FALSE(IPFromString("1::2::3::4::5::6::7::8", &ipv6));

  // Three ':'s in a row.
  EXPECT_FALSE(IPFromString("1:::2", &ipv6));

  // Non-hex character.
  EXPECT_FALSE(IPFromString("test::1", &ipv6));

  // More than 4 hex digits per group.
  EXPECT_FALSE(IPFromString("abcde::1", &ipv6));

  // More than 8 groups.
  EXPECT_FALSE(IPFromString("1:2:3:4:5:6:7:8:9", &ipv6));

  // Less than 8 groups.
  EXPECT_FALSE(IPFromString("1:2:3:4:5:6:7", &ipv6));
}

}  // namespace rtc
