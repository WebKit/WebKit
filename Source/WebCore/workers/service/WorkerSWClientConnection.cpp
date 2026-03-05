/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#include "BackgroundFetchInformation.h"
#include "BackgroundFetchOptions.h"
#include "BackgroundFetchRecordInformation.h"
#include "BackgroundFetchRequest.h"
#include "CacheQueryOptions.h"
#include "CookieChangeSubscription.h"
#include "ExceptionOr.h"
#include "NotificationData.h"
#include "RetrieveRecordsOptions.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerClientData.h"
#include "ServiceWorkerJobData.h"
#include "ServiceWorkerProvider.h"
#include "ServiceWorkerRegistration.h"
#include "ServiceWorkerRoute.h"
#include "WorkerFetchResult.h"
#include "WorkerGlobalScope.h"
#include "WorkerThread.h"
#include <wtf/Vector.h>

namespace WebCore {

WorkerSWClientConnection::WorkerSWClientConnection(WorkerGlobalScope& scope)
    : m_thread(scope.thread())
{
}

WorkerSWClientConnection::~WorkerSWClientConnection()
{
    auto matchRegistrations = WTF::move(m_matchRegistrationRequests);
    for (auto& callback : matchRegistrations.values())
        callback({ });

    auto getRegistrationsRequests = WTF::move(m_getRegistrationsRequests);
    for (auto& callback : getRegistrationsRequests.values())
        callback({ });

    auto unregisterRequests = WTF::move(m_unregisterRequests);
    for (auto& callback : unregisterRequests.values())
        callback(Exception { ExceptionCode::TypeError, "context stopped"_s });

    auto subscribeToPushServiceRequests = WTF::move(m_subscribeToPushServiceRequests);
    for (auto& callback : subscribeToPushServiceRequests.values())
        callback(Exception { ExceptionCode::AbortError, "context stopped"_s });

    auto unsubscribeFromPushServiceRequests = WTF::move(m_unsubscribeFromPushServiceRequests);
    for (auto& callback : unsubscribeFromPushServiceRequests.values())
        callback(Exception { ExceptionCode::AbortError, "context stopped"_s });

    auto getPushSubscriptionRequests = WTF::move(m_getPushSubscriptionRequests);
    for (auto& callback : getPushSubscriptionRequests.values())
        callback(Exception { ExceptionCode::AbortError, "context stopped"_s });

    auto getPushPermissionStateCallbacks = WTF::move(m_getPushPermissionStateCallbacks);
    for (auto& callback : getPushPermissionStateCallbacks.values())
        callback(Exception { ExceptionCode::AbortError, "context stopped"_s });

    auto voidCallbacks = WTF::move(m_voidCallbacks);
    for (auto& callback : voidCallbacks.values())
        callback(Exception { ExceptionCode::AbortError, "context stopped"_s });

    auto navigationPreloadStateCallbacks = WTF::move(m_navigationPreloadStateCallbacks);
    for (auto& callback : navigationPreloadStateCallbacks.values())
        callback(Exception { ExceptionCode::AbortError, "context stopped"_s });

#if ENABLE(NOTIFICATION_EVENT)
    auto getNotificationsCallbacks = WTF::move(m_getNotificationsCallbacks);
    for (auto& callback : getNotificationsCallbacks.values())
        callback(Exception { ExceptionCode::AbortError, "context stopped"_s });
#endif

    auto backgroundFetchInformationCallbacks = std::exchange(m_backgroundFetchInformationCallbacks, { });
    for (auto& callback : backgroundFetchInformationCallbacks.values())
        callback(Exception { ExceptionCode::AbortError, "context stopped"_s });

    auto backgroundFetchIdentifiersCallbacks = std::exchange(m_backgroundFetchIdentifiersCallbacks, { });
    for (auto& callback : backgroundFetchIdentifiersCallbacks.values())
        callback({ });

    auto abortBackgroundFetchCallbacks = std::exchange(m_abortBackgroundFetchCallbacks, { });
    for (auto& callback : m_abortBackgroundFetchCallbacks.values())
        callback(false);

    auto matchBackgroundFetchCallbacks = std::exchange(m_matchBackgroundFetchCallbacks, { });
    for (auto& callback : matchBackgroundFetchCallbacks.values())
        callback({ });

    auto retrieveRecordResponseCallbacks = std::exchange(m_retrieveRecordResponseCallbacks, { });
    for (auto& callback : retrieveRecordResponseCallbacks.values())
        callback(Exception { ExceptionCode::AbortError, "context stopped"_s });

    auto retrieveRecordResponseBodyCallbacks = std::exchange(m_retrieveRecordResponseBodyCallbacks, { });
    for (auto& callback : retrieveRecordResponseBodyCallbacks.values())
        callback(makeUnexpected(ResourceError { errorDomainWebKitInternal, 0, { }, "context stopped"_s }));

    auto cookieChangeSubscriptionsCallbacks = std::exchange(m_cookieChangeSubscriptionsCallback, { });
    for (auto& callback : cookieChangeSubscriptionsCallbacks.values())
        callback(Exception { ExceptionCode::AbortError, "context stopped"_s });
}

void WorkerSWClientConnection::matchRegistration(SecurityOriginData&& topOrigin, const URL& clientURL, RegistrationCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_matchRegistrationRequests.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, topOrigin = crossThreadCopy(WTF::move(topOrigin)), clientURL = crossThreadCopy(clientURL)]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->matchRegistration(WTF::move(topOrigin), clientURL, [thread = WTF::move(thread), requestIdentifier](std::optional<ServiceWorkerRegistrationData>&& result) mutable {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))] (auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_matchRegistrationRequests.take(requestIdentifier);
                callback(WTF::move(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::getRegistrations(SecurityOriginData&& topOrigin, const URL& clientURL, GetRegistrationsCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_getRegistrationsRequests.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, topOrigin = crossThreadCopy(WTF::move(topOrigin)), clientURL = crossThreadCopy(clientURL)]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->getRegistrations(WTF::move(topOrigin), clientURL, [thread = WTF::move(thread), requestIdentifier](Vector<ServiceWorkerRegistrationData>&& data) mutable {
            thread->runLoop().postTaskForMode([requestIdentifier, data = crossThreadCopy(WTF::move(data))] (auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_getRegistrationsRequests.take(requestIdentifier);
                callback(WTF::move(data));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::whenRegistrationReady(const SecurityOriginData& topOrigin, const URL& clientURL, WhenRegistrationReadyCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_whenRegistrationReadyRequests.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, topOrigin = topOrigin.isolatedCopy(), clientURL = crossThreadCopy(clientURL)]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->whenRegistrationReady(topOrigin, clientURL, [thread = WTF::move(thread), requestIdentifier](ServiceWorkerRegistrationData&& result) mutable {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))] (auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_whenRegistrationReadyRequests.take(requestIdentifier);
                callback(WTF::move(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::addServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    callOnMainThread([identifier]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->addServiceWorkerRegistrationInServer(identifier);
    });
}

void WorkerSWClientConnection::removeServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    callOnMainThread([identifier]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->removeServiceWorkerRegistrationInServer(identifier);
    });
}

void WorkerSWClientConnection::registerServiceWorkerInServer(ServiceWorkerIdentifier)
{
    ASSERT_NOT_REACHED();
}

void WorkerSWClientConnection::unregisterServiceWorkerInServer(ServiceWorkerIdentifier)
{
    ASSERT_NOT_REACHED();
}

void WorkerSWClientConnection::didResolveRegistrationPromise(const ServiceWorkerRegistrationKey& key)
{
    callOnMainThread([key = crossThreadCopy(key)]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->didResolveRegistrationPromise(key);
    });
}

void WorkerSWClientConnection::postMessageToServiceWorker(ServiceWorkerIdentifier destination, MessageWithMessagePorts&& ports, const ServiceWorkerOrClientIdentifier& source)
{
    callOnMainThreadAndWait([&] {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->postMessageToServiceWorker(destination, WTF::move(ports), source);
    });
}

SWServerConnectionIdentifier WorkerSWClientConnection::serverConnectionIdentifier() const
{
    std::optional<SWServerConnectionIdentifier> identifier;
    callOnMainThreadAndWait([&] {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        identifier = connection->serverConnectionIdentifier();
    });
    return *identifier;
}

bool WorkerSWClientConnection::mayHaveServiceWorkerRegisteredForOrigin(const SecurityOriginData&) const
{
    ASSERT_NOT_REACHED();
    return true;
}

void WorkerSWClientConnection::registerServiceWorkerClient(const ClientOrigin& clientOrigin, ServiceWorkerClientData&& data, const std::optional<ServiceWorkerRegistrationIdentifier>& identifier, String&& userAgent)
{
    callOnMainThread([clientOrigin = clientOrigin.isolatedCopy(), data = crossThreadCopy(WTF::move(data)), identifier, userAgent = crossThreadCopy(WTF::move(userAgent))]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->registerServiceWorkerClient(clientOrigin, WTF::move(data), identifier, WTF::move(userAgent));
    });
}

void WorkerSWClientConnection::unregisterServiceWorkerClient(ScriptExecutionContextIdentifier identifier)
{
    callOnMainThread([identifier] {
        if (RefPtr serviceWorkerConnection = ServiceWorkerProvider::singleton().existingServiceWorkerConnection())
            serviceWorkerConnection->unregisterServiceWorkerClient(identifier);
    });
}

void WorkerSWClientConnection::finishFetchingScriptInServer(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, ServiceWorkerRegistrationKey&& registrationKey, WorkerFetchResult&& result)
{
    callOnMainThread([jobDataIdentifier, registrationKey = crossThreadCopy(WTF::move(registrationKey)), result = crossThreadCopy(WTF::move(result))]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->finishFetchingScriptInServer(jobDataIdentifier, WTF::move(registrationKey), WTF::move(result));
    });
}

void WorkerSWClientConnection::scheduleJob(ServiceWorkerOrClientIdentifier identifier, const ServiceWorkerJobData& data)
{
    callOnMainThread([identifier, data = crossThreadCopy(data)]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->scheduleJob(identifier, data);
    });
}

void WorkerSWClientConnection::scheduleUnregisterJobInServer(ServiceWorkerRegistrationIdentifier registrationIdentifier, ServiceWorkerOrClientIdentifier contextIdentifier, CompletionHandler<void(ExceptionOr<bool>&&)>&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_unregisterRequests.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier, contextIdentifier]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->scheduleUnregisterJobInServer(registrationIdentifier, contextIdentifier, [thread = WTF::move(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_unregisterRequests.take(requestIdentifier);
                callback(WTF::move(result));
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
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_subscribeToPushServiceRequests.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier, applicationServerKey]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->subscribeToPushService(registrationIdentifier, applicationServerKey, [thread = WTF::move(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_subscribeToPushServiceRequests.take(requestIdentifier);
                callback(WTF::move(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::unsubscribeFromPushService(ServiceWorkerRegistrationIdentifier registrationIdentifier, PushSubscriptionIdentifier subscriptionIdentifier, UnsubscribeFromPushServiceCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_unsubscribeFromPushServiceRequests.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier, subscriptionIdentifier]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->unsubscribeFromPushService(registrationIdentifier, subscriptionIdentifier, [thread = WTF::move(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_unsubscribeFromPushServiceRequests.take(requestIdentifier);
                callback(WTF::move(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::getPushSubscription(ServiceWorkerRegistrationIdentifier registrationIdentifier, GetPushSubscriptionCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_getPushSubscriptionRequests.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->getPushSubscription(registrationIdentifier, [thread = WTF::move(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_getPushSubscriptionRequests.take(requestIdentifier);
                callback(WTF::move(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::getPushPermissionState(ServiceWorkerRegistrationIdentifier registrationIdentifier, GetPushPermissionStateCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_getPushPermissionStateCallbacks.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->getPushPermissionState(registrationIdentifier, [thread = WTF::move(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_getPushPermissionStateCallbacks.take(requestIdentifier);
                callback(WTF::move(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

#if ENABLE(NOTIFICATION_EVENT)
void WorkerSWClientConnection::getNotifications(const URL& serviceWorkerRegistrationURL, const String& tag, GetNotificationsCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_getNotificationsCallbacks.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, serviceWorkerRegistrationURL = serviceWorkerRegistrationURL.isolatedCopy(), tag = tag.isolatedCopy()]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->getNotifications(serviceWorkerRegistrationURL, tag, [thread = WTF::move(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_getNotificationsCallbacks.take(requestIdentifier);
                callback(WTF::move(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}
#endif

void WorkerSWClientConnection::enableNavigationPreload(ServiceWorkerRegistrationIdentifier registrationIdentifier, ExceptionOrVoidCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_voidCallbacks.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->enableNavigationPreload(registrationIdentifier, [thread = WTF::move(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_voidCallbacks.take(requestIdentifier);
                callback(WTF::move(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::disableNavigationPreload(ServiceWorkerRegistrationIdentifier registrationIdentifier, ExceptionOrVoidCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_voidCallbacks.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->disableNavigationPreload(registrationIdentifier, [thread = WTF::move(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_voidCallbacks.take(requestIdentifier);
                callback(WTF::move(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::setNavigationPreloadHeaderValue(ServiceWorkerRegistrationIdentifier registrationIdentifier, String&& headerValue, ExceptionOrVoidCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_voidCallbacks.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier, headerValue = WTF::move(headerValue).isolatedCopy()]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->setNavigationPreloadHeaderValue(registrationIdentifier, WTF::move(headerValue), [thread = WTF::move(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_voidCallbacks.take(requestIdentifier);
                callback(WTF::move(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::getNavigationPreloadState(ServiceWorkerRegistrationIdentifier registrationIdentifier, ExceptionOrNavigationPreloadStateCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_navigationPreloadStateCallbacks.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->getNavigationPreloadState(registrationIdentifier, [thread = WTF::move(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_navigationPreloadStateCallbacks.take(requestIdentifier);
                callback(WTF::move(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::startBackgroundFetch(ServiceWorkerRegistrationIdentifier registrationIdentifier, const String& backgroundFetchIdentifier, Vector<BackgroundFetchRequest>&& requests, BackgroundFetchOptions&& options, ExceptionOrBackgroundFetchInformationCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_backgroundFetchInformationCallbacks.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier, backgroundFetchIdentifier = backgroundFetchIdentifier.isolatedCopy(), requests = crossThreadCopy(WTF::move(requests)), options = WTF::move(options).isolatedCopy()]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->startBackgroundFetch(registrationIdentifier, backgroundFetchIdentifier, WTF::move(requests), WTF::move(options), [thread = WTF::move(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_backgroundFetchInformationCallbacks.take(requestIdentifier);
                callback(WTF::move(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::backgroundFetchInformation(ServiceWorkerRegistrationIdentifier registrationIdentifier, const String& backgroundFetchIdentifier, ExceptionOrBackgroundFetchInformationCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_backgroundFetchInformationCallbacks.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier, backgroundFetchIdentifier = backgroundFetchIdentifier.isolatedCopy()]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->backgroundFetchInformation(registrationIdentifier, backgroundFetchIdentifier, [thread = WTF::move(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_backgroundFetchInformationCallbacks.take(requestIdentifier);
                callback(WTF::move(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::backgroundFetchIdentifiers(ServiceWorkerRegistrationIdentifier registrationIdentifier, BackgroundFetchIdentifiersCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_backgroundFetchIdentifiersCallbacks.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->backgroundFetchIdentifiers(registrationIdentifier, [thread = WTF::move(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_backgroundFetchIdentifiersCallbacks.take(requestIdentifier);
                callback(WTF::move(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::abortBackgroundFetch(ServiceWorkerRegistrationIdentifier registrationIdentifier, const String& backgroundFetchIdentifier, AbortBackgroundFetchCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_abortBackgroundFetchCallbacks.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier, backgroundFetchIdentifier = backgroundFetchIdentifier.isolatedCopy()]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->abortBackgroundFetch(registrationIdentifier, backgroundFetchIdentifier, [thread = WTF::move(thread), requestIdentifier](bool result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_abortBackgroundFetchCallbacks.take(requestIdentifier);
                callback(result);
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::matchBackgroundFetch(ServiceWorkerRegistrationIdentifier registrationIdentifier, const String& backgroundFetchIdentifier, RetrieveRecordsOptions&& recordOptions, MatchBackgroundFetchCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_matchBackgroundFetchCallbacks.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier, backgroundFetchIdentifier = backgroundFetchIdentifier.isolatedCopy(), recordOptions = WTF::move(recordOptions).isolatedCopy()]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->matchBackgroundFetch(registrationIdentifier, backgroundFetchIdentifier, WTF::move(recordOptions), [thread = WTF::move(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_matchBackgroundFetchCallbacks.take(requestIdentifier);
                callback(WTF::move(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

static ExceptionOr<ResourceResponse::CrossThreadData> toCrossThreadData(ExceptionOr<ResourceResponse>&& data)
{
    if (data.hasException())
        return data.releaseException().isolatedCopy();
    return data.releaseReturnValue().crossThreadData();
}

static ExceptionOr<ResourceResponse> fromCrossThreadData(ExceptionOr<ResourceResponse::CrossThreadData>&& data)
{
    if (data.hasException())
        return data.releaseException();
    return ResourceResponse::fromCrossThreadData(data.releaseReturnValue());
}

void WorkerSWClientConnection::retrieveRecordResponse(BackgroundFetchRecordIdentifier recordIdentifier, RetrieveRecordResponseCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_retrieveRecordResponseCallbacks.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, recordIdentifier]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->retrieveRecordResponse(recordIdentifier, [thread = WTF::move(thread), requestIdentifier](ExceptionOr<ResourceResponse>&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = toCrossThreadData(WTF::move(result))](auto& scope) mutable {
                auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_retrieveRecordResponseCallbacks.take(requestIdentifier);
                callback(fromCrossThreadData(WTF::move(result)));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::retrieveRecordResponseBody(BackgroundFetchRecordIdentifier recordIdentifier, RetrieveRecordResponseBodyCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_retrieveRecordResponseBodyCallbacks.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, recordIdentifier]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->retrieveRecordResponseBody(recordIdentifier, [thread = WTF::move(thread), requestIdentifier](auto&& result) {
            RefPtr<SharedBuffer> buffer;
            ResourceError error;
            if (!result.has_value())
                error = WTF::move(result.error());
            else
                buffer = WTF::move(result.value());
            thread->runLoop().postTaskForMode([requestIdentifier, buffer = WTF::move(buffer), error = WTF::move(error).isolatedCopy()](auto& scope) mutable {
                auto& callbacks = downcast<WorkerGlobalScope>(scope).swClientConnection().m_retrieveRecordResponseBodyCallbacks;
                auto iterator = callbacks.find(requestIdentifier);
                ASSERT(iterator != callbacks.end());
                if (!error.isNull()) {
                    iterator->value(makeUnexpected(WTF::move(error)));
                    callbacks.remove(iterator);
                    return;
                }
                bool isDone = !buffer;
                iterator->value(WTF::move(buffer));
                if (isDone)
                    callbacks.remove(iterator);
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::addCookieChangeSubscriptions(ServiceWorkerRegistrationIdentifier registrationIdentifier, Vector<CookieChangeSubscription>&& subscriptions, ExceptionOrVoidCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_voidCallbacks.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier, subscriptions = crossThreadCopy(WTF::move(subscriptions))]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->addCookieChangeSubscriptions(registrationIdentifier, WTF::move(subscriptions), [thread = WTF::move(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))](auto& scope) mutable {
                if (auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_voidCallbacks.take(requestIdentifier))
                    callback(WTF::move(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::removeCookieChangeSubscriptions(ServiceWorkerRegistrationIdentifier registrationIdentifier, Vector<CookieChangeSubscription>&& subscriptions, ExceptionOrVoidCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_voidCallbacks.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier, subscriptions = crossThreadCopy(WTF::move(subscriptions))]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->removeCookieChangeSubscriptions(registrationIdentifier, WTF::move(subscriptions), [thread = WTF::move(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))](auto& scope) mutable {
                if (auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_voidCallbacks.take(requestIdentifier))
                    callback(WTF::move(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

void WorkerSWClientConnection::cookieChangeSubscriptions(ServiceWorkerRegistrationIdentifier registrationIdentifier, ExceptionOrCookieChangeSubscriptionsCallback&& callback)
{
    auto requestIdentifier = SWClientRequestIdentifier::generate();
    m_cookieChangeSubscriptionsCallback.add(requestIdentifier, WTF::move(callback));

    callOnMainThread([thread = m_thread, requestIdentifier, registrationIdentifier]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->cookieChangeSubscriptions(registrationIdentifier, [thread = WTF::move(thread), requestIdentifier](auto&& result) {
            thread->runLoop().postTaskForMode([requestIdentifier, result = crossThreadCopy(WTF::move(result))](auto& scope) mutable {
                if (auto callback = downcast<WorkerGlobalScope>(scope).swClientConnection().m_cookieChangeSubscriptionsCallback.take(requestIdentifier))
                    callback(WTF::move(result));
            }, WorkerRunLoop::defaultMode());
        });
    });
}

Ref<SWClientConnection::AddRoutePromise> WorkerSWClientConnection::addRoutes(ServiceWorkerRegistrationIdentifier identifier, Vector<ServiceWorkerRoute>&& routes)
{
    AddRoutePromise::Producer producer;
    Ref promise = producer.promise();
    callOnMainThread([producer = WTF::move(producer), identifier, routes = crossThreadCopy(WTF::move(routes))]() mutable {
        Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
        connection->addRoutes(identifier, WTF::move(routes))->chainTo(WTF::move(producer));
    });
    return promise;
}

} // namespace WebCore
