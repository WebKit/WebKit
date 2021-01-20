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
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkProcessMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include "WebProcessPoolMessages.h"
#include "WebSWOriginTable.h"
#include "WebSWServerConnectionMessages.h"
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/ProcessIdentifier.h>
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


WebSWClientConnection::WebSWClientConnection()
    : m_identifier(Process::identifier())
    , m_swOriginTable(makeUniqueRef<WebSWOriginTable>())
{
}

WebSWClientConnection::~WebSWClientConnection()
{
    clear();
}

IPC::Connection* WebSWClientConnection::messageSenderConnection() const
{
    return &WebProcess::singleton().ensureNetworkProcessConnection().connection();
}

void WebSWClientConnection::scheduleJobInServer(const ServiceWorkerJobData& jobData)
{
    runOrDelayTaskForImport([this, jobData] {
        send(Messages::WebSWServerConnection::ScheduleJobInServer { jobData });
    });
}

void WebSWClientConnection::finishFetchingScriptInServer(const ServiceWorkerFetchResult& result)
{
    send(Messages::WebSWServerConnection::FinishFetchingScriptInServer { result });
}

void WebSWClientConnection::addServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    // FIXME: We should send the message to network process only if this is a new registration, once we correctly handle recovery upon network process crash.
    WebProcess::singleton().addServiceWorkerRegistration(identifier);
    send(Messages::WebSWServerConnection::AddServiceWorkerRegistrationInServer { identifier });
}

void WebSWClientConnection::removeServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    if (WebProcess::singleton().removeServiceWorkerRegistration(identifier))
        send(Messages::WebSWServerConnection::RemoveServiceWorkerRegistrationInServer { identifier });
}

void WebSWClientConnection::scheduleUnregisterJobInServer(ServiceWorkerRegistrationIdentifier registrationIdentifier, WebCore::DocumentOrWorkerIdentifier documentIdentifier, CompletionHandler<void(ExceptionOr<bool>&&)>&& completionHandler)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::ScheduleUnregisterJobInServer { ServiceWorkerJobIdentifier::generateThreadSafe(), registrationIdentifier, documentIdentifier }, [completionHandler = WTFMove(completionHandler)](auto&& result) mutable {
        if (!result.has_value())
            return completionHandler(result.error().toException());
        completionHandler(result.value());
    });
}

void WebSWClientConnection::postMessageToServiceWorker(ServiceWorkerIdentifier destinationIdentifier, MessageWithMessagePorts&& message, const ServiceWorkerOrClientIdentifier& sourceIdentifier)
{
    send(Messages::WebSWServerConnection::PostMessageToServiceWorker { destinationIdentifier, WTFMove(message), sourceIdentifier });
}

void WebSWClientConnection::registerServiceWorkerClient(const SecurityOrigin& topOrigin, const WebCore::ServiceWorkerClientData& data, const Optional<WebCore::ServiceWorkerRegistrationIdentifier>& controllingServiceWorkerRegistrationIdentifier, const String& userAgent)
{
    send(Messages::WebSWServerConnection::RegisterServiceWorkerClient { topOrigin.data(), data, controllingServiceWorkerRegistrationIdentifier, userAgent });
}

void WebSWClientConnection::unregisterServiceWorkerClient(DocumentIdentifier contextIdentifier)
{
    send(Messages::WebSWServerConnection::UnregisterServiceWorkerClient { ServiceWorkerClientIdentifier { serverConnectionIdentifier(), contextIdentifier } });
}

void WebSWClientConnection::didResolveRegistrationPromise(const ServiceWorkerRegistrationKey& key)
{
    send(Messages::WebSWServerConnection::DidResolveRegistrationPromise { key });
}

bool WebSWClientConnection::mayHaveServiceWorkerRegisteredForOrigin(const SecurityOriginData& origin) const
{
    if (!m_swOriginTable->isImported())
        return true;

    return m_swOriginTable->contains(origin);
}

void WebSWClientConnection::setSWOriginTableSharedMemory(const SharedMemory::IPCHandle& ipcHandle)
{
    m_swOriginTable->setSharedMemory(ipcHandle.handle);
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
        send(Messages::WebSWServerConnection::MatchRegistration(callbackID, topOrigin, clientURL));
    });
}

void WebSWClientConnection::runOrDelayTaskForImport(Function<void()>&& task)
{
    if (m_swOriginTable->isImported()) {
        task();
        return;
    }
    m_tasksPendingOriginImport.append(WTFMove(task));
}

void WebSWClientConnection::whenRegistrationReady(const SecurityOriginData& topOrigin, const URL& clientURL, WhenRegistrationReadyCallback&& callback)
{
    uint64_t callbackID = ++m_previousCallbackIdentifier;
    m_ongoingRegistrationReadyTasks.add(callbackID, WTFMove(callback));
    send(Messages::WebSWServerConnection::WhenRegistrationReady(callbackID, topOrigin, clientURL));
}

void WebSWClientConnection::registrationReady(uint64_t callbackID, ServiceWorkerRegistrationData&& registrationData)
{
    ASSERT(registrationData.activeWorker);
    if (auto callback = m_ongoingRegistrationReadyTasks.take(callbackID))
        callback(WTFMove(registrationData));
}

void WebSWClientConnection::setDocumentIsControlled(DocumentIdentifier documentIdentifier, ServiceWorkerRegistrationData&& data, CompletionHandler<void(bool)>&& completionHandler)
{
    auto* documentLoader = DocumentLoader::fromTemporaryDocumentIdentifier(documentIdentifier);
    bool result = documentLoader ? documentLoader->setControllingServiceWorkerRegistration(WTFMove(data)) : false;
    completionHandler(result);
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
        send(Messages::WebSWServerConnection::GetRegistrations { callbackID, topOrigin, clientURL });
    });
}

void WebSWClientConnection::connectionToServerLost()
{
    clear();
}

void WebSWClientConnection::clear()
{
    auto registrationTasks = WTFMove(m_ongoingMatchRegistrationTasks);
    for (auto& callback : registrationTasks.values())
        callback(WTF::nullopt);

    auto getRegistrationTasks = WTFMove(m_ongoingGetRegistrationsTasks);
    for (auto& callback : getRegistrationTasks.values())
        callback({ });

    m_ongoingRegistrationReadyTasks.clear();

    clearPendingJobs();
}

void WebSWClientConnection::terminateWorkerForTesting(ServiceWorkerIdentifier identifier, CompletionHandler<void()>&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::TerminateWorkerFromClient { identifier }, WTFMove(callback));
}

void WebSWClientConnection::whenServiceWorkerIsTerminatedForTesting(ServiceWorkerIdentifier identifier, CompletionHandler<void()>&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::WhenServiceWorkerIsTerminatedForTesting { identifier }, WTFMove(callback));
}

void WebSWClientConnection::updateThrottleState()
{
    m_isThrottleable = WebProcess::singleton().areAllPagesThrottleable();
    send(Messages::WebSWServerConnection::SetThrottleState { m_isThrottleable });
}

void WebSWClientConnection::storeRegistrationsOnDiskForTesting(CompletionHandler<void()>&& callback)
{
    sendWithAsyncReply(Messages::WebSWServerConnection::StoreRegistrationsOnDisk { }, WTFMove(callback));
}

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
