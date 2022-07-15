/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "WebDriverService.h"

#include <wtf/text/StringToIntegerConversion.h>

namespace WebDriver {

static bool parseVersion(const String& version, uint64_t& major, uint64_t& minor, uint64_t& micro)
{
    major = minor = micro = 0;

    Vector<String> tokens = version.split('.');
    switch (tokens.size()) {
    case 3: {
        auto parsedMicro = parseIntegerAllowingTrailingJunk<uint64_t>(tokens[2]);
        if (!parsedMicro)
            return false;
        micro = *parsedMicro;
    }
        FALLTHROUGH;
    case 2: {
        auto parsedMinor = parseIntegerAllowingTrailingJunk<uint64_t>(tokens[1]);
        if (!parsedMinor)
            return false;
        minor = *parsedMinor;
    }
        FALLTHROUGH;
    case 1: {
        auto parsedMajor = parseIntegerAllowingTrailingJunk<uint64_t>(tokens[0]);
        if (!parsedMajor)
            return false;
        major = *parsedMajor;
    }
        break;
    default:
        return false;
    }

    return true;
}

bool WebDriverService::platformCompareBrowserVersions(const String& requiredVersion, const String& proposedVersion)
{
    // We require clients to use format major.micro.minor as version string.
    uint64_t requiredMajor, requiredMinor, requiredMicro;
    if (!parseVersion(requiredVersion, requiredMajor, requiredMinor, requiredMicro))
        return false;

    uint64_t proposedMajor, proposedMinor, proposedMicro;
    if (!parseVersion(proposedVersion, proposedMajor, proposedMinor, proposedMicro))
        return false;

    return proposedMajor > requiredMajor
        || (proposedMajor == requiredMajor && proposedMinor > requiredMinor)
        || (proposedMajor == requiredMajor && proposedMinor == requiredMinor && proposedMicro >= requiredMicro);
}

bool WebDriverService::platformSupportProxyType(const String&) const
{
    return true;
}

} // namespace WebDriver
