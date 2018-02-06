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

#define SWSERVERCONNECTION_RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(m_sessionID.isAlwaysOnLoggingAllowed(), ServiceWorker, "%p - WebSWServerConnection::" fmt, this, ##__VA_ARGS__)
#define SWSERVERCONNECTION_RELEASE_LOG_ERROR_IF_ALLOWED(fmt, ...) RELEASE_LOG_ERROR_IF(m_sessionID.isAlwaysOnLoggingAllowed(), ServiceWorker, "%p - WebSWServerConnection::" fmt, this, ##__VA_ARGS__)

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
        server().unregisterServiceWorkerClient(keyValue.value, keyValue.key);
}

void WebSWServerConnection::disconnectedFromWebProcess()
{
    notImplemented();
}

void WebSWServerConnection::rejectJobInClient(ServiceWorkerJobIdentifier jobIdentifier, const ExceptionData& exceptionData)
{
    send(Messages::WebSWClientConnection::JobRejectedInServer(jobIdentifier, exceptionData));
}

void WebSWServerConnection::resolveRegistrationJobInClient(ServiceWorkerJobIdentifier jobIdentifier, const ServiceWorkerRegistrationData& registrationData, ShouldNotifyWhenResolved shouldNotifyWhenResolved)
{
    send(Messages::WebSWClientConnection::RegistrationJobResolvedInServer(jobIdentifier, registrationData, shouldNotifyWhenResolved));
}

void WebSWServerConnection::resolveUnregistrationJobInClient(ServiceWorkerJobIdentifier jobIdentifier, const ServiceWorkerRegistrationKey& registrationKey, bool unregistrationResult)
{
    send(Messages::WebSWClientConnection::UnregistrationJobResolvedInServer(jobIdentifier, unregistrationResult));
}

void WebSWServerConnection::startScriptFetchInClient(ServiceWorkerJobIdentifier jobIdentifier, const ServiceWorkerRegistrationKey& registrationKey, FetchOptions::Cache cachePolicy)
{
    send(Messages::WebSWClientConnection::StartScriptFetchForServer(jobIdentifier, registrationKey, cachePolicy));
}

void WebSWServerConnection::updateRegistrationStateInClient(ServiceWorkerRegistrationIdentifier identifier, ServiceWorkerRegistrationState state, const std::optional<ServiceWorkerData>& serviceWorkerData)
{
    send(Messages::WebSWClientConnection::UpdateRegistrationState(identifier, state, serviceWorkerData));
}

void WebSWServerConnection::fireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier identifier)
{
    send(Messages::WebSWClientConnection::FireUpdateFoundEvent(identifier));
}

void WebSWServerConnection::setRegistrationLastUpdateTime(ServiceWorkerRegistrationIdentifier identifier, WallTime lastUpdateTime)
{
    send(Messages::WebSWClientConnection::SetRegistrationLastUpdateTime(identifier, lastUpdateTime));
}

void WebSWServerConnection::setRegistrationUpdateViaCache(ServiceWorkerRegistrationIdentifier identifier, ServiceWorkerUpdateViaCache updateViaCache)
{
    send(Messages::WebSWClientConnection::SetRegistrationUpdateViaCache(identifier, updateViaCache));
}

void WebSWServerConnection::notifyClientsOfControllerChange(const HashSet<DocumentIdentifier>& contextIdentifiers, const ServiceWorkerData& newController)
{
    send(Messages::WebSWClientConnection::NotifyClientsOfControllerChange(contextIdentifiers, newController));
}

void WebSWServerConnection::updateWorkerStateInClient(ServiceWorkerIdentifier worker, ServiceWorkerState state)
{
    send(Messages::WebSWClientConnection::UpdateWorkerState(worker, state));
}

void WebSWServerConnection::startFetch(uint64_t fetchIdentifier, ServiceWorkerRegistrationIdentifier serviceWorkerRegistrationIdentifier, ResourceRequest&& request, FetchOptions&& options, IPC::FormDataReference&& formData, String&& referrer)
{
    auto* worker = server().activeWorkerFromRegistrationID(serviceWorkerRegistrationIdentifier);
    if (!worker) {
        SWSERVERCONNECTION_RELEASE_LOG_ERROR_IF_ALLOWED("startFetch: fetchIdentifier: %llu -> DidNotHandle because no active worker", fetchIdentifier);
        m_contentConnection->send(Messages::ServiceWorkerClientFetch::DidNotHandle { }, fetchIdentifier);
        return;
    }
    auto serviceWorkerIdentifier = worker->identifier();

    auto runServerWorkerAndStartFetch = [weakThis = makeWeakPtr(this), this, fetchIdentifier, serviceWorkerIdentifier, request = WTFMove(request), options = WTFMove(options), formData = WTFMove(formData), referrer = WTFMove(referrer)](bool success) mutable {
        if (!weakThis)
            return;

        if (!success) {
            SWSERVERCONNECTION_RELEASE_LOG_ERROR_IF_ALLOWED("startFetch: fetchIdentifier: %llu -> DidNotHandle because worker did not become activated", fetchIdentifier);
            m_contentConnection->send(Messages::ServiceWorkerClientFetch::DidNotHandle { }, fetchIdentifier);
            return;
        }

        if (!StorageProcess::singleton().globalServerToContextConnection())
            StorageProcess::singleton().createServerToContextConnection(server().sessionID());

        server().runServiceWorkerIfNecessary(serviceWorkerIdentifier, [weakThis = WTFMove(weakThis), this, fetchIdentifier, serviceWorkerIdentifier, request = WTFMove(request), options = WTFMove(options), formData = WTFMove(formData), referrer = WTFMove(referrer)](bool success, auto& contextConnection) {
            if (!weakThis)
                return;

            if (success) {
                SWSERVERCONNECTION_RELEASE_LOG_IF_ALLOWED("startFetch: Starting fetch %llu via service worker %llu", fetchIdentifier, serviceWorkerIdentifier.toUInt64());
                sendToContextProcess(contextConnection, Messages::WebSWContextManagerConnection::StartFetch { identifier(), fetchIdentifier, serviceWorkerIdentifier, request, options, formData, referrer });
            } else {
                SWSERVERCONNECTION_RELEASE_LOG_ERROR_IF_ALLOWED("startFetch: fetchIdentifier: %llu -> DidNotHandle because failed to run service worker", fetchIdentifier);
                m_contentConnection->send(Messages::ServiceWorkerClientFetch::DidNotHandle { }, fetchIdentifier);
            }
        });
    };

    if (worker->state() == ServiceWorkerState::Activating) {
        worker->whenActivated(WTFMove(runServerWorkerAndStartFetch));
        return;
    }
    ASSERT(worker->state() == ServiceWorkerState::Activated);
    runServerWorkerAndStartFetch(true);
}

