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
#include <WebCore/MessageWithMessagePorts.h>
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

class WebSWClientConnection final : public WebCore::SWClientConnection, public IPC::MessageSender, public IPC::MessageReceiver {
public:
    static Ref<WebSWClientConnection> create(IPC::Connection& connection, PAL::SessionID sessionID) { return adoptRef(*new WebSWClientConnection { connection, sessionID }); }
    ~WebSWClientConnection();

    WebCore::SWServerConnectionIdentifier serverConnectionIdentifier() const final { return m_identifier; }

    void addServiceWorkerRegistrationInServer(WebCore::ServiceWorkerRegistrationIdentifier) final;
    void removeServiceWorkerRegistrationInServer(WebCore::ServiceWorkerRegistrationIdentifier) final;

    void disconnectedFromWebProcess();
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    bool mayHaveServiceWorkerRegisteredForOrigin(const WebCore::SecurityOriginData&) const final;
    void startFetch(WebCore::FetchIdentifier, WebCore::ServiceWorkerRegistrationIdentifier, const WebCore::ResourceRequest&, const WebCore::FetchOptions&, const String& referrer);
    void cancelFetch(WebCore::FetchIdentifier, WebCore::ServiceWorkerRegistrationIdentifier);

    void postMessageToServiceWorkerClient(WebCore::DocumentIdentifier destinationContextIdentifier, WebCore::MessageWithMessagePorts&&, WebCore::ServiceWorkerData&& source, const String& sourceOrigin);

    void connectionToServerLost();

    void syncTerminateWorker(WebCore::ServiceWorkerIdentifier) final;

private:
    WebSWClientConnection(IPC::Connection&, PAL::SessionID);

    void scheduleJobInServer(const WebCore::ServiceWorkerJobData&) final;
    void finishFetchingScriptInServer(const WebCore::ServiceWorkerFetchResult&) final;
    void postMessageToServiceWorker(WebCore::ServiceWorkerIdentifier destinationIdentifier, WebCore::MessageWithMessagePorts&&, const WebCore::ServiceWorkerOrClientIdentifier& source) final;
    void registerServiceWorkerClient(const WebCore::SecurityOrigin& topOrigin, const WebCore::ServiceWorkerClientData&, const std::optional<WebCore::ServiceWorkerRegistrationIdentifier>&) final;
    void unregisterServiceWorkerClient(WebCore::DocumentIdentifier) final;

    void matchRegistration(WebCore::SecurityOriginData&& topOrigin, const URL& clientURL, RegistrationCallback&&) final;
    void didMatchRegistration(uint64_t matchRequestIdentifier, std::optional<WebCore::ServiceWorkerRegistrationData>&&);
    void didGetRegistrations(uint64_t matchRequestIdentifier, Vector<WebCore::ServiceWorkerRegistrationData>&&);
    void whenRegistrationReady(const WebCore::SecurityOrigin& topOrigin, const URL& clientURL, WhenRegistrationReadyCallback&&) final;
    void registrationReady(uint64_t callbackID, WebCore::ServiceWorkerRegistrationData&&);

    void getRegistrations(WebCore::SecurityOriginData&& topOrigin, const URL& clientURL, GetRegistrationsCallback&&) final;

    void didResolveRegistrationPromise(const WebCore::ServiceWorkerRegistrationKey&) final;

    void scheduleStorageJob(const WebCore::ServiceWorkerJobData&);

    void runOrDelayTaskForImport(WTF::Function<void()>&& task);

    IPC::Connection* messageSenderConnection() final { return m_connection.ptr(); }
    uint64_t messageSenderDestinationID() final { return m_identifier.toUInt64(); }

    void setSWOriginTableSharedMemory(const SharedMemory::Handle&);
    void setSWOriginTableIsImported();

    PAL::SessionID m_sessionID;
    WebCore::SWServerConnectionIdentifier m_identifier;

    Ref<IPC::Connection> m_connection;
    UniqueRef<WebSWOriginTable> m_swOriginTable;

    uint64_t m_previousCallbackIdentifier { 0 };
    HashMap<uint64_t, RegistrationCallback> m_ongoingMatchRegistrationTasks;
    HashMap<uint64_t, GetRegistrationsCallback> m_ongoingGetRegistrationsTasks;
    HashMap<uint64_t, WhenRegistrationReadyCallback> m_ongoingRegistrationReadyTasks;
    Deque<WTF::Function<void()>> m_tasksPendingOriginImport;

}; // class WebSWServerConnection

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
