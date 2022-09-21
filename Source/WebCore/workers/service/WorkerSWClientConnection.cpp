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

#include "NotificationData.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerClientData.h"
#include "ServiceWorkerJobData.h"
#include "ServiceWorkerProvider.h"
#include "ServiceWorkerRegistration.h"
#include "WorkerFetchResult.h"
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

    auto unregisterRequests = WTFMove(m_unregisterRequests);
    for (auto& callback : unregisterRequests.values())
        callback(Exception { TypeError, "context stopped"_s });
    
    auto subscribeToPushServiceRequests = WTFMove(m_subscribeToPushServiceRequests);
    for (auto& callback : subscribeToPushServiceRequests.values())
        callback(Exception { AbortError, "context stopped"_s });
    
    auto unsubscribeFromPushServiceRequests = WTFMove(m_unsubscribeFromPushServiceRequests);
    for (auto& callback : unsubscribeFromPushServiceRequests.values())
        callback(Exception { AbortError, "context stopped"_s });
    
    auto getPushSubscriptionRequests = WTFMove(m_getPushSubscriptionRequests);
    for (auto& callback : getPushSubscriptionRequests.values())
        callback(Exception { AbortError, "context stopped"_s });
    
    auto getPushPermissionStateCallbacks = WTFMove(m_getPushPermissionStateCallbacks);
    for (auto& callback : getPushPermissionStateCallbacks.values())
        callback(Exception { AbortError, "context stopped"_s });

    auto voidCallbacks = WTFMove(m_voidCallbacks);
    for (auto& callback : voidCallbacks.values())
        callback(Exception { AbortError, "context stopped"_s });

    auto navigationPreloadStateCallbacks = WTFMove(m_navigationPreloadStateCallbacks);
    for (auto& callback : navigationPreloadStateCallbacks.values())
        callback(Exception { AbortError, "context stopped"_s });

    auto getNotificationsCallbacks = WTFMove(m_getNotificationsCallbacks);
    for (auto& callback : getNotificationsCallbacks.values())
        callback(Exception { AbortError, "context stopped"_s });
}

