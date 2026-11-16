/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/CookieJar.h>
#include <wtf/URL.h>

namespace TestWebKitAPI {

// Expose the protected static method for testing.
struct CookieJarForTesting : public WebCore::CookieJar {
    using WebCore::CookieJar::shouldIncludeSecureCookies;
};

TEST(CookieJar, ShouldIncludeSecureCookiesForHTTPS)
{
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("https://example.com/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("https://8.8.8.8/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("https://example.com:8443/"_s)), WebCore::IncludeSecureCookies::Yes);
}

TEST(CookieJar, ShouldNotIncludeSecureCookiesForPlainHTTP)
{
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://example.com/"_s)), WebCore::IncludeSecureCookies::No);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://8.8.8.8/"_s)), WebCore::IncludeSecureCookies::No);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://192.168.1.1/"_s)), WebCore::IncludeSecureCookies::No);
}

TEST(CookieJar, ShouldNotIncludeSecureCookiesForNonLocalHostnames)
{
    // "localhost" must appear exactly or as the rightmost label.
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://notlocalhost/"_s)), WebCore::IncludeSecureCookies::No);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://localhost.example.com/"_s)), WebCore::IncludeSecureCookies::No);
}

TEST(CookieJar, ShouldNotIncludeSecureCookiesForNonLoopbackIPv4)
{
    // Starts with "127." but has non-digit characters.
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.example.com/"_s)), WebCore::IncludeSecureCookies::No);
}

#if HAVE(LOCALHOST_TIED_TO_LOOPBACK)
TEST(CookieJar, ShouldIncludeSecureCookiesForIPv6Loopback)
{
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://[::1]/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://[::1]:8080/"_s)), WebCore::IncludeSecureCookies::Yes);
}

TEST(CookieJar, ShouldIncludeSecureCookiesForIPv4Loopback)
{
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.0.0.1/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.0.0.2/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.1.2.3/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.255.255.255/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.0.0.1:8080/"_s)), WebCore::IncludeSecureCookies::Yes);
}

TEST(CookieJar, ShouldIncludeSecureCookiesForLocalhost)
{
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://localhost/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://localhost:3000/"_s)), WebCore::IncludeSecureCookies::Yes);

    // Case insensitive.
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://LOCALHOST/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://LocalHost/"_s)), WebCore::IncludeSecureCookies::Yes);
}

TEST(CookieJar, ShouldIncludeSecureCookiesForLocalhostSubdomains)
{
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://foo.localhost/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://foo.bar.localhost/"_s)), WebCore::IncludeSecureCookies::Yes);

    // Case insensitive.
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://foo.LOCALHOST/"_s)), WebCore::IncludeSecureCookies::Yes);
}

TEST(CookieJar, ShouldIncludeSecureCookiesForNormalizedIPv4Loopback)
{
    // The WHATWG URL parser normalizes abbreviated IPv4 addresses, so "127.0.1"
    // expands to "127.0.0.1" before shouldIncludeSecureCookies sees the host.
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.0.1/"_s)), WebCore::IncludeSecureCookies::Yes);
}
#endif

} // namespace TestWebKitAPI
