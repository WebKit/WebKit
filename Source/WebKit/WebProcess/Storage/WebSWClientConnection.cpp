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
#include "NetworkProcessConnection.h"
#include "ServiceWorkerClientFetch.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include "WebProcessPoolMessages.h"
#include "WebSWOriginTable.h"
#include "WebSWServerConnectionMessages.h"
#include <WebCore/Document.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/ServiceWorkerClientData.h>
#include <WebCore/ServiceWorkerFetchResult.h>
#include <WebCore/ServiceWorkerJobData.h>
#include <WebCore/ServiceWorkerRegistrationData.h>
#include <WebCore/ServiceWorkerRegistrationKey.h>

namespace WebKit {
using namespace PAL;
using namespace WebCore;


WebSWClientConnection::WebSWClientConnection(SessionID sessionID)
    : m_sessionID(sessionID)
    , m_swOriginTable(makeUniqueRef<WebSWOriginTable>())
{
    ASSERT(m_sessionID.isValid());
    initializeConnectionIfNeeded();
}

WebSWClientConnection::~WebSWClientConnection()
{
    if (m_connection)
        WebProcess::singleton().ensureNetworkProcessConnection().removeSWClientConnection(*this);
}

void WebSWClientConnection::initializeConnectionIfNeeded()
{
    if (m_connection)
        return;

    auto& networkProcessConnection = WebProcess::singleton().ensureNetworkProcessConnection();

    m_connection = &networkProcessConnection.connection();
    m_identifier = networkProcessConnection.initializeSWClientConnection(*this);

    updateThrottleState();
}

template<typename U>
void WebSWClientConnection::ensureConnectionAndSend(const U& message)
{
    initializeConnectionIfNeeded();
    if (m_connection)
        send(message);
}

void WebSWClientConnection::scheduleJobInServer(const ServiceWorkerJobData& jobData)
{
    ensureConnectionAndSend(Messages::WebSWServerConnection::ScheduleJobInServer(jobData));
}

void WebSWClientConnection::finishFetchingScriptInServer(const ServiceWorkerFetchResult& result)
{
    ensureConnectionAndSend(Messages::WebSWServerConnection::FinishFetchingScriptInServer(result));
}

void WebSWClientConnection::addServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    ensureConnectionAndSend(Messages::WebSWServerConnection::AddServiceWorkerRegistrationInServer(identifier));
}

void WebSWClientConnection::removeServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    ensureConnectionAndSend(Messages::WebSWServerConnection::RemoveServiceWorkerRegistrationInServer(identifier));
}

void WebSWClientConnection::postMessageToServiceWorker(ServiceWorkerIdentifier destinationIdentifier, MessageWithMessagePorts&& message, const ServiceWorkerOrClientIdentifier& sourceIdentifier)
{
    // FIXME: Temporarily pipe the SW postMessage messages via the UIProcess since this is where the MessagePort registry lives
    // and this avoids races.
    WebProcess::singleton().send(Messages::WebProcessPool::PostMessageToServiceWorker(destinationIdentifier, WTFMove(message), sourceIdentifier, serverConnectionIdentifier()), 0);
}

void WebSWClientConnection::registerServiceWorkerClient(const SecurityOrigin& topOrigin, const WebCore::ServiceWorkerClientData& data, const Optional<WebCore::ServiceWorkerRegistrationIdentifier>& controllingServiceWorkerRegistrationIdentifier, const String& userAgent)
{
    ensureConnectionAndSend(Messages::WebSWServerConnection::RegisterServiceWorkerClient { topOrigin.data(), data, controllingServiceWorkerRegistrationIdentifier, userAgent });
}

void WebSWClientConnection::unregisterServiceWorkerClient(DocumentIdentifier contextIdentifier)
{
    ensureConnectionAndSend(Messages::WebSWServerConnection::UnregisterServiceWorkerClient { ServiceWorkerClientIdentifier { serverConnectionIdentifier(), contextIdentifier } });
}

void WebSWClientConnection::didResolveRegistrationPromise(const ServiceWorkerRegistrationKey& key)
{
    ensureConnectionAndSend(Messages::WebSWServerConnection::DidResolveRegistrationPromise(key));
}

bool WebSWClientConnection::mayHaveServiceWorkerRegisteredForOrigin(const SecurityOriginData& origin) const
{
    if (!m_swOriginTable->isImported())
        return true;

    return m_swOriginTable->contains(origin);
}

void WebSWClientConnection::setSWOriginTableSharedMemory(const SharedMemory::Handle& handle)
{
    m_swOriginTable->setSharedMemory(handle);
}

void WebSWClientConnection::setSWOriginTableIsImported()
{
    m_swOriginTable->setIsImported();
    while (!m_tasksPendingOriginImport.isEmpty())
        m_tasksPendingOriginImport.takeFirst()();
}

void WebSWClientConnection::didMatchRegistration(uint64_t matchingRequest, Optional<ServiceWorkerRegistrationData>&& result)
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

