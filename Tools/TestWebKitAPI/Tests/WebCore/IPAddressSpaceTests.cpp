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

#include <WebCore/DNS.h>
#include <WebCore/IPAddressSpace.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/Site.h>
#include <WebCore/WebCorePersistentCoders.h>
#include <wtf/URL.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>

namespace TestWebKitAPI {

// Test IPv4 loopback addresses (127.0.0.0/8)
TEST(IPAddressSpace, IPv4Loopback)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://127.0.0.1/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://127.0.0.2/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://127.255.255.255/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://127.1.2.3:8080/"_s)), WebCore::IPAddressSpace::Loopback);
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

TEST(IPAddressSpace, IPv4Benchmarking)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://198.18.0.1/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://198.19.255.255/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://198.18.100.50:443/"_s)), WebCore::IPAddressSpace::Loopback);

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
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[::1]/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://[::1]:8080/"_s)), WebCore::IPAddressSpace::Loopback);
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
}

TEST(IPAddressSpace, IPv6SiteLocal)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[fec0::1]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[feff::1]/"_s)), WebCore::IPAddressSpace::Local);
}

TEST(IPAddressSpace, UnspecifiedAddresses)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://0.0.0.0/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[::]/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://0/"_s)), WebCore::IPAddressSpace::Loopback);

    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://0.1.2.3/"_s)), WebCore::IPAddressSpace::Local);
}

TEST(IPAddressSpace, DocumentationRanges)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[2001:db8::1]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[3fff::1]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[3fff:0fff::1]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://192.0.2.1/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://198.51.100.1/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://203.0.113.1/"_s)), WebCore::IPAddressSpace::Local);

    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[3fff:1000::1]/"_s)), WebCore::IPAddressSpace::Public);
}

TEST(IPAddressSpace, NonGloballyRoutableRanges)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://224.0.0.1/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://239.255.255.250/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[ff02::1]/"_s)), WebCore::IPAddressSpace::Local);

    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://240.0.0.1/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://255.255.255.255/"_s)), WebCore::IPAddressSpace::Local);

    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://192.0.0.1/"_s)), WebCore::IPAddressSpace::Local);

    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://192.88.99.1/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://192.88.98.255/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://192.88.100.0/"_s)), WebCore::IPAddressSpace::Public);

    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[2001:0:1::1]/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[2002::1]/"_s)), WebCore::IPAddressSpace::Public);

    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[100::1]/"_s)), WebCore::IPAddressSpace::Local);
}

// The URL parser canonicalises non-dotted IPv4 hosts before classification sees them, so these
// spellings all reach determineIPAddressSpace() as 127.0.0.1. Asserted because classifyHost() only
// parses dotted quads, and would return Public for any form the parser stopped normalising.
TEST(IPAddressSpace, IPv4AlternateHostFormats)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://2130706433/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://0x7f000001/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://017700000001/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://127.1/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://127.0.1/"_s)), WebCore::IPAddressSpace::Loopback);

    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://3232235777/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://0xc0a80101/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://192.168.257/"_s)), WebCore::IPAddressSpace::Local);

    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://134744072/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://0x08080808/"_s)), WebCore::IPAddressSpace::Public);
}

// NAT64 (RFC 6052) embeds an IPv4 address in an IPv6 one, so a translated address is only as public as
// the IPv4 address it carries. Without this a public page could reach a private host through a NAT64
// prefix with no permission at all.
TEST(IPAddressSpace, NAT64WellKnownPrefix)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[64:ff9b::192.168.1.1]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[64:ff9b::c0a8:101]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[64:ff9b::10.0.0.1]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[64:ff9b::127.0.0.1]/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[64:ff9b::169.254.169.254]/"_s)), WebCore::IPAddressSpace::Local);

    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[64:ff9b::8.8.8.8]/"_s)), WebCore::IPAddressSpace::Public);

    // 64:ff9b:1::/48 is a different prefix and must not be read with the /96 layout.
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[64:ff9c::192.168.1.1]/"_s)), WebCore::IPAddressSpace::Public);
}

