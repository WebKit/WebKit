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
#include "SWContextManager.h"

#if ENABLE(SERVICE_WORKER)
#include "FrameLoaderClient.h"
#include "Logging.h"
#include "MessageWithMessagePorts.h"
#include "ServiceWorkerGlobalScope.h"

namespace WebCore {

SWContextManager& SWContextManager::singleton()
{
    static SWContextManager* sharedManager = new SWContextManager;
    return *sharedManager;
}

void SWContextManager::setConnection(Ref<Connection>&& connection)
{
    ASSERT(!m_connection || m_connection->isClosed());
    if (m_connection)
        m_connection->stop();
    m_connection = WTFMove(connection);
}

auto SWContextManager::connection() const -> Connection*
{
    return m_connection.get();
}

void SWContextManager::registerServiceWorkerThreadForInstall(Ref<ServiceWorkerThreadProxy>&& serviceWorkerThreadProxy)
{
    ASSERT(isMainThread());

    auto serviceWorkerIdentifier = serviceWorkerThreadProxy->identifier();
    auto jobDataIdentifier = serviceWorkerThreadProxy->thread().jobDataIdentifier();
    auto* threadProxy = serviceWorkerThreadProxy.ptr();

    {
        Locker locker { m_workerMapLock };
        auto result = m_workerMap.add(serviceWorkerIdentifier, WTFMove(serviceWorkerThreadProxy));
        ASSERT_UNUSED(result, result.isNewEntry);
    }

    threadProxy->thread().start([jobDataIdentifier, serviceWorkerIdentifier](const String& exceptionMessage, bool doesHandleFetch) {
        SWContextManager::singleton().startedServiceWorker(jobDataIdentifier, serviceWorkerIdentifier, exceptionMessage, doesHandleFetch);
    });
    if (m_serviceWorkerCreationCallback)
        m_serviceWorkerCreationCallback(serviceWorkerIdentifier.toUInt64());
}

void SWContextManager::startedServiceWorker(std::optional<ServiceWorkerJobDataIdentifier> jobDataIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier, const String& exceptionMessage, bool doesHandleFetch)
{
    if (!exceptionMessage.isEmpty()) {
        connection()->serviceWorkerFailedToStart(jobDataIdentifier, serviceWorkerIdentifier, exceptionMessage);
        return;
    }
    connection()->serviceWorkerStarted(jobDataIdentifier, serviceWorkerIdentifier, doesHandleFetch);
}

ServiceWorkerThreadProxy* SWContextManager::serviceWorkerThreadProxy(ServiceWorkerIdentifier identifier) const
{
    ASSERT(isMainThread());
    Locker locker { m_workerMapLock };
    return m_workerMap.get(identifier);
}

RefPtr<ServiceWorkerThreadProxy> SWContextManager::serviceWorkerThreadProxyFromBackgroundThread(ServiceWorkerIdentifier identifier) const
{
    Locker locker { m_workerMapLock };
    RefPtr result = m_workerMap.get(identifier);
    return result;
}

void SWContextManager::fireInstallEvent(ServiceWorkerIdentifier identifier)
{
    auto* serviceWorker = serviceWorkerThreadProxy(identifier);
    if (!serviceWorker)
        return;

    serviceWorker->fireInstallEvent();
}

void SWContextManager::fireActivateEvent(ServiceWorkerIdentifier identifier)
{
    auto* serviceWorker = serviceWorkerThreadProxy(identifier);
    if (!serviceWorker)
        return;

    serviceWorker->fireActivateEvent();
}

void SWContextManager::firePushEvent(ServiceWorkerIdentifier identifier, std::optional<Vector<uint8_t>>&& data, CompletionHandler<void(bool)>&& callback)
{
    auto* serviceWorker = serviceWorkerThreadProxy(identifier);
    if (!serviceWorker) {
        callback(false);
        return;
    }

    serviceWorker->firePushEvent(WTFMove(data), WTFMove(callback));
}

void SWContextManager::firePushSubscriptionChangeEvent(ServiceWorkerIdentifier identifier, std::optional<PushSubscriptionData>&& newSubscriptionData, std::optional<PushSubscriptionData>&& oldSubscriptionData)
{
    auto* serviceWorker = serviceWorkerThreadProxy(identifier);
    if (!serviceWorker)
        return;

    serviceWorker->firePushSubscriptionChangeEvent(WTFMove(newSubscriptionData), WTFMove(oldSubscriptionData));
}

void SWContextManager::fireNotificationEvent(ServiceWorkerIdentifier identifier, NotificationData&& data, NotificationEventType eventType, CompletionHandler<void(bool)>&& callback)
{
    auto* serviceWorker = serviceWorkerThreadProxy(identifier);
    if (!serviceWorker)
        return;

    serviceWorker->fireNotificationEvent(WTFMove(data), eventType, WTFMove(callback));
}

void SWContextManager::terminateWorker(ServiceWorkerIdentifier identifier, Seconds timeout, Function<void()>&& completionHandler)
{
    RELEASE_LOG(ServiceWorker, "SWContextManager::terminateWorker");

    RefPtr<ServiceWorkerThreadProxy> serviceWorker;
    {
        Locker locker { m_workerMapLock };
        serviceWorker = m_workerMap.take(identifier);
    }

    if (!serviceWorker) {
        if (completionHandler)
            completionHandler();
        return;
    }
    stopWorker(*serviceWorker, timeout, WTFMove(completionHandler));
}

void SWContextManager::stopWorker(ServiceWorkerThreadProxy& serviceWorker, Seconds timeout, Function<void()>&& completionHandler)
{
    auto identifier = serviceWorker.identifier();
    serviceWorker.setAsTerminatingOrTerminated();

    m_pendingServiceWorkerTerminationRequests.add(identifier, makeUnique<ServiceWorkerTerminationRequest>(*this, identifier, timeout));

    auto& thread = serviceWorker.thread();
    thread.stop([this, identifier, serviceWorker = Ref { serviceWorker }, completionHandler = WTFMove(completionHandler)]() mutable {
        m_pendingServiceWorkerTerminationRequests.remove(identifier);

        if (auto* connection = SWContextManager::singleton().connection())
            connection->workerTerminated(identifier);

        if (completionHandler)
            completionHandler();

        // Spin the runloop before releasing the worker thread proxy, as there would otherwise be
        // a race towards its destruction.
        callOnMainThread([serviceWorker = WTFMove(serviceWorker)] { });
    });
}

void SWContextManager::forEachServiceWorker(const Function<Function<void(ScriptExecutionContext&)>()>& createTask)
{
    Locker locker { m_workerMapLock };
    for (auto& worker : m_workerMap.values())
        worker->thread().runLoop().postTask(createTask());
}

bool SWContextManager::postTaskToServiceWorker(ServiceWorkerIdentifier identifier, Function<void(ServiceWorkerGlobalScope&)>&& task)
{
    auto* serviceWorker = serviceWorkerThreadProxy(identifier);
    if (!serviceWorker)
        return false;

    serviceWorker->thread().runLoop().postTask([task = WTFMove(task)] (auto& context) {
        task(downcast<ServiceWorkerGlobalScope>(context));
    });
    return true;
}

void SWContextManager::serviceWorkerFailedToTerminate(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    UNUSED_PARAM(serviceWorkerIdentifier);
    RELEASE_LOG_ERROR(ServiceWorker, "Failed to terminate service worker with identifier %s, killing the service worker process", serviceWorkerIdentifier.loggingString().utf8().data());
    ASSERT_NOT_REACHED();
    _exit(EXIT_FAILURE);
}

SWContextManager::ServiceWorkerTerminationRequest::ServiceWorkerTerminationRequest(SWContextManager& manager, ServiceWorkerIdentifier serviceWorkerIdentifier, Seconds timeout)
    : m_timeoutTimer([&manager, serviceWorkerIdentifier] { manager.serviceWorkerFailedToTerminate(serviceWorkerIdentifier); })
{
    m_timeoutTimer.startOneShot(timeout);
}

void SWContextManager::stopAllServiceWorkers()
{
    HashMap<ServiceWorkerIdentifier, Ref<ServiceWorkerThreadProxy>> workerMap;
    {
        Locker locker { m_workerMapLock };
        workerMap = WTFMove(m_workerMap);
    }
    for (auto& serviceWorker : workerMap.values())
        stopWorker(serviceWorker, workerTerminationTimeout, [] { });
}

void SWContextManager::setAsInspected(ServiceWorkerIdentifier identifier, bool isInspected)
{
    if (m_connection)
        m_connection->setAsInspected(identifier, isInspected);
}

} // namespace WebCore

#endif
