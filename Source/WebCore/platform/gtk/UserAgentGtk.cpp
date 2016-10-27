/*
 * Copyright (C) 2012, 2014 Igalia S.L.
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
#include "UserAgentGtk.h"

#include "PublicSuffix.h"
#include "URL.h"
#include <wtf/NeverDestroyed.h>
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

class UserAgentQuirks {
public:
    enum UserAgentQuirk {
        NeedsChromeBrowser,
        NeedsMacintoshPlatform,

        NumUserAgentQuirks
    };

    UserAgentQuirks()
        : m_quirks(0)
    {
        COMPILE_ASSERT(sizeof(m_quirks) * 8 >= NumUserAgentQuirks, not_enough_room_for_quirks);
    }

    void add(UserAgentQuirk quirk)
    {
        ASSERT(quirk >= 0);
        ASSERT_WITH_SECURITY_IMPLICATION(quirk < NumUserAgentQuirks);

        m_quirks |= (1 << quirk);
    }

    bool contains(UserAgentQuirk quirk) const
    {
        return m_quirks & (1 << quirk);
    }

    bool isEmpty() const { return !m_quirks; }

private:
    uint32_t m_quirks;
};

static const char* platformForUAString()
{
#if OS(MAC_OS_X)
    return "Macintosh";
#else
    return "X11";
#endif
}

static const String platformVersionForUAString()
{
#if OS(UNIX)
    struct utsname name;
    uname(&name);
    static NeverDestroyed<const String> uaOSVersion(String::format("%s %s", name.sysname, name.machine));
    return uaOSVersion;
#else
    // We will always claim to be Safari in Intel Mac OS X, since Safari without
    // OS X or anything on ARM triggers mobile versions of some websites.
    //
    // FIXME: The final result should include OS version, e.g. "Intel Mac OS X 10_8_4".
    static NeverDestroyed<const String> uaOSVersion(ASCIILiteral("Intel Mac OS X"));
    return uaOSVersion;
#endif
}

static const char* versionForUAString()
{
    return USER_AGENT_GTK_MAJOR_VERSION "." USER_AGENT_GTK_MINOR_VERSION;
}

static String buildUserAgentString(const UserAgentQuirks& quirks)
{
    StringBuilder uaString;
    uaString.appendLiteral("Mozilla/5.0 ");
    uaString.append('(');

    if (quirks.contains(UserAgentQuirks::NeedsMacintoshPlatform))
        uaString.appendLiteral("Macintosh");
    else
        uaString.append(platformForUAString());

    uaString.appendLiteral("; ");

    if (quirks.contains(UserAgentQuirks::NeedsMacintoshPlatform))
        uaString.appendLiteral("Intel Mac OS X 10_12");
    else
        uaString.append(platformVersionForUAString());

    uaString.appendLiteral(") AppleWebKit/");
    uaString.append(versionForUAString());
    uaString.appendLiteral(" (KHTML, like Gecko) ");

    // Note that Chrome UAs advertise *both* Chrome and Safari.
    // We set a meaningful value only for the first two digits here.
    if (quirks.contains(UserAgentQuirks::NeedsChromeBrowser))
        uaString.append("Chrome/54.0.2704.106 ");

    // Version/X is mandatory *before* Safari/X to be a valid Safari UA. See
    // https://bugs.webkit.org/show_bug.cgi?id=133403 for details.
    uaString.appendLiteral(" Version/10.0 Safari/");
    uaString.append(versionForUAString());

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
    if (applicationName.isEmpty())
        return standardUserAgentStatic();

    String finalApplicationVersion = applicationVersion;
    if (finalApplicationVersion.isEmpty())
        finalApplicationVersion = versionForUAString();

    return standardUserAgentStatic() + ' ' + applicationName + '/' + finalApplicationVersion;
}

// Be careful with this quirk: it's an invitation for sites to use JavaScript we can't handle.
static bool urlRequiresChromeBrowser(const URL& url)
{
    String baseDomain = topPrivatelyControlledDomain(url.host());

    // Needed for fonts on many sites, https://bugs.webkit.org/show_bug.cgi?id=147296
    if (baseDomain == "typekit.net" || baseDomain == "typekit.com")
        return true;

    // Shut off Chrome ads. Avoid missing features on maps.google.com.
    if (baseDomain.startsWith("google"))
        return true;

    // Needed for YouTube 360 (requires ENABLE_MEDIA_SOURCE).
    if (baseDomain == "youtube.com")
        return true;

    // Slack completely blocks users with our standard user agent.
    if (baseDomain == "slack.com")
        return true;

    return false;
}

static bool urlRequiresMacintoshPlatform(const URL& url)
{
    String baseDomain = topPrivatelyControlledDomain(url.host());

    // At least finance.yahoo.com displays a mobile version with our standard user agent.
    if (baseDomain == "yahoo.com")
        return true;

    // taobao.com displays a mobile version with our standard user agent.
    if (baseDomain == "taobao.com")
        return true;

    // web.whatsapp.com completely blocks users with our standard user agent.
    if (baseDomain == "whatsapp.com")
        return true;

    return false;
}

String standardUserAgentForURL(const URL& url)
{
    ASSERT(!url.isNull());
    UserAgentQuirks quirks;
    if (urlRequiresChromeBrowser(url))
        quirks.add(UserAgentQuirks::NeedsChromeBrowser);
    if (urlRequiresMacintoshPlatform(url))
        quirks.add(UserAgentQuirks::NeedsMacintoshPlatform);

    // The null string means we don't need a specific UA for the given URL.
    return quirks.isEmpty() ? String() : buildUserAgentString(quirks);
}

} // namespace WebCore