// At /48 the embedded address straddles the octet at bits 64-71 that the addressing architecture
// reserves: two bytes before it and two after. 64:ff9b:1:c0a8:1:100:: therefore carries 192.168.1.1,
// and reading it with the /96 layout would see 0.0.0.0 instead.
TEST(IPAddressSpace, NAT64LocalUsePrefix)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[64:ff9b:1:c0a8:1:100::]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[64:ff9b:1:a00:0:100::]/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[64:ff9b:1:7f00:0:100::]/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[64:ff9b:1:a9fe:a9:fe00::]/"_s)), WebCore::IPAddressSpace::Local);

    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[64:ff9b:1:808:8:800::]/"_s)), WebCore::IPAddressSpace::Public);

    // Trailing bytes after the embedded address are suffix and must not affect the result.
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[64:ff9b:1:c0a8:1:100:dead:beef]/"_s)), WebCore::IPAddressSpace::Local);

    // 64:ff9b:2::/48 is not the local-use prefix.
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[64:ff9b:2:c0a8:1:100::]/"_s)), WebCore::IPAddressSpace::Public);
}

// Test IPv4-Mapped IPv6 addresses (::ffff:0:0/96) with dotted decimal notation
TEST(IPAddressSpace, IPv6MappedIPv4DottedDecimal)
{
    // Local IPv4 addresses mapped to IPv6
    // ::ffff:127.0.0.1 maps to a loopback address, so it's classified as ::Loopback, not ::Local.
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[::ffff:127.0.0.1]/"_s)), WebCore::IPAddressSpace::Loopback);
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
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[::2]/"_s)), WebCore::IPAddressSpace::Public);
}

// Test non-IP addresses (hostnames)
// morePublicOf() is what a blob: document's address space is chosen with: HTML offers two sources for it
// (the navigation initiator and the blob URL entry's environment) and they can disagree, so the more
// public of the two is taken because that can only add Local Network Access checks, never skip them.
TEST(IPAddressSpace, MorePublicOfPicksTheHigherRank)
{
    EXPECT_EQ(WebCore::IPAddressSpace::Public, WebCore::morePublicOf(WebCore::IPAddressSpace::Public, WebCore::IPAddressSpace::Loopback));
    EXPECT_EQ(WebCore::IPAddressSpace::Public, WebCore::morePublicOf(WebCore::IPAddressSpace::Loopback, WebCore::IPAddressSpace::Public));
    EXPECT_EQ(WebCore::IPAddressSpace::Public, WebCore::morePublicOf(WebCore::IPAddressSpace::Public, WebCore::IPAddressSpace::Local));
    EXPECT_EQ(WebCore::IPAddressSpace::Local, WebCore::morePublicOf(WebCore::IPAddressSpace::Local, WebCore::IPAddressSpace::Loopback));
    EXPECT_EQ(WebCore::IPAddressSpace::Local, WebCore::morePublicOf(WebCore::IPAddressSpace::Loopback, WebCore::IPAddressSpace::Local));

    EXPECT_EQ(WebCore::IPAddressSpace::Public, WebCore::morePublicOf(WebCore::IPAddressSpace::Public, WebCore::IPAddressSpace::Public));
    EXPECT_EQ(WebCore::IPAddressSpace::Local, WebCore::morePublicOf(WebCore::IPAddressSpace::Local, WebCore::IPAddressSpace::Local));
    EXPECT_EQ(WebCore::IPAddressSpace::Loopback, WebCore::morePublicOf(WebCore::IPAddressSpace::Loopback, WebCore::IPAddressSpace::Loopback));
}

