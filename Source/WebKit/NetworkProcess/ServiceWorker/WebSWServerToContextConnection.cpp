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
#include "NetworkProcess.h"
#include "ServiceWorkerFetchTask.h"
#include "ServiceWorkerFetchTaskMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebSWContextManagerConnectionMessages.h"
#include <WebCore/ServiceWorkerContextData.h>

namespace WebKit {
using namespace WebCore;

WebSWServerToContextConnection::WebSWServerToContextConnection(NetworkProcess& networkProcess, const RegistrableDomain& registrableDomain, Ref<IPC::Connection>&& connection)
    : SWServerToContextConnection(registrableDomain)
    , m_ipcConnection(WTFMove(connection))
    , m_networkProcess(networkProcess)
{
}

WebSWServerToContextConnection::~WebSWServerToContextConnection() = default;

IPC::Connection* WebSWServerToContextConnection::messageSenderConnection() const
{
    return m_ipcConnection.ptr();
}

uint64_t WebSWServerToContextConnection::messageSenderDestinationID() const
{
    return 0;
}

void WebSWServerToContextConnection::connectionClosed()
{
    auto fetches = WTFMove(m_ongoingFetches);
    for (auto& fetch : fetches.values())
        fetch->fail(ResourceError { errorDomainWebKitInternal, 0, { }, "Service Worker context closed"_s });
}

void WebSWServerToContextConnection::installServiceWorkerContext(const ServiceWorkerContextData& data, PAL::SessionID sessionID, const String& userAgent)
{
    send(Messages::WebSWContextManagerConnection::InstallServiceWorker { data, sessionID, userAgent });
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
    sendSync(Messages::WebSWContextManagerConnection::SyncTerminateWorker(serviceWorkerIdentifier), Messages::WebSWContextManagerConnection::SyncTerminateWorker::Reply());
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

void WebSWServerToContextConnection::didFinishSkipWaiting(uint64_t callbackID)
{
    send(Messages::WebSWContextManagerConnection::DidFinishSkipWaiting { callbackID });
}

void WebSWServerToContextConnection::connectionMayNoLongerBeNeeded()
{
    m_networkProcess->swContextConnectionMayNoLongerBeNeeded(*this);
}

void WebSWServerToContextConnection::setThrottleState(bool isThrottleable)
{
    m_isThrottleable = isThrottleable;
    send(Messages::WebSWContextManagerConnection::SetThrottleState { isThrottleable });
}

void WebSWServerToContextConnection::terminate()
{
    send(Messages::WebSWContextManagerConnection::TerminateProcess());
}

void WebSWServerToContextConnection::startFetch(PAL::SessionID sessionID, Ref<IPC::Connection>&& contentConnection, WebCore::SWServerConnectionIdentifier serverConnectionIdentifier, FetchIdentifier contentFetchIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, const ResourceRequest& request, const FetchOptions& options, const IPC::FormDataReference& data, const String& referrer)
{
    auto fetchIdentifier = FetchIdentifier::generate();
    
    m_ongoingFetches.add(fetchIdentifier, ServiceWorkerFetchTask::create(sessionID, WTFMove(contentConnection), serverConnectionIdentifier, contentFetchIdentifier));

    ASSERT(!m_ongoingFetchIdentifiers.contains({serverConnectionIdentifier, contentFetchIdentifier}));
    m_ongoingFetchIdentifiers.add({serverConnectionIdentifier, contentFetchIdentifier}, fetchIdentifier);

    send(Messages::WebSWContextManagerConnection::StartFetch { serverConnectionIdentifier, serviceWorkerIdentifier, fetchIdentifier, request, options, data, referrer });
}

void WebSWServerToContextConnection::cancelFetch(WebCore::SWServerConnectionIdentifier serverConnectionIdentifier, FetchIdentifier contentFetchIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    auto iterator = m_ongoingFetchIdentifiers.find({ serverConnectionIdentifier, contentFetchIdentifier });
    if (iterator == m_ongoingFetchIdentifiers.end())
        return;

    send(Messages::WebSWContextManagerConnection::CancelFetch { serverConnectionIdentifier, serviceWorkerIdentifier, iterator->value });

    m_ongoingFetches.remove(iterator->value);
    m_ongoingFetchIdentifiers.remove(iterator);
}

void WebSWServerToContextConnection::continueDidReceiveFetchResponse(WebCore::SWServerConnectionIdentifier serverConnectionIdentifier, FetchIdentifier contentFetchIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    auto iterator = m_ongoingFetchIdentifiers.find({ serverConnectionIdentifier, contentFetchIdentifier });
    if (iterator == m_ongoingFetchIdentifiers.end())
        return;
    
    send(Messages::WebSWContextManagerConnection::ContinueDidReceiveFetchResponse { serverConnectionIdentifier, serviceWorkerIdentifier, iterator->value });
}

void WebSWServerToContextConnection::didReceiveFetchTaskMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    auto iterator = m_ongoingFetches.find(makeObjectIdentifier<FetchIdentifierType>(decoder.destinationID()));
    if (iterator == m_ongoingFetches.end())
        return;
    
    bool shouldRemove = decoder.messageName() == Messages::ServiceWorkerFetchTask::DidFail::name()
        || decoder.messageName() == Messages::ServiceWorkerFetchTask::DidNotHandle::name()
        || decoder.messageName() == Messages::ServiceWorkerFetchTask::DidFinish::name()
        || decoder.messageName() == Messages::ServiceWorkerFetchTask::DidReceiveRedirectResponse::name();

    iterator->value->didReceiveMessage(connection, decoder);

    if (shouldRemove) {
        ASSERT(m_ongoingFetchIdentifiers.contains(iterator->value->identifier()));
        m_ongoingFetchIdentifiers.remove(iterator->value->identifier());
        m_ongoingFetches.remove(iterator);
    }
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
