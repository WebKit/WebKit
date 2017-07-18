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

bool WebDriverService::platformParseCapabilities(InspectorObject& desiredCapabilities, Capabilities& capabilities, Function<void (CommandResult&&)>& completionHandler)
{
    RefPtr<InspectorValue> value;
    RefPtr<InspectorObject> browserOptions;
    if (desiredCapabilities.getValue(ASCIILiteral("webkitgtk:browserOptions"), value) && !value->asObject(browserOptions)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("webkitgtk:browserOptions is invalid in capabilities")));
        return false;
    }
    if (browserOptions->isNull()) {
        capabilities.browserBinary = LIBEXECDIR "/webkit2gtk-" WEBKITGTK_API_VERSION_STRING "/MiniBrowser";
        capabilities.browserArguments = { ASCIILiteral("--automation") };
        return true;
    }

    if (!browserOptions->getString(ASCIILiteral("binary"), capabilities.browserBinary)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("binary parameter is invalid or missing in webkitgtk:browserOptions")));
        return false;
    }

    RefPtr<InspectorArray> browserArguments;
    if (browserOptions->getValue(ASCIILiteral("args"), value) && !value->asArray(browserArguments)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("args parameter is invalid in webkitgtk:browserOptions")));
        return false;
    }
    unsigned browserArgumentsLength = browserArguments->length();
    if (!browserArgumentsLength)
        return true;
    capabilities.browserArguments.reserveInitialCapacity(browserArgumentsLength);
    for (unsigned i = 0; i < browserArgumentsLength; ++i) {
        RefPtr<InspectorValue> value = browserArguments->get(i);
        String argument;
        if (!value->asString(argument)) {
            capabilities.browserArguments.clear();
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("Failed to extract arguments from webkitgtk:browserOptions::args")));
            return false;
        }
        capabilities.browserArguments.uncheckedAppend(WTFMove(argument));
    }

    if (browserOptions->getValue(ASCIILiteral("useOverlayScrollbars"), value) && !value->asBoolean(capabilities.useOverlayScrollbars)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("useOverlayScrollbars parameter is invalid in webkitgtk:browserOptions")));
        return false;
    }

    return true;
}

} // namespace WebDriver
