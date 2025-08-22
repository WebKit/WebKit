/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/IPAddressSpace.h>
#include <wtf/URL.h>

namespace TestWebKitAPI {

// Test IPv4 loopback addresses (127.0.0.0/8)
TEST(IPAddressSpace, IPv4Loopback)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://127.0.0.1/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://127.0.0.2/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://127.255.255.255/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://127.1.2.3:8080/"_s)), WebCore::IPAddressSpace::Local);
}

// Test IPv4 private address ranges
TEST(IPAddressSpace, IPv4PrivateAddresses)
{
    // 10.0.0.0/8 - Local Use
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://10.0.0.1/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://10.255.255.255/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://10.192.168.1:443/"_s)), WebCore::IPAddressSpace::Local);

    // 172.16.0.0/12 - Local Use
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://172.16.0.1/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://172.31.255.255/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://172.20.1.2:8443/"_s)), WebCore::IPAddressSpace::Local);

    // Edge cases - should NOT be local
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://172.15.255.255/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://172.32.0.1/"_s)), WebCore::IPAddressSpace::Public);

    // 192.168.0.0/16 - Local Use
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://192.168.0.1/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://192.168.255.255/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://192.168.1.100:8080/"_s)), WebCore::IPAddressSpace::Local);

    // Edge cases - should NOT be local
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://192.167.255.255/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://192.169.0.1/"_s)), WebCore::IPAddressSpace::Public);
}

// Test Carrier-Grade NAT addresses (100.64.0.0/10)
TEST(IPAddressSpace, IPv4CarrierGradeNAT)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://100.64.0.1/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://100.127.255.255/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://100.100.100.100:443/"_s)), WebCore::IPAddressSpace::Local);

    // Edge cases - should NOT be local
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://100.63.255.255/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://100.128.0.1/"_s)), WebCore::IPAddressSpace::Public);
}

// Test Link Local addresses (169.254.0.0/16)
TEST(IPAddressSpace, IPv4LinkLocal)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://169.254.0.1/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://169.254.255.255/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://169.254.1.1:8080/"_s)), WebCore::IPAddressSpace::Local);

    // Edge cases - should NOT be local
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://169.253.255.255/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://169.255.0.1/"_s)), WebCore::IPAddressSpace::Public);
}

// Test Benchmarking addresses (198.18.0.0/15)
TEST(IPAddressSpace, IPv4Benchmarking)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://198.18.0.1/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://198.19.255.255/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://198.18.100.50:443/"_s)), WebCore::IPAddressSpace::Local);

    // Edge cases - should NOT be local
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://198.17.255.255/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://198.20.0.1/"_s)), WebCore::IPAddressSpace::Public);
}

// Test IPv4 public addresses
TEST(IPAddressSpace, IPv4PublicAddresses)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://8.8.8.8/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://1.1.1.1/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://172.64.0.1:443/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://208.67.222.222/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://64.233.160.0:443/"_s)), WebCore::IPAddressSpace::Public);
}

// Test IPv6 loopback (::1/128)
TEST(IPAddressSpace, IPv6Loopback)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[::1]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://[::1]:8080/"_s)), WebCore::IPAddressSpace::Local);
}

// Test IPv6 Unique Local addresses (fc00::/7)
TEST(IPAddressSpace, IPv6UniqueLocal)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[fc00::1]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[fd00::1]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://[fcff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]:443/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://[fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]:8080/"_s)), WebCore::IPAddressSpace::Local);

    // Edge cases - should NOT be local
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[fbff::1]/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[fe00::1]/"_s)), WebCore::IPAddressSpace::Public);
}

// Test IPv6 Link-Local addresses (fe80::/10)
TEST(IPAddressSpace, IPv6LinkLocal)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[fe80::1]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[fe90::1]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[fea0::1]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[feb0::1]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://[febf:ffff:ffff:ffff:ffff:ffff:ffff:ffff]:8080/"_s)), WebCore::IPAddressSpace::Local);

    // Edge cases - should NOT be local
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[fe7f::1]/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[fec0::1]/"_s)), WebCore::IPAddressSpace::Public);
}

// Test IPv4-Mapped IPv6 addresses (::ffff:0:0/96) with dotted decimal notation
TEST(IPAddressSpace, IPv6MappedIPv4DottedDecimal)
{
    // Local IPv4 addresses mapped to IPv6
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[::ffff:127.0.0.1]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[::ffff:10.0.0.1]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[::ffff:192.168.1.1]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://[::ffff:172.16.0.1]:443/"_s)), WebCore::IPAddressSpace::Local);

    // Public IPv4 addresses mapped to IPv6
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[::ffff:8.8.8.8]/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://[::ffff:1.1.1.1]:8080/"_s)), WebCore::IPAddressSpace::Public);
}

