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
#include "WebSWClientConnection.h"

#if ENABLE(SERVICE_WORKER)

#include "DataReference.h"
#include "FormDataReference.h"
#include "Logging.h"
#include "ServiceWorkerClientFetch.h"
#include "StorageToWebProcessConnectionMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebSWOriginTable.h"
#include "WebSWServerConnectionMessages.h"
#include <WebCore/Document.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/ServiceWorkerClientData.h>
#include <WebCore/ServiceWorkerFetchResult.h>
#include <WebCore/ServiceWorkerJobData.h>
#include <WebCore/ServiceWorkerRegistrationData.h>

using namespace PAL;
using namespace WebCore;

namespace WebKit {

WebSWClientConnection::WebSWClientConnection(IPC::Connection& connection, SessionID sessionID)
    : m_sessionID(sessionID)
    , m_connection(connection)
    , m_swOriginTable(makeUniqueRef<WebSWOriginTable>())
{
    bool result = sendSync(Messages::StorageToWebProcessConnection::EstablishSWServerConnection(sessionID), Messages::StorageToWebProcessConnection::EstablishSWServerConnection::Reply(m_identifier));

    ASSERT_UNUSED(result, result);
}

WebSWClientConnection::~WebSWClientConnection()
{
}

void WebSWClientConnection::scheduleJobInServer(const ServiceWorkerJobData& jobData)
{
    send(Messages::WebSWServerConnection::ScheduleJobInServer(jobData));
}

void WebSWClientConnection::finishFetchingScriptInServer(const ServiceWorkerFetchResult& result)
{
    send(Messages::WebSWServerConnection::FinishFetchingScriptInServer(result));
}

void WebSWClientConnection::addServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    send(Messages::WebSWServerConnection::AddServiceWorkerRegistrationInServer(identifier));
}

void WebSWClientConnection::removeServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    send(Messages::WebSWServerConnection::RemoveServiceWorkerRegistrationInServer(identifier));
}

void WebSWClientConnection::postMessageToServiceWorker(ServiceWorkerIdentifier destinationIdentifier, Ref<SerializedScriptValue>&& scriptValue, ServiceWorkerClientIdentifier sourceIdentifier, ServiceWorkerClientData&& source)
{
    send(Messages::WebSWServerConnection::PostMessageToServiceWorkerFromClient(destinationIdentifier, IPC::DataReference { scriptValue->data() }, sourceIdentifier, WTFMove(source)));
}

void WebSWClientConnection::postMessageToServiceWorker(ServiceWorkerIdentifier destinationIdentifier, Ref<SerializedScriptValue>&& scriptValue, ServiceWorkerIdentifier sourceWorkerIdentifier)
{
    send(Messages::WebSWServerConnection::PostMessageToServiceWorkerFromServiceWorker(destinationIdentifier, IPC::DataReference { scriptValue->data() }, sourceWorkerIdentifier));
}

void WebSWClientConnection::registerServiceWorkerClient(const SecurityOrigin& topOrigin, WebCore::DocumentIdentifier contextIdentifier, const WebCore::ServiceWorkerClientData& data, const std::optional<WebCore::ServiceWorkerIdentifier>& controllingServiceWorkerIdentifier)
{
    send(Messages::WebSWServerConnection::RegisterServiceWorkerClient { SecurityOriginData::fromSecurityOrigin(topOrigin), contextIdentifier, data, controllingServiceWorkerIdentifier });
}

void WebSWClientConnection::unregisterServiceWorkerClient(WebCore::DocumentIdentifier contextIdentifier)
{
    send(Messages::WebSWServerConnection::UnregisterServiceWorkerClient { contextIdentifier });
}

void WebSWClientConnection::didResolveRegistrationPromise(const ServiceWorkerRegistrationKey& key)
{
    send(Messages::WebSWServerConnection::DidResolveRegistrationPromise(key));
}

bool WebSWClientConnection::mayHaveServiceWorkerRegisteredForOrigin(const SecurityOrigin& origin) const
{
    if (!m_swOriginTable->isInitialized())
        return true;

    return m_swOriginTable->contains(origin);
}

void WebSWClientConnection::setSWOriginTableSharedMemory(const SharedMemory::Handle& handle)
{
    m_swOriginTable->setSharedMemory(handle);
}

void WebSWClientConnection::initializeSWOriginTableAsEmpty()
{
    m_swOriginTable->initializeAsEmpty();
}

