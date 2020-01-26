/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "MessageReceiver.h"
#include "UserContentControllerIdentifier.h"
#include "WebScriptMessageHandler.h"
#include "WebUserContentControllerDataTypes.h"
#include <WebCore/UserContentProvider.h>
#include <wtf/HashMap.h>

#if ENABLE(CONTENT_EXTENSIONS)
#include <WebCore/ContentExtensionsBackend.h>
#endif

namespace WebCore {
namespace ContentExtensions {
class CompiledContentExtension;
}
}

namespace WebKit {

class InjectedBundleScriptWorld;
class WebCompiledContentRuleListData;
class WebUserMessageHandlerDescriptorProxy;
enum class InjectUserScriptImmediately : bool;

class WebUserContentController final : public WebCore::UserContentProvider, private IPC::MessageReceiver {
public:
    static Ref<WebUserContentController> getOrCreate(UserContentControllerIdentifier);
    virtual ~WebUserContentController();

    UserContentControllerIdentifier identifier() { return m_identifier; }

    void addUserScript(InjectedBundleScriptWorld&, WebCore::UserScript&&);
    void removeUserScriptWithURL(InjectedBundleScriptWorld&, const URL&);
    void removeUserScripts(InjectedBundleScriptWorld&);
    void addUserStyleSheet(InjectedBundleScriptWorld&, WebCore::UserStyleSheet&&);
    void removeUserStyleSheetWithURL(InjectedBundleScriptWorld&, const URL&);
    void removeUserStyleSheets(InjectedBundleScriptWorld&);
    void removeAllUserContent();

    InjectedBundleScriptWorld* worldForIdentifier(ContentWorldIdentifier);

    void addUserContentWorlds(const Vector<std::pair<ContentWorldIdentifier, String>>&);
    void addUserContentWorld(const std::pair<ContentWorldIdentifier, String>&);
    void addUserScripts(Vector<WebUserScriptData>&&, InjectUserScriptImmediately);
    void addUserStyleSheets(const Vector<WebUserStyleSheetData>&);
    void addUserScriptMessageHandlers(const Vector<WebScriptMessageHandlerData>&);
#if ENABLE(CONTENT_EXTENSIONS)
    void addContentRuleLists(Vector<std::pair<String, WebCompiledContentRuleListData>>&&);
#endif

private:
    explicit WebUserContentController(UserContentControllerIdentifier);

    // WebCore::UserContentProvider
    void forEachUserScript(Function<void(WebCore::DOMWrapperWorld&, const WebCore::UserScript&)>&&) const final;
    void forEachUserStyleSheet(Function<void(const WebCore::UserStyleSheet&)>&&) const final;
#if ENABLE(USER_MESSAGE_HANDLERS)
    void forEachUserMessageHandler(Function<void(const WebCore::UserMessageHandlerDescriptor&)>&&) const final;
#endif
#if ENABLE(CONTENT_EXTENSIONS)
    WebCore::ContentExtensions::ContentExtensionsBackend& userContentExtensionBackend() override { return m_contentExtensionBackend; }
#endif

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void removeUserContentWorlds(const Vector<ContentWorldIdentifier>&);

    void removeUserScript(ContentWorldIdentifier, uint64_t userScriptIdentifier);
    void removeAllUserScripts(const Vector<ContentWorldIdentifier>&);

    void removeUserStyleSheet(ContentWorldIdentifier, uint64_t userScriptIdentifier);
    void removeAllUserStyleSheets(const Vector<ContentWorldIdentifier>&);

    void removeUserScriptMessageHandler(ContentWorldIdentifier, uint64_t userScriptIdentifier);
    void removeAllUserScriptMessageHandlers(const Vector<ContentWorldIdentifier>&);

#if ENABLE(CONTENT_EXTENSIONS)
    void removeContentRuleList(const String& name);
    void removeAllContentRuleLists();
#endif

    void addUserScriptInternal(InjectedBundleScriptWorld&, const Optional<uint64_t>& userScriptIdentifier, WebCore::UserScript&&, InjectUserScriptImmediately);
    void removeUserScriptInternal(InjectedBundleScriptWorld&, uint64_t userScriptIdentifier);
    void addUserStyleSheetInternal(InjectedBundleScriptWorld&, const Optional<uint64_t>& userStyleSheetIdentifier, WebCore::UserStyleSheet&&);
    void removeUserStyleSheetInternal(InjectedBundleScriptWorld&, uint64_t userStyleSheetIdentifier);
#if ENABLE(USER_MESSAGE_HANDLERS)
    void addUserScriptMessageHandlerInternal(InjectedBundleScriptWorld&, uint64_t userScriptMessageHandlerIdentifier, const String& name);
    void removeUserScriptMessageHandlerInternal(InjectedBundleScriptWorld&, uint64_t userScriptMessageHandlerIdentifier);
#endif

    UserContentControllerIdentifier m_identifier;

    typedef HashMap<RefPtr<InjectedBundleScriptWorld>, Vector<std::pair<Optional<uint64_t>, WebCore::UserScript>>> WorldToUserScriptMap;
    WorldToUserScriptMap m_userScripts;

    typedef HashMap<RefPtr<InjectedBundleScriptWorld>, Vector<std::pair<Optional<uint64_t>, WebCore::UserStyleSheet>>> WorldToUserStyleSheetMap;
    WorldToUserStyleSheetMap m_userStyleSheets;

#if ENABLE(USER_MESSAGE_HANDLERS)
    typedef HashMap<RefPtr<InjectedBundleScriptWorld>, Vector<std::pair<uint64_t, RefPtr<WebUserMessageHandlerDescriptorProxy>>>> WorldToUserMessageHandlerVectorMap;
    WorldToUserMessageHandlerVectorMap m_userMessageHandlers;
#endif
#if ENABLE(CONTENT_EXTENSIONS)
    WebCore::ContentExtensions::ContentExtensionsBackend m_contentExtensionBackend;
#endif
};

} // namespace WebKit
