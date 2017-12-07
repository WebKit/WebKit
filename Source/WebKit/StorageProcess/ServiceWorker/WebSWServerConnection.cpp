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

WebSWServerConnection::WebSWServerConnection(SWServer& server, IPC::Connection& connection, SessionID sessionID)
    : SWServer::Connection(server)
    , m_sessionID(sessionID)
    , m_contentConnection(connection)
{
    StorageProcess::singleton().registerSWServerConnection(*this);
}

WebSWServerConnection::~WebSWServerConnection()
{
    StorageProcess::singleton().unregisterSWServerConnection(*this);
    for (auto keyValue : m_clientOrigins)
        server().unregisterServiceWorkerClient(keyValue.value, ServiceWorkerClientIdentifier { identifier(), keyValue.key });
}

void WebSWServerConnection::disconnectedFromWebProcess()
{
    notImplemented();
}

void WebSWServerConnection::rejectJobInClient(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, const ExceptionData& exceptionData)
{
    send(Messages::WebSWClientConnection::JobRejectedInServer(jobDataIdentifier, exceptionData));
}

void WebSWServerConnection::resolveRegistrationJobInClient(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, const ServiceWorkerRegistrationData& registrationData, ShouldNotifyWhenResolved shouldNotifyWhenResolved)
{
    send(Messages::WebSWClientConnection::RegistrationJobResolvedInServer(jobDataIdentifier, registrationData, shouldNotifyWhenResolved));
}

void WebSWServerConnection::resolveUnregistrationJobInClient(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, const ServiceWorkerRegistrationKey& registrationKey, bool unregistrationResult)
{
    send(Messages::WebSWClientConnection::UnregistrationJobResolvedInServer(jobDataIdentifier, unregistrationResult));
}

void WebSWServerConnection::startScriptFetchInClient(const ServiceWorkerJobDataIdentifier& jobDataIdentifier)
{
    send(Messages::WebSWClientConnection::StartScriptFetchForServer(jobDataIdentifier));
}

void WebSWServerConnection::updateRegistrationStateInClient(ServiceWorkerRegistrationIdentifier identifier, ServiceWorkerRegistrationState state, const std::optional<ServiceWorkerData>& serviceWorkerData)
{
    send(Messages::WebSWClientConnection::UpdateRegistrationState(identifier, state, serviceWorkerData));
}

void WebSWServerConnection::fireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier identifier)
{
    send(Messages::WebSWClientConnection::FireUpdateFoundEvent(identifier));
}

void WebSWServerConnection::notifyClientsOfControllerChange(const HashSet<DocumentIdentifier>& contextIdentifiers, const ServiceWorkerData& newController)
{
    send(Messages::WebSWClientConnection::NotifyClientsOfControllerChange(contextIdentifiers, newController));
}

void WebSWServerConnection::updateWorkerStateInClient(ServiceWorkerIdentifier worker, ServiceWorkerState state)
{
    send(Messages::WebSWClientConnection::UpdateWorkerState(worker, state));
}

void WebSWServerConnection::startFetch(uint64_t fetchIdentifier, std::optional<ServiceWorkerIdentifier> serviceWorkerIdentifier, ResourceRequest&& request, FetchOptions&& options, IPC::FormDataReference&& formData)
{
    // It's possible this specific worker cannot be re-run (e.g. its registration has been removed)
    if (serviceWorkerIdentifier) {
        server().runServiceWorkerIfNecessary(*serviceWorkerIdentifier, [contentConnection = m_contentConnection.copyRef(), connectionIdentifier = identifier(), fetchIdentifier, serviceWorkerIdentifier = *serviceWorkerIdentifier, request = WTFMove(request), options = WTFMove(options), formData = WTFMove(formData)](bool success, auto& contextConnection) {
            if (success)
                sendToContextProcess(contextConnection, Messages::WebSWContextManagerConnection::StartFetch { connectionIdentifier, fetchIdentifier, serviceWorkerIdentifier, request, options, formData });
            else
                contentConnection->send(Messages::ServiceWorkerClientFetch::DidNotHandle { }, fetchIdentifier);
        });
        return;
    }

    // FIXME: If we don't have a ServiceWorkerIdentifier here then we need to create and run the appropriate Service Worker,
    // but it's unclear that we have enough information here to do that.

    sendToContextProcess(Messages::WebSWContextManagerConnection::StartFetch { identifier(), fetchIdentifier, serviceWorkerIdentifier, request, options, formData });
}

