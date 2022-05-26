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
    String uaString = standardUserAgentForURL(URL(String::fromLatin1(url)));

    EXPECT_TRUE(uaString.contains("Chrome"_s));
    EXPECT_TRUE(uaString.contains("Safari"_s));
    EXPECT_FALSE(uaString.contains("Chromium"_s));
    EXPECT_FALSE(uaString.contains("Firefox"_s));
    EXPECT_FALSE(uaString.contains("Version"_s));
}

static void assertUserAgentForURLHasFirefoxBrowserQuirk(const char* url)
{
    String uaString = standardUserAgentForURL(URL(String::fromLatin1(url)));

    EXPECT_FALSE(uaString.contains("Chrome"_s));
    EXPECT_FALSE(uaString.contains("Safari"_s));
    EXPECT_FALSE(uaString.contains("Chromium"_s));
    EXPECT_TRUE(uaString.contains("Firefox"_s));
    EXPECT_FALSE(uaString.contains("Version"_s));
}

static void assertUserAgentForURLHasMacPlatformQuirk(const char* url)
{
    String uaString = standardUserAgentForURL(URL(String::fromLatin1(url)));

    EXPECT_TRUE(uaString.contains("Macintosh"_s));
    EXPECT_TRUE(uaString.contains("Mac OS X"_s));
    EXPECT_FALSE(uaString.contains("Linux"_s));
    EXPECT_FALSE(uaString.contains("Chrome"_s));
    EXPECT_FALSE(uaString.contains("FreeBSD"_s));
}

// Some Google domains require an unbranded user agent, which is a little
// awkward to test for. We want to check that standardUserAgentForURL is
// non-null to ensure *any* quirk URL is returned, so that application branding
// does not get used. (standardUserAgentForURL returns a null string to indicate
// that the standard user agent should be used.)
static void assertUserAgentForURLHasEmptyQuirk(const char* url)
{
    String uaString = standardUserAgentForURL(URL(String::fromLatin1(url)));
    EXPECT_FALSE(uaString.isNull());
}

TEST(UserAgentTest, Quirks)
{
    // A site with no quirks should return a null String.
    String uaString = standardUserAgentForURL(URL("http://www.webkit.org/"_s));
    EXPECT_TRUE(uaString.isNull());

    assertUserAgentForURLHasChromeBrowserQuirk("http://typekit.com/");
    assertUserAgentForURLHasChromeBrowserQuirk("http://typekit.net/");
    assertUserAgentForURLHasChromeBrowserQuirk("http://auth.mayohr.com/");
    assertUserAgentForURLHasChromeBrowserQuirk("http://bankofamerica.com/");
    assertUserAgentForURLHasChromeBrowserQuirk("http://soundcloud.com/");

    assertUserAgentForURLHasFirefoxBrowserQuirk("http://bugzilla.redhat.com/");

#if ENABLE(THUNDER)
    assertUserAgentForURLHasFirefoxBrowserQuirk("http://www.netflix.com/");
#endif

    assertUserAgentForURLHasMacPlatformQuirk("http://www.yahoo.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://finance.yahoo.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://intl.taobao.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://www.whatsapp.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://web.whatsapp.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://www.chase.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://paypal.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://outlook.office.com/");
    assertUserAgentForURLHasMacPlatformQuirk("http://mail.ntu.edu.tw/");
    assertUserAgentForURLHasMacPlatformQuirk("http://exchange.tu-berlin.de/");
    assertUserAgentForURLHasMacPlatformQuirk("http://www.sspa.juntadeandalucia.es/");

    assertUserAgentForURLHasEmptyQuirk("http://accounts.google.com/");
    assertUserAgentForURLHasEmptyQuirk("http://docs.google.com/");
    assertUserAgentForURLHasEmptyQuirk("http://drive.google.com/");
}

} // namespace TestWebKitAPI