void WebSWClientConnection::didMatchRegistration(uint64_t matchingRequest, std::optional<ServiceWorkerRegistrationData>&& result)
{
    ASSERT(isMainThread());

    if (auto completionHandler = m_ongoingMatchRegistrationTasks.take(matchingRequest))
        completionHandler(WTFMove(result));
}

void WebSWClientConnection::didGetRegistrations(uint64_t matchingRequest, Vector<ServiceWorkerRegistrationData>&& registrations)
{
    ASSERT(isMainThread());

    if (auto completionHandler = m_ongoingGetRegistrationsTasks.take(matchingRequest))
        completionHandler(WTFMove(registrations));
}

void WebSWClientConnection::matchRegistration(const SecurityOrigin& topOrigin, const URL& clientURL, RegistrationCallback&& callback)
{
    ASSERT(isMainThread());

    if (!mayHaveServiceWorkerRegisteredForOrigin(topOrigin)) {
        callback(std::nullopt);
        return;
    }

    uint64_t callbackID = ++m_previousCallbackIdentifier;
    m_ongoingMatchRegistrationTasks.add(callbackID, WTFMove(callback));
    send(Messages::WebSWServerConnection::MatchRegistration(callbackID, SecurityOriginData::fromSecurityOrigin(topOrigin), clientURL));
}

void WebSWClientConnection::whenRegistrationReady(const SecurityOrigin& topOrigin, const URL& clientURL, WhenRegistrationReadyCallback&& callback)
{
    uint64_t callbackID = ++m_previousCallbackIdentifier;
    m_ongoingRegistrationReadyTasks.add(callbackID, WTFMove(callback));
    send(Messages::WebSWServerConnection::WhenRegistrationReady(callbackID, SecurityOriginData::fromSecurityOrigin(topOrigin), clientURL));
}

void WebSWClientConnection::registrationReady(uint64_t callbackID, WebCore::ServiceWorkerRegistrationData&& registrationData)
{
    ASSERT(registrationData.activeWorker);
    if (auto callback = m_ongoingRegistrationReadyTasks.take(callbackID))
        callback(WTFMove(registrationData));
}

void WebSWClientConnection::getRegistrations(const SecurityOrigin& topOrigin, const URL& clientURL, GetRegistrationsCallback&& callback)
{
    ASSERT(isMainThread());

    if (!mayHaveServiceWorkerRegisteredForOrigin(topOrigin)) {
        callback({ });
        return;
    }

    uint64_t callbackID = ++m_previousCallbackIdentifier;
    m_ongoingGetRegistrationsTasks.add(callbackID, WTFMove(callback));
    send(Messages::WebSWServerConnection::GetRegistrations(callbackID, SecurityOriginData::fromSecurityOrigin(topOrigin), clientURL));
}

void WebSWClientConnection::startFetch(const ResourceLoader& loader, uint64_t identifier)
{
    ASSERT(loader.options().serviceWorkersMode != ServiceWorkersMode::None && loader.options().serviceWorkerIdentifier);
    send(Messages::WebSWServerConnection::StartFetch { identifier, loader.options().serviceWorkerIdentifier.value(), loader.request(), loader.options(), IPC::FormDataReference { loader.request().httpBody() } });
}

void WebSWClientConnection::postMessageToServiceWorkerClient(DocumentIdentifier destinationContextIdentifier, const IPC::DataReference& message, ServiceWorkerData&& source, const String& sourceOrigin)
{
    SWClientConnection::postMessageToServiceWorkerClient(destinationContextIdentifier, SerializedScriptValue::adopt(message.vector()), WTFMove(source), sourceOrigin);
}

void WebSWClientConnection::connectionToServerLost()
{
    auto registrationTasks = WTFMove(m_ongoingMatchRegistrationTasks);
    for (auto& callback : registrationTasks.values())
        callback(std::nullopt);

    auto getRegistrationTasks = WTFMove(m_ongoingGetRegistrationsTasks);
    for (auto& callback : getRegistrationTasks.values())
        callback({ });

    clearPendingJobs();
}

void WebSWClientConnection::syncTerminateWorker(ServiceWorkerIdentifier identifier)
{
    sendSync(Messages::WebSWServerConnection::SyncTerminateWorker(identifier), Messages::WebSWServerConnection::SyncTerminateWorker::Reply());
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
