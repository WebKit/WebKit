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

#pragma once

#if ENABLE(INSPECTOR_EXTENSIONS)

#include "Connection.h"
#include "DataReference.h"
#include "InspectorExtensionTypes.h"
#include "MessageReceiver.h"
#include <WebCore/InspectorFrontendAPIDispatcher.h>
#include <WebCore/PageIdentifier.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>

namespace JSC {
class JSValue;
class JSObject;
}

namespace WebCore {
class InspectorFrontendClient;
}

namespace WebKit {

class WebInspectorUI;

class WebInspectorUIExtensionController
    : public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(WebInspectorUIExtensionController);
public:
    explicit WebInspectorUIExtensionController(WebCore::InspectorFrontendClient&);
    ~WebInspectorUIExtensionController();

    // Implemented in generated WebInspectorUIExtensionControllerMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // WebInspectorUIExtensionController IPC messages.
    void registerExtension(const Inspector::ExtensionID&, const String& displayName, CompletionHandler<void(Expected<bool, Inspector::ExtensionError>)>&&);
    void unregisterExtension(const Inspector::ExtensionID&, CompletionHandler<void(Expected<bool, Inspector::ExtensionError>)>&&);
    void createTabForExtension(const Inspector::ExtensionID&, const String& tabName, const URL& tabIconURL, const URL& sourceURL, CompletionHandler<void(Expected<Inspector::ExtensionTabID, Inspector::ExtensionError>)>&&);
    void evaluateScriptForExtension(const Inspector::ExtensionID&, const String& scriptSource, const std::optional<URL>& frameURL, const std::optional<URL>& contextSecurityOrigin, const std::optional<bool>& useContentScriptContext, CompletionHandler<void(const IPC::DataReference&, const std::optional<WebCore::ExceptionDetails>&, const std::optional<Inspector::ExtensionError>&)>&&);
    void reloadForExtension(const Inspector::ExtensionID&, const std::optional<bool>& ignoreCache, const std::optional<String>& userAgent, const std::optional<String>& injectedScript, CompletionHandler<void(const std::optional<Inspector::ExtensionError>&)>&&);

    // Callbacks from the frontend.
#if ENABLE(INSPECTOR_EXTENSIONS)
    void didShowExtensionTab(const Inspector::ExtensionID&, const Inspector::ExtensionTabID&);
    void didHideExtensionTab(const Inspector::ExtensionID&, const Inspector::ExtensionTabID&);
#endif

private:
    JSC::JSObject* unwrapEvaluationResultAsObject(WebCore::InspectorFrontendAPIDispatcher::EvaluationResult);
    std::optional<Inspector::ExtensionError> parseExtensionErrorFromEvaluationResult(WebCore::InspectorFrontendAPIDispatcher::EvaluationResult);

    WeakPtr<WebCore::InspectorFrontendClient> m_frontendClient;
    WebCore::PageIdentifier m_inspectorPageIdentifier;
};

} // namespace WebKit

#endif // ENABLE(INSPECTOR_EXTENSIONS)