// Test IPv4-Mapped IPv6 addresses with hex notation
TEST(IPAddressSpace, IPv6MappedIPv4HexNotation)
{
    // 127.0.0.1 = 0x7f000001 -> c0a8:101 represents 192.168.1.1
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[::ffff:c0a8:101]/"_s)), WebCore::IPAddressSpace::Local);

    // 10.0.0.1 = 0x0a000001
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[::ffff:a00:1]/"_s)), WebCore::IPAddressSpace::Local);

    // 8.8.8.8 = 0x08080808
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[::ffff:808:808]/"_s)), WebCore::IPAddressSpace::Public);

    // 172.16.0.1 = 0xac100001
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://[::ffff:ac10:1]:443/"_s)), WebCore::IPAddressSpace::Local);
}

// Test IPv6 public addresses
TEST(IPAddressSpace, IPv6PublicAddresses)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[2001:4860:4860::8888]/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[2606:4700:4700::1111]/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://[2001:db8::1]:443/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[::2]/"_s)), WebCore::IPAddressSpace::Public);
}

// Test non-IP addresses (hostnames)
TEST(IPAddressSpace, HostnameAddresses)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://example.com/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://www.google.com/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://localhost/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://internal.company.local:8080/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("ftp://ftp.example.org/"_s)), WebCore::IPAddressSpace::Public);
}

// Test edge cases and malformed addresses
TEST(IPAddressSpace, EdgeCasesAndMalformed)
{
    // Empty or invalid URLs
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL(""_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://"_s)), WebCore::IPAddressSpace::Public);

    // URLs without hosts
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("file:///path/to/file"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("data:text/plain,hello"_s)), WebCore::IPAddressSpace::Public);

    // Malformed IP addresses
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://256.256.256.256/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://192.168.1.1.1/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[invalid::ipv6::address]/"_s)), WebCore::IPAddressSpace::Public);

    // IPv6 addresses without brackets (should be treated as hostnames)
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://::1/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://2001:db8::1/"_s)), WebCore::IPAddressSpace::Public);
}

// Test the utility functions
TEST(IPAddressSpace, UtilityFunctions)
{
    // Test isLocalIPAddressSpace(const URL&)
    EXPECT_TRUE(WebCore::isLocalIPAddressSpace(URL("http://127.0.0.1/"_s)));
    EXPECT_TRUE(WebCore::isLocalIPAddressSpace(URL("http://192.168.1.1/"_s)));
    EXPECT_TRUE(WebCore::isLocalIPAddressSpace(URL("http://[::1]/"_s)));
    EXPECT_TRUE(WebCore::isLocalIPAddressSpace(URL("http://[fc00::1]/"_s)));

    EXPECT_FALSE(WebCore::isLocalIPAddressSpace(URL("http://8.8.8.8/"_s)));
    EXPECT_FALSE(WebCore::isLocalIPAddressSpace(URL("https://www.example.com/"_s)));
    EXPECT_FALSE(WebCore::isLocalIPAddressSpace(URL("http://[2001:db8::1]/"_s)));
}

// Test different URL schemes
TEST(IPAddressSpace, DifferentURLSchemes)
{
    // HTTP and HTTPS
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://192.168.1.1/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://192.168.1.1/"_s)), WebCore::IPAddressSpace::Local);

    // Other schemes
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("ftp://192.168.1.1/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("ws://192.168.1.1/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("wss://192.168.1.1/"_s)), WebCore::IPAddressSpace::Local);

    // Custom schemes
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("custom://192.168.1.1/"_s)), WebCore::IPAddressSpace::Local);

    // Public addresses with different schemes
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://8.8.8.8/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("ftp://8.8.8.8/"_s)), WebCore::IPAddressSpace::Public);
}

// Test URLs with ports
TEST(IPAddressSpace, URLsWithPorts)
{
    // Local addresses with various ports
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://127.0.0.1:8080/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://192.168.1.1:443/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[::1]:3000/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://[fc00::1]:8443/"_s)), WebCore::IPAddressSpace::Local);

    // Public addresses with ports
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://8.8.8.8:53/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://[2001:4860:4860::8888]:443/"_s)), WebCore::IPAddressSpace::Public);
}

// Test comprehensive IPv4 boundary conditions
TEST(IPAddressSpace, IPv4BoundaryConditions)
{
    // Test exact boundaries for 172.16.0.0/12
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://172.16.0.0/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://172.31.255.255/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://172.15.255.255/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://172.32.0.0/"_s)), WebCore::IPAddressSpace::Public);

    // Test exact boundaries for 100.64.0.0/10
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://100.64.0.0/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://100.127.255.255/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://100.63.255.255/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://100.128.0.0/"_s)), WebCore::IPAddressSpace::Public);

    // Test exact boundaries for 198.18.0.0/15
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://198.18.0.0/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://198.19.255.255/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://198.17.255.255/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://198.20.0.0/"_s)), WebCore::IPAddressSpace::Public);
}

}

