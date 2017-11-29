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
#include "MessageSender.h"
#include "ServiceWorkerClientFetch.h"
#include "SharedMemory.h"
#include <WebCore/SWClientConnection.h>
#include <pal/SessionID.h>
#include <wtf/UniqueRef.h>

namespace WebCore {
struct ExceptionData;
class ResourceLoader;
}

namespace WebKit {

class WebSWOriginTable;
class WebServiceWorkerProvider;

class WebSWClientConnection : public WebCore::SWClientConnection, public IPC::MessageSender, public IPC::MessageReceiver {
public:
    WebSWClientConnection(IPC::Connection&, PAL::SessionID);
    WebSWClientConnection(const WebSWClientConnection&) = delete;
    ~WebSWClientConnection() final;

    WebCore::SWServerConnectionIdentifier serverConnectionIdentifier() const final { return m_identifier; }

    void addServiceWorkerRegistrationInServer(WebCore::ServiceWorkerRegistrationIdentifier) final;
    void removeServiceWorkerRegistrationInServer(WebCore::ServiceWorkerRegistrationIdentifier) final;

    void disconnectedFromWebProcess();
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    bool mayHaveServiceWorkerRegisteredForOrigin(const WebCore::SecurityOrigin&) const final;
    Ref<ServiceWorkerClientFetch> startFetch(WebServiceWorkerProvider&, Ref<WebCore::ResourceLoader>&&, uint64_t identifier, ServiceWorkerClientFetch::Callback&&);

    void postMessageToServiceWorkerClient(WebCore::DocumentIdentifier destinationContextIdentifier, const IPC::DataReference& message, WebCore::ServiceWorkerData&& source, const String& sourceOrigin);

private:
    void scheduleJobInServer(const WebCore::ServiceWorkerJobData&) final;
    void finishFetchingScriptInServer(const WebCore::ServiceWorkerFetchResult&) final;
    void postMessageToServiceWorkerGlobalScope(WebCore::ServiceWorkerIdentifier destinationIdentifier, Ref<WebCore::SerializedScriptValue>&&, WebCore::DocumentIdentifier sourceContextIdentifier, WebCore::ServiceWorkerClientData&& source) final;
    void serviceWorkerStartedControllingClient(WebCore::ServiceWorkerIdentifier, WebCore::ServiceWorkerRegistrationIdentifier, WebCore::DocumentIdentifier) final;
    void serviceWorkerStoppedControllingClient(WebCore::ServiceWorkerIdentifier, WebCore::ServiceWorkerRegistrationIdentifier, WebCore::DocumentIdentifier) final;
    void registerServiceWorkerClient(const WebCore::SecurityOrigin& topOrigin, WebCore::DocumentIdentifier, const WebCore::ServiceWorkerClientData&) final;
    void unregisterServiceWorkerClient(WebCore::DocumentIdentifier) final;

    void matchRegistration(const WebCore::SecurityOrigin& topOrigin, const WebCore::URL& clientURL, RegistrationCallback&&) final;
    void didMatchRegistration(uint64_t matchRequestIdentifier, std::optional<WebCore::ServiceWorkerRegistrationData>&&);
    void didGetRegistrations(uint64_t matchRequestIdentifier, Vector<WebCore::ServiceWorkerRegistrationData>&&);

    void getRegistrations(const WebCore::SecurityOrigin& topOrigin, const WebCore::URL& clientURL, GetRegistrationsCallback&&) final;

    void didResolveRegistrationPromise(const WebCore::ServiceWorkerRegistrationKey&) final;

    void scheduleStorageJob(const WebCore::ServiceWorkerJobData&);

    IPC::Connection* messageSenderConnection() final { return m_connection.ptr(); }
    uint64_t messageSenderDestinationID() final { return m_identifier.toUInt64(); }

    void setSWOriginTableSharedMemory(const SharedMemory::Handle&);
    void initializeSWOriginTableAsEmpty();

    PAL::SessionID m_sessionID;
    WebCore::SWServerConnectionIdentifier m_identifier;

    Ref<IPC::Connection> m_connection;
    UniqueRef<WebSWOriginTable> m_swOriginTable;

    uint64_t m_previousMatchRegistrationTaskIdentifier { 0 };
    HashMap<uint64_t, RegistrationCallback> m_ongoingMatchRegistrationTasks;

    uint64_t m_previousGetRegistrationsTaskIdentifier { 0 };
    HashMap<uint64_t, GetRegistrationsCallback> m_ongoingGetRegistrationsTasks;

}; // class WebSWServerConnection

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
