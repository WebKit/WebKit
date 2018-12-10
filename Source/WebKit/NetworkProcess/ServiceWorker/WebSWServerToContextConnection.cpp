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

#include "NetworkProcess.h"
#include "WebCoreArgumentCoders.h"
#include "WebSWContextManagerConnectionMessages.h"
#include <WebCore/ServiceWorkerContextData.h>

namespace WebKit {
using namespace WebCore;

WebSWServerToContextConnection::WebSWServerToContextConnection(const SecurityOriginData& securityOrigin, Ref<IPC::Connection>&& connection)
    : SWServerToContextConnection(securityOrigin)
    , m_ipcConnection(WTFMove(connection))
{
}

IPC::Connection* WebSWServerToContextConnection::messageSenderConnection()
{
    return m_ipcConnection.ptr();
}

uint64_t WebSWServerToContextConnection::messageSenderDestinationID()
{
    return 0;
}

void WebSWServerToContextConnection::connectionClosed()
{
    // FIXME: Do what here...?
}

void WebSWServerToContextConnection::installServiceWorkerContext(const ServiceWorkerContextData& data, PAL::SessionID sessionID)
{
    send(Messages::WebSWContextManagerConnection::InstallServiceWorker { data, sessionID });
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

void WebSWServerToContextConnection::findClientByIdentifierCompleted(uint64_t requestIdentifier, const std::optional<ServiceWorkerClientData>& data, bool hasSecurityError)
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
    NetworkProcess::singleton().swContextConnectionMayNoLongerBeNeeded(*this);
}

void WebSWServerToContextConnection::terminate()
{
    send(Messages::WebSWContextManagerConnection::TerminateProcess());
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