void WebSWServerConnection::postMessageToServiceWorker(ServiceWorkerIdentifier destinationIdentifier, MessageWithMessagePorts&& message, const ServiceWorkerOrClientIdentifier& sourceIdentifier)
{
    std::optional<ServiceWorkerOrClientData> sourceData;
    WTF::switchOn(sourceIdentifier, [&](ServiceWorkerIdentifier identifier) {
        if (auto* sourceWorker = server().workerByID(identifier))
            sourceData = ServiceWorkerOrClientData { sourceWorker->data() };
    }, [&](ServiceWorkerClientIdentifier identifier) {
        auto* destinationWorker = server().workerByID(destinationIdentifier);
        if (!destinationWorker)
            return;

        if (auto clientData = destinationWorker->findClientByIdentifier(identifier))
            sourceData = ServiceWorkerOrClientData { *clientData };
    });

    if (!sourceData)
        return;

    if (!StorageProcess::singleton().globalServerToContextConnection())
        StorageProcess::singleton().createServerToContextConnection(server().sessionID());

    // It's possible this specific worker cannot be re-run (e.g. its registration has been removed)
    server().runServiceWorkerIfNecessary(destinationIdentifier, [destinationIdentifier, message = WTFMove(message), sourceData = WTFMove(*sourceData)](bool success, auto& contextConnection) mutable {
        if (success)
            sendToContextProcess(contextConnection, Messages::WebSWContextManagerConnection::PostMessageToServiceWorker { destinationIdentifier, WTFMove(message), WTFMove(sourceData) });
    });
}

void WebSWServerConnection::scheduleJobInServer(ServiceWorkerJobData&& jobData)
{
    if (!StorageProcess::singleton().globalServerToContextConnection())
        StorageProcess::singleton().createServerToContextConnection(server().sessionID());

    SWSERVERCONNECTION_RELEASE_LOG_IF_ALLOWED("Scheduling ServiceWorker job %s in server", jobData.identifier().loggingString().utf8().data());
    ASSERT(identifier() == jobData.connectionIdentifier());

    server().scheduleJob(WTFMove(jobData));
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
    SWSERVERCONNECTION_RELEASE_LOG_IF_ALLOWED("didFinishFetch: fetchIdentifier: %llu", fetchIdentifier);
    m_contentConnection->send(Messages::ServiceWorkerClientFetch::DidFinish { }, fetchIdentifier);
}

void WebSWServerConnection::didFailFetch(uint64_t fetchIdentifier)
{
    SWSERVERCONNECTION_RELEASE_LOG_ERROR_IF_ALLOWED("didFailFetch: fetchIdentifier: %llu", fetchIdentifier);
    m_contentConnection->send(Messages::ServiceWorkerClientFetch::DidFail { }, fetchIdentifier);
}

void WebSWServerConnection::didNotHandleFetch(uint64_t fetchIdentifier)
{
    SWSERVERCONNECTION_RELEASE_LOG_IF_ALLOWED("didNotHandleFetch: fetchIdentifier: %llu", fetchIdentifier);
    m_contentConnection->send(Messages::ServiceWorkerClientFetch::DidNotHandle { }, fetchIdentifier);
}

void WebSWServerConnection::postMessageToServiceWorkerClient(DocumentIdentifier destinationContextIdentifier, MessageWithMessagePorts&& message, ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin)
{
    auto* sourceServiceWorker = server().workerByID(sourceIdentifier);
    if (!sourceServiceWorker)
        return;

    send(Messages::WebSWClientConnection::PostMessageToServiceWorkerClient { destinationContextIdentifier, WTFMove(message), sourceServiceWorker->data(), sourceOrigin });
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

void WebSWServerConnection::registerServiceWorkerClient(SecurityOriginData&& topOrigin, ServiceWorkerClientData&& data, const std::optional<ServiceWorkerIdentifier>& controllingServiceWorkerIdentifier)
{
    auto clientOrigin = ClientOrigin { WTFMove(topOrigin), SecurityOriginData::fromSecurityOrigin(SecurityOrigin::create(data.url)) };
    m_clientOrigins.add(data.identifier, clientOrigin);
    server().registerServiceWorkerClient(WTFMove(clientOrigin), WTFMove(data), controllingServiceWorkerIdentifier);
}

void WebSWServerConnection::unregisterServiceWorkerClient(const ServiceWorkerClientIdentifier& clientIdentifier)
{
    auto iterator = m_clientOrigins.find(clientIdentifier);
    if (iterator == m_clientOrigins.end())
        return;

    server().unregisterServiceWorkerClient(iterator->value, clientIdentifier);
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
