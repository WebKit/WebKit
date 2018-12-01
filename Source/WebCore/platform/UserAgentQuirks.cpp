/*
 * Copyright (C) 2012, 2014, 2016 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "UserAgentQuirks.h"

#include "PublicSuffix.h"
#include <wtf/URL.h>

namespace WebCore {

// When editing the quirks in this file, be sure to update
// Tools/TestWebKitAPI/Tests/WebCore/UserAgentQuirks.cpp.

static bool isGoogle(const URL& url)
{
    String baseDomain = topPrivatelyControlledDomain(url.host().toString());

    // Our Google UA is *very* complicated to get right. Read
    // https://webkit.org/b/142074 carefully before changing. Test that Earth
    // view is available in Google Maps. Test Google Calendar. Test downloading
    // the Hangouts browser plugin. Test logging out and logging in to a Google
    // account. Change platformVersionForUAString() to return "FreeBSD amd64"
    // and test everything again.
    if (baseDomain.startsWith("google."))
        return true;
    if (baseDomain == "gstatic.com")
        return true;
    if (baseDomain == "googleapis.com")
        return true;
    if (baseDomain == "googleusercontent.com")
        return true;

    return false;
}

// Be careful with this quirk: it's an invitation for sites to use JavaScript
// that works in Chrome that WebKit cannot handle. Prefer other quirks instead.
static bool urlRequiresChromeBrowser(const URL& url)
{
    String baseDomain = topPrivatelyControlledDomain(url.host().toString());

    // Needed for fonts on many sites to work with WebKit.
    // https://bugs.webkit.org/show_bug.cgi?id=147296
    if (baseDomain == "typekit.net" || baseDomain == "typekit.com")
        return true;

    // Washington Post decides the image type based on the user agent,
    // giving image/jp2 with WebKitGTK+'s standard user agent.
    // https://bugs.webkit.org/show_bug.cgi?id=181421
    if (baseDomain == "washingtonpost.com")
        return true;

    return false;
}

static bool urlRequiresMacintoshPlatform(const URL& url)
{
    String domain = url.host().toString();
    String baseDomain = topPrivatelyControlledDomain(domain);

    // At least finance.yahoo.com displays a mobile version with WebKitGTK+'s standard user agent.
    if (baseDomain == "yahoo.com")
        return true;

    // taobao.com displays a mobile version with WebKitGTK+'s standard user agent.
    if (baseDomain == "taobao.com")
        return true;

    // web.whatsapp.com completely blocks users with WebKitGTK+'s standard user agent.
    if (baseDomain == "whatsapp.com")
        return true;

    // paypal.com completely blocks users with WebKitGTK+'s standard user agent.
    if (baseDomain == "paypal.com")
        return true;

    // chase.com displays a huge "please update your browser" warning with
    // WebKitGTK+'s standard user agent.
    if (baseDomain == "chase.com")
        return true;

    // Microsoft Outlook Web App forces users with WebKitGTK+'s standard user
    // agent to use the light version. Earlier versions even blocks users from
    // accessing the calendar.
    if (domain == "mail.ntu.edu.tw")
        return true;

    // Google Docs shows a scary unsupported browser warning with WebKitGTK+'s
    // standard user agent.
    if (domain == "docs.google.com")
        return true;

    return false;
}

static bool urlRequiresLinuxDesktopPlatform(const URL& url)
{
    // docs.google.com requires the macOS platform quirk.
    return isGoogle(url) && url.host() != "docs.google.com";
}

UserAgentQuirks UserAgentQuirks::quirksForURL(const URL& url)
{
    ASSERT(!url.isNull());

    UserAgentQuirks quirks;

    if (urlRequiresChromeBrowser(url))
        quirks.add(UserAgentQuirks::NeedsChromeBrowser);

    if (urlRequiresMacintoshPlatform(url))
        quirks.add(UserAgentQuirks::NeedsMacintoshPlatform);
    else if (urlRequiresLinuxDesktopPlatform(url))
        quirks.add(UserAgentQuirks::NeedsLinuxDesktopPlatform);

    return quirks;
}

String UserAgentQuirks::stringForQuirk(UserAgentQuirk quirk)
{
    switch (quirk) {
    case NeedsChromeBrowser:
        // Get versions from https://chromium.googlesource.com/chromium/src.git
        return "Chrome/58.0.3029.81"_s;
    case NeedsMacintoshPlatform:
        return "Macintosh; Intel Mac OS X 10_13_4"_s;
    case NeedsLinuxDesktopPlatform:
        return "X11; Linux x86_64"_s;
    case NumUserAgentQuirks:
    default:
        ASSERT_NOT_REACHED();
    }
    return ""_s;
}

}
