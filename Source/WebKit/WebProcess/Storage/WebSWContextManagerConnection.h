/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(SERVICE_WORKER)

#include "Connection.h"
#include "IdentifierTypes.h"
#include "MessageReceiver.h"
#include "NavigatingToAppBoundDomain.h"
#include "ShareableResource.h"
#include "UserContentControllerIdentifier.h"
#include "WebPageProxyIdentifier.h"
#include "WebSWContextManagerConnectionMessagesReplies.h"
#include <WebCore/EmptyFrameLoaderClient.h>
#include <WebCore/SWContextManager.h>
#include <WebCore/ServiceWorkerClientData.h>
#include <WebCore/ServiceWorkerTypes.h>
#include <wtf/URLHash.h>

namespace IPC {
class FormDataReference;
}

namespace WebCore {
struct FetchOptions;
class ResourceRequest;
struct ServiceWorkerContextData;
}

namespace WebKit {

class ServiceWorkerFrameLoaderClient;
struct ServiceWorkerInitializationData;
struct WebPreferencesStore;
class WebUserContentController;

class WebSWContextManagerConnection final : public WebCore::SWContextManager::Connection, public IPC::MessageReceiver {
public:
    WebSWContextManagerConnection(Ref<IPC::Connection>&&, WebCore::RegistrableDomain&&, PageGroupIdentifier, WebPageProxyIdentifier, WebCore::PageIdentifier, const WebPreferencesStore&, ServiceWorkerInitializationData&&);
    ~WebSWContextManagerConnection();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

private:
    void updatePreferencesStore(const WebPreferencesStore&);

    // WebCore::SWContextManager::Connection.
    void establishConnection(CompletionHandler<void()>&&) final;
    void postMessageToServiceWorkerClient(const WebCore::ServiceWorkerClientIdentifier& destinationIdentifier, const WebCore::MessageWithMessagePorts&, WebCore::ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin) final;
    void didFinishInstall(std::optional<WebCore::ServiceWorkerJobDataIdentifier>, WebCore::ServiceWorkerIdentifier, bool wasSuccessful) final;
    void didFinishActivation(WebCore::ServiceWorkerIdentifier) final;
    void setServiceWorkerHasPendingEvents(WebCore::ServiceWorkerIdentifier, bool) final;
    void workerTerminated(WebCore::ServiceWorkerIdentifier) final;
    void findClientByIdentifier(WebCore::ServiceWorkerIdentifier, WebCore::ServiceWorkerClientIdentifier, FindClientByIdentifierCallback&&) final;
    void matchAll(WebCore::ServiceWorkerIdentifier, const WebCore::ServiceWorkerClientQueryOptions&, WebCore::ServiceWorkerClientsMatchAllCallback&&) final;
    void claim(WebCore::ServiceWorkerIdentifier, CompletionHandler<void(WebCore::ExceptionOr<void>&&)>&&) final;
    void skipWaiting(WebCore::ServiceWorkerIdentifier, CompletionHandler<void()>&&) final;
    void setScriptResource(WebCore::ServiceWorkerIdentifier, const URL&, const WebCore::ServiceWorkerContextData::ImportedScript&) final;
    bool isThrottleable() const final;
    void didFailHeartBeatCheck(WebCore::ServiceWorkerIdentifier) final;

