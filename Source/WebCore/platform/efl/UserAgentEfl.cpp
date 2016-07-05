/*
 * Copyright (C) 2015 Naver Corp. All rights reserved.
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
#include "UserAgentEfl.h"

#include <wtf/NeverDestroyed.h>

#if OS(UNIX)
#include <sys/utsname.h>
#endif

namespace WebCore {

static const char* platformForUAString()
{
#if PLATFORM(X11)
    return "X11";
#else
    return "Unknown";
#endif
}

static String platformVersionForUAString()
{
    String version;
    struct utsname name;
    if (uname(&name) != -1)
        version = makeString(name.sysname, ' ', name.machine);
    else
        version = ASCIILiteral("Unknown");
    return version;
}

static const char* versionForUAString()
{
    return USER_AGENT_EFL_MAJOR_VERSION "." USER_AGENT_EFL_MINOR_VERSION;
}

String standardUserAgent(const String& applicationName, const String& applicationVersion)
{
    const String& version = ASCIILiteral(versionForUAString());
    static NeverDestroyed<String> standardUserAgentString = makeString("Mozilla/5.0 (", platformForUAString(), "; ", platformVersionForUAString(),
        ") AppleWebKit/", version, " (KHTML, like Gecko) Version/8.0 Safari/601.2.7");

    if (applicationName.isEmpty())
        return standardUserAgentString;

    String finalApplicationVersion = applicationVersion;
    if (finalApplicationVersion.isEmpty())
        finalApplicationVersion = ASCIILiteral(versionForUAString());

    return standardUserAgentString + ' ' + applicationName + '/' + finalApplicationVersion;
}

} // namespace WebCore

