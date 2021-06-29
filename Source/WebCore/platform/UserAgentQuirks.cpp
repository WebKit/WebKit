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
#include <wtf/glib/ChassisType.h>

namespace WebCore {

// When editing the quirks in this file, be sure to update
// Tools/TestWebKitAPI/Tests/WebCore/UserAgentQuirks.cpp.
//
// When testing changes, be sure to test with application branding enabled.
// Otherwise, we will not notice when urlRequiresUnbrandedUserAgent is needed.

// Be careful with this quirk: it's an invitation for sites to use JavaScript
// that works in Chrome that WebKit cannot handle. Prefer other quirks instead.
static bool urlRequiresChromeBrowser(const String& domain, const String& baseDomain)
{
    // Needed for fonts on many sites to work with WebKit.
    // https://bugs.webkit.org/show_bug.cgi?id=147296
    if (baseDomain == "typekit.net" || baseDomain == "typekit.com")
        return true;

    // This site completely blocks the login page with WebKitGTK's standard user
    // agent and ask users to use Google Chrome or Microsoft Internet Explorer.
    if (domain == "auth.mayohr.com")
        return true;

    // Bank of America shows an unsupported browser warning with WebKitGTK's
    // standard user agent.
    if (baseDomain == "bankofamerica.com")
        return true;

    // Google Docs shows an unsupported browser warning with WebKitGTK's
    // standard user agent.
    if (domain == "docs.google.com")
        return true;

    // soundcloud.com serves broken MSE audio fragments with WebKitGTK's standard user agent.
    if (baseDomain == "soundcloud.com")
        return true;

    return false;
}

// Prefer using the macOS platform quirk rather than the Firefox quirk. This
// quirk is good for websites that do macOS-specific things we don't want on
// other platforms, and when the risk of the website doing Firefox-specific
// things is relatively low.
static bool urlRequiresFirefoxBrowser(const String& domain)
{
    // Red Hat Bugzilla displays a warning page when performing searches with WebKitGTK's standard
    // user agent.
    if (domain == "bugzilla.redhat.com")
        return true;

#if ENABLE(THUNDER)
    if (domain == "www.netflix.com")
        return true;
#endif

    return false;
}

static bool urlRequiresMacintoshPlatform(const String& domain, const String& baseDomain)
{
    // At least finance.yahoo.com displays a mobile version with WebKitGTK's standard user agent.
    if (chassisType() != WTF::ChassisType::Mobile && baseDomain == "yahoo.com")
        return true;

    // taobao.com displays a mobile version with WebKitGTK's standard user agent.
    if (chassisType() != WTF::ChassisType::Mobile && baseDomain == "taobao.com")
        return true;

    // web.whatsapp.com completely blocks users with WebKitGTK's standard user agent.
    if (baseDomain == "whatsapp.com")
        return true;

    // paypal.com completely blocks users with WebKitGTK's standard user agent.
    if (baseDomain == "paypal.com")
        return true;

    // chase.com displays a huge "please update your browser" warning with
    // WebKitGTK's standard user agent.
    if (baseDomain == "chase.com")
        return true;

    // Microsoft Outlook Web App forces users with WebKitGTK's standard user
    // agent to use the light version. Earlier versions even block users from
    // accessing the calendar.
    if (domain == "outlook.office.com"
        || domain == "mail.ntu.edu.tw"
        || domain == "exchange.tu-berlin.de")
        return true;

    return false;
}

static bool urlRequiresUnbrandedUserAgent(const String& domain)
{
    // Google uses an ugly fallback login page if application branding is
    // appended to WebKitGTK's standard user agent.
    if (domain == "accounts.google.com")
        return true;

    // Google Docs displays an unsupported browser warning if application
    // branding is appended to WebKitGTK's standard user agent.
    if (domain == "docs.google.com")
        return true;

    // Google Drive displays an unsupported browser warning if application
    // branding is appended to WebKitGTK's standard user agent.
    if (domain == "drive.google.com")
        return true;

    return false;
}

UserAgentQuirks UserAgentQuirks::quirksForURL(const URL& url)
{
    ASSERT(!url.isNull());

    String domain = url.host().toString();
    String baseDomain = topPrivatelyControlledDomain(domain);
    UserAgentQuirks quirks;

    if (urlRequiresChromeBrowser(domain, baseDomain))
        quirks.add(UserAgentQuirks::NeedsChromeBrowser);
    else if (urlRequiresFirefoxBrowser(domain))
        quirks.add(UserAgentQuirks::NeedsFirefoxBrowser);

    if (urlRequiresMacintoshPlatform(domain, baseDomain))
        quirks.add(UserAgentQuirks::NeedsMacintoshPlatform);

    if (urlRequiresUnbrandedUserAgent(domain))
        quirks.add(UserAgentQuirks::NeedsUnbrandedUserAgent);

    return quirks;
}

String UserAgentQuirks::stringForQuirk(UserAgentQuirk quirk)
{
    switch (quirk) {
    case NeedsChromeBrowser:
        // Get versions from https://chromium.googlesource.com/chromium/src.git
        return "Chrome/90.0.4419.1"_s;
    case NeedsFirefoxBrowser:
        return "; rv:87.0) Gecko/20100101 Firefox/87.0"_s;
    case NeedsMacintoshPlatform:
        return "Macintosh; Intel Mac OS X 10_15"_s;
    case NeedsUnbrandedUserAgent:
    case NumUserAgentQuirks:
        ASSERT_NOT_REACHED();
    }
    return ""_s;
}

}