    // IPC messages.
    void serviceWorkerStarted(std::optional<WebCore::ServiceWorkerJobDataIdentifier>, WebCore::ServiceWorkerIdentifier, bool doesHandleFetch) final;
    void serviceWorkerFailedToStart(std::optional<WebCore::ServiceWorkerJobDataIdentifier>, WebCore::ServiceWorkerIdentifier, const String& exceptionMessage) final;
    void installServiceWorker(WebCore::ServiceWorkerContextData&&, String&& userAgent);
    void updateAppInitiatedValue(WebCore::ServiceWorkerIdentifier, WebCore::LastNavigationWasAppInitiated);
    void startFetch(WebCore::SWServerConnectionIdentifier, WebCore::ServiceWorkerIdentifier, WebCore::FetchIdentifier, WebCore::ResourceRequest&&, WebCore::FetchOptions&&, IPC::FormDataReference&&, String&& referrer);
    void cancelFetch(WebCore::SWServerConnectionIdentifier, WebCore::ServiceWorkerIdentifier, WebCore::FetchIdentifier);
    void continueDidReceiveFetchResponse(WebCore::SWServerConnectionIdentifier, WebCore::ServiceWorkerIdentifier, WebCore::FetchIdentifier);
    void postMessageToServiceWorker(WebCore::ServiceWorkerIdentifier destinationIdentifier, WebCore::MessageWithMessagePorts&&, WebCore::ServiceWorkerOrClientData&& sourceData);
    void fireInstallEvent(WebCore::ServiceWorkerIdentifier);
    void fireActivateEvent(WebCore::ServiceWorkerIdentifier);
    void terminateWorker(WebCore::ServiceWorkerIdentifier);
#if ENABLE(SHAREABLE_RESOURCE) && PLATFORM(COCOA)
    void didSaveScriptsToDisk(WebCore::ServiceWorkerIdentifier, WebCore::ScriptBuffer&&, HashMap<URL, WebCore::ScriptBuffer>&& importedScripts);
#endif
    void findClientByIdentifierCompleted(uint64_t requestIdentifier, std::optional<WebCore::ServiceWorkerClientData>&&, bool hasSecurityError);
    void matchAllCompleted(uint64_t matchAllRequestIdentifier, Vector<WebCore::ServiceWorkerClientData>&&);
    void setUserAgent(String&& userAgent);
    void close();
    void setThrottleState(bool isThrottleable);

    Ref<IPC::Connection> m_connectionToNetworkProcess;
    WebCore::RegistrableDomain m_registrableDomain;
    PageGroupIdentifier m_pageGroupID;
    WebPageProxyIdentifier m_webPageProxyID;
    WebCore::PageIdentifier m_pageID;

    WebCore::StorageBlockingPolicy m_storageBlockingPolicy { WebCore::StorageBlockingPolicy::AllowAll };

    HashSet<std::unique_ptr<ServiceWorkerFrameLoaderClient>> m_loaders;
    HashMap<uint64_t, FindClientByIdentifierCallback> m_findClientByIdentifierRequests;
    HashMap<uint64_t, WebCore::ServiceWorkerClientsMatchAllCallback> m_matchAllRequests;
    uint64_t m_previousRequestIdentifier { 0 };
    String m_userAgent;
    bool m_isThrottleable { true };
    Ref<WebUserContentController> m_userContentController;
};

class ServiceWorkerFrameLoaderClient final : public WebCore::EmptyFrameLoaderClient {
public:
    ServiceWorkerFrameLoaderClient(WebPageProxyIdentifier, WebCore::PageIdentifier, WebCore::FrameIdentifier, const String& userAgent);

    WebPageProxyIdentifier webPageProxyID() const { return m_webPageProxyID; }

    void setUserAgent(String&& userAgent) { m_userAgent = WTFMove(userAgent); }

private:
    Ref<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&, const WebCore::SubstituteData&) final;

    std::optional<WebCore::PageIdentifier> pageID() const final { return m_pageID; }
    std::optional<WebCore::FrameIdentifier> frameID() const final { return m_frameID; }

    bool shouldUseCredentialStorage(WebCore::DocumentLoader*, unsigned long) final { return true; }
    bool isServiceWorkerFrameLoaderClient() const final { return true; }

    String userAgent(const URL&) const final { return m_userAgent; }

    WebPageProxyIdentifier m_webPageProxyID;
    WebCore::PageIdentifier m_pageID;
    WebCore::FrameIdentifier m_frameID;
    String m_userAgent;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::ServiceWorkerFrameLoaderClient)
    static bool isType(const WebCore::FrameLoaderClient& frameLoaderClient) { return frameLoaderClient.isServiceWorkerFrameLoaderClient(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(SERVICE_WORKER)
