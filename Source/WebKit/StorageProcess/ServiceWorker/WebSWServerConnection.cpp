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
#include "WebSWServerConnection.h"

#if ENABLE(SERVICE_WORKER)

#include "DataReference.h"
#include "Logging.h"
#include "ServiceWorkerClientFetchMessages.h"
#include "StorageProcess.h"
#include "StorageToWebProcessConnectionMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include "WebProcessMessages.h"
#include "WebSWClientConnectionMessages.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebSWServerConnectionMessages.h"
#include "WebSWServerToContextConnection.h"
#include "WebToStorageProcessConnection.h"
#include <WebCore/ExceptionData.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/SWServerRegistration.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/ServiceWorkerClientData.h>
#include <WebCore/ServiceWorkerClientIdentifier.h>
#include <WebCore/ServiceWorkerContextData.h>
#include <WebCore/ServiceWorkerJobData.h>
#include <wtf/MainThread.h>

using namespace PAL;
using namespace WebCore;

namespace WebKit {

WebSWServerConnection::WebSWServerConnection(SWServer& server, IPC::Connection& connection, uint64_t connectionIdentifier, SessionID sessionID)
    : SWServer::Connection(server, connectionIdentifier)
    , m_sessionID(sessionID)
    , m_contentConnection(connection)
{
}

WebSWServerConnection::~WebSWServerConnection()
{
}

void WebSWServerConnection::disconnectedFromWebProcess()
{
    notImplemented();
}

void WebSWServerConnection::rejectJobInClient(uint64_t jobIdentifier, const ExceptionData& exceptionData)
{
    send(Messages::WebSWClientConnection::JobRejectedInServer(jobIdentifier, exceptionData));
}

void WebSWServerConnection::resolveRegistrationJobInClient(uint64_t jobIdentifier, const ServiceWorkerRegistrationData& registrationData, ShouldNotifyWhenResolved shouldNotifyWhenResolved)
{
    send(Messages::WebSWClientConnection::RegistrationJobResolvedInServer(jobIdentifier, registrationData, shouldNotifyWhenResolved));
}

void WebSWServerConnection::resolveUnregistrationJobInClient(uint64_t jobIdentifier, const ServiceWorkerRegistrationKey& registrationKey, bool unregistrationResult)
{
    send(Messages::WebSWClientConnection::UnregistrationJobResolvedInServer(jobIdentifier, unregistrationResult));
}

void WebSWServerConnection::startScriptFetchInClient(uint64_t jobIdentifier)
{
    send(Messages::WebSWClientConnection::StartScriptFetchForServer(jobIdentifier));
}

void WebSWServerConnection::updateRegistrationStateInClient(ServiceWorkerRegistrationIdentifier identifier, ServiceWorkerRegistrationState state, const std::optional<ServiceWorkerData>& serviceWorkerData)
{
    send(Messages::WebSWClientConnection::UpdateRegistrationState(identifier, state, serviceWorkerData));
}

void WebSWServerConnection::fireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier identifier)
{
    send(Messages::WebSWClientConnection::FireUpdateFoundEvent(identifier));
}

void WebSWServerConnection::updateWorkerStateInClient(ServiceWorkerIdentifier worker, ServiceWorkerState state)
{
    send(Messages::WebSWClientConnection::UpdateWorkerState(worker, state));
}

void WebSWServerConnection::startFetch(uint64_t fetchIdentifier, std::optional<ServiceWorkerIdentifier> serviceWorkerIdentifier, const ResourceRequest& request, const FetchOptions& options)
{
    sendToContextProcess(Messages::WebSWContextManagerConnection::StartFetch(identifier(), fetchIdentifier, serviceWorkerIdentifier, request, options));
}

void WebSWServerConnection::postMessageToServiceWorkerGlobalScope(ServiceWorkerIdentifier destinationServiceWorkerIdentifier, const IPC::DataReference& message, ServiceWorkerClientData&& source)
{
    source.identifier.serverConnectionIdentifier = identifier();
    sendToContextProcess(Messages::WebSWContextManagerConnection::PostMessageToServiceWorkerGlobalScope { destinationServiceWorkerIdentifier, message, WTFMove(source) });
}

void WebSWServerConnection::didReceiveFetchResponse(uint64_t fetchIdentifier, const ResourceResponse& response)
{
    m_contentConnection->send(Messages::ServiceWorkerClientFetch::DidReceiveResponse { response }, fetchIdentifier);
}

void WebSWServerConnection::didReceiveFetchData(uint64_t fetchIdentifier, const IPC::DataReference& data, int64_t encodedDataLength)
{
    m_contentConnection->send(Messages::ServiceWorkerClientFetch::DidReceiveData { data, encodedDataLength }, fetchIdentifier);
}

void WebSWServerConnection::didReceiveFetchFormData(uint64_t fetchIdentifier, const IPC::FormDataReference& formData)
{
    m_contentConnection->send(Messages::ServiceWorkerClientFetch::DidReceiveFormData { formData }, fetchIdentifier);
}

void WebSWServerConnection::didFinishFetch(uint64_t fetchIdentifier)
{
    m_contentConnection->send(Messages::ServiceWorkerClientFetch::DidFinish { }, fetchIdentifier);
}

void WebSWServerConnection::didFailFetch(uint64_t fetchIdentifier)
{
    m_contentConnection->send(Messages::ServiceWorkerClientFetch::DidFail { }, fetchIdentifier);
}

void WebSWServerConnection::didNotHandleFetch(uint64_t fetchIdentifier)
{
    m_contentConnection->send(Messages::ServiceWorkerClientFetch::DidNotHandle { }, fetchIdentifier);
}

void WebSWServerConnection::postMessageToServiceWorkerClient(uint64_t destinationScriptExecutionContextIdentifier, const IPC::DataReference& message, ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin)
{
    auto* sourceServiceWorker = server().workerByID(sourceIdentifier);
    if (!sourceServiceWorker)
        return;

    send(Messages::WebSWClientConnection::PostMessageToServiceWorkerClient { destinationScriptExecutionContextIdentifier, message, sourceServiceWorker->data(), sourceOrigin });
}

void WebSWServerConnection::matchRegistration(uint64_t registrationMatchRequestIdentifier, const SecurityOriginData& topOrigin, const URL& clientURL)
{
    if (auto* registration = doRegistrationMatching(topOrigin, clientURL)) {
        send(Messages::WebSWClientConnection::DidMatchRegistration { registrationMatchRequestIdentifier, registration->data() });
        return;
    }
    send(Messages::WebSWClientConnection::DidMatchRegistration { registrationMatchRequestIdentifier, std::nullopt });
}

void WebSWServerConnection::getRegistrations(uint64_t registrationMatchRequestIdentifier, const SecurityOriginData& topOrigin, const URL& clientURL)
{
    auto registrations = server().getRegistrations(topOrigin, clientURL);
    send(Messages::WebSWClientConnection::DidGetRegistrations { registrationMatchRequestIdentifier, registrations });
}

template<typename U> void WebSWServerConnection::sendToContextProcess(U&& message)
{
    if (auto* connection = StorageProcess::singleton().globalServerToContextConnection())
        connection->send(WTFMove(message));
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
