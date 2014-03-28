/*
 * Copyright (C) 2012 Igalia S.L.
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

#include <glib.h>

#if OS(UNIX)
#include <sys/utsname.h>
#endif

namespace WebCore {

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

static String platformVersionForUAString()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(String, uaOSVersion, (String()));
    if (!uaOSVersion.isEmpty())
        return uaOSVersion;

    // We will always claim to be Safari in Mac OS X, since Safari in Linux triggers the iOS path on
    // some websites.
    uaOSVersion = String::format("%s Mac OS X", cpuDescriptionForUAString());
    return uaOSVersion;
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
    DEPRECATED_DEFINE_STATIC_LOCAL(const CString, uaVersion, (String::format("%i.%i", USER_AGENT_GTK_MAJOR_VERSION, USER_AGENT_GTK_MINOR_VERSION).utf8()));
    DEPRECATED_DEFINE_STATIC_LOCAL(const String, staticUA, (String::format("Mozilla/5.0 (Macintosh; %s) AppleWebKit/%s (KHTML, like Gecko) Safari/%s Version/6.0",
        platformVersionForUAString().utf8().data(), uaVersion.data(), uaVersion.data())));

    if (applicationName.isEmpty())
        return staticUA;

    String finalApplicationVersion = applicationVersion;
    if (finalApplicationVersion.isEmpty())
        finalApplicationVersion = uaVersion.data();

    return String::format("%s %s/%s", staticUA.utf8().data(), applicationName.utf8().data(), finalApplicationVersion.utf8().data());
}

} // namespace WebCore

