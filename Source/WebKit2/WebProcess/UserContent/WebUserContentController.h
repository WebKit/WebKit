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

#ifndef WebUserContentController_h
#define WebUserContentController_h

#include "MessageReceiver.h"
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
class WebCompiledContentExtensionData;
class WebUserMessageHandlerDescriptorProxy;

class WebUserContentController final : public WebCore::UserContentProvider, private IPC::MessageReceiver {
public:
    static Ref<WebUserContentController> getOrCreate(uint64_t identifier);
    virtual ~WebUserContentController();

    uint64_t identifier() { return m_identifier; } 

    void addUserScript(InjectedBundleScriptWorld&, WebCore::UserScript&&);
    void removeUserScriptWithURL(InjectedBundleScriptWorld&, const WebCore::URL&);
    void removeUserScripts(InjectedBundleScriptWorld&);
    void addUserStyleSheet(InjectedBundleScriptWorld&, WebCore::UserStyleSheet&&);
    void removeUserStyleSheetWithURL(InjectedBundleScriptWorld&, const WebCore::URL&);
    void removeUserStyleSheets(InjectedBundleScriptWorld&);
    void removeAllUserContent();

private:
    explicit WebUserContentController(uint64_t identifier);

    // WebCore::UserContentProvider
    void forEachUserScript(const std::function<void(WebCore::DOMWrapperWorld&, const WebCore::UserScript&)>&) const override;
    void forEachUserStyleSheet(const std::function<void(const WebCore::UserStyleSheet&)>&) const override;
#if ENABLE(USER_MESSAGE_HANDLERS)
    void forEachUserMessageHandler(const std::function<void(const WebCore::UserMessageHandlerDescriptor&)>&) const override;
#endif
#if ENABLE(CONTENT_EXTENSIONS)
    WebCore::ContentExtensions::ContentExtensionsBackend& userContentExtensionBackend() override { return m_contentExtensionBackend; }
#endif

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;

    void addUserContentWorlds(const Vector<std::pair<uint64_t, String>>&);
    void removeUserContentWorlds(const Vector<uint64_t>&);

    void addUserScripts(const Vector<WebUserScriptData>&);
    void removeUserScript(uint64_t worldIdentifier, uint64_t userScriptIdentifier);
    void removeAllUserScripts(const Vector<uint64_t>&);

    void addUserStyleSheets(const Vector<WebUserStyleSheetData>&);
    void removeUserStyleSheet(uint64_t worldIdentifier, uint64_t userScriptIdentifier);
    void removeAllUserStyleSheets(const Vector<uint64_t>&);

    void addUserScriptMessageHandlers(const Vector<WebScriptMessageHandlerData>&);
    void removeUserScriptMessageHandler(uint64_t worldIdentifier, uint64_t userScriptIdentifier);
    void removeAllUserScriptMessageHandlers(const Vector<uint64_t>&);

#if ENABLE(CONTENT_EXTENSIONS)
    void addUserContentExtensions(const Vector<std::pair<String, WebCompiledContentExtensionData>>&);
    void removeUserContentExtension(const String& name);
    void removeAllUserContentExtensions();
#endif

    void addUserScriptInternal(InjectedBundleScriptWorld&, uint64_t userScriptIdentifier, WebCore::UserScript&&);
    void removeUserScriptInternal(InjectedBundleScriptWorld&, uint64_t userScriptIdentifier);
    void addUserStyleSheetInternal(InjectedBundleScriptWorld&, uint64_t userStyleSheetIdentifier, WebCore::UserStyleSheet&&);
    void removeUserStyleSheetInternal(InjectedBundleScriptWorld&, uint64_t userStyleSheetIdentifier);
#if ENABLE(USER_MESSAGE_HANDLERS)
    void addUserScriptMessageHandlerInternal(InjectedBundleScriptWorld&, uint64_t userScriptMessageHandlerIdentifier, const String& name);
    void removeUserScriptMessageHandlerInternal(InjectedBundleScriptWorld&, uint64_t userScriptMessageHandlerIdentifier);
#endif

    uint64_t m_identifier;

    typedef HashMap<RefPtr<InjectedBundleScriptWorld>, Vector<std::pair<uint64_t, WebCore::UserScript>>> WorldToUserScriptMap;
    WorldToUserScriptMap m_userScripts;

    typedef HashMap<RefPtr<InjectedBundleScriptWorld>, Vector<std::pair<uint64_t, WebCore::UserStyleSheet>>> WorldToUserStyleSheetMap;
    WorldToUserStyleSheetMap m_userStyleSheets;

#if ENABLE(USER_MESSAGE_HANDLERS)
    typedef HashMap<RefPtr<InjectedBundleScriptWorld>, Vector<RefPtr<WebUserMessageHandlerDescriptorProxy>>> WorldToUserMessageHandlerVectorMap;
    WorldToUserMessageHandlerVectorMap m_userMessageHandlers;
#endif
#if ENABLE(CONTENT_EXTENSIONS)
    WebCore::ContentExtensions::ContentExtensionsBackend m_contentExtensionBackend;
#endif

};

} // namespace WebKit

#endif // WebUserContentController_h
