/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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

#include "LocalFrameLoaderClient.h"
#include "Logging.h"
#include "MessageWithMessagePorts.h"
#include "NotificationPayload.h"
#include "ServiceWorkerContainer.h"
#include "ServiceWorkerGlobalScope.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WTFProcess.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SWContextManager::ServiceWorkerTerminationRequest);

SWContextManager& SWContextManager::singleton()
{
    static SWContextManager* sharedManager = new SWContextManager;
    return *sharedManager;
}

void SWContextManager::setConnection(Ref<Connection>&& connection)
{
    ASSERT(!m_connection || m_connection->isClosed());
    if (RefPtr connection = m_connection)
        connection->stop();
    m_connection = WTF::move(connection);
}

auto SWContextManager::connection() const -> Connection*
{
    return m_connection.get();
}

void SWContextManager::registerServiceWorkerThreadForInstall(Ref<ServiceWorkerThreadProxy>&& serviceWorkerThreadProxy, Function<void()>&& debuggerTasksStartedCallback)
{
    ASSERT(isMainThread());

    auto serviceWorkerIdentifier = serviceWorkerThreadProxy->identifier();
    auto jobDataIdentifier = serviceWorkerThreadProxy->thread().jobDataIdentifier();
    auto* threadProxy = serviceWorkerThreadProxy.ptr();

    {
        Locker locker { m_workerMapLock };
        auto result = m_workerMap.add(serviceWorkerIdentifier, WTF::move(serviceWorkerThreadProxy));
        ASSERT_UNUSED(result, result.isNewEntry);
    }

    if (debuggerTasksStartedCallback) {
        threadProxy->thread().runLoop().postDebuggerTask([debuggerTasksStartedCallback = WTF::move(debuggerTasksStartedCallback)](auto&) {
            debuggerTasksStartedCallback();
        });
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
        protect(connection())->serviceWorkerFailedToStart(jobDataIdentifier, serviceWorkerIdentifier, exceptionMessage);
        return;
    }
    protect(connection())->serviceWorkerStarted(jobDataIdentifier, serviceWorkerIdentifier, doesHandleFetch);
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
    RefPtr serviceWorker = serviceWorkerThreadProxy(identifier);
    if (!serviceWorker) {
        RELEASE_LOG_ERROR(ServiceWorker, "SWContextManager::fireInstallEvent but service worker %" PRIu64 " not found", identifier.toUInt64());
        return;
    }

    serviceWorker->fireInstallEvent();
}

void SWContextManager::fireActivateEvent(ServiceWorkerIdentifier identifier)
{
    RefPtr serviceWorker = serviceWorkerThreadProxy(identifier);
    if (!serviceWorker) {
        RELEASE_LOG_ERROR(ServiceWorker, "SWContextManager::fireActivateEvent but service worker %" PRIu64 " not found", identifier.toUInt64());
        return;
    }

    serviceWorker->fireActivateEvent();
}

void SWContextManager::firePushEvent(ServiceWorkerIdentifier identifier, std::optional<Vector<uint8_t>>&& data, std::optional<NotificationPayload>&& proposedPayload, CompletionHandler<void(bool, std::optional<NotificationPayload>&&)>&& callback)
{
    RefPtr serviceWorker = serviceWorkerThreadProxy(identifier);
    if (!serviceWorker) {
        RELEASE_LOG_ERROR(ServiceWorker, "SWContextManager::firePushEvent but service worker %" PRIu64 " not found", identifier.toUInt64());
        callback(false, WTF::move(proposedPayload));
        return;
    }

    serviceWorker->firePushEvent(WTF::move(data), WTF::move(proposedPayload), WTF::move(callback));
}

void SWContextManager::firePushSubscriptionChangeEvent(ServiceWorkerIdentifier identifier, std::optional<PushSubscriptionData>&& newSubscriptionData, std::optional<PushSubscriptionData>&& oldSubscriptionData)
{
    RefPtr serviceWorker = serviceWorkerThreadProxy(identifier);
    if (!serviceWorker) {
        RELEASE_LOG_ERROR(ServiceWorker, "SWContextManager::firePushSubscriptionChangeEvent but service worker %" PRIu64 " not found", identifier.toUInt64());
        return;
    }

    serviceWorker->firePushSubscriptionChangeEvent(WTF::move(newSubscriptionData), WTF::move(oldSubscriptionData));
}