// Unknown ranks alongside Loopback, so a plain maximum would let a missing value win and drag the answer
// to the most private space. It has to mean "no opinion" instead, or a blob URL whose creator is in
// another process would silently read as loopback.
TEST(IPAddressSpace, MorePublicOfTreatsUnknownAsNoOpinion)
{
    EXPECT_EQ(WebCore::IPAddressSpace::Public, WebCore::morePublicOf(WebCore::IPAddressSpace::Unknown, WebCore::IPAddressSpace::Public));
    EXPECT_EQ(WebCore::IPAddressSpace::Public, WebCore::morePublicOf(WebCore::IPAddressSpace::Public, WebCore::IPAddressSpace::Unknown));
    EXPECT_EQ(WebCore::IPAddressSpace::Local, WebCore::morePublicOf(WebCore::IPAddressSpace::Unknown, WebCore::IPAddressSpace::Local));
    EXPECT_EQ(WebCore::IPAddressSpace::Local, WebCore::morePublicOf(WebCore::IPAddressSpace::Local, WebCore::IPAddressSpace::Unknown));

    EXPECT_EQ(WebCore::IPAddressSpace::Loopback, WebCore::morePublicOf(WebCore::IPAddressSpace::Unknown, WebCore::IPAddressSpace::Loopback));
    EXPECT_EQ(WebCore::IPAddressSpace::Loopback, WebCore::morePublicOf(WebCore::IPAddressSpace::Loopback, WebCore::IPAddressSpace::Unknown));

    EXPECT_EQ(WebCore::IPAddressSpace::Unknown, WebCore::morePublicOf(WebCore::IPAddressSpace::Unknown, WebCore::IPAddressSpace::Unknown));
}

TEST(IPAddressSpace, HostnameAddresses)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://example.com/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://www.google.com/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("ftp://ftp.example.org/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://localhost/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://internal.company.local:8080/"_s)), WebCore::IPAddressSpace::Local);
}

TEST(IPAddressSpace, FullyQualifiedNames)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://printer.local./"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://localhost./"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://dev.localhost./"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://127.0.0.1./"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://example.com./"_s)), WebCore::IPAddressSpace::Public);
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
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://127.0.0.1/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_TRUE(WebCore::isLessPublicThan(WebCore::IPAddressSpace::Loopback, WebCore::IPAddressSpace::Local));
    EXPECT_TRUE(WebCore::isLessPublicThan(WebCore::IPAddressSpace::Loopback, WebCore::IPAddressSpace::Public));
    EXPECT_TRUE(WebCore::isLessPublicThan(WebCore::IPAddressSpace::Local, WebCore::IPAddressSpace::Public));
    EXPECT_FALSE(WebCore::isLessPublicThan(WebCore::IPAddressSpace::Public, WebCore::IPAddressSpace::Local));
    EXPECT_FALSE(WebCore::isLessPublicThan(WebCore::IPAddressSpace::Local, WebCore::IPAddressSpace::Loopback));
    EXPECT_FALSE(WebCore::isLessPublicThan(WebCore::IPAddressSpace::Public, WebCore::IPAddressSpace::Public));
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
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://127.0.0.1:8080/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://192.168.1.1:443/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://[::1]:3000/"_s)), WebCore::IPAddressSpace::Loopback);
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

    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://198.18.0.0/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://198.19.255.255/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://198.17.255.255/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://198.20.0.0/"_s)), WebCore::IPAddressSpace::Public);
}

