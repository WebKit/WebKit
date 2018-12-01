/*
 * Copyright (C) 2014 Igalia S.L.
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

#include <WebCore/UserAgent.h>
#include <wtf/URL.h>

using namespace WebCore;

namespace TestWebKitAPI {

static void assertUserAgentForURLHasChromeBrowserQuirk(const char* url)
{
    String uaString = standardUserAgentForURL(URL({ }, url));

    EXPECT_TRUE(uaString.contains("Chrome"));
    EXPECT_TRUE(uaString.contains("Safari"));
    EXPECT_FALSE(uaString.contains("Chromium"));
    EXPECT_FALSE(uaString.contains("Firefox"));
}

static void assertUserAgentForURLHasLinuxPlatformQuirk(const char* url)
{
    String uaString = standardUserAgentForURL(URL({ }, url));

    EXPECT_TRUE(uaString.contains("Linux"));
    EXPECT_FALSE(uaString.contains("Macintosh"));
    EXPECT_FALSE(uaString.contains("Mac OS X"));
    EXPECT_FALSE(uaString.contains("Windows"));
    EXPECT_FALSE(uaString.contains("Chrome"));
    EXPECT_FALSE(uaString.contains("FreeBSD"));
}

static void assertUserAgentForURLHasMacPlatformQuirk(const char* url)
{
    String uaString = standardUserAgentForURL(URL({ }, url));

    EXPECT_TRUE(uaString.contains("Macintosh"));
    EXPECT_TRUE(uaString.contains("Mac OS X"));
    EXPECT_FALSE(uaString.contains("Linux"));
    EXPECT_FALSE(uaString.contains("Windows"));
    EXPECT_FALSE(uaString.contains("Chrome"));
    EXPECT_FALSE(uaString.contains("FreeBSD"));
}

TEST(UserAgentTest, Quirks)
{
    // A site with not quirks should return a null String.
    String uaString = standardUserAgentForURL(URL({ }, "http://www.webkit.org/"));
    EXPECT_TRUE(uaString.isNull());

#if !OS(LINUX) || !CPU(X86_64)
    // Google quirk should not affect sites with similar domains.
    uaString = standardUserAgentForURL(URL({ }, "http://www.googleblog.com/"));
    EXPECT_FALSE(uaString.contains("Linux x86_64"));
#endif

    assertUserAgentForURLHasChromeBrowserQuirk("http://typekit.com/");
    assertUserAgentForURLHasChromeBrowserQuirk("http://typekit.net/");
    assertUserAgentForURLHasChromeBrowserQuirk("http://www.washingtonpost.com/");

    assertUserAgentForURLHasLinuxPlatformQuirk("http://www.google.com/");
    assertUserAgentForURLHasLinuxPlatformQuirk("http://www.google.es/");
    assertUserAgentForURLHasLinuxPlatformQuirk("http://calendar.google.com/");
    assertUserAgentForURLHasLinuxPlatformQuirk("http://plus.google.com/");

    assertUserAgentForURLHasMacPlatformQuirk("http://www.yahoo.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://finance.yahoo.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://intl.taobao.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://www.whatsapp.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://web.whatsapp.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://www.chase.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://docs.google.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://paypal.com/");
}

} // namespace TestWebKitAPI
