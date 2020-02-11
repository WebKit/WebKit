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
#include "MessageReceiver.h"
#include "UserContentControllerIdentifier.h"
#include "WebPageProxyIdentifier.h"
#include "WebSWContextManagerConnectionMessagesReplies.h"
#include <WebCore/EmptyFrameLoaderClient.h>
#include <WebCore/SWContextManager.h>
#include <WebCore/ServiceWorkerClientData.h>
#include <WebCore/ServiceWorkerTypes.h>

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
    WebSWContextManagerConnection(Ref<IPC::Connection>&&, WebCore::RegistrableDomain&&, uint64_t pageGroupID, WebPageProxyIdentifier, WebCore::PageIdentifier, const WebPreferencesStore&, ServiceWorkerInitializationData&&);
    ~WebSWContextManagerConnection();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) final;

    void removeFrameLoaderClient(ServiceWorkerFrameLoaderClient&);

private:
    void updatePreferencesStore(const WebPreferencesStore&);

    // WebCore::SWContextManager::Connection.
    void establishConnection(CompletionHandler<void()>&&) final;
    void postMessageToServiceWorkerClient(const WebCore::ServiceWorkerClientIdentifier& destinationIdentifier, const WebCore::MessageWithMessagePorts&, WebCore::ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin) final;
    void didFinishInstall(Optional<WebCore::ServiceWorkerJobDataIdentifier>, WebCore::ServiceWorkerIdentifier, bool wasSuccessful) final;
    void didFinishActivation(WebCore::ServiceWorkerIdentifier) final;
    void setServiceWorkerHasPendingEvents(WebCore::ServiceWorkerIdentifier, bool) final;
    void workerTerminated(WebCore::ServiceWorkerIdentifier) final;
    void findClientByIdentifier(WebCore::ServiceWorkerIdentifier, WebCore::ServiceWorkerClientIdentifier, FindClientByIdentifierCallback&&) final;
    void matchAll(WebCore::ServiceWorkerIdentifier, const WebCore::ServiceWorkerClientQueryOptions&, WebCore::ServiceWorkerClientsMatchAllCallback&&) final;
    void claim(WebCore::ServiceWorkerIdentifier, CompletionHandler<void()>&&) final;
    void skipWaiting(WebCore::ServiceWorkerIdentifier, CompletionHandler<void()>&&) final;
    void setScriptResource(WebCore::ServiceWorkerIdentifier, const URL&, const WebCore::ServiceWorkerContextData::ImportedScript&) final;
    bool isThrottleable() const final;
    void didFailHeartBeatCheck(WebCore::ServiceWorkerIdentifier) final;

    // IPC messages.
    void serviceWorkerStarted(Optional<WebCore::ServiceWorkerJobDataIdentifier>, WebCore::ServiceWorkerIdentifier, bool doesHandleFetch) final;
    void serviceWorkerFailedToStart(Optional<WebCore::ServiceWorkerJobDataIdentifier>, WebCore::ServiceWorkerIdentifier, const String& exceptionMessage) final;
    void installServiceWorker(const WebCore::ServiceWorkerContextData&, String&& userAgent);
    void startFetch(WebCore::SWServerConnectionIdentifier, WebCore::ServiceWorkerIdentifier, WebCore::FetchIdentifier, WebCore::ResourceRequest&&, WebCore::FetchOptions&&, IPC::FormDataReference&&, String&& referrer);
    void cancelFetch(WebCore::SWServerConnectionIdentifier, WebCore::ServiceWorkerIdentifier, WebCore::FetchIdentifier);
    void continueDidReceiveFetchResponse(WebCore::SWServerConnectionIdentifier, WebCore::ServiceWorkerIdentifier, WebCore::FetchIdentifier);
    void postMessageToServiceWorker(WebCore::ServiceWorkerIdentifier destinationIdentifier, WebCore::MessageWithMessagePorts&&, WebCore::ServiceWorkerOrClientData&& sourceData);
    void fireInstallEvent(WebCore::ServiceWorkerIdentifier);
    void fireActivateEvent(WebCore::ServiceWorkerIdentifier);
    void terminateWorker(WebCore::ServiceWorkerIdentifier);
    void syncTerminateWorker(WebCore::ServiceWorkerIdentifier, Messages::WebSWContextManagerConnection::SyncTerminateWorkerDelayedReply&&);
    void findClientByIdentifierCompleted(uint64_t requestIdentifier, Optional<WebCore::ServiceWorkerClientData>&&, bool hasSecurityError);
    void matchAllCompleted(uint64_t matchAllRequestIdentifier, Vector<WebCore::ServiceWorkerClientData>&&);
    void claimCompleted(uint64_t claimRequestIdentifier);
    void setUserAgent(String&& userAgent);
    void close();
    void setThrottleState(bool isThrottleable);

    Ref<IPC::Connection> m_connectionToNetworkProcess;
    WebCore::RegistrableDomain m_registrableDomain;
    uint64_t m_pageGroupID;
    WebPageProxyIdentifier m_webPageProxyID;
    WebCore::PageIdentifier m_pageID;
    uint64_t m_previousServiceWorkerID { 0 };

    WebCore::SecurityOrigin::StorageBlockingPolicy m_storageBlockingPolicy { WebCore::SecurityOrigin::StorageBlockingPolicy::AllowAllStorage };

    HashSet<std::unique_ptr<ServiceWorkerFrameLoaderClient>> m_loaders;
    HashMap<uint64_t, FindClientByIdentifierCallback> m_findClientByIdentifierRequests;
    HashMap<uint64_t, WebCore::ServiceWorkerClientsMatchAllCallback> m_matchAllRequests;
    HashMap<uint64_t, WTF::CompletionHandler<void()>> m_claimRequests;
    uint64_t m_previousRequestIdentifier { 0 };
    String m_userAgent;
    bool m_isThrottleable { true };
    RefPtr<WebUserContentController> m_userContentController;
};

class ServiceWorkerFrameLoaderClient final : public WebCore::EmptyFrameLoaderClient {
public:
    ServiceWorkerFrameLoaderClient(WebSWContextManagerConnection&, WebPageProxyIdentifier, WebCore::PageIdentifier, WebCore::FrameIdentifier, const String& userAgent);

    void setUserAgent(String&& userAgent) { m_userAgent = WTFMove(userAgent); }
    
    WebPageProxyIdentifier webPageProxyID() const { return m_webPageProxyID; }
    Optional<WebCore::PageIdentifier> pageID() const final { return m_pageID; }
    Optional<WebCore::FrameIdentifier> frameID() const final { return m_frameID; }

private:
    Ref<WebCore::DocumentLoader> createDocumentLoader(const WebCore::ResourceRequest&, const WebCore::SubstituteData&) final;

    void frameLoaderDestroyed() final { m_connection.removeFrameLoaderClient(*this); }

    bool shouldUseCredentialStorage(WebCore::DocumentLoader*, unsigned long) final { return true; }
    bool isServiceWorkerFrameLoaderClient() const final { return true; }

    String userAgent(const URL&) final { return m_userAgent; }

    WebSWContextManagerConnection& m_connection;
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
