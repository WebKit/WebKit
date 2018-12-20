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
#include "WebSWContextManagerConnectionMessages.h"
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
struct WebPreferencesStore;

class WebSWContextManagerConnection final : public WebCore::SWContextManager::Connection, public IPC::MessageReceiver {
public:
    WebSWContextManagerConnection(Ref<IPC::Connection>&&, uint64_t pageGroupID, uint64_t pageID, const WebPreferencesStore&);
    ~WebSWContextManagerConnection();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) final;

    void removeFrameLoaderClient(ServiceWorkerFrameLoaderClient&);

private:
    void updatePreferencesStore(const WebPreferencesStore&);

    // WebCore::SWContextManager::Connection.
    void postMessageToServiceWorkerClient(const WebCore::ServiceWorkerClientIdentifier& destinationIdentifier, WebCore::MessageWithMessagePorts&&, WebCore::ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin) final;
    void didFinishInstall(Optional<WebCore::ServiceWorkerJobDataIdentifier>, WebCore::ServiceWorkerIdentifier, bool wasSuccessful) final;
    void didFinishActivation(WebCore::ServiceWorkerIdentifier) final;
    void setServiceWorkerHasPendingEvents(WebCore::ServiceWorkerIdentifier, bool) final;
    void workerTerminated(WebCore::ServiceWorkerIdentifier) final;
    void findClientByIdentifier(WebCore::ServiceWorkerIdentifier, WebCore::ServiceWorkerClientIdentifier, FindClientByIdentifierCallback&&) final;
    void matchAll(WebCore::ServiceWorkerIdentifier, const WebCore::ServiceWorkerClientQueryOptions&, WebCore::ServiceWorkerClientsMatchAllCallback&&) final;
    void claim(WebCore::ServiceWorkerIdentifier, CompletionHandler<void()>&&) final;
    void skipWaiting(WebCore::ServiceWorkerIdentifier, Function<void()>&&) final;
    void setScriptResource(WebCore::ServiceWorkerIdentifier, const URL&, const WebCore::ServiceWorkerContextData::ImportedScript&) final;

    // IPC messages.
    void serviceWorkerStartedWithMessage(Optional<WebCore::ServiceWorkerJobDataIdentifier>, WebCore::ServiceWorkerIdentifier, const String& exceptionMessage) final;
    void installServiceWorker(const WebCore::ServiceWorkerContextData&, PAL::SessionID);
    void startFetch(WebCore::SWServerConnectionIdentifier, WebCore::ServiceWorkerIdentifier, WebCore::FetchIdentifier, WebCore::ResourceRequest&&, WebCore::FetchOptions&&, IPC::FormDataReference&&, String&& referrer);
    void cancelFetch(WebCore::SWServerConnectionIdentifier, WebCore::ServiceWorkerIdentifier, WebCore::FetchIdentifier);
    void postMessageToServiceWorker(WebCore::ServiceWorkerIdentifier destinationIdentifier, WebCore::MessageWithMessagePorts&&, WebCore::ServiceWorkerOrClientData&& sourceData);
    void fireInstallEvent(WebCore::ServiceWorkerIdentifier);
    void fireActivateEvent(WebCore::ServiceWorkerIdentifier);
    void terminateWorker(WebCore::ServiceWorkerIdentifier);
    void syncTerminateWorker(WebCore::ServiceWorkerIdentifier, Messages::WebSWContextManagerConnection::SyncTerminateWorker::DelayedReply&&);
    void findClientByIdentifierCompleted(uint64_t requestIdentifier, Optional<WebCore::ServiceWorkerClientData>&&, bool hasSecurityError);
    void matchAllCompleted(uint64_t matchAllRequestIdentifier, Vector<WebCore::ServiceWorkerClientData>&&);
    void claimCompleted(uint64_t claimRequestIdentifier);
    void didFinishSkipWaiting(uint64_t callbackID);
    void setUserAgent(String&& userAgent);
    void terminateProcess();

    Ref<IPC::Connection> m_connectionToNetworkProcess;
    uint64_t m_pageGroupID;
    uint64_t m_pageID { 0 };
    uint64_t m_previousServiceWorkerID { 0 };

    WebCore::SecurityOrigin::StorageBlockingPolicy m_storageBlockingPolicy { WebCore::SecurityOrigin::StorageBlockingPolicy::AllowAllStorage };

    HashSet<std::unique_ptr<ServiceWorkerFrameLoaderClient>> m_loaders;
    HashMap<uint64_t, FindClientByIdentifierCallback> m_findClientByIdentifierRequests;
    HashMap<uint64_t, WebCore::ServiceWorkerClientsMatchAllCallback> m_matchAllRequests;
    HashMap<uint64_t, WTF::CompletionHandler<void()>> m_claimRequests;
    HashMap<uint64_t, WTF::Function<void()>> m_skipWaitingRequests;
    uint64_t m_previousRequestIdentifier { 0 };
    String m_userAgent;
};

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