// classifyIPAddressSpace() goes through IPAddress::fromString() -> inet_ntop() rather than URL
// parsing, so it's worth confirming it agrees with determineIPAddressSpace() above.
TEST(IPAddressSpace, ClassifyIPAddressSpaceFromResolvedAddress)
{
    EXPECT_EQ(WebCore::classifyIPAddressSpace(*WebCore::IPAddress::fromString("127.0.0.1"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::classifyIPAddressSpace(*WebCore::IPAddress::fromString("::1"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::classifyIPAddressSpace(*WebCore::IPAddress::fromString("192.168.1.1"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::classifyIPAddressSpace(*WebCore::IPAddress::fromString("fc00::1"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::classifyIPAddressSpace(*WebCore::IPAddress::fromString("8.8.8.8"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::classifyIPAddressSpace(*WebCore::IPAddress::fromString("2001:4860:4860::8888"_s)), WebCore::IPAddressSpace::Public);

    // IPv4-mapped IPv6 addresses must classify by their embedded IPv4 address, not as opaque IPv6.
    EXPECT_EQ(WebCore::classifyIPAddressSpace(*WebCore::IPAddress::fromString("::ffff:192.168.1.1"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::classifyIPAddressSpace(*WebCore::IPAddress::fromString("::ffff:127.0.0.1"_s)), WebCore::IPAddressSpace::Loopback);
}

TEST(IPAddressSpace, ClassifyUnclassifiableAddressIsUnknown)
{
    WebCore::IPAddress unclassifiable { WTF::HashTableEmptyValue };
    EXPECT_EQ(WebCore::classifyIPAddressSpace(unclassifiable), WebCore::IPAddressSpace::Unknown);
}

TEST(IPAddressSpace, ResourceResponseAddressSpaceDefaultsToUnknown)
{
    WebCore::ResourceResponse response;
    EXPECT_EQ(response.ipAddressSpace(), WebCore::IPAddressSpace::Unknown);

    response.setIPAddressSpace(WebCore::IPAddressSpace::Local);
    EXPECT_EQ(response.ipAddressSpace(), WebCore::IPAddressSpace::Local);
}

TEST(IPAddressSpace, SiteMatchesURLForIPLiterals)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(WebCore::Site(URL("http://127.0.0.1/"_s))), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(WebCore::Site(URL("http://[::1]/"_s))), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(WebCore::Site(URL("http://192.168.1.1/"_s))), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(WebCore::Site(URL("http://[fc00::1]/"_s))), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(WebCore::Site(URL("http://8.8.8.8/"_s))), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(WebCore::Site(URL("https://example.com/"_s))), WebCore::IPAddressSpace::Public);
}

TEST(IPAddressSpace, ClassifiesLocalhostNameAsLoopback)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://localhost/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://localhost:8000/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://LOCALHOST/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://foo.localhost/"_s)), WebCore::IPAddressSpace::Loopback);
    EXPECT_EQ(WebCore::determineIPAddressSpace(WebCore::Site(URL("http://localhost/"_s))), WebCore::IPAddressSpace::Loopback);
}

TEST(IPAddressSpace, ClassifiesMDNSNameAsLocal)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://printer.local/"_s)), WebCore::IPAddressSpace::Local);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("http://PRINTER.LOCAL/"_s)), WebCore::IPAddressSpace::Local);
}

TEST(IPAddressSpace, DoesNotOvermatchReservedNames)
{
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://notlocalhost/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://localhost.example.com/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://mylocal/"_s)), WebCore::IPAddressSpace::Public);
    EXPECT_EQ(WebCore::determineIPAddressSpace(URL("https://local.example.com/"_s)), WebCore::IPAddressSpace::Public);
}

// Both persistence coders are covered: ResourceResponseData is what the network cache stores, and
// ResourceResponse is what other persistent callers encode.
TEST(IPAddressSpace, SurvivesResponseDataPersistenceRoundTrip)
{
    for (auto space : { WebCore::IPAddressSpace::Public, WebCore::IPAddressSpace::Local, WebCore::IPAddressSpace::Loopback, WebCore::IPAddressSpace::Unknown }) {
        WebCore::ResourceResponse response { URL { "http://192.168.1.1/"_s }, "text/plain"_s, 5, "UTF-8"_s };
        response.setIPAddressSpace(space);

        auto data = response.getResponseData();
        ASSERT_TRUE(data.has_value());

        WTF::Persistence::Encoder encoder;
        WTF::Persistence::Coder<WebCore::ResourceResponseData>::encodeForPersistence(encoder, *data);

        WTF::Persistence::Decoder decoder(encoder.span());
        auto decoded = WTF::Persistence::Coder<WebCore::ResourceResponseData>::decodeForPersistence(decoder);
        ASSERT_TRUE(decoded.has_value());

        EXPECT_EQ(decoded->ipAddressSpace, space);
    }
}

TEST(IPAddressSpace, SurvivesResourceResponsePersistenceRoundTrip)
{
    for (auto space : { WebCore::IPAddressSpace::Public, WebCore::IPAddressSpace::Local, WebCore::IPAddressSpace::Loopback, WebCore::IPAddressSpace::Unknown }) {
        WebCore::ResourceResponse response { URL { "http://192.168.1.1/"_s }, "text/plain"_s, 5, "UTF-8"_s };
        response.setIPAddressSpace(space);

        WTF::Persistence::Encoder encoder;
        WTF::Persistence::Coder<WebCore::ResourceResponse>::encodeForPersistence(encoder, response);

        WTF::Persistence::Decoder decoder(encoder.span());
        auto decoded = WTF::Persistence::Coder<WebCore::ResourceResponse>::decodeForPersistence(decoder);
        ASSERT_TRUE(decoded.has_value());

        EXPECT_EQ(decoded->ipAddressSpace(), space);
    }
}

}
