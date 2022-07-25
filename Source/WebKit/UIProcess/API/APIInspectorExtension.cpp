/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#include "APIInspectorExtension.h"

#if ENABLE(INSPECTOR_EXTENSIONS)

#include "InspectorExtensionTypes.h"
#include "WebInspectorUIExtensionControllerProxy.h"
#include <WebCore/ExceptionDetails.h>
#include <wtf/UniqueRef.h>

namespace API {

InspectorExtension::InspectorExtension(const WTF::String& identifier, WebKit::WebInspectorUIExtensionControllerProxy& extensionControllerProxy)
    : m_identifier(identifier)
    , m_extensionControllerProxy(extensionControllerProxy)
{
}

Ref<InspectorExtension> InspectorExtension::create(const WTF::String& identifier, WebKit::WebInspectorUIExtensionControllerProxy& extensionControllerProxy)
{
    return adoptRef(*new InspectorExtension(identifier, extensionControllerProxy));
}

void InspectorExtension::setClient(UniqueRef<InspectorExtensionClient>&& client)
{
    m_client = client.moveToUniquePtr();
}

void InspectorExtension::createTab(const WTF::String& tabName, const WTF::URL& tabIconURL, const WTF::URL& sourceURL, WTF::CompletionHandler<void(Expected<Inspector::ExtensionTabID, Inspector::ExtensionError>)>&& completionHandler)
{
    if (!m_extensionControllerProxy) {
        completionHandler(makeUnexpected(Inspector::ExtensionError::ContextDestroyed));
        return;
    }

    m_extensionControllerProxy->createTabForExtension(m_identifier, tabName, tabIconURL, sourceURL, WTFMove(completionHandler));
}

void InspectorExtension::evaluateScript(const WTF::String& scriptSource, const std::optional<WTF::URL>& frameURL, const std::optional<WTF::URL>& contextSecurityOrigin, const std::optional<bool>& useContentScriptContext, WTF::CompletionHandler<void(Inspector::ExtensionEvaluationResult)>&& completionHandler)
{
    if (!m_extensionControllerProxy) {
        completionHandler(makeUnexpected(Inspector::ExtensionError::ContextDestroyed));
        return;
    }

    m_extensionControllerProxy->evaluateScriptForExtension(m_identifier, scriptSource, frameURL, contextSecurityOrigin, useContentScriptContext, WTFMove(completionHandler));
}

void InspectorExtension::navigateTab(const Inspector::ExtensionTabID& extensionTabID, const WTF::URL& sourceURL, WTF::CompletionHandler<void(const std::optional<Inspector::ExtensionError>)>&& completionHandler)
{
    if (!m_extensionControllerProxy) {
        completionHandler(Inspector::ExtensionError::ContextDestroyed);
        return;
    }

    m_extensionControllerProxy->navigateTabForExtension(extensionTabID, sourceURL, WTFMove(completionHandler));
}

void InspectorExtension::reloadIgnoringCache(const std::optional<bool>& ignoreCache, const std::optional<WTF::String>& userAgent, const std::optional<WTF::String>& injectedScript,  WTF::CompletionHandler<void(Inspector::ExtensionEvaluationResult)>&& completionHandler)
{
    if (!m_extensionControllerProxy) {
        completionHandler(makeUnexpected(Inspector::ExtensionError::ContextDestroyed));
        return;
    }

    m_extensionControllerProxy->reloadForExtension(m_identifier, ignoreCache, userAgent, injectedScript, WTFMove(completionHandler));
}

// For testing.

void InspectorExtension::evaluateScriptInExtensionTab(const Inspector::ExtensionTabID& extensionTabID, const WTF::String& scriptSource, WTF::CompletionHandler<void(Inspector::ExtensionEvaluationResult)>&& completionHandler)
{
    if (!m_extensionControllerProxy) {
        completionHandler(makeUnexpected(Inspector::ExtensionError::ContextDestroyed));
        return;
    }

    m_extensionControllerProxy->evaluateScriptInExtensionTab(extensionTabID, scriptSource, WTFMove(completionHandler));
}

} // namespace API

#endif // ENABLE(INSPECTOR_EXTENSIONS)
