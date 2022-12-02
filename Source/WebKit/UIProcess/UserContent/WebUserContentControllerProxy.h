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

#include "APIObject.h"
#include "ContentWorldShared.h"
#include "DataReference.h"
#include "MessageReceiver.h"
#include "UserContentControllerIdentifier.h"
#include "WebPageProxyIdentifier.h"
#include "WebUserContentControllerProxyMessages.h"
#include <WebCore/PageIdentifier.h>
#include <wtf/Forward.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/URL.h>
#include <wtf/URLHash.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/StringHash.h>

namespace API {
class Array;
class ContentRuleList;
class ContentWorld;
class UserScript;
class UserStyleSheet;
}

namespace WebCore {
struct SecurityOriginData;
}

namespace WebKit {

class NetworkProcessProxy;
class WebProcessProxy;
class WebScriptMessageHandler;
struct FrameInfoData;
class WebCompiledContentRuleListData;
struct WebPageCreationParameters;
struct UserContentControllerParameters;
enum class InjectUserScriptImmediately : bool;

class WebUserContentControllerProxy : public API::ObjectImpl<API::Object::Type::UserContentController>, public IPC::MessageReceiver {
public:
    static Ref<WebUserContentControllerProxy> create()
    { 
        return adoptRef(*new WebUserContentControllerProxy);
    }
    WebUserContentControllerProxy();
    ~WebUserContentControllerProxy();

    static WebUserContentControllerProxy* get(UserContentControllerIdentifier);

    UserContentControllerParameters parameters() const;

    void addProcess(WebProcessProxy&);
    void removeProcess(WebProcessProxy&);

    API::Array& userScripts() { return m_userScripts.get(); }
    void addUserScript(API::UserScript&, InjectUserScriptImmediately);
    void removeUserScript(API::UserScript&);
    void removeAllUserScripts(API::ContentWorld&);
    void removeAllUserScripts();

    API::Array& userStyleSheets() { return m_userStyleSheets.get(); }
    void addUserStyleSheet(API::UserStyleSheet&);
    void removeUserStyleSheet(API::UserStyleSheet&);
    void removeAllUserStyleSheets(API::ContentWorld&);
    void removeAllUserStyleSheets();

    // Returns false if there was a name conflict.
    bool addUserScriptMessageHandler(WebScriptMessageHandler&);
    void removeUserMessageHandlerForName(const String&, API::ContentWorld&);
    void removeAllUserMessageHandlers(API::ContentWorld&);
    void removeAllUserMessageHandlers();

#if ENABLE(CONTENT_EXTENSIONS)
    void addNetworkProcess(NetworkProcessProxy&);
    void removeNetworkProcess(NetworkProcessProxy&);

    void addContentRuleList(API::ContentRuleList&, const WTF::URL& extensionBaseURL = { });
    void removeContentRuleList(const String&);
    void removeAllContentRuleLists();
    const HashMap<String, std::pair<Ref<API::ContentRuleList>, URL>>& contentExtensionRules() { return m_contentRuleLists; }
    Vector<std::pair<WebCompiledContentRuleListData, URL>> contentRuleListData() const;
#endif

    UserContentControllerIdentifier identifier() const { return m_identifier; }

    void contentWorldDestroyed(API::ContentWorld&);

    bool operator==(const WebUserContentControllerProxy& other) const { return (this == &other); }
    bool operator!=(const WebUserContentControllerProxy& other) const { return !(this == &other); }

private:
    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void didPostMessage(WebPageProxyIdentifier, FrameInfoData&&, uint64_t messageHandlerID, const IPC::DataReference&, CompletionHandler<void(IPC::DataReference&&, const String&)>&&);

    void addContentWorld(API::ContentWorld&);

    UserContentControllerIdentifier m_identifier;
    WeakHashSet<WebProcessProxy> m_processes;
    Ref<API::Array> m_userScripts;
    Ref<API::Array> m_userStyleSheets;
    HashMap<uint64_t, RefPtr<WebScriptMessageHandler>> m_scriptMessageHandlers;
    HashSet<ContentWorldIdentifier> m_associatedContentWorlds;

#if ENABLE(CONTENT_EXTENSIONS)
    WeakHashSet<NetworkProcessProxy> m_networkProcesses;
    HashMap<String, std::pair<Ref<API::ContentRuleList>, URL>> m_contentRuleLists;
#endif
};

} // namespace WebKit
