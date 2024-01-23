/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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

#include "APIHTTPCookieStore.h"
#include "APIObject.h"
#include "APIPageConfiguration.h"
#include "MessageReceiver.h"
#include "WebExtensionContext.h"
#include "WebExtensionContextIdentifier.h"
#include "WebExtensionControllerConfiguration.h"
#include "WebExtensionControllerIdentifier.h"
#include "WebExtensionFrameIdentifier.h"
#include "WebExtensionURLSchemeHandler.h"
#include "WebProcessProxy.h"
#include "WebUserContentControllerProxy.h"
#include <WebCore/Timer.h>
#include <wtf/Forward.h>
#include <wtf/URLHash.h>
#include <wtf/WeakHashSet.h>

OBJC_CLASS NSError;
OBJC_CLASS NSMenu;
OBJC_PROTOCOL(_WKWebExtensionControllerDelegatePrivate);

#ifdef __OBJC__
#import "_WKWebExtensionController.h"
#endif

namespace WebKit {

class ContextMenuContextData;
class WebExtensionContext;
class WebPageProxy;
class WebProcessPool;
class WebsiteDataStore;
struct WebExtensionControllerParameters;

class WebExtensionController : public API::ObjectImpl<API::Object::Type::WebExtensionController>, public IPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(WebExtensionController);

public:
    static Ref<WebExtensionController> create(Ref<WebExtensionControllerConfiguration> configuration) { return adoptRef(*new WebExtensionController(configuration)); }
    static WebExtensionController* get(WebExtensionControllerIdentifier);

    explicit WebExtensionController(Ref<WebExtensionControllerConfiguration>);
    ~WebExtensionController();

    using WebExtensionContextSet = HashSet<Ref<WebExtensionContext>>;
    using WebExtensionSet = HashSet<Ref<WebExtension>>;
    using WebExtensionContextBaseURLMap = HashMap<String, Ref<WebExtensionContext>>;
    using WebExtensionURLSchemeHandlerMap = HashMap<String, Ref<WebExtensionURLSchemeHandler>>;

    using WebProcessProxySet = WeakHashSet<WebProcessProxy>;
    using WebProcessPoolSet = WeakHashSet<WebProcessPool>;
    using WebPageProxySet = WeakHashSet<WebPageProxy>;
    using UserContentControllerProxySet = WeakHashSet<WebUserContentControllerProxy>;
    using WebsiteDataStoreSet = WeakHashSet<WebsiteDataStore>;

    enum class ForPrivateBrowsing { No, Yes };

    WebExtensionControllerConfiguration& configuration() const { return m_configuration.get(); }
    WebExtensionControllerIdentifier identifier() const { return m_identifier; }
    WebExtensionControllerParameters parameters() const;

    bool operator==(const WebExtensionController& other) const { return (this == &other); }

    bool storageIsPersistent() const { return m_configuration->storageIsPersistent(); }
    String storageDirectory(WebExtensionContext&) const;

    bool hasLoadedContexts() const { return !m_extensionContexts.isEmpty(); }
    bool isFreshlyCreated() const { return m_freshlyCreated; }

    bool load(WebExtensionContext&, NSError ** = nullptr);
    bool unload(WebExtensionContext&, NSError ** = nullptr);

    void unloadAll();

    void addPage(WebPageProxy&);
    void removePage(WebPageProxy&);

    const WebPageProxySet& allPages() const { return m_pages; }

    const WebsiteDataStoreSet& allWebsiteDataStores() const { return m_websiteDataStores; }
    WebsiteDataStore* websiteDataStore(std::optional<PAL::SessionID> = std::nullopt) const;

    // Includes both non-private and private browsing content controllers.
    const UserContentControllerProxySet& allUserContentControllers() const { return m_allUserContentControllers; }
    const UserContentControllerProxySet& allNonPrivateUserContentControllers() const { return m_allNonPrivateUserContentControllers; }
    const UserContentControllerProxySet& allPrivateUserContentControllers() const { return m_allPrivateUserContentControllers; }

    const WebProcessPoolSet& allProcessPools() const { return m_processPools; }
    WebProcessProxySet allProcesses() const;

    RefPtr<WebExtensionContext> extensionContext(const WebExtension&) const;
    RefPtr<WebExtensionContext> extensionContext(const URL&) const;

    const WebExtensionContextSet& extensionContexts() const { return m_extensionContexts; }
    WebExtensionSet extensions() const;

    void cookiesDidChange(API::HTTPCookieStore&);