void WebSWClientConnection::matchRegistration(SecurityOriginData&& topOrigin, const URL& clientURL, RegistrationCallback&& callback)
{
    ASSERT(isMainThread());

    if (!mayHaveServiceWorkerRegisteredForOrigin(topOrigin)) {
        callback(WTF::nullopt);
        return;
    }

    runOrDelayTaskForImport([this, callback = WTFMove(callback), topOrigin = WTFMove(topOrigin), clientURL]() mutable {
        uint64_t callbackID = ++m_previousCallbackIdentifier;
        m_ongoingMatchRegistrationTasks.add(callbackID, WTFMove(callback));
        ensureConnectionAndSend(Messages::WebSWServerConnection::MatchRegistration(callbackID, topOrigin, clientURL));
    });
}

void WebSWClientConnection::runOrDelayTaskForImport(WTF::Function<void()>&& task)
{
    if (m_swOriginTable->isImported()) {
        task();
        return;
    }
    m_tasksPendingOriginImport.append(WTFMove(task));
    initializeConnectionIfNeeded();
}

void WebSWClientConnection::whenRegistrationReady(const SecurityOrigin& topOrigin, const URL& clientURL, WhenRegistrationReadyCallback&& callback)
{
    uint64_t callbackID = ++m_previousCallbackIdentifier;
    m_ongoingRegistrationReadyTasks.add(callbackID, WTFMove(callback));
    ensureConnectionAndSend(Messages::WebSWServerConnection::WhenRegistrationReady(callbackID, topOrigin.data(), clientURL));
}

void WebSWClientConnection::registrationReady(uint64_t callbackID, WebCore::ServiceWorkerRegistrationData&& registrationData)
{
    ASSERT(registrationData.activeWorker);
    if (auto callback = m_ongoingRegistrationReadyTasks.take(callbackID))
        callback(WTFMove(registrationData));
}

void WebSWClientConnection::getRegistrations(SecurityOriginData&& topOrigin, const URL& clientURL, GetRegistrationsCallback&& callback)
{
    ASSERT(isMainThread());

    if (!mayHaveServiceWorkerRegisteredForOrigin(topOrigin)) {
        callback({ });
        return;
    }

    runOrDelayTaskForImport([this, callback = WTFMove(callback), topOrigin = WTFMove(topOrigin), clientURL]() mutable {
        uint64_t callbackID = ++m_previousCallbackIdentifier;
        m_ongoingGetRegistrationsTasks.add(callbackID, WTFMove(callback));
        ensureConnectionAndSend(Messages::WebSWServerConnection::GetRegistrations(callbackID, topOrigin, clientURL));
    });
}

void WebSWClientConnection::startFetch(FetchIdentifier fetchIdentifier, ServiceWorkerRegistrationIdentifier serviceWorkerRegistrationIdentifier, const ResourceRequest& request, const FetchOptions& options, const String& referrer)
{
    ensureConnectionAndSend(Messages::WebSWServerConnection::StartFetch { serviceWorkerRegistrationIdentifier, fetchIdentifier, request, options, IPC::FormDataReference { request.httpBody() }, referrer });
}

void WebSWClientConnection::cancelFetch(FetchIdentifier fetchIdentifier, ServiceWorkerRegistrationIdentifier serviceWorkerRegistrationIdentifier)
{
    ensureConnectionAndSend(Messages::WebSWServerConnection::CancelFetch { serviceWorkerRegistrationIdentifier, fetchIdentifier });
}

void WebSWClientConnection::continueDidReceiveFetchResponse(FetchIdentifier fetchIdentifier, ServiceWorkerRegistrationIdentifier serviceWorkerRegistrationIdentifier)
{
    ensureConnectionAndSend(Messages::WebSWServerConnection::ContinueDidReceiveFetchResponse { serviceWorkerRegistrationIdentifier, fetchIdentifier });
}

void WebSWClientConnection::connectionToServerLost()
{
    m_connection = nullptr;

    auto registrationTasks = WTFMove(m_ongoingMatchRegistrationTasks);
    for (auto& callback : registrationTasks.values())
        callback(WTF::nullopt);

    auto getRegistrationTasks = WTFMove(m_ongoingGetRegistrationsTasks);
    for (auto& callback : getRegistrationTasks.values())
        callback({ });

    clearPendingJobs();
}

void WebSWClientConnection::syncTerminateWorker(ServiceWorkerIdentifier identifier)
{
    initializeConnectionIfNeeded();

    sendSync(Messages::WebSWServerConnection::SyncTerminateWorkerFromClient(identifier), Messages::WebSWServerConnection::SyncTerminateWorkerFromClient::Reply());
}

WebCore::SWServerConnectionIdentifier WebSWClientConnection::serverConnectionIdentifier() const
{
    const_cast<WebSWClientConnection*>(this)->initializeConnectionIfNeeded();
    return m_identifier;
}

void WebSWClientConnection::updateThrottleState()
{
    m_isThrottleable = WebProcess::singleton().areAllPagesThrottleable();
    ensureConnectionAndSend(Messages::WebSWServerConnection::SetThrottleState { m_isThrottleable });
}

void WebSWClientConnection::storeRegistrationsOnDiskForTesting(CompletionHandler<void()>&& callback)
{
    initializeConnectionIfNeeded();
    sendWithAsyncReply(Messages::WebSWServerConnection::StoreRegistrationsOnDisk { }, WTFMove(callback));
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
