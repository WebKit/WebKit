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

#include "MessageReceiver.h"
#include "MessageSender.h"
#include "ServiceWorkerFetchTask.h"
#include <WebCore/ExceptionOr.h>
#include <WebCore/FetchIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/SWServer.h>
#include <pal/SessionID.h>
#include <wtf/HashMap.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class FormDataReference;

template<> struct AsyncReplyError<WebCore::ExceptionOr<bool>> {
    static WebCore::ExceptionOr<bool> create() { return WebCore::Exception { WebCore::TypeError, "Internal error"_s }; }
};

}

namespace WebCore {
class ServiceWorkerRegistrationKey;
struct ClientOrigin;
struct ExceptionData;
struct MessageWithMessagePorts;
struct ServiceWorkerClientData;
}

namespace WebKit {

class NetworkProcess;
class NetworkResourceLoader;
class ServiceWorkerFetchTask;

class WebSWServerConnection final : public WebCore::SWServer::Connection, public IPC::MessageSender, public IPC::MessageReceiver {
public:
    WebSWServerConnection(NetworkProcess&, WebCore::SWServer&, IPC::Connection&, WebCore::ProcessIdentifier);
    WebSWServerConnection(const WebSWServerConnection&) = delete;
    ~WebSWServerConnection() final;

    using WebCore::SWServer::Connection::weakPtrFactory;
    using WeakValueType = WebCore::SWServer::Connection::WeakValueType;

    IPC::Connection& ipcConnection() const { return m_contentConnection.get(); }

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    
    PAL::SessionID sessionID() const;

    std::unique_ptr<ServiceWorkerFetchTask> createFetchTask(NetworkResourceLoader&, const WebCore::ResourceRequest&);
    void fetchTaskTimedOut(WebCore::ServiceWorkerIdentifier);

private:
    // Implement SWServer::Connection (Messages to the client WebProcess)
    void rejectJobInClient(WebCore::ServiceWorkerJobIdentifier, const WebCore::ExceptionData&) final;
    void resolveRegistrationJobInClient(WebCore::ServiceWorkerJobIdentifier, const WebCore::ServiceWorkerRegistrationData&, WebCore::ShouldNotifyWhenResolved) final;
    void resolveUnregistrationJobInClient(WebCore::ServiceWorkerJobIdentifier, const WebCore::ServiceWorkerRegistrationKey&, bool unregistrationResult) final;
    void startScriptFetchInClient(WebCore::ServiceWorkerJobIdentifier, const WebCore::ServiceWorkerRegistrationKey&, WebCore::FetchOptions::Cache) final;
    void updateRegistrationStateInClient(WebCore::ServiceWorkerRegistrationIdentifier, WebCore::ServiceWorkerRegistrationState, const std::optional<WebCore::ServiceWorkerData>&) final;
    void updateWorkerStateInClient(WebCore::ServiceWorkerIdentifier, WebCore::ServiceWorkerState) final;
    void fireUpdateFoundEvent(WebCore::ServiceWorkerRegistrationIdentifier) final;
    void setRegistrationLastUpdateTime(WebCore::ServiceWorkerRegistrationIdentifier, WallTime) final;
    void setRegistrationUpdateViaCache(WebCore::ServiceWorkerRegistrationIdentifier, WebCore::ServiceWorkerUpdateViaCache) final;
    void notifyClientsOfControllerChange(const HashSet<WebCore::DocumentIdentifier>& contextIdentifiers, const WebCore::ServiceWorkerData& newController);
    void registrationReady(uint64_t registrationReadyRequestIdentifier, WebCore::ServiceWorkerRegistrationData&&) final;

    void scheduleJobInServer(WebCore::ServiceWorkerJobData&&);

    using UnregisterJobResult = Expected<bool, WebCore::ExceptionData>;
    void scheduleUnregisterJobInServer(WebCore::ServiceWorkerJobIdentifier, WebCore::ServiceWorkerRegistrationIdentifier, WebCore::DocumentOrWorkerIdentifier, CompletionHandler<void(UnregisterJobResult&&)>&&);

    void startFetch(ServiceWorkerFetchTask&, WebCore::SWServerWorker&);

    void matchRegistration(uint64_t registrationMatchRequestIdentifier, const WebCore::SecurityOriginData& topOrigin, const URL& clientURL);
    void getRegistrations(uint64_t registrationMatchRequestIdentifier, const WebCore::SecurityOriginData& topOrigin, const URL& clientURL);

    void registerServiceWorkerClient(WebCore::SecurityOriginData&& topOrigin, WebCore::ServiceWorkerClientData&&, const std::optional<WebCore::ServiceWorkerRegistrationIdentifier>&, String&& userAgent);
    void unregisterServiceWorkerClient(const WebCore::ServiceWorkerClientIdentifier&);
    void terminateWorkerFromClient(WebCore::ServiceWorkerIdentifier, CompletionHandler<void()>&&);
    void whenServiceWorkerIsTerminatedForTesting(WebCore::ServiceWorkerIdentifier, CompletionHandler<void()>&&);

    void postMessageToServiceWorkerClient(WebCore::DocumentIdentifier destinationContextIdentifier, const WebCore::MessageWithMessagePorts&, WebCore::ServiceWorkerIdentifier sourceServiceWorkerIdentifier, const String& sourceOrigin) final;

    void contextConnectionCreated(WebCore::SWServerToContextConnection&) final;

    bool isThrottleable() const { return m_isThrottleable; }
    bool hasMatchingClient(const WebCore::RegistrableDomain&) const;
    bool computeThrottleState(const WebCore::RegistrableDomain&) const;
    void setThrottleState(bool isThrottleable);
    void updateThrottleState();

    void postMessageToServiceWorker(WebCore::ServiceWorkerIdentifier destination, WebCore::MessageWithMessagePorts&&, const WebCore::ServiceWorkerOrClientIdentifier& source);
    void controlClient(WebCore::ServiceWorkerClientIdentifier, WebCore::SWServerRegistration&, const WebCore::ResourceRequest&);

    URL clientURLFromIdentifier(WebCore::DocumentOrWorkerIdentifier);

    IPC::Connection* messageSenderConnection() const final { return m_contentConnection.ptr(); }
    uint64_t messageSenderDestinationID() const final { return 0; }
    
    template<typename U> static void sendToContextProcess(WebCore::SWServerToContextConnection&, U&& message);

    Ref<IPC::Connection> m_contentConnection;
    Ref<NetworkProcess> m_networkProcess;
    HashMap<WebCore::ServiceWorkerClientIdentifier, WebCore::ClientOrigin> m_clientOrigins;
    HashMap<WebCore::ServiceWorkerJobIdentifier, CompletionHandler<void(UnregisterJobResult&&)>> m_unregisterJobs;
    bool m_isThrottleable { true };
};

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
