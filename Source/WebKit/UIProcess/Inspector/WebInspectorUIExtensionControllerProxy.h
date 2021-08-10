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

#include "InspectorExtensionTypes.h"
#include "MessageReceiver.h"
#include <wtf/Expected.h>
#include <wtf/Forward.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>

namespace API {
class InspectorExtension;
}

namespace WebKit {

class WebPageProxy;

class WebInspectorUIExtensionControllerProxy final
    : public RefCounted<WebInspectorUIExtensionControllerProxy>
    , public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(WebInspectorUIExtensionControllerProxy);
public:
    static Ref<WebInspectorUIExtensionControllerProxy> create(WebPageProxy& inspectorPage);
    virtual ~WebInspectorUIExtensionControllerProxy();

    // Implemented in generated WebInspectorUIExtensionControllerProxyMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // API.
    void registerExtension(const Inspector::ExtensionID&, const String& displayName, WTF::CompletionHandler<void(Expected<RefPtr<API::InspectorExtension>, Inspector::ExtensionError>)>&&);
    void unregisterExtension(const Inspector::ExtensionID&, WTF::CompletionHandler<void(Expected<bool, Inspector::ExtensionError>)>&&);
    void createTabForExtension(const Inspector::ExtensionID&, const String& tabName, const URL& tabIconURL, const URL& sourceURL, WTF::CompletionHandler<void(Expected<Inspector::ExtensionTabID, Inspector::ExtensionError>)>&&);
    void evaluateScriptForExtension(const Inspector::ExtensionID&, const String& scriptSource, const Optional<URL>& frameURL, const Optional<URL>& contextSecurityOrigin, const Optional<bool>& useContentScriptContext, WTF::CompletionHandler<void(Inspector::ExtensionEvaluationResult)>&&);
    void reloadForExtension(const Inspector::ExtensionID&, const Optional<bool>& ignoreCache, const Optional<String>& userAgent, const Optional<String>& injectedScript, WTF::CompletionHandler<void(Inspector::ExtensionEvaluationResult)>&&);

    // WebInspectorUIExtensionControllerProxy IPC messages.
    void didShowExtensionTab(const Inspector::ExtensionID&, const Inspector::ExtensionTabID&);
    void didHideExtensionTab(const Inspector::ExtensionID&, const Inspector::ExtensionTabID&);

    // Notifications.
    void inspectorFrontendLoaded();

private:
    explicit WebInspectorUIExtensionControllerProxy(WebPageProxy& inspectorPage);

    void whenFrontendHasLoaded(Function<void()>&&);

    WeakPtr<WebPageProxy> m_inspectorPage;
    HashMap<Inspector::ExtensionID, RefPtr<API::InspectorExtension>> m_extensionAPIObjectMap;

    // Used to queue actions such as registering extensions that happen early on.
    // There's no point sending these before the frontend is fully loaded.
    Vector<Function<void()>> m_frontendLoadedCallbackQueue;

    bool m_frontendLoaded { false };
};

} // namespace WebKit

#endif // ENABLE(INSPECTOR_EXTENSIONS)
