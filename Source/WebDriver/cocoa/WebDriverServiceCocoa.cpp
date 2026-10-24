/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#include "Capabilities.h"
#include <signal.h>
#include <wtf/RunLoop.h>
#include <wtf/darwin/DispatchExtras.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebDriver {

// On SIGTERM or SIGINT, stop the run loop, so that WebDriverService is torn down and
// ~SessionHost terminates the browser that it launched. A dispatch signal source doesn't
// replace the signal's default action, so ignore the signal first. The sources live as long
// as the process.
static dispatch_source_t terminationSignalSources[2];

static void installTerminationSignalSource(size_t index, int signalNumber)
{
    signal(signalNumber, SIG_IGN);
    dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, signalNumber, 0, mainDispatchQueueSingleton());
    dispatch_source_set_event_handler(source, ^{
        RunLoop::mainSingleton().stop();
    });
    dispatch_resume(source);
    terminationSignalSources[index] = source;
}

void WebDriverService::platformInit()
{
    installTerminationSignalSource(0, SIGTERM);
    installTerminationSignalSource(1, SIGINT);
}

Capabilities WebDriverService::platformCapabilities()
{
    Capabilities capabilities;
    capabilities.platformName = String("mac"_s);
    capabilities.setWindowRect = true;
    return capabilities;
}

bool WebDriverService::platformValidateCapability(const String& name, const Ref<JSON::Value>& value) const
{
    if (name != "webkit:browserOptions"_s)
        return true;

    auto browserOptions = value->asObject();
    if (!browserOptions)
        return false;

    // minidriver always launches the MiniBrowser next to it.
    if (browserOptions->getValue("binary"_s))
        return false;

    if (auto argsValue = browserOptions->getValue("args"_s)) {
        auto args = argsValue->asArray();
        if (!args)
            return false;
        for (unsigned i = 0; i < args->length(); ++i) {
            if (!args->get(i)->asString())
                return false;
        }
    }

    return true;
}

bool WebDriverService::platformMatchCapability(const String&, const Ref<JSON::Value>&) const
{
    return true;
}

void WebDriverService::platformParseCapabilities(const JSON::Object& matchedCapabilities, Capabilities& capabilities) const
{
    capabilities.browserArguments = Vector<String> { };

    auto browserOptions = matchedCapabilities.getObject("webkit:browserOptions"_s);
    if (!browserOptions)
        return;

    auto browserArguments = browserOptions->getArray("args"_s);
    if (browserArguments && browserArguments->length()) {
        unsigned browserArgumentsLength = browserArguments->length();
        capabilities.browserArguments = Vector<String>();
        capabilities.browserArguments->reserveInitialCapacity(browserArgumentsLength);
        for (unsigned i = 0; i < browserArgumentsLength; ++i) {
            auto argument = browserArguments->get(i)->asString();
            capabilities.browserArguments->append(WTF::move(argument));
        }
    }
}

bool WebDriverService::platformCompareBrowserVersions(const String& requiredVersion, const String& proposedVersion)
{
    // Simple major.minor.micro version comparison.
    auto parseVersion = [](const String& version, uint64_t& major, uint64_t& minor, uint64_t& micro) -> bool {
        major = minor = micro = 0;
        Vector<String> tokens = version.split('.');
        switch (tokens.size()) {
        case 3:
            if (auto parsed = parseIntegerAllowingTrailingJunk<uint64_t>(tokens[2]))
                micro = *parsed;
            else
                return false;
            [[fallthrough]];
        case 2:
            if (auto parsed = parseIntegerAllowingTrailingJunk<uint64_t>(tokens[1]))
                minor = *parsed;
            else
                return false;
            [[fallthrough]];
        case 1:
            if (auto parsed = parseIntegerAllowingTrailingJunk<uint64_t>(tokens[0]))
                major = *parsed;
            else
                return false;
            break;
        default:
            return false;
        }
        return true;
    };

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
    return false;
}

bool WebDriverService::platformSupportBidi() const
{
#if ENABLE(WEBDRIVER_BIDI)
    // The WebSocket server needs nw_ws_request_copy_path() to route connections.
    if (__builtin_available(macOS 26.0, *))
        return true;
#endif
    return false;
}

} // namespace WebDriver

#endif // PLATFORM(COCOA)
