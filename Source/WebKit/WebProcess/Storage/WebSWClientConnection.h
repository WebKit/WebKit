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
#include "SharedMemory.h"
#include <WebCore/MessageWithMessagePorts.h>
#include <WebCore/SWClientConnection.h>
#include <wtf/UniqueRef.h>

namespace WebCore {
struct ExceptionData;
class ResourceLoader;
}

namespace WebKit {

class WebSWOriginTable;
class WebServiceWorkerProvider;

class WebSWClientConnection final : public WebCore::SWClientConnection, private IPC::MessageSender, public IPC::MessageReceiver {
public:
    static Ref<WebSWClientConnection> create() { return adoptRef(*new WebSWClientConnection); }
    ~WebSWClientConnection();

    WebCore::SWServerConnectionIdentifier serverConnectionIdentifier() const final { return m_identifier; }

    void addServiceWorkerRegistrationInServer(WebCore::ServiceWorkerRegistrationIdentifier) final;
    void removeServiceWorkerRegistrationInServer(WebCore::ServiceWorkerRegistrationIdentifier) final;

    void disconnectedFromWebProcess();
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    bool mayHaveServiceWorkerRegisteredForOrigin(const WebCore::SecurityOriginData&) const final;

    void connectionToServerLost();

    bool isThrottleable() const { return m_isThrottleable; }
    void updateThrottleState();

    void terminateWorkerForTesting(WebCore::ServiceWorkerIdentifier, CompletionHandler<void()>&&);

private:
    WebSWClientConnection();

    void scheduleJobInServer(const WebCore::ServiceWorkerJobData&) final;
    void finishFetchingScriptInServer(const WebCore::ServiceWorkerFetchResult&) final;
    void postMessageToServiceWorker(WebCore::ServiceWorkerIdentifier destinationIdentifier, WebCore::MessageWithMessagePorts&&, const WebCore::ServiceWorkerOrClientIdentifier& source) final;
    void registerServiceWorkerClient(const WebCore::SecurityOrigin& topOrigin, const WebCore::ServiceWorkerClientData&, const std::optional<WebCore::ServiceWorkerRegistrationIdentifier>&, const String& userAgent) final;
    void unregisterServiceWorkerClient(WebCore::DocumentIdentifier) final;
    void scheduleUnregisterJobInServer(WebCore::ServiceWorkerRegistrationIdentifier, WebCore::DocumentOrWorkerIdentifier, CompletionHandler<void(WebCore::ExceptionOr<bool>&&)>&&) final;

    void matchRegistration(WebCore::SecurityOriginData&& topOrigin, const URL& clientURL, RegistrationCallback&&) final;
    void didMatchRegistration(uint64_t matchRequestIdentifier, std::optional<WebCore::ServiceWorkerRegistrationData>&&);
    void didGetRegistrations(uint64_t matchRequestIdentifier, Vector<WebCore::ServiceWorkerRegistrationData>&&);
    void whenRegistrationReady(const WebCore::SecurityOriginData& topOrigin, const URL& clientURL, WhenRegistrationReadyCallback&&) final;
    void registrationReady(uint64_t callbackID, WebCore::ServiceWorkerRegistrationData&&);

    void setDocumentIsControlled(WebCore::DocumentIdentifier, WebCore::ServiceWorkerRegistrationData&&, CompletionHandler<void(bool)>&&);

    void getRegistrations(WebCore::SecurityOriginData&& topOrigin, const URL& clientURL, GetRegistrationsCallback&&) final;
    void whenServiceWorkerIsTerminatedForTesting(WebCore::ServiceWorkerIdentifier, CompletionHandler<void()>&&) final;

    void didResolveRegistrationPromise(const WebCore::ServiceWorkerRegistrationKey&) final;
    void storeRegistrationsOnDiskForTesting(CompletionHandler<void()>&&) final;

    void scheduleStorageJob(const WebCore::ServiceWorkerJobData&);

    void runOrDelayTaskForImport(Function<void()>&& task);

    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final { return 0; }

    void setSWOriginTableSharedMemory(const SharedMemory::IPCHandle&);
    void setSWOriginTableIsImported();

    void clear();

    WebCore::SWServerConnectionIdentifier m_identifier;

    UniqueRef<WebSWOriginTable> m_swOriginTable;

    uint64_t m_previousCallbackIdentifier { 0 };
    HashMap<uint64_t, RegistrationCallback> m_ongoingMatchRegistrationTasks;
    HashMap<uint64_t, GetRegistrationsCallback> m_ongoingGetRegistrationsTasks;
    HashMap<uint64_t, WhenRegistrationReadyCallback> m_ongoingRegistrationReadyTasks;
    Deque<Function<void()>> m_tasksPendingOriginImport;
    bool m_isThrottleable { true };
}; // class WebSWServerConnection

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