void SWContextManager::fireNotificationEvent(ServiceWorkerIdentifier identifier, NotificationData&& data, NotificationEventType eventType, CompletionHandler<void(bool)>&& callback)
{
    RefPtr serviceWorker = serviceWorkerThreadProxy(identifier);
    if (!serviceWorker) {
        RELEASE_LOG_ERROR(ServiceWorker, "SWContextManager::fireNotificationEvent but service worker %" PRIu64 " not found", identifier.toUInt64());
        callback(false);
        return;
    }

    serviceWorker->fireNotificationEvent(WTF::move(data), eventType, WTF::move(callback));
}

void SWContextManager::fireBackgroundFetchEvent(ServiceWorkerIdentifier identifier, BackgroundFetchInformation&& info, CompletionHandler<void(bool)>&& callback)
{
    RefPtr serviceWorker = serviceWorkerThreadProxy(identifier);
    if (!serviceWorker) {
        RELEASE_LOG_ERROR(ServiceWorker, "SWContextManager::fireBackgroundFetchEvent but service worker %" PRIu64 " not found", identifier.toUInt64());
        callback(false);
        return;
    }

    serviceWorker->fireBackgroundFetchEvent(WTF::move(info), WTF::move(callback));
}

void SWContextManager::fireBackgroundFetchClickEvent(ServiceWorkerIdentifier identifier, BackgroundFetchInformation&& info, CompletionHandler<void(bool)>&& callback)
{
    RefPtr serviceWorker = serviceWorkerThreadProxy(identifier);
    if (!serviceWorker) {
        RELEASE_LOG_ERROR(ServiceWorker, "SWContextManager::fireBackgroundFetchClickEvent but service worker %" PRIu64 " not found", identifier.toUInt64());
        callback(false);
        return;
    }

    serviceWorker->fireBackgroundFetchClickEvent(WTF::move(info), WTF::move(callback));
}

void SWContextManager::terminateWorker(ServiceWorkerIdentifier identifier, Seconds timeout, Function<void()>&& completionHandler)
{
    RELEASE_LOG(ServiceWorker, "SWContextManager::terminateWorker %" PRIu64, identifier.toUInt64());

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
    stopWorker(*serviceWorker, timeout, WTF::move(completionHandler));
}

void SWContextManager::stopWorker(ServiceWorkerThreadProxy& serviceWorker, Seconds timeout, Function<void()>&& completionHandler)
{
    auto identifier = serviceWorker.identifier();
    serviceWorker.setAsTerminatingOrTerminated();

    m_pendingServiceWorkerTerminationRequests.add(identifier, makeUnique<ServiceWorkerTerminationRequest>(*this, identifier, timeout));

    Ref thread = serviceWorker.thread();
    thread->stop([this, identifier, serviceWorker = Ref { serviceWorker }, completionHandler = WTF::move(completionHandler)]() mutable {
        m_pendingServiceWorkerTerminationRequests.remove(identifier);

        if (RefPtr connection = SWContextManager::singleton().connection())
            connection->workerTerminated(identifier);

        if (completionHandler)
            completionHandler();

        // Spin the runloop before releasing the worker thread proxy, as there would otherwise be
        // a race towards its destruction.
        callOnMainThread([serviceWorker = WTF::move(serviceWorker)] { });
    });
}

void SWContextManager::forEachServiceWorker(NOESCAPE const Function<Function<void(ScriptExecutionContext&)>()>& createTask)
{
    Locker locker { m_workerMapLock };
    for (auto& worker : m_workerMap.values())
        worker->thread().runLoop().postTask(createTask());
}

bool SWContextManager::postTaskToServiceWorker(ServiceWorkerIdentifier identifier, Function<void(ServiceWorkerGlobalScope&)>&& task)
{
    RefPtr serviceWorker = serviceWorkerThreadProxy(identifier);
    if (!serviceWorker)
        return false;

    serviceWorker->thread().runLoop().postTask([task = WTF::move(task)] (auto& context) {
        task(downcast<ServiceWorkerGlobalScope>(context));
    });
    return true;
}

