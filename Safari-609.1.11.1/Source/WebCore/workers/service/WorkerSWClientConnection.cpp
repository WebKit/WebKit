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
#include "WorkerSWClientConnection.h"

#if ENABLE(SERVICE_WORKER)

#include "SecurityOrigin.h"
#include "ServiceWorkerClientData.h"
#include "ServiceWorkerFetchResult.h"
#include "ServiceWorkerJobData.h"
#include "ServiceWorkerProvider.h"
#include "ServiceWorkerRegistration.h"
#include "WorkerGlobalScope.h"
#include "WorkerThread.h"

namespace WebCore {

WorkerSWClientConnection::WorkerSWClientConnection(WorkerGlobalScope& scope)
    : m_thread(scope.thread())
{
}

WorkerSWClientConnection::~WorkerSWClientConnection()
{
    auto matchRegistrations = WTFMove(m_matchRegistrationRequests);
    for (auto& callback : matchRegistrations.values())
        callback({ });

    auto getRegistrationsRequests = WTFMove(m_getRegistrationsRequests);
    for (auto& callback : getRegistrationsRequests.values())
        callback({ });

    auto whenRegistrationReadyRequests = WTFMove(m_whenRegistrationReadyRequests);
    for (auto& callback : whenRegistrationReadyRequests.values())
        callback({ });
}

void WorkerSWClientConnection::matchRegistration(SecurityOriginData&& topOrigin, const URL& clientURL, RegistrationCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_matchRegistrationRequests.add(requestIdentifier, WTFMove(callback));

    callOnMainThread([thread = m_thread.copyRef(), requestIdentifier, topOrigin = crossThreadCopy(WTFMove(topOrigin)), clientURL = crossThreadCopy(clientURL)]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.matchRegistration(WTFMove(topOrigin), clientURL, [thread = WTFMove(thread), requestIdentifier](auto&& result) mutable {
            thread->runLoop().postTaskForMode([requestIdentifier, result = WTFMove(result)] (auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_matchRegistrationRequests.take(requestIdentifier);
                callback(WTFMove(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::getRegistrations(SecurityOriginData&& topOrigin, const URL& clientURL, GetRegistrationsCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_getRegistrationsRequests.add(requestIdentifier, WTFMove(callback));

    callOnMainThread([thread = m_thread.copyRef(), requestIdentifier, topOrigin = crossThreadCopy(WTFMove(topOrigin)), clientURL = crossThreadCopy(clientURL)]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.getRegistrations(WTFMove(topOrigin), clientURL, [thread = WTFMove(thread), requestIdentifier](auto&& data) mutable {
            thread->runLoop().postTaskForMode([requestIdentifier, data = crossThreadCopy(WTFMove(data))] (auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_getRegistrationsRequests.take(requestIdentifier);
                callback(WTFMove(data));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::whenRegistrationReady(const SecurityOriginData& topOrigin, const URL& clientURL, WhenRegistrationReadyCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_whenRegistrationReadyRequests.add(requestIdentifier, WTFMove(callback));

    callOnMainThread([thread = m_thread.copyRef(), requestIdentifier, topOrigin = topOrigin.isolatedCopy(), clientURL = crossThreadCopy(clientURL)]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.whenRegistrationReady(topOrigin, clientURL, [thread = WTFMove(thread), requestIdentifier](auto&& result) mutable {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTFMove(result))] (auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_whenRegistrationReadyRequests.take(requestIdentifier);
                callback(WTFMove(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::addServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    callOnMainThread([identifier]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.addServiceWorkerRegistrationInServer(identifier);
    });
}

void WorkerSWClientConnection::removeServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    callOnMainThread([identifier]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.removeServiceWorkerRegistrationInServer(identifier);
    });
}

void WorkerSWClientConnection::didResolveRegistrationPromise(const ServiceWorkerRegistrationKey& key)
{
    callOnMainThread([key = crossThreadCopy(key)]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.didResolveRegistrationPromise(key);
    });
}

void WorkerSWClientConnection::postMessageToServiceWorker(ServiceWorkerIdentifier destination, MessageWithMessagePorts&& ports, const ServiceWorkerOrClientIdentifier& source)
{
    callOnMainThreadAndWait([&] {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.postMessageToServiceWorker(destination, WTFMove(ports), source);
    });
}

SWServerConnectionIdentifier WorkerSWClientConnection::serverConnectionIdentifier() const
{
    SWServerConnectionIdentifier identifier;
    callOnMainThreadAndWait([&] {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        identifier = connection.serverConnectionIdentifier();
    });
    return identifier;
}

bool WorkerSWClientConnection::mayHaveServiceWorkerRegisteredForOrigin(const SecurityOriginData&) const
{
    ASSERT_NOT_REACHED();
    return true;
}

void WorkerSWClientConnection::syncTerminateWorker(ServiceWorkerIdentifier identifier)
{
    callOnMainThread([identifier]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.syncTerminateWorker(identifier);
    });
}

void WorkerSWClientConnection::registerServiceWorkerClient(const SecurityOrigin& topOrigin, const ServiceWorkerClientData& data, const Optional<ServiceWorkerRegistrationIdentifier>& identifier, const String& userAgent)
{
    callOnMainThread([topOrigin = topOrigin.isolatedCopy(), data = crossThreadCopy(data), identifier, userAgent = crossThreadCopy(userAgent)] {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.registerServiceWorkerClient(topOrigin, data, identifier, userAgent);
    });
}

void WorkerSWClientConnection::unregisterServiceWorkerClient(DocumentIdentifier)
{
    ASSERT_NOT_REACHED();
}

void WorkerSWClientConnection::finishFetchingScriptInServer(const ServiceWorkerFetchResult& result)
{
    callOnMainThread([result = crossThreadCopy(result)]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.finishFetchingScriptInServer(result);
    });
}

void WorkerSWClientConnection::scheduleJob(DocumentOrWorkerIdentifier identifier, const ServiceWorkerJobData& data)
{
    callOnMainThread([identifier, data = crossThreadCopy(data)]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.scheduleJob(identifier, data);
    });
}

void WorkerSWClientConnection::scheduleJobInServer(const ServiceWorkerJobData&)
{
    ASSERT_NOT_REACHED();
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