    template<typename T>
    void sendToAllProcesses(const T& message, const ObjectIdentifierGenericBase& destinationID);

#if PLATFORM(MAC)
    void addItemsToContextMenu(WebPageProxy&, const ContextMenuContextData&, NSMenu *);
#endif

    void handleContentRuleListNotification(WebPageProxyIdentifier, URL&, WebCore::ContentRuleListResults&);

    // webRequest support
    void resourceLoadDidSendRequest(WebPageProxyIdentifier, const ResourceLoadInfo&, const WebCore::ResourceRequest&);
    void resourceLoadDidPerformHTTPRedirection(WebPageProxyIdentifier, const ResourceLoadInfo&, const WebCore::ResourceResponse&, const WebCore::ResourceRequest&);
    void resourceLoadDidReceiveChallenge(WebPageProxyIdentifier, const ResourceLoadInfo&, const WebCore::AuthenticationChallenge&);
    void resourceLoadDidReceiveResponse(WebPageProxyIdentifier, const ResourceLoadInfo&, const WebCore::ResourceResponse&);
    void resourceLoadDidCompleteWithError(WebPageProxyIdentifier, const ResourceLoadInfo&, const WebCore::ResourceResponse&, const WebCore::ResourceError&);

#ifdef __OBJC__
    _WKWebExtensionController *wrapper() const { return (_WKWebExtensionController *)API::ObjectImpl<API::Object::Type::WebExtensionController>::wrapper(); }
    _WKWebExtensionControllerDelegatePrivate *delegate() const { return (_WKWebExtensionControllerDelegatePrivate *)wrapper().delegate; }
#endif

private:
    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void addProcessPool(WebProcessPool&);
    void removeProcessPool(WebProcessPool&);

    void addUserContentController(WebUserContentControllerProxy&, ForPrivateBrowsing);
    void removeUserContentController(WebUserContentControllerProxy&);

    void addWebsiteDataStore(WebsiteDataStore&);
    void removeWebsiteDataStore(WebsiteDataStore&);

    void didStartProvisionalLoadForFrame(WebPageProxyIdentifier, WebExtensionFrameIdentifier, WebExtensionFrameIdentifier parentFrameID, const URL&, WallTime);
    void didCommitLoadForFrame(WebPageProxyIdentifier, WebExtensionFrameIdentifier, WebExtensionFrameIdentifier parentFrameID, const URL&, WallTime);
    void didFinishLoadForFrame(WebPageProxyIdentifier, WebExtensionFrameIdentifier, WebExtensionFrameIdentifier parentFrameID, const URL&, WallTime);
    void didFailLoadForFrame(WebPageProxyIdentifier, WebExtensionFrameIdentifier, WebExtensionFrameIdentifier parentFrameID, const URL&, WallTime);

    void purgeOldMatchedRules();

    class HTTPCookieStoreObserver : public API::HTTPCookieStore::Observer {
        WTF_MAKE_FAST_ALLOCATED;

    public:
        explicit HTTPCookieStoreObserver(WebExtensionController& extensionController)
            : m_extensionController(extensionController)
        {
        }

    private:
        void cookiesDidChange(API::HTTPCookieStore& cookieStore) final
        {
            // FIXME: <https://webkit.org/b/267514> Add support for changeInfo.

            if (RefPtr extensionController = m_extensionController.get(); extensionController)
                extensionController->cookiesDidChange(cookieStore);
        }

        WeakPtr<WebExtensionController> m_extensionController;
    };

    Ref<WebExtensionControllerConfiguration> m_configuration;
    WebExtensionControllerIdentifier m_identifier;

    WebExtensionContextSet m_extensionContexts;
    WebExtensionContextBaseURLMap m_extensionContextBaseURLMap;
    WebPageProxySet m_pages;
    WebProcessPoolSet m_processPools;
    WebsiteDataStoreSet m_websiteDataStores;
    UserContentControllerProxySet m_allUserContentControllers;
    UserContentControllerProxySet m_allNonPrivateUserContentControllers;
    UserContentControllerProxySet m_allPrivateUserContentControllers;
    WebExtensionURLSchemeHandlerMap m_registeredSchemeHandlers;
    bool m_freshlyCreated { true };

    std::unique_ptr<WebCore::Timer> m_purgeOldMatchedRulesTimer;
    std::unique_ptr<HTTPCookieStoreObserver> m_cookieStoreObserver;
};

template<typename T>
void WebExtensionController::sendToAllProcesses(const T& message, const ObjectIdentifierGenericBase& destinationID)
{
    for (auto& process : allProcesses()) {
        if (process.canSendMessage())
            process.send(T(message), destinationID.toUInt64());
    }
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