void WebSWServerConnection::postMessageToServiceWorkerFromClient(ServiceWorkerIdentifier destinationIdentifier, IPC::DataReference&& message, ServiceWorkerClientIdentifier sourceIdentifier, ServiceWorkerClientData&& sourceData)
{
    // It's possible this specific worker cannot be re-run (e.g. its registration has been removed)
    server().runServiceWorkerIfNecessary(destinationIdentifier, [destinationIdentifier, message = WTFMove(message), sourceIdentifier, sourceData = WTFMove(sourceData)](bool success, auto& contextConnection) mutable {
        if (success)
            sendToContextProcess(contextConnection, Messages::WebSWContextManagerConnection::PostMessageToServiceWorkerFromClient { destinationIdentifier, message, sourceIdentifier, WTFMove(sourceData) });
    });
}

void WebSWServerConnection::postMessageToServiceWorkerFromServiceWorker(ServiceWorkerIdentifier destinationIdentifier, IPC::DataReference&& message, ServiceWorkerIdentifier sourceIdentifier)
{
    auto* sourceWorker = server().workerByID(sourceIdentifier);
    if (!sourceWorker)
        return;

    // It's possible this specific worker cannot be re-run (e.g. its registration has been removed)
    server().runServiceWorkerIfNecessary(destinationIdentifier, [destinationIdentifier, message = WTFMove(message), sourceIdentifier, sourceData = sourceWorker->data()](bool success, auto& contextConnection) mutable {
        if (!success)
            return;

        sendToContextProcess(contextConnection, Messages::WebSWContextManagerConnection::PostMessageToServiceWorkerFromServiceWorker { destinationIdentifier, message, WTFMove(sourceData) });
    });
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

void WebSWServerConnection::postMessageToServiceWorkerClient(DocumentIdentifier destinationContextIdentifier, const IPC::DataReference& message, ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin)
{
    auto* sourceServiceWorker = server().workerByID(sourceIdentifier);
    if (!sourceServiceWorker)
        return;

    send(Messages::WebSWClientConnection::PostMessageToServiceWorkerClient { destinationContextIdentifier, message, sourceServiceWorker->data(), sourceOrigin });
}

void WebSWServerConnection::matchRegistration(uint64_t registrationMatchRequestIdentifier, const SecurityOriginData& topOrigin, const URL& clientURL)
{
    if (auto* registration = doRegistrationMatching(topOrigin, clientURL)) {
        send(Messages::WebSWClientConnection::DidMatchRegistration { registrationMatchRequestIdentifier, registration->data() });
        return;
    }
    send(Messages::WebSWClientConnection::DidMatchRegistration { registrationMatchRequestIdentifier, std::nullopt });
}

void WebSWServerConnection::registrationReady(uint64_t registrationReadyRequestIdentifier, ServiceWorkerRegistrationData&& registrationData)
{
    send(Messages::WebSWClientConnection::RegistrationReady { registrationReadyRequestIdentifier, WTFMove(registrationData) });
}

void WebSWServerConnection::getRegistrations(uint64_t registrationMatchRequestIdentifier, const SecurityOriginData& topOrigin, const URL& clientURL)
{
    auto registrations = server().getRegistrations(topOrigin, clientURL);
    send(Messages::WebSWClientConnection::DidGetRegistrations { registrationMatchRequestIdentifier, registrations });
}

void WebSWServerConnection::registerServiceWorkerClient(SecurityOriginData&& topOrigin, DocumentIdentifier contextIdentifier, ServiceWorkerClientData&& data, const std::optional<ServiceWorkerIdentifier>& controllingServiceWorkerIdentifier)
{
    auto clientOrigin = ClientOrigin { WTFMove(topOrigin), SecurityOriginData::fromSecurityOrigin(SecurityOrigin::create(data.url)) };
    m_clientOrigins.add(contextIdentifier, clientOrigin);
    server().registerServiceWorkerClient(WTFMove(clientOrigin), ServiceWorkerClientIdentifier { this->identifier(), contextIdentifier } , WTFMove(data), controllingServiceWorkerIdentifier);
}

void WebSWServerConnection::unregisterServiceWorkerClient(DocumentIdentifier contextIdentifier)
{
    auto iterator = m_clientOrigins.find(contextIdentifier);
    if (iterator == m_clientOrigins.end())
        return;

    server().unregisterServiceWorkerClient(iterator->value, ServiceWorkerClientIdentifier { this->identifier(), contextIdentifier });
    m_clientOrigins.remove(iterator);
}

template<typename U> void WebSWServerConnection::sendToContextProcess(U&& message)
{
    if (auto* connection = StorageProcess::singleton().globalServerToContextConnection())
        connection->send(WTFMove(message));
}

template<typename U> void WebSWServerConnection::sendToContextProcess(WebCore::SWServerToContextConnection& connection, U&& message)
{
    static_cast<WebSWServerToContextConnection&>(connection).send(WTFMove(message));
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
