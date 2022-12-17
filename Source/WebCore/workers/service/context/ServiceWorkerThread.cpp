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
#include "ServiceWorkerThread.h"

#if ENABLE(SERVICE_WORKER)

#include "CacheStorageProvider.h"
#include "CommonAtomStrings.h"
#include "ContentSecurityPolicyResponseHeaders.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "ExtendableMessageEvent.h"
#include "JSDOMPromise.h"
#include "LoaderStrategy.h"
#include "Logging.h"
#include "NotificationData.h"
#include "NotificationEvent.h"
#include "PlatformStrategies.h"
#include "PushEvent.h"
#include "PushSubscription.h"
#include "PushSubscriptionChangeEvent.h"
#include "SWContextManager.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerFetch.h"
#include "ServiceWorkerGlobalScope.h"
#include "ServiceWorkerWindowClient.h"
#include "WorkerDebuggerProxy.h"
#include "WorkerLoaderProxy.h"
#include "WorkerObjectProxy.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/RuntimeFlags.h>
#include <wtf/NeverDestroyed.h>

using namespace PAL;

namespace WebCore {

class DummyServiceWorkerThreadProxy : public WorkerObjectProxy {
public:
    static DummyServiceWorkerThreadProxy& shared()
    {
        static NeverDestroyed<DummyServiceWorkerThreadProxy> proxy;
        return proxy;
    }

private:
    void postExceptionToWorkerObject(const String&, int, int, const String&) final { };
    void workerGlobalScopeDestroyed() final { };
    void postMessageToWorkerObject(MessageWithMessagePorts&&) final { };
};

// FIXME: Use a valid WorkerReportingProxy
// FIXME: Use a valid WorkerObjectProxy
// FIXME: Use valid runtime flags

static WorkerParameters generateWorkerParameters(const ServiceWorkerContextData& contextData, String&& userAgent, WorkerThreadMode workerThreadMode, const Settings::Values& settingsValues, PAL::SessionID sessionID)
{
    return {
        contextData.scriptURL,
        URL(), // FIXME: Should pass owner URL.
        emptyString(),
        "serviceworker:" + Inspector::IdentifiersFactory::createIdentifier(),
        WTFMove(userAgent),
        platformStrategies()->loaderStrategy()->isOnLine(),
        contextData.contentSecurityPolicy,
        false,
        contextData.crossOriginEmbedderPolicy,
        MonotonicTime::now(),
        { },
        contextData.workerType,
        FetchRequestCredentials::Omit,
        settingsValues,
        workerThreadMode,
        sessionID,
        { },
        { }
    };
}

ServiceWorkerThread::ServiceWorkerThread(ServiceWorkerContextData&& contextData, ServiceWorkerData&& workerData, String&& userAgent, WorkerThreadMode workerThreadMode, const Settings::Values& settingsValues, WorkerLoaderProxy& loaderProxy, WorkerDebuggerProxy& debuggerProxy, WorkerBadgeProxy& badgeProxy, IDBClient::IDBConnectionProxy* idbConnectionProxy, SocketProvider* socketProvider, std::unique_ptr<NotificationClient>&& notificationClient, PAL::SessionID sessionID)
    : WorkerThread(generateWorkerParameters(contextData, WTFMove(userAgent), workerThreadMode, settingsValues, sessionID), contextData.script, loaderProxy, debuggerProxy, DummyServiceWorkerThreadProxy::shared(), badgeProxy, WorkerThreadStartMode::Normal, contextData.registration.key.topOrigin().securityOrigin().get(), idbConnectionProxy, socketProvider, JSC::RuntimeFlags::createAllEnabled())
    , m_serviceWorkerIdentifier(contextData.serviceWorkerIdentifier)
    , m_jobDataIdentifier(contextData.jobDataIdentifier)
    , m_contextData(crossThreadCopy(WTFMove(contextData)))
    , m_workerData(crossThreadCopy(WTFMove(workerData)))
    , m_workerObjectProxy(DummyServiceWorkerThreadProxy::shared())
    , m_heartBeatTimeout(settingsValues.shouldUseServiceWorkerShortTimeout ? heartBeatTimeoutForTest : heartBeatTimeout)
    , m_heartBeatTimer { *this, &ServiceWorkerThread::heartBeatTimerFired }
    , m_notificationClient(WTFMove(notificationClient))
{
    ASSERT(isMainThread());
    initializeCommonAtomStrings();
}

ServiceWorkerThread::~ServiceWorkerThread() = default;

Ref<WorkerGlobalScope> ServiceWorkerThread::createWorkerGlobalScope(const WorkerParameters& params, Ref<SecurityOrigin>&& origin, Ref<SecurityOrigin>&& topOrigin)
{
    RELEASE_ASSERT(m_contextData);
    return ServiceWorkerGlobalScope::create(*std::exchange(m_contextData, std::nullopt), *std::exchange(m_workerData, std::nullopt), params, WTFMove(origin), *this, WTFMove(topOrigin), idbConnectionProxy(), socketProvider(), WTFMove(m_notificationClient));
}

void ServiceWorkerThread::runEventLoop()
{
    // FIXME: There will be ServiceWorker specific things to do here.
    WorkerThread::runEventLoop();
}

void ServiceWorkerThread::queueTaskToFireFetchEvent(Ref<ServiceWorkerFetch::Client>&& client, ResourceRequest&& request, String&& referrer, FetchOptions&& options, FetchIdentifier fetchIdentifier, bool isServiceWorkerNavigationPreloadEnabled, String&& clientIdentifier, String&& resultingClientIdentifier)
{
    Ref serviceWorkerGlobalScope = downcast<ServiceWorkerGlobalScope>(*globalScope());
    serviceWorkerGlobalScope->eventLoop().queueTask(TaskSource::DOMManipulation, [serviceWorkerGlobalScope, client = WTFMove(client), request = WTFMove(request), referrer = WTFMove(referrer), options = WTFMove(options), fetchIdentifier, isServiceWorkerNavigationPreloadEnabled, clientIdentifier = WTFMove(clientIdentifier), resultingClientIdentifier = WTFMove(resultingClientIdentifier)]() mutable {
        ServiceWorkerFetch::dispatchFetchEvent(WTFMove(client), serviceWorkerGlobalScope, WTFMove(request), WTFMove(referrer), WTFMove(options), fetchIdentifier, isServiceWorkerNavigationPreloadEnabled, WTFMove(clientIdentifier), WTFMove(resultingClientIdentifier));
    });
}

static void fireMessageEvent(ServiceWorkerGlobalScope& scope, MessageWithMessagePorts&& message, ExtendableMessageEventSource&& source, const URL& sourceURL)
{
    auto ports = MessagePort::entanglePorts(scope, WTFMove(message.transferredPorts));
    // FIXME: Add support for messageerror event when message deserialization fails.
    auto messageEvent = ExtendableMessageEvent::create(WTFMove(ports), WTFMove(message.message), SecurityOriginData::fromURL(sourceURL).toString(), { }, source);
    scope.dispatchEvent(messageEvent);
    scope.updateExtendedEventsSet(messageEvent.ptr());
}

void ServiceWorkerThread::queueTaskToPostMessage(MessageWithMessagePorts&& message, ServiceWorkerOrClientData&& sourceData)
{
    Ref serviceWorkerGlobalScope = downcast<ServiceWorkerGlobalScope>(*globalScope());
    serviceWorkerGlobalScope->eventLoop().queueTask(TaskSource::DOMManipulation, [weakThis = WeakPtr { *this }, serviceWorkerGlobalScope, message = WTFMove(message), sourceData = WTFMove(sourceData)]() mutable {
        URL sourceURL;
        ExtendableMessageEventSource source;
        if (std::holds_alternative<ServiceWorkerClientData>(sourceData)) {
            RefPtr<ServiceWorkerClient> sourceClient = ServiceWorkerClient::create(serviceWorkerGlobalScope, WTFMove(std::get<ServiceWorkerClientData>(sourceData)));

            RELEASE_ASSERT(!sourceClient->url().protocolIsInHTTPFamily() || !serviceWorkerGlobalScope->url().protocolIsInHTTPFamily() || protocolHostAndPortAreEqual(serviceWorkerGlobalScope->url(), sourceClient->url()));

            sourceURL = sourceClient->url();
            source = WTFMove(sourceClient);
        } else {
            RefPtr<ServiceWorker> sourceWorker = ServiceWorker::getOrCreate(serviceWorkerGlobalScope, WTFMove(std::get<ServiceWorkerData>(sourceData)));

            RELEASE_ASSERT(!sourceWorker->scriptURL().protocolIsInHTTPFamily() || !serviceWorkerGlobalScope->url().protocolIsInHTTPFamily() || protocolHostAndPortAreEqual(serviceWorkerGlobalScope->url(), sourceWorker->scriptURL()));

            sourceURL = sourceWorker->scriptURL();
            source = WTFMove(sourceWorker);
        }
        fireMessageEvent(serviceWorkerGlobalScope, WTFMove(message), ExtendableMessageEventSource { source }, sourceURL);
        callOnMainThread([weakThis = WTFMove(weakThis)] {
            if (weakThis)
                weakThis->finishedFiringMessageEvent();
        });
    });
}

void ServiceWorkerThread::queueTaskToFireInstallEvent()
{
    Ref serviceWorkerGlobalScope = downcast<ServiceWorkerGlobalScope>(*globalScope());
    serviceWorkerGlobalScope->eventLoop().queueTask(TaskSource::DOMManipulation, [weakThis = WeakPtr { *this }, serviceWorkerGlobalScope]() mutable {
        RELEASE_LOG(ServiceWorker, "ServiceWorkerThread::queueTaskToFireInstallEvent firing event for worker %llu", serviceWorkerGlobalScope->thread().identifier().toUInt64());

        auto installEvent = ExtendableEvent::create(eventNames().installEvent, { }, ExtendableEvent::IsTrusted::Yes);
        serviceWorkerGlobalScope->dispatchEvent(installEvent);

        installEvent->whenAllExtendLifetimePromisesAreSettled([weakThis = WTFMove(weakThis)](HashSet<Ref<DOMPromise>>&& extendLifetimePromises) mutable {
            bool hasRejectedAnyPromise = false;
            for (auto& promise : extendLifetimePromises) {
                if (promise->status() == DOMPromise::Status::Rejected) {
                    hasRejectedAnyPromise = true;
                    break;
                }
            }
            callOnMainThread([weakThis = WTFMove(weakThis), hasRejectedAnyPromise] {
                RELEASE_LOG(ServiceWorker, "ServiceWorkerThread::queueTaskToFireInstallEvent finishing for worker %llu", weakThis ? weakThis->identifier().toUInt64() : 0);
                if (weakThis)
                    weakThis->finishedFiringInstallEvent(hasRejectedAnyPromise);
            });
        });
    });
}

void ServiceWorkerThread::queueTaskToFireActivateEvent()
{
    Ref serviceWorkerGlobalScope = downcast<ServiceWorkerGlobalScope>(*globalScope());
    serviceWorkerGlobalScope->eventLoop().queueTask(TaskSource::DOMManipulation, [weakThis = WeakPtr { *this }, serviceWorkerGlobalScope]() mutable {
        RELEASE_LOG(ServiceWorker, "ServiceWorkerThread::queueTaskToFireActivateEvent firing event for worker %llu", serviceWorkerGlobalScope->thread().identifier().toUInt64());

        auto activateEvent = ExtendableEvent::create(eventNames().activateEvent, { }, ExtendableEvent::IsTrusted::Yes);
        serviceWorkerGlobalScope->dispatchEvent(activateEvent);

        activateEvent->whenAllExtendLifetimePromisesAreSettled([weakThis = WTFMove(weakThis)](auto&&) mutable {
            callOnMainThread([weakThis = WTFMove(weakThis)] {
                RELEASE_LOG(ServiceWorker, "ServiceWorkerThread::queueTaskToFireActivateEvent finishing for worker %llu", weakThis ? weakThis->identifier().toUInt64() : 0);
                if (weakThis)
                    weakThis->finishedFiringActivateEvent();
            });
        });
    });
}

void ServiceWorkerThread::queueTaskToFirePushEvent(std::optional<Vector<uint8_t>>&& data, Function<void(bool)>&& callback)
{
    auto& serviceWorkerGlobalScope = downcast<ServiceWorkerGlobalScope>(*globalScope());
    serviceWorkerGlobalScope.eventLoop().queueTask(TaskSource::DOMManipulation, [weakThis = WeakPtr { *this }, serviceWorkerGlobalScope = Ref { serviceWorkerGlobalScope }, data = WTFMove(data), callback = WTFMove(callback)]() mutable {
        RELEASE_LOG(ServiceWorker, "ServiceWorkerThread::queueTaskToFirePushEvent firing event for worker %" PRIu64, serviceWorkerGlobalScope->thread().identifier().toUInt64());

        serviceWorkerGlobalScope->setHasPendingSilentPushEvent(true);

        auto pushEvent = PushEvent::create(eventNames().pushEvent, { }, WTFMove(data), ExtendableEvent::IsTrusted::Yes);
        serviceWorkerGlobalScope->dispatchPushEvent(pushEvent);

        pushEvent->whenAllExtendLifetimePromisesAreSettled([serviceWorkerGlobalScope, eventCreationTime = pushEvent->timeStamp(), callback = WTFMove(callback)](auto&& extendLifetimePromises) mutable {
            bool hasRejectedAnyPromise = false;
            for (auto& promise : extendLifetimePromises) {
                if (promise->status() == DOMPromise::Status::Rejected) {
                    hasRejectedAnyPromise = true;
                    break;
                }
            }

            bool showedNotification = !serviceWorkerGlobalScope->hasPendingSilentPushEvent();
            bool success = !hasRejectedAnyPromise && showedNotification;

            RELEASE_LOG_ERROR_IF(!success, ServiceWorker, "ServiceWorkerThread::queueTaskToFirePushEvent failed to process push event (rejectedPromise = %d, showedNotification = %d)", hasRejectedAnyPromise, showedNotification);

            callback(success);
        });
    });
}

void ServiceWorkerThread::queueTaskToFirePushSubscriptionChangeEvent(std::optional<PushSubscriptionData>&& newSubscriptionData, std::optional<PushSubscriptionData>&& oldSubscriptionData)
{
    auto& serviceWorkerGlobalScope = downcast<ServiceWorkerGlobalScope>(*globalScope());
    serviceWorkerGlobalScope.eventLoop().queueTask(TaskSource::DOMManipulation, [weakThis = WeakPtr { *this }, serviceWorkerGlobalScope = Ref { serviceWorkerGlobalScope }, newSubscriptionData = WTFMove(newSubscriptionData), oldSubscriptionData = WTFMove(oldSubscriptionData)]() mutable {
        RELEASE_LOG(ServiceWorker, "ServiceWorkerThread::queueTaskToFirePushSubscriptionChangeEvent firing event for worker %" PRIu64, serviceWorkerGlobalScope->thread().identifier().toUInt64());

        RefPtr<PushSubscription> newSubscription;
        RefPtr<PushSubscription> oldSubscription;

        if (newSubscriptionData)
            newSubscription = PushSubscription::create(WTFMove(*newSubscriptionData), &serviceWorkerGlobalScope->registration());
        if (oldSubscriptionData)
            oldSubscription = PushSubscription::create(WTFMove(*oldSubscriptionData));

        auto pushSubscriptionChangeEvent = PushSubscriptionChangeEvent::create(eventNames().pushsubscriptionchangeEvent, { }, WTFMove(newSubscription), WTFMove(oldSubscription), ExtendableEvent::IsTrusted::Yes);
        serviceWorkerGlobalScope->dispatchEvent(pushSubscriptionChangeEvent);

        pushSubscriptionChangeEvent->whenAllExtendLifetimePromisesAreSettled([weakThis = WTFMove(weakThis)](auto&&) mutable {
            callOnMainThread([weakThis = WTFMove(weakThis)] {
                RELEASE_LOG(ServiceWorker, "ServiceWorkerThread::queueTaskToFirePushSubscriptionChangeEvent finishing for worker %llu", weakThis ? weakThis->identifier().toUInt64() : 0);
                if (weakThis)
                    weakThis->finishedFiringPushSubscriptionChangeEvent();
            });
        });
    });
}

#if ENABLE(NOTIFICATION_EVENT)
void ServiceWorkerThread::queueTaskToFireNotificationEvent(NotificationData&& data, NotificationEventType eventType, Function<void(bool)>&& callback)
{
    auto& serviceWorkerGlobalScope = downcast<ServiceWorkerGlobalScope>(*globalScope());
    serviceWorkerGlobalScope.eventLoop().queueTask(TaskSource::DOMManipulation, [weakThis = WeakPtr { *this }, serviceWorkerGlobalScope = Ref { serviceWorkerGlobalScope }, data = WTFMove(data), eventType, callback = WTFMove(callback)]() mutable {
        RELEASE_LOG(ServiceWorker, "ServiceWorkerThread::queueTaskToFireNotificationEvent firing event for worker %" PRIu64, serviceWorkerGlobalScope->thread().identifier().toUInt64());

        auto notification = Notification::create(serviceWorkerGlobalScope.get(), WTFMove(data));
        AtomString eventName;
        switch (eventType) {
        case NotificationEventType::Click:
            eventName = eventNames().notificationclickEvent;
            notification->markAsShown();
            serviceWorkerGlobalScope->recordUserGesture();
            break;
        case NotificationEventType::Close:
            eventName = eventNames().notificationcloseEvent;
            break;
        }

        auto notificationEvent = NotificationEvent::create(eventName, notification.ptr(), emptyString(), ExtendableEvent::IsTrusted::Yes);
        serviceWorkerGlobalScope->dispatchEvent(notificationEvent);

        notificationEvent->whenAllExtendLifetimePromisesAreSettled([serviceWorkerGlobalScope, callback = WTFMove(callback)](auto&& extendLifetimePromises) mutable {
            bool success = true;
            for (auto& promise : extendLifetimePromises) {
                if (promise->status() == DOMPromise::Status::Rejected) {
                    success = false;
                    break;
                }
            }

            RELEASE_LOG_ERROR_IF(!success, ServiceWorker, "ServiceWorkerThread::queueTaskToFireNotificationEvent failed to process notification event");
            callback(success);
        });
    });
}
#endif

void ServiceWorkerThread::finishedEvaluatingScript()
{
    ASSERT(globalScope()->isContextThread());
    m_doesHandleFetch = globalScope()->hasEventListeners(eventNames().fetchEvent);
}

void ServiceWorkerThread::start(Function<void(const String&, bool)>&& callback)
{
    m_state = State::Starting;
    startHeartBeatTimer();

    WorkerThread::start([callback = WTFMove(callback), weakThis = WeakPtr { *this }](auto& errorMessage) mutable {
#ifndef NDEBUG
        if (!errorMessage.isEmpty())
            LOG(ServiceWorker, "Service worker thread failed to start: %s", errorMessage.utf8().data());
#endif
        bool doesHandleFetch = true;
        if (weakThis) {
            weakThis->finishedStarting();
            doesHandleFetch = weakThis->doesHandleFetch();
        }
        callback(errorMessage, doesHandleFetch);
    });
}

void ServiceWorkerThread::finishedStarting()
{
    m_state = State::Idle;
}

void ServiceWorkerThread::startFetchEventMonitoring()
{
    m_isHandlingFetchEvent = true;
    startHeartBeatTimer();
}

void ServiceWorkerThread::startFunctionalEventMonitoring()
{
    m_isHandlingFunctionalEvent = true;
    startHeartBeatTimer();
}

void ServiceWorkerThread::startHeartBeatTimer()
{
    // We cannot detect responsiveness for service workers running on the main thread by using a main thread timer.
    if (is<WorkerMainRunLoop>(runLoop()))
        return;

    if (m_heartBeatTimer.isActive())
        return;

    m_ongoingHeartBeatCheck = true;
    runLoop().postTask([this, protectedThis = Ref { *this }](auto&) mutable {
        callOnMainThread([this, protectedThis = WTFMove(protectedThis)]() {
            m_ongoingHeartBeatCheck = false;
        });
    });

    m_heartBeatTimer.startOneShot(m_heartBeatTimeout);
}

void ServiceWorkerThread::heartBeatTimerFired()
{
    if (!m_ongoingHeartBeatCheck) {
        if (m_state == State::Installing || m_state == State::Activating || m_isHandlingFetchEvent || m_isHandlingFunctionalEvent || m_pushSubscriptionChangeEventCount || m_messageEventCount)
            startHeartBeatTimer();
        return;
    }

    auto* serviceWorkerThreadProxy = SWContextManager::singleton().serviceWorkerThreadProxy(identifier());
    if (!serviceWorkerThreadProxy || serviceWorkerThreadProxy->isTerminatingOrTerminated())
        return;

    auto* connection = SWContextManager::singleton().connection();
    if (!connection)
        return;

    switch (m_state) {
    case State::Idle:
    case State::Activating:
        connection->didFailHeartBeatCheck(identifier());
        break;
    case State::Starting:
        connection->serviceWorkerFailedToStart(m_jobDataIdentifier, identifier(), "Service Worker script execution timed out"_s);
        break;
    case State::Installing:
        connection->didFinishInstall(m_jobDataIdentifier, identifier(), false);
        break;
    }
}

void ServiceWorkerThread::willPostTaskToFireInstallEvent()
{
    m_state = State::Installing;
    startHeartBeatTimer();
}

void ServiceWorkerThread::finishedFiringInstallEvent(bool hasRejectedAnyPromise)
{
    m_state = State::Idle;

    if (auto* connection = SWContextManager::singleton().connection())
        connection->didFinishInstall(m_jobDataIdentifier, identifier(), !hasRejectedAnyPromise);
}

void ServiceWorkerThread::willPostTaskToFireActivateEvent()
{
    m_state = State::Activating;
    startHeartBeatTimer();
}

void ServiceWorkerThread::finishedFiringActivateEvent()
{
    m_state = State::Idle;

    if (auto* connection = SWContextManager::singleton().connection())
        connection->didFinishActivation(identifier());
}

void ServiceWorkerThread::willPostTaskToFireMessageEvent()
{
    if (!m_messageEventCount++)
        startHeartBeatTimer();
}

void ServiceWorkerThread::finishedFiringMessageEvent()
{
    ASSERT(m_messageEventCount);
    --m_messageEventCount;
}

void ServiceWorkerThread::willPostTaskToFirePushSubscriptionChangeEvent()
{
    if (!m_pushSubscriptionChangeEventCount++)
        startHeartBeatTimer();
}

void ServiceWorkerThread::finishedFiringPushSubscriptionChangeEvent()
{
    ASSERT(m_pushSubscriptionChangeEventCount);
    --m_pushSubscriptionChangeEventCount;
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
