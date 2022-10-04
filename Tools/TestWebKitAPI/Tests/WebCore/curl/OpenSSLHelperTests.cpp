/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(CURL)

#include <WebCore/OpenSSLHelper.h>

using namespace WebCore;

namespace TestWebKitAPI {

namespace Curl {

class OpenSSLHelperTests : public testing::Test {
public:
    OpenSSLHelperTests() { }
};

TEST(OpenSSLHelper, IPv6AddressCompression)
{
    String ipv6;
    unsigned char ipAddress[] = { 0x2a, 0x00, 0x8a, 0x00, 0xa0, 0x00, 0x11, 0x90, 0x00, 0x00, 0x00, 0x01, 0x00, 0x0, 0x02, 0x52 };
    ipv6 = OpenSSL::canonicalizeIPv6Address(ipAddress);
    EXPECT_EQ("2a00:8a00:a000:1190:0:1:0:252"_s, ipv6);

    // Ensure the zero compression could handle multiple octects.
    unsigned char ipAddress2[] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x01 };
    ipv6 = OpenSSL::canonicalizeIPv6Address(ipAddress2);
    EXPECT_EQ("::1"_s, ipv6);

    // Make sure multiple 0 octects compressed.
    unsigned char ipAddress3[] = { 0xfe, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x02, 0xaa, 0x0, 0xff, 0xfe, 0x9a, 0x4c, 0xa2 };
    ipv6 = OpenSSL::canonicalizeIPv6Address(ipAddress3);
    EXPECT_EQ("fe80::2aa:ff:fe9a:4ca2"_s, ipv6);

    // Test zero compression at the end of string.
    unsigned char ipAddress4[] = { 0x2a, 0x0, 0x0, 0x0, 0x0, 0x0, 0x11, 0x90, 0x0, 0x01, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };
    ipv6 = OpenSSL::canonicalizeIPv6Address(ipAddress4);
    EXPECT_EQ("2a00:0:0:1190:1::"_s, ipv6);

    // Test zero compression at the beginning of string.
    unsigned char ipAddress5[] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x11, 0x90, 0x0, 0x0, 0x0, 0x01, 0x0, 0x0, 0x0, 0x0 };
    ipv6 = OpenSSL::canonicalizeIPv6Address(ipAddress5);
    EXPECT_EQ("::1190:0:1:0:0"_s, ipv6);

    // Test zero compression only done once.
    unsigned char ipAddress6[] = { 0x0, 0x01, 0x0, 0x01, 0x0, 0x0, 0x11, 0x90, 0x0, 0x0, 0x0, 0x0, 0x0, 0x01, 0x0, 0x0 };
    ipv6 = OpenSSL::canonicalizeIPv6Address(ipAddress6);
    EXPECT_EQ("1:1:0:1190::1:0"_s, ipv6);

    // Make sure noncompressable IPv6 is the same.
    unsigned char ipAddress7[] = { 0x12, 0x34, 0x56, 0x78, 0xab, 0xcd, 0x12, 0x34, 0x56, 0x78, 0xab, 0xcd, 0x12, 0x34, 0x56, 0x78 };
    ipv6 = OpenSSL::canonicalizeIPv6Address(ipAddress7);
    EXPECT_EQ("1234:5678:abcd:1234:5678:abcd:1234:5678"_s, ipv6);

    // When there are multiple consecutive octet sections that can be compressed, make sure the first occurring one is compressed.
    unsigned char ipAddress8[] = { 0x0, 0x01, 0x0, 0x0, 0x0, 0x0, 0x0, 0x01, 0x0, 0x0, 0x0, 0x0, 0x0, 0x01, 0x0, 0x0 };
    ipv6 = OpenSSL::canonicalizeIPv6Address(ipAddress8);
    EXPECT_EQ("1::1:0:0:1:0"_s, ipv6);
}

}

}

#endif