void WorkerSWClientConnection::matchRegistration(SecurityOriginData&& topOrigin, const URL& clientURL, RegistrationCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_matchRegistrationRequests.add(requestIdentifier, WTFMove(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, topOrigin = crossThreadCopy(WTFMove(topOrigin)), clientURL = crossThreadCopy(clientURL)]() mutable {
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

    callOnMainThread([thread = m_thread, requestIdentifier, topOrigin = crossThreadCopy(WTFMove(topOrigin)), clientURL = crossThreadCopy(clientURL)]() mutable {
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

    callOnMainThread([thread = m_thread, requestIdentifier, topOrigin = topOrigin.isolatedCopy(), clientURL = crossThreadCopy(clientURL)]() mutable {
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

void WorkerSWClientConnection::registerServiceWorkerClient(const ClientOrigin& clientOrigin, ServiceWorkerClientData&& data, const std::optional<ServiceWorkerRegistrationIdentifier>& identifier, String&& userAgent)
{
    callOnMainThread([clientOrigin = clientOrigin.isolatedCopy(), data = crossThreadCopy(WTFMove(data)), identifier, userAgent = crossThreadCopy(WTFMove(userAgent))]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.registerServiceWorkerClient(clientOrigin, WTFMove(data), identifier, WTFMove(userAgent));
    });
}

void WorkerSWClientConnection::unregisterServiceWorkerClient(ScriptExecutionContextIdentifier identifier)
{
    callOnMainThread([identifier] {
        if (auto* serviceWorkerConnection = ServiceWorkerProvider::singleton().existingServiceWorkerConnection())
            serviceWorkerConnection->unregisterServiceWorkerClient(identifier);
    });
}

void WorkerSWClientConnection::finishFetchingScriptInServer(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, ServiceWorkerRegistrationKey&& registrationKey, WorkerFetchResult&& result)
{
    callOnMainThread([jobDataIdentifier, registrationKey = crossThreadCopy(WTFMove(registrationKey)), result = crossThreadCopy(WTFMove(result))]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.finishFetchingScriptInServer(jobDataIdentifier, WTFMove(registrationKey), WTFMove(result));
    });
}

void WorkerSWClientConnection::scheduleJob(ServiceWorkerOrClientIdentifier identifier, const ServiceWorkerJobData& data)
{
    callOnMainThread([identifier, data = crossThreadCopy(data)]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.scheduleJob(identifier, data);
    });
}

void WorkerSWClientConnection::scheduleUnregisterJobInServer(ServiceWorkerRegistrationIdentifier registrationIdentifier, ServiceWorkerOrClientIdentifier contextIdentifier, CompletionHandler<void(ExceptionOr<bool>&&)>&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_unregisterRequests.add(requestIdentifier, WTFMove(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier, contextIdentifier]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.scheduleUnregisterJobInServer(registrationIdentifier, contextIdentifier, [thread = WTFMove(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTFMove(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_unregisterRequests.take(requestIdentifier);
                callback(WTFMove(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::scheduleJobInServer(const ServiceWorkerJobData&)
{
    ASSERT_NOT_REACHED();
}

void WorkerSWClientConnection::subscribeToPushService(ServiceWorkerRegistrationIdentifier registrationIdentifier, const Vector<uint8_t>& applicationServerKey, SubscribeToPushServiceCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_subscribeToPushServiceRequests.add(requestIdentifier, WTFMove(callback));
    
    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier, applicationServerKey]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.subscribeToPushService(registrationIdentifier, applicationServerKey, [thread = WTFMove(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTFMove(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_subscribeToPushServiceRequests.take(requestIdentifier);
                callback(WTFMove(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::unsubscribeFromPushService(ServiceWorkerRegistrationIdentifier registrationIdentifier, PushSubscriptionIdentifier subscriptionIdentifier, UnsubscribeFromPushServiceCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_unsubscribeFromPushServiceRequests.add(requestIdentifier, WTFMove(callback));
    
    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier, subscriptionIdentifier]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.unsubscribeFromPushService(registrationIdentifier, subscriptionIdentifier, [thread = WTFMove(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTFMove(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_unsubscribeFromPushServiceRequests.take(requestIdentifier);
                callback(WTFMove(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::getPushSubscription(ServiceWorkerRegistrationIdentifier registrationIdentifier, GetPushSubscriptionCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_getPushSubscriptionRequests.add(requestIdentifier, WTFMove(callback));
    
    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.getPushSubscription(registrationIdentifier, [thread = WTFMove(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTFMove(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_getPushSubscriptionRequests.take(requestIdentifier);
                callback(WTFMove(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::getPushPermissionState(ServiceWorkerRegistrationIdentifier registrationIdentifier, GetPushPermissionStateCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_getPushPermissionStateCallbacks.add(requestIdentifier, WTFMove(callback));
    
    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.getPushPermissionState(registrationIdentifier, [thread = WTFMove(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTFMove(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_getPushPermissionStateCallbacks.take(requestIdentifier);
                callback(WTFMove(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::getNotifications(const URL& serviceWorkerRegistrationURL, const String& tag, GetNotificationsCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_getNotificationsCallbacks.add(requestIdentifier, WTFMove(callback));
    
    callOnMainThread([thread = m_thread, requestIdentifier, serviceWorkerRegistrationURL = serviceWorkerRegistrationURL.isolatedCopy(), tag = tag.isolatedCopy()]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.getNotifications(serviceWorkerRegistrationURL, tag, [thread = WTFMove(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTFMove(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_getNotificationsCallbacks.take(requestIdentifier);
                callback(WTFMove(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::enableNavigationPreload(ServiceWorkerRegistrationIdentifier registrationIdentifier, ExceptionOrVoidCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_voidCallbacks.add(requestIdentifier, WTFMove(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.enableNavigationPreload(registrationIdentifier, [thread = WTFMove(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTFMove(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_voidCallbacks.take(requestIdentifier);
                callback(WTFMove(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::disableNavigationPreload(ServiceWorkerRegistrationIdentifier registrationIdentifier, ExceptionOrVoidCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_voidCallbacks.add(requestIdentifier, WTFMove(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.disableNavigationPreload(registrationIdentifier, [thread = WTFMove(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTFMove(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_voidCallbacks.take(requestIdentifier);
                callback(WTFMove(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::setNavigationPreloadHeaderValue(ServiceWorkerRegistrationIdentifier registrationIdentifier, String&& headerValue, ExceptionOrVoidCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_voidCallbacks.add(requestIdentifier, WTFMove(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier, headerValue = WTFMove(headerValue).isolatedCopy()]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.setNavigationPreloadHeaderValue(registrationIdentifier, WTFMove(headerValue), [thread = WTFMove(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTFMove(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_voidCallbacks.take(requestIdentifier);
                callback(WTFMove(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::getNavigationPreloadState(ServiceWorkerRegistrationIdentifier registrationIdentifier, ExceptionOrNavigationPreloadStateCallback&& callback)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_navigationPreloadStateCallbacks.add(requestIdentifier, WTFMove(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier]() mutable {
        auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection.getNavigationPreloadState(registrationIdentifier, [thread = WTFMove(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTFMove(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_navigationPreloadStateCallbacks.take(requestIdentifier);
                callback(WTFMove(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
