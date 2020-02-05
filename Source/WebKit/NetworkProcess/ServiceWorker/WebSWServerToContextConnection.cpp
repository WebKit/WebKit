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

#include "config.h"
#include "WebSWServerToContextConnection.h"

#if ENABLE(SERVICE_WORKER)

#include "FormDataReference.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "ServiceWorkerFetchTask.h"
#include "ServiceWorkerFetchTaskMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebSWServerConnection.h"
#include <WebCore/SWServer.h>
#include <WebCore/ServiceWorkerContextData.h>

namespace WebKit {
using namespace WebCore;

WebSWServerToContextConnection::WebSWServerToContextConnection(NetworkConnectionToWebProcess& connection, RegistrableDomain&& registrableDomain, SWServer& server)
    : SWServerToContextConnection(WTFMove(registrableDomain))
    , m_connection(connection)
    , m_server(makeWeakPtr(server))
{
    server.addContextConnection(*this);
}

WebSWServerToContextConnection::~WebSWServerToContextConnection()
{
    auto fetches = WTFMove(m_ongoingFetches);
    for (auto& fetch : fetches.values())
        fetch->contextClosed();

    if (m_server && m_server->contextConnectionForRegistrableDomain(registrableDomain()) == this)
        m_server->removeContextConnection(*this);
}

IPC::Connection& WebSWServerToContextConnection::ipcConnection() const
{
    return m_connection.connection();
}

IPC::Connection* WebSWServerToContextConnection::messageSenderConnection() const
{
    return &ipcConnection();
}

uint64_t WebSWServerToContextConnection::messageSenderDestinationID() const
{
    return 0;
}

void WebSWServerToContextConnection::postMessageToServiceWorkerClient(const ServiceWorkerClientIdentifier& destinationIdentifier, const MessageWithMessagePorts& message, ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin)
{
    if (!m_server)
        return;

    if (auto* connection = m_server->connection(destinationIdentifier.serverConnectionIdentifier))
        connection->postMessageToServiceWorkerClient(destinationIdentifier.contextIdentifier, message, sourceIdentifier, sourceOrigin);
}

void WebSWServerToContextConnection::installServiceWorkerContext(const ServiceWorkerContextData& data, const String& userAgent)
{
    send(Messages::WebSWContextManagerConnection::InstallServiceWorker { data, userAgent });
}

void WebSWServerToContextConnection::fireInstallEvent(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    send(Messages::WebSWContextManagerConnection::FireInstallEvent(serviceWorkerIdentifier));
}

void WebSWServerToContextConnection::fireActivateEvent(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    send(Messages::WebSWContextManagerConnection::FireActivateEvent(serviceWorkerIdentifier));
}

void WebSWServerToContextConnection::terminateWorker(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    send(Messages::WebSWContextManagerConnection::TerminateWorker(serviceWorkerIdentifier));
}

void WebSWServerToContextConnection::syncTerminateWorker(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    if (!sendSync(Messages::WebSWContextManagerConnection::SyncTerminateWorker(serviceWorkerIdentifier), Messages::WebSWContextManagerConnection::SyncTerminateWorker::Reply(), 0, 10_s, IPC::SendSyncOption::ForceDispatchWhenDestinationIsWaitingForUnboundedSyncReply))
        m_connection.networkProcess().parentProcessConnection()->send(Messages::NetworkProcessProxy::TerminateUnresponsiveServiceWorkerProcesses(registrableDomain(), m_connection.sessionID()), 0);
}

void WebSWServerToContextConnection::findClientByIdentifierCompleted(uint64_t requestIdentifier, const Optional<ServiceWorkerClientData>& data, bool hasSecurityError)
{
    send(Messages::WebSWContextManagerConnection::FindClientByIdentifierCompleted { requestIdentifier, data, hasSecurityError });
}

void WebSWServerToContextConnection::matchAllCompleted(uint64_t requestIdentifier, const Vector<ServiceWorkerClientData>& clientsData)
{
    send(Messages::WebSWContextManagerConnection::MatchAllCompleted { requestIdentifier, clientsData });
}

void WebSWServerToContextConnection::claimCompleted(uint64_t requestIdentifier)
{
    send(Messages::WebSWContextManagerConnection::ClaimCompleted { requestIdentifier });
}

void WebSWServerToContextConnection::connectionIsNoLongerNeeded()
{
    m_connection.serverToContextConnectionNoLongerNeeded();
}

void WebSWServerToContextConnection::setThrottleState(bool isThrottleable)
{
    m_isThrottleable = isThrottleable;
    send(Messages::WebSWContextManagerConnection::SetThrottleState { isThrottleable });
}

void WebSWServerToContextConnection::startFetch(ServiceWorkerFetchTask& task)
{
    task.start(*this);
}

void WebSWServerToContextConnection::didReceiveFetchTaskMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    auto iterator = m_ongoingFetches.find(makeObjectIdentifier<FetchIdentifierType>(decoder.destinationID()));
    if (iterator == m_ongoingFetches.end())
        return;

    iterator->value->didReceiveMessage(connection, decoder);
}

void WebSWServerToContextConnection::registerFetch(ServiceWorkerFetchTask& task)
{
    ASSERT(!m_ongoingFetches.contains(task.fetchIdentifier()));
    m_ongoingFetches.add(task.fetchIdentifier(), makeWeakPtr(task));
}

void WebSWServerToContextConnection::unregisterFetch(ServiceWorkerFetchTask& task)
{
    ASSERT(m_ongoingFetches.contains(task.fetchIdentifier()));
    m_ongoingFetches.remove(task.fetchIdentifier());
}

WebCore::ProcessIdentifier WebSWServerToContextConnection::webProcessIdentifier() const
{
    return m_connection.webProcessIdentifier();
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
