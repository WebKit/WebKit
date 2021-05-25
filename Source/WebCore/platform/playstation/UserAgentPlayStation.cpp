/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringConcatenate.h>

// WARNING! WARNING! WARNING!
//
// The user agent is ludicrously fragile. The most innocent change can
// and will break websites. Read the git log for this file and
// WebCore/platform/glib/UserAgentGLib.cpp carefully before changing
// user agent construction. You have been warned.

namespace WebCore {

static String getSystemSoftwareName()
{
#if HAS_GETENV_NP
    char buf[32];
    if (!getenv_np("SYSTEM_SOFTWARE_NAME", buf, sizeof(buf)))
        return buf;
#endif
    return "PlayStation";
}

static String getSystemSoftwareVersion()
{
#if HAS_GETENV_NP
    char buf[32];
    if (!getenv_np("SYSTEM_SOFTWARE_VERSION", buf, sizeof(buf)))
        return buf;
#endif
    return "0.00";
}

static constexpr const char* versionForUAString()
{
    // https://bugs.webkit.org/show_bug.cgi?id=180365
    return "605.1.15";
}

static String standardUserAgentStatic()
{
    // Version/X is mandatory *before* Safari/X to be a valid Safari UA. See
    // https://bugs.webkit.org/show_bug.cgi?id=133403 for details.
    static NeverDestroyed<String> uaStatic(makeString("Mozilla/5.0 (PlayStation; ", getSystemSoftwareName(), '/', getSystemSoftwareVersion(), ") AppleWebKit/", versionForUAString(), " (KHTML, like Gecko) ", "Version/14.0 Safari/", versionForUAString()));
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

    return makeString(standardUserAgentStatic(), ' ', applicationName, '/', finalApplicationVersion);
}

String standardUserAgentForURL(const URL&)
{
    return standardUserAgentStatic();
}

} // namespace WebCore
