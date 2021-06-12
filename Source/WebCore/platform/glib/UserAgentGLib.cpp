/*
 * Copyright (C) 2012, 2014, 2016 Igalia S.L.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "UserAgent.h"

#include "HTTPParsers.h"
#include "UserAgentQuirks.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/URL.h>
#include <wtf/glib/ChassisType.h>
#include <wtf/text/StringBuilder.h>

#if OS(UNIX)
#include <sys/utsname.h>
#endif

// WARNING! WARNING! WARNING!
//
// The user agent is ludicrously fragile. The most innocent change can
// and will break websites. Read the git log for this file carefully
// before changing user agent construction. You have been warned.

namespace WebCore {

static const char* platformForUAString()
{
#if OS(MAC_OS_X)
    return "Macintosh";
#else
    if (chassisType() == WTF::ChassisType::Mobile)
        return "Linux";
    return "X11";
#endif
}

static const String platformVersionForUAString()
{
#if OS(UNIX)
    if (chassisType() == WTF::ChassisType::Mobile)
        return "like Android 4.4";

    struct utsname name;
    uname(&name);
    static NeverDestroyed<const String> uaOSVersion(makeString(name.sysname, ' ', name.machine));
    return uaOSVersion;
#else
    // We will always claim to be Safari in Intel Mac OS X, since Safari without
    // OS X or anything on ARM triggers mobile versions of some websites.
    static NeverDestroyed<const String> uaOSVersion(MAKE_STATIC_STRING_IMPL("Intel Mac OS X 10_13_4"));
    return uaOSVersion;
#endif
}

static String buildUserAgentString(const UserAgentQuirks& quirks)
{
    StringBuilder uaString;
    uaString.append("Mozilla/5.0 (");

    if (quirks.contains(UserAgentQuirks::NeedsMacintoshPlatform))
        uaString.append(UserAgentQuirks::stringForQuirk(UserAgentQuirks::NeedsMacintoshPlatform));
    else {
        uaString.append(platformForUAString(), "; ");
#if defined(USER_AGENT_BRANDING)
        uaString.append(USER_AGENT_BRANDING "; ");
#endif
        uaString.append(platformVersionForUAString());
    }

    if (quirks.contains(UserAgentQuirks::NeedsFirefoxBrowser)) {
        uaString.append(UserAgentQuirks::stringForQuirk(UserAgentQuirks::NeedsFirefoxBrowser));
        return uaString.toString();
    }

    uaString.append(") AppleWebKit/605.1.15 (KHTML, like Gecko) ");

    // Note that Chrome UAs advertise *both* Chrome/X and Safari/X, but it does
    // not advertise Version/X.
    if (quirks.contains(UserAgentQuirks::NeedsChromeBrowser)) {
        uaString.append(UserAgentQuirks::stringForQuirk(UserAgentQuirks::NeedsChromeBrowser), ' ');
    // Version/X is mandatory *before* Safari/X to be a valid Safari UA. See
    // https://bugs.webkit.org/show_bug.cgi?id=133403 for details.
    } else
        uaString.append("Version/14.0 ");

    if (chassisType() == WTF::ChassisType::Mobile)
        uaString.append("Mobile ");
    uaString.append("Safari/605.1.15");

    return uaString.toString();
}

static const String standardUserAgentStatic()
{
    static NeverDestroyed<const String> uaStatic(buildUserAgentString(UserAgentQuirks()));
    return uaStatic;
}

String standardUserAgent(const String& applicationName, const String& applicationVersion)
{
    // Create a default user agent string with a liberal interpretation of
    // https://developer.mozilla.org/en-US/docs/User_Agent_Strings_Reference
    //
    // Forming a functional user agent is really difficult. We must mention Safari, because some
    // sites check for that when detecting WebKit browsers. Additionally some sites assume that
    // browsers that are "Safari" but not running on OS X are the Safari iOS browser. Getting this
    // wrong can cause sites to load the wrong JavaScript, CSS, or custom fonts. In some cases
    // sites won't load resources at all.

    String userAgent;
    if (applicationName.isEmpty()) {
        userAgent = standardUserAgentStatic();
    } else {
        String finalApplicationVersion = applicationVersion;
        if (finalApplicationVersion.isEmpty())
            finalApplicationVersion = "605.1.15";
        userAgent = standardUserAgentStatic() + ' ' + applicationName + '/' + finalApplicationVersion;
    }

    static bool checked = false;
    if (!checked) {
        // For release builds, we'll only check the first resource load, mainly to ensure that any
        // configured application details or user agent branding is OK.
        RELEASE_ASSERT_WITH_MESSAGE(isValidUserAgentHeaderValue(userAgent), "%s is not a valid user agent header", userAgent.utf8().data());
        checked = true;
    }
    ASSERT(isValidUserAgentHeaderValue(userAgent));
    return userAgent;
}

String standardUserAgentForURL(const URL& url)
{
    auto quirks = UserAgentQuirks::quirksForURL(url);
    // The null string means we don't need a specific UA for the given URL.
    // Note: UserAgentQuirks::NeedsUnbrandedUserAgent is implemented by simply
    // not returning here.
    if (quirks.isEmpty())
        return String();

    String userAgent(buildUserAgentString(quirks));
    ASSERT(isValidUserAgentHeaderValue(userAgent));
    return userAgent;
}

} // namespace WebCore