bool SWContextManager::stopRunningDebuggerTasksOnServiceWorker(ServiceWorkerIdentifier identifier)
{
    RefPtr serviceWorker = serviceWorkerThreadProxyFromBackgroundThread(identifier);
    if (!serviceWorker)
        return false;

    serviceWorker->thread().runLoop().postDebuggerTask([serviceWorker](auto&) {
        // In case the worker is paused running debugger tasks, ensure we break out of
        // the pause since this will be the last debugger task we send to the worker.
        serviceWorker->thread().stopRunningDebuggerTasks();
    });
    return true;
}

void SWContextManager::serviceWorkerFailedToTerminate(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    UNUSED_PARAM(serviceWorkerIdentifier);
    RELEASE_LOG_ERROR(ServiceWorker, "Failed to terminate service worker with identifier %s, killing the service worker process", serviceWorkerIdentifier.loggingString().utf8().data());
    ASSERT_NOT_REACHED();
    terminateProcess(EXIT_FAILURE);
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
        workerMap = WTF::move(m_workerMap);
    }
    for (auto& serviceWorker : workerMap.values())
        stopWorker(serviceWorker, workerTerminationTimeout, [] { });
}

void SWContextManager::setAsInspected(ServiceWorkerIdentifier identifier, bool isInspected)
{
    if (RefPtr connection = m_connection)
        connection->setAsInspected(identifier, isInspected);
}

void SWContextManager::setInspectable(bool inspectable)
{
    Vector<Ref<ServiceWorkerThreadProxy>> workers;
    {
        Locker locker { m_workerMapLock };
        workers = copyToVector(m_workerMap.values());
    }
    for (auto& serviceWorker : workers)
        serviceWorker->setInspectable(inspectable);
}


void SWContextManager::updateRegistrationState(ServiceWorkerRegistrationIdentifier identifier, ServiceWorkerRegistrationState state, const std::optional<ServiceWorkerData>& serviceWorkerData)
{
    forEachServiceWorker([identifier, state, &serviceWorkerData] {
        return [identifier, state, serviceWorkerData = crossThreadCopy(serviceWorkerData)] (auto& context) mutable {
            if (auto* container = context.serviceWorkerContainer())
                container->updateRegistrationState(identifier, state, WTF::move(serviceWorkerData));
        };
    });
}

void SWContextManager::updateWorkerState(ServiceWorkerIdentifier identifier, ServiceWorkerState state)
{
    forEachServiceWorker([identifier, state] {
        return [identifier, state] (auto& context) {
            if (auto* container = context.serviceWorkerContainer())
                container->updateWorkerState(identifier, state);
        };
    });
}

void SWContextManager::fireUpdateFoundEvent(ServiceWorkerRegistrationIdentifier identifier)
{
    forEachServiceWorker([identifier] {
        return [identifier] (auto& context) {
            if (auto* container = context.serviceWorkerContainer())
                container->queueTaskToFireUpdateFoundEvent(identifier);
        };
    });
}

void SWContextManager::setRegistrationLastUpdateTime(ServiceWorkerRegistrationIdentifier identifier, WallTime lastUpdateTime)
{
    forEachServiceWorker([identifier, lastUpdateTime] {
        return [identifier, lastUpdateTime] (auto& context) {
            if (auto* container = context.serviceWorkerContainer()) {
                if (auto* registration = container->registration(identifier))
                    registration->setLastUpdateTime(lastUpdateTime);
            }
        };
    });
}

void SWContextManager::setRegistrationUpdateViaCache(ServiceWorkerRegistrationIdentifier identifier, ServiceWorkerUpdateViaCache updateViaCache)
{
    forEachServiceWorker([identifier, updateViaCache] {
        return [identifier, updateViaCache] (auto& context) {
            if (auto* container = context.serviceWorkerContainer()) {
                if (auto* registration = container->registration(identifier))
                    registration->setUpdateViaCache(updateViaCache);
            }
        };
    });
}

void SWContextManager::removeFetch(ServiceWorkerIdentifier serviceWorkerIdentifier, SWServerConnectionIdentifier serverConnectionIdentifier, FetchIdentifier fetchIdentifier, bool isNavigationFetch)
{
    ASSERT(isMainThread());
    if (RefPtr proxy = serviceWorkerThreadProxy(serviceWorkerIdentifier))
        proxy->removeFetch(serverConnectionIdentifier, fetchIdentifier);
    if (isNavigationFetch) {
        if (RefPtr connection = m_connection)
            connection->removeNavigationFetch(serverConnectionIdentifier, fetchIdentifier);
    }
}

} // namespace WebCore
