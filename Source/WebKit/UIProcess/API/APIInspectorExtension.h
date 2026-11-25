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

#include "APIInspectorExtensionClient.h"
#include "APIObject.h"
#include "InspectorExtensionTypes.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
class WebInspectorUIExtensionControllerProxy;
}

namespace API {

class InspectorExtension final : public API::ObjectImpl<Object::Type::InspectorExtension> {
public:
    static Ref<InspectorExtension> create(StringImpl* identifier, WebKit::WebInspectorUIExtensionControllerProxy&);

    const RefPtr<StringImpl>& identifier() const LIFETIME_BOUND { return m_identifier; }

    void createTab(StringImpl* tabName, const WTF::URL& tabIconURL, const WTF::URL& sourceURL, WTF::CompletionHandler<void(std::expected<Inspector::ExtensionTabID, Inspector::ExtensionError>)>&&);
    void evaluateScript(StringImpl* scriptSource, const std::optional<WTF::URL>& frameURL, const std::optional<WTF::URL>& contextSecurityOrigin, const std::optional<bool>& useContentScriptContext, WTF::CompletionHandler<void(Inspector::ExtensionEvaluationResult)>&&);
    void navigateTab(const Inspector::ExtensionTabID&, const WTF::URL& sourceURL, WTF::CompletionHandler<void(const std::optional<Inspector::ExtensionError>)>&&);
    void reloadIgnoringCache(const std::optional<bool>& ignoreCache, const std::optional<RefPtr<StringImpl>>& userAgent, const std::optional<RefPtr<StringImpl>>& injectedScript, WTF::CompletionHandler<void(Inspector::ExtensionVoidResult)>&&);

    // For testing.
    void evaluateScriptInExtensionTab(const Inspector::ExtensionTabID&, StringImpl* scriptSource, WTF::CompletionHandler<void(Inspector::ExtensionEvaluationResult)>&&);

    InspectorExtensionClient* client() const LIFETIME_BOUND { return m_client.get(); }
    void setClient(UniqueRef<InspectorExtensionClient>&&);

private:
    InspectorExtension(StringImpl* identifier, WebKit::WebInspectorUIExtensionControllerProxy&);

    RefPtr<StringImpl> m_identifier;
    WeakPtr<WebKit::WebInspectorUIExtensionControllerProxy> m_extensionControllerProxy;
    std::unique_ptr<API::InspectorExtensionClient> m_client;
};

} // namespace API

SPECIALIZE_TYPE_TRAITS_API_OBJECT(InspectorExtension);

#endif // ENABLE(INSPECTOR_EXTENSIONS)
