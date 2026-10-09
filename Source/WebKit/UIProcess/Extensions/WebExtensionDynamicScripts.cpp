/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "WebExtensionDynamicScripts.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionContext.h"
#include "WebExtensionScriptInjectionParameters.h"

namespace WebKit {

namespace WebExtensionDynamicScripts {

std::optional<SourcePair> sourcePairForResource(const String& path, WebExtensionContext& extensionContext)
{
    Ref extension = extensionContext.extension();
    auto scriptStringResult = extension->resourceStringForPath(path, WebExtension::CacheResult::Yes);
    if (!scriptStringResult) {
        extensionContext.recordErrorIfNeeded(scriptStringResult.error());
        return std::nullopt;
    }

    auto scriptString = scriptStringResult.value();
    scriptString = extensionContext.localizedResourceString(scriptString, extension->resourceMIMETypeForPath(path));

    return SourcePair { scriptString, { extensionContext.baseURL(), path } };
}

SourcePairs getSourcePairsForParameters(const WebExtensionScriptInjectionParameters& parameters, WebExtensionContext& extensionContext)
{
    if (parameters.files) {
        return WTF::compactMap(parameters.files.value(), [&](auto& file) -> std::optional<SourcePair> {
            return sourcePairForResource(file, extensionContext);
        });
    }

    return { SourcePair { parameters.code.value_or(parameters.function.value_or(parameters.css.value_or(""_s))), URL { } } };
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebExtensionRegisteredScript);

void WebExtensionRegisteredScript::updateParameters(const WebExtensionRegisteredScriptParameters& parameters)
{
    m_parameters = parameters;
}

void WebExtensionRegisteredScript::merge(WebExtensionRegisteredScriptParameters& parameters)
{
    if (!parameters.css && m_parameters.css)
        parameters.css = m_parameters.css.value();

    if (!parameters.js && m_parameters.js)
        parameters.js = m_parameters.js.value();

    if (!parameters.injectionTime && m_parameters.injectionTime)
        parameters.injectionTime = m_parameters.injectionTime.value();

    if (!parameters.excludeMatchPatterns && m_parameters.excludeMatchPatterns)
        parameters.excludeMatchPatterns = m_parameters.excludeMatchPatterns.value();

    if (!parameters.matchPatterns && m_parameters.matchPatterns)
        parameters.matchPatterns = m_parameters.matchPatterns.value();

    if (!parameters.allFrames && m_parameters.allFrames)
        parameters.allFrames = m_parameters.allFrames.value();

    if (!parameters.matchParentFrame && m_parameters.matchParentFrame)
        parameters.matchParentFrame = m_parameters.matchParentFrame.value();

    if (!parameters.persistent && m_parameters.persistent)
        parameters.persistent = m_parameters.persistent.value();

    if (!parameters.world && m_parameters.world)
        parameters.world = m_parameters.world.value();
}

void WebExtensionRegisteredScript::addUserScript(const String& identifier, API::UserScript& userScript)
{
    auto& userScripts = m_userScriptsMap.ensure(identifier, [&] {
        return UserScriptVector { };
    }).iterator->value;
    userScripts.append(userScript);
}

void WebExtensionRegisteredScript::addUserStyleSheet(const String& identifier, API::UserStyleSheet& userStyleSheet)
{
    auto& userStyleSheets = m_userStyleSheetsMap.ensure(identifier, [&] {
        return UserStyleSheetVector { };
    }).iterator->value;
    userStyleSheets.append(userStyleSheet);
}

void WebExtensionRegisteredScript::removeUserScriptsAndStyleSheets(const String& identifier)
{
    removeUserScripts(identifier);
    removeUserStyleSheets(identifier);
}

void WebExtensionRegisteredScript::removeUserScripts(const String& identifier)
{
    auto userScripts = m_userScriptsMap.take(identifier);
    auto allUserContentControllers = m_extensionContext->extensionController()->allUserContentControllers();

    for (auto& userScript : userScripts) {
        for (Ref userContentController : allUserContentControllers)
            userContentController->removeUserScript(userScript);
    }
}

void WebExtensionRegisteredScript::removeUserStyleSheets(const String& identifier)
{
    auto userStyleSheets = m_userStyleSheetsMap.take(identifier);
    auto allUserContentControllers = m_extensionContext->extensionController()->allUserContentControllers();

    for (auto& userStyleSheet : userStyleSheets) {
        for (Ref userContentController : allUserContentControllers)
            userContentController->removeUserStyleSheet(userStyleSheet);
    }
}

} // namespace WebExtensionDynamicScripts

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
