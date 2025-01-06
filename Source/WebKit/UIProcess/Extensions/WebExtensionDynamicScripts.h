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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "APIContentWorld.h"
#include "APIUserScript.h"
#include "APIUserStyleSheet.h"
#include "WebExtension.h"
#include "WebExtensionFrameIdentifier.h"
#include "WebExtensionRegisteredScriptParameters.h"
#include "WebExtensionScriptInjectionResultParameters.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

OBJC_CLASS WKWebView;
OBJC_CLASS WKFrameInfo;
OBJC_CLASS _WKFrameTreeNode;

namespace WebKit {

class WebExtensionContext;
class WebExtensionTab;
struct WebExtensionScriptInjectionParameters;

namespace WebExtensionDynamicScripts {

using InjectionResults = Vector<WebExtensionScriptInjectionResultParameters>;

using SourcePair = std::pair<String, URL>;
using SourcePairs = Vector<SourcePair>;

using InjectionTime = WebExtension::InjectionTime;
using InjectedContentData = WebExtension::InjectedContentData;

using UserScriptVector = Vector<Ref<API::UserScript>>;
using UserStyleSheetVector = Vector<Ref<API::UserStyleSheet>>;

class WebExtensionRegisteredScript : public RefCounted<WebExtensionRegisteredScript> {
    WTF_MAKE_NONCOPYABLE(WebExtensionRegisteredScript);
    WTF_MAKE_TZONE_ALLOCATED(WebExtensionRegisteredScript);

public:
    template<typename... Args>
    static Ref<WebExtensionRegisteredScript> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionRegisteredScript(std::forward<Args>(args)...));
    }

    enum class FirstTimeRegistration { No, Yes };

    void updateParameters(const WebExtensionRegisteredScriptParameters&);

    void merge(WebExtensionRegisteredScriptParameters&);

    void addUserScript(const String& identifier, API::UserScript&);
    void addUserStyleSheet(const String& identifier, API::UserStyleSheet&);
    void removeUserScriptsAndStyleSheets(const String& identifier);

    void updateInjectedContent(InjectedContentData& injectedContent) { m_injectedContent = injectedContent; }
    const InjectedContentData& injectedContent() const { return m_injectedContent; }

    WebExtensionRegisteredScriptParameters parameters() const { return m_parameters; };

private:
    explicit WebExtensionRegisteredScript(WebExtensionContext& extensionContext, const WebExtensionRegisteredScriptParameters& parameters, const InjectedContentData& injectedContent)
        : m_extensionContext(extensionContext)
        , m_parameters(parameters)
        , m_injectedContent(injectedContent)
    {
    }

    WeakPtr<WebExtensionContext> m_extensionContext;
    WebExtensionRegisteredScriptParameters m_parameters;
    InjectedContentData m_injectedContent;

    HashMap<String, UserScriptVector> m_userScriptsMap;
    HashMap<String, UserStyleSheetVector> m_userStyleSheetsMap;

    void removeUserStyleSheets(const String& identifier);
    void removeUserScripts(const String& identifier);
};

std::optional<SourcePair> sourcePairForResource(const String& path, WebExtensionContext&);
SourcePairs getSourcePairsForParameters(const WebExtensionScriptInjectionParameters&, WebExtensionContext&);

void executeScript(const SourcePairs&, WKWebView *, API::ContentWorld&, WebExtensionTab&, const WebExtensionScriptInjectionParameters&, WebExtensionContext&, CompletionHandler<void(InjectionResults&&)>&&);
void injectStyleSheets(const SourcePairs&, WKWebView *, API::ContentWorld&, WebCore::UserStyleLevel, WebCore::UserContentInjectedFrames, WebExtensionContext&);
void removeStyleSheets(const SourcePairs&, WKWebView *, WebCore::UserContentInjectedFrames, WebExtensionContext&);

#if PLATFORM(COCOA)
WebExtensionScriptInjectionResultParameters toInjectionResultParameters(id resultOfExecution, WKFrameInfo *, NSString *errorMessage);
#endif

} // namespace WebExtensionDynamicScripts

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
