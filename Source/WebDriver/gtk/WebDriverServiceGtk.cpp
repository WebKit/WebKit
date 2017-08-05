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

#include "Capabilities.h"
#include "CommandResult.h"
#include <inspector/InspectorValues.h>

using namespace Inspector;

namespace WebDriver {

Capabilities WebDriverService::platformCapabilities()
{
    Capabilities capabilities;
    capabilities.platformName = String("linux");
    capabilities.acceptInsecureCerts = false;
    return capabilities;
}

bool WebDriverService::platformValidateCapability(const String& name, const RefPtr<InspectorValue>& value) const
{
    if (name != "webkitgtk:browserOptions")
        return true;

    RefPtr<InspectorObject> browserOptions;
    if (!value->asObject(browserOptions))
        return false;

    if (browserOptions->isNull())
        return true;

    // If browser options are provided, binary is required.
    String binary;
    if (!browserOptions->getString(ASCIILiteral("binary"), binary))
        return false;

    RefPtr<InspectorValue> useOverlayScrollbarsValue;
    bool useOverlayScrollbars;
    if (browserOptions->getValue(ASCIILiteral("useOverlayScrollbars"), useOverlayScrollbarsValue) && !useOverlayScrollbarsValue->asBoolean(useOverlayScrollbars))
        return false;

    RefPtr<InspectorValue> browserArgumentsValue;
    RefPtr<InspectorArray> browserArguments;
    if (browserOptions->getValue(ASCIILiteral("args"), browserArgumentsValue) && !browserArgumentsValue->asArray(browserArguments))
        return false;

    unsigned browserArgumentsLength = browserArguments->length();
    for (unsigned i = 0; i < browserArgumentsLength; ++i) {
        RefPtr<InspectorValue> value = browserArguments->get(i);
        String argument;
        if (!value->asString(argument))
            return false;
    }

    return true;
}

std::optional<String> WebDriverService::platformMatchCapability(const String&, const RefPtr<InspectorValue>&) const
{
    return std::nullopt;
}

static bool parseVersion(const String& version, uint64_t& major, uint64_t& minor, uint64_t& micro)
{
    major = minor = micro = 0;

    Vector<String> tokens;
    version.split(".", false, tokens);
    bool ok;
    switch (tokens.size()) {
    case 3:
        micro = tokens[2].toInt64(&ok);
        if (!ok)
            return false;
        FALLTHROUGH;
    case 2:
        minor = tokens[1].toInt64(&ok);
        if (!ok)
            return false;
        FALLTHROUGH;
    case 1:
        major = tokens[0].toInt64(&ok);
        if (!ok)
            return false;
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

void WebDriverService::platformParseCapabilities(const InspectorObject& matchedCapabilities, Capabilities& capabilities) const
{
    RefPtr<InspectorObject> browserOptions;
    if (!matchedCapabilities.getObject(ASCIILiteral("webkitgtk:browserOptions"), browserOptions)) {
        capabilities.browserBinary = String(LIBEXECDIR "/webkit2gtk-" WEBKITGTK_API_VERSION_STRING "/MiniBrowser");
        capabilities.browserArguments = Vector<String> { ASCIILiteral("--automation") };
        capabilities.useOverlayScrollbars = true;
        return;
    }

    String browserBinary;
    browserOptions->getString(ASCIILiteral("binary"), browserBinary);
    ASSERT(!browserBinary.isNull());
    capabilities.browserBinary = browserBinary;

    capabilities.browserArguments = Vector<String>();
    RefPtr<InspectorArray> browserArguments;
    if (browserOptions->getArray(ASCIILiteral("args"), browserArguments)) {
        unsigned browserArgumentsLength = browserArguments->length();
        capabilities.browserArguments->reserveInitialCapacity(browserArgumentsLength);
        for (unsigned i = 0; i < browserArgumentsLength; ++i) {
            RefPtr<InspectorValue> value = browserArguments->get(i);
            String argument;
            value->asString(argument);
            ASSERT(!argument.isNull());
            capabilities.browserArguments->uncheckedAppend(WTFMove(argument));
        }
    }

    bool useOverlayScrollbars;
    if (browserOptions->getBoolean(ASCIILiteral("useOverlayScrollbars"), useOverlayScrollbars))
        capabilities.useOverlayScrollbars = useOverlayScrollbars;
    else
        capabilities.useOverlayScrollbars = true;
}

} // namespace WebDriver
