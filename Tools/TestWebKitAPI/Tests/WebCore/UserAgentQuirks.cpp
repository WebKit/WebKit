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

#include <WebCore/URL.h>
#include <WebCore/UserAgent.h>

using namespace WebCore;

namespace TestWebKitAPI {

static void assertUserAgentForURLHasChromeBrowserQuirk(const char* url)
{
    String uaString = standardUserAgentForURL(URL(ParsedURLString, url));

#if !OS(MAC_OS_X)
    EXPECT_FALSE(uaString.contains("Macintosh"));
    EXPECT_FALSE(uaString.contains("Mac OS X"));
#endif
#if OS(LINUX)
    EXPECT_TRUE(uaString.contains("Linux"));
#endif
#if OS(WINDOWS)
    EXPECT_TRUE(uaString.contains("Windows"));
#endif

    EXPECT_TRUE(uaString.contains("Chrome"));
    EXPECT_TRUE(uaString.contains("Safari"));
    EXPECT_FALSE(uaString.contains("Chromium"));
}

static void assertUserAgentForURLHasMacPlatformQuirk(const char* url)
{
    String uaString = standardUserAgentForURL(URL(ParsedURLString, url));

    EXPECT_TRUE(uaString.contains("Macintosh"));
    EXPECT_TRUE(uaString.contains("Mac OS X"));
    EXPECT_FALSE(uaString.contains("Linux"));
    EXPECT_FALSE(uaString.contains("Windows"));
    EXPECT_FALSE(uaString.contains("Chrome"));
}

TEST(UserAgentTest, Quirks)
{
    // A site with not quirks should return a null String.
    String uaString = standardUserAgentForURL(URL(ParsedURLString, "http://www.webkit.org/"));
    EXPECT_TRUE(uaString.isNull());

#if !OS(MAC_OS_X)
    // Google quirk should not affect sites with similar domains.
    uaString = standardUserAgentForURL(URL(ParsedURLString, "http://www.googleblog.com/"));
    EXPECT_FALSE(uaString.contains("Macintosh"));
    EXPECT_FALSE(uaString.contains("Mac OS X"));
#endif

    assertUserAgentForURLHasChromeBrowserQuirk("http://typekit.com/");
    assertUserAgentForURLHasChromeBrowserQuirk("http://typekit.net/");
    assertUserAgentForURLHasChromeBrowserQuirk("http://www.youtube.com/");
    assertUserAgentForURLHasChromeBrowserQuirk("http://www.slack.com/");

    assertUserAgentForURLHasMacPlatformQuirk("http://www.yahoo.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://finance.yahoo.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://intl.taobao.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://www.whatsapp.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://web.whatsapp.com/");

    assertUserAgentForURLHasMacPlatformQuirk("http://www.google.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://www.google.es/");
    assertUserAgentForURLHasMacPlatformQuirk("http://calendar.google.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://maps.google.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://plus.google.com/");
}

} // namespace TestWebKitAPI
