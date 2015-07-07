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

#include "URL.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

#if OS(UNIX)
#include <sys/utsname.h>
#endif

namespace WebCore {

class UserAgentQuirks {
public:
    enum UserAgentQuirk {
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

static const char* cpuDescriptionForUAString()
{
#if CPU(PPC) || CPU(PPC64)
    return "PPC";
#elif CPU(X86) || CPU(X86_64)
    return "Intel";
#elif CPU(ARM) || CPU(ARM64)
    return "ARM";
#else
    return "Unknown";
#endif
}

static const char* platformForUAString()
{
#if PLATFORM(X11)
    return "X11";
#elif OS(WINDOWS)
    return "";
#elif PLATFORM(MAC)
    return "Macintosh";
#elif defined(GDK_WINDOWING_DIRECTFB)
    return "DirectFB";
#else
    return "Unknown";
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
    // We will always claim to be Safari in Mac OS X, since Safari in Linux triggers the iOS path on some websites.
    static NeverDestroyed<const String> uaOSVersion(String::format("%s Mac OS X", cpuDescriptionForUAString()));
    return uaOSVersion;
#endif
}

static const String versionForUAString()
{
    static NeverDestroyed<const String> uaVersion(String::format("%i.%i", USER_AGENT_GTK_MAJOR_VERSION, USER_AGENT_GTK_MINOR_VERSION));
    return uaVersion;
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

    if (quirks.contains(UserAgentQuirks::NeedsMacintoshPlatform)) {
        uaString.append(cpuDescriptionForUAString());
        uaString.appendLiteral(" Mac OS X");
    } else
        uaString.append(platformVersionForUAString());

    uaString.appendLiteral(") AppleWebKit/");
    uaString.append(versionForUAString());
    // Version/X is mandatory *before* Safari/X to be a valid Safari UA. See
    // https://bugs.webkit.org/show_bug.cgi?id=133403 for details.
    uaString.appendLiteral(" (KHTML, like Gecko) Version/8.0 Safari/");
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
    // browsers that are "Safari" but not running on OS X are the Safari iOS browse. Getting this
    // wrong can cause sites to load the wrong JavaScript, CSS, or custom fonts. In some cases
    // sites won't load resources at all.
    if (applicationName.isEmpty())
        return standardUserAgentStatic();

    String finalApplicationVersion = applicationVersion;
    if (finalApplicationVersion.isEmpty())
        finalApplicationVersion = versionForUAString();

    return standardUserAgentStatic() + ' ' + applicationName + '/' + finalApplicationVersion;
}

String standardUserAgentForURL(const URL& url)
{
    ASSERT(!url.isNull());
    UserAgentQuirks quirks;
    if (url.host().endsWith(".yahoo.com")) {
        // www.yahoo.com redirects to the mobile version when Linux is present in the UA,
        // use always Macintosh as platform. See https://bugs.webkit.org/show_bug.cgi?id=125444.
        quirks.add(UserAgentQuirks::NeedsMacintoshPlatform);
    }

    // The null string means we don't need a specific UA for the given URL.
    return quirks.isEmpty() ? String() : buildUserAgentString(quirks);
}

} // namespace WebCore

