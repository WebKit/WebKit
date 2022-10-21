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
#include "ServiceWorkerThreadProxy.h"

#if ENABLE(SERVICE_WORKER)

#include "CacheStorageProvider.h"
#include "DocumentLoader.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FetchLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "LoaderStrategy.h"
#include "Logging.h"
#include "MessageWithMessagePorts.h"
#include "NotificationData.h"
#include "PlatformStrategies.h"
#include "ScriptExecutionContextIdentifier.h"
#include "ServiceWorkerClientData.h"
#include "ServiceWorkerGlobalScope.h"
#include "Settings.h"
#include "WebRTCProvider.h"
#include "WorkerGlobalScope.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

namespace WebCore {

static inline IDBClient::IDBConnectionProxy* idbConnectionProxy(Document& document)
{
    return document.idbConnectionProxy();
}

static HashSet<ServiceWorkerThreadProxy*>& allServiceWorkerThreadProxies()
{
    static NeverDestroyed<HashSet<ServiceWorkerThreadProxy*>> set;
    return set;
}

ServiceWorkerThreadProxy::ServiceWorkerThreadProxy(UniqueRef<Page>&& page, ServiceWorkerContextData&& contextData, ServiceWorkerData&& workerData, String&& userAgent, WorkerThreadMode workerThreadMode, CacheStorageProvider& cacheStorageProvider, std::unique_ptr<NotificationClient>&& notificationClient)
    : m_page(WTFMove(page))
    , m_document(*m_page->mainFrame().document())
#if ENABLE(REMOTE_INSPECTOR)
    , m_remoteDebuggable(makeUnique<ServiceWorkerDebuggable>(*this, contextData))
#endif
    , m_serviceWorkerThread(ServiceWorkerThread::create(WTFMove(contextData), WTFMove(workerData), WTFMove(userAgent), workerThreadMode, m_document->settingsValues(), *this, *this, idbConnectionProxy(m_document), m_document->socketProvider(), WTFMove(notificationClient), m_page->sessionID()))
    , m_cacheStorageProvider(cacheStorageProvider)
    , m_inspectorProxy(*this)
{
    static bool addedListener;
    if (!addedListener) {
        platformStrategies()->loaderStrategy()->addOnlineStateChangeListener(&networkStateChanged);
        addedListener = true;
    }

    ASSERT(!allServiceWorkerThreadProxies().contains(this));
    allServiceWorkerThreadProxies().add(this);

#if ENABLE(REMOTE_INSPECTOR)
    m_remoteDebuggable->setInspectable(true);
    m_remoteDebuggable->init();
#endif
}

ServiceWorkerThreadProxy::~ServiceWorkerThreadProxy()
{
    ASSERT(allServiceWorkerThreadProxies().contains(this));
    allServiceWorkerThreadProxies().remove(this);

    auto functionalEventTasks = WTFMove(m_ongoingFunctionalEventTasks);
    for (auto& callback : functionalEventTasks.values())
        callback(false);
}

void ServiceWorkerThreadProxy::setLastNavigationWasAppInitiated(bool wasAppInitiated)
{
    if (m_document->loader())
        m_document->loader()->setLastNavigationWasAppInitiated(wasAppInitiated);
}

bool ServiceWorkerThreadProxy::lastNavigationWasAppInitiated()
{
    return m_document->loader() ? m_document->loader()->lastNavigationWasAppInitiated() : true;
}

bool ServiceWorkerThreadProxy::postTaskForModeToWorkerOrWorkletGlobalScope(ScriptExecutionContext::Task&& task, const String& mode)
{
    if (m_isTerminatingOrTerminated)
        return false;

    m_serviceWorkerThread->runLoop().postTaskForMode(WTFMove(task), mode);
    return true;
}

ScriptExecutionContextIdentifier ServiceWorkerThreadProxy::loaderContextIdentifier() const
{
    return m_document->identifier();
}

void ServiceWorkerThreadProxy::postTaskToLoader(ScriptExecutionContext::Task&& task)
{
    callOnMainThread([task = WTFMove(task), this, protectedThis = Ref { *this }] () mutable {
        task.performTask(m_document.get());
    });
}

void ServiceWorkerThreadProxy::postMessageToDebugger(const String& message)
{
    RunLoop::main().dispatch([this, protectedThis = Ref { *this }, message = message.isolatedCopy()]() mutable {
        // FIXME: Handle terminated case.
        m_inspectorProxy.sendMessageFromWorkerToFrontend(WTFMove(message));
    });
}

void ServiceWorkerThreadProxy::setResourceCachingDisabledByWebInspector(bool disabled)
{
    postTaskToLoader([this, protectedThis = Ref { *this }, disabled] (ScriptExecutionContext&) {
        ASSERT(isMainThread());
        m_page->setResourceCachingDisabledByWebInspector(disabled);
    });   
}

RefPtr<CacheStorageConnection> ServiceWorkerThreadProxy::createCacheStorageConnection()
{
    ASSERT(isMainThread());
    if (!m_cacheStorageConnection)
        m_cacheStorageConnection = m_cacheStorageProvider.createCacheStorageConnection();
    return m_cacheStorageConnection;
}

RefPtr<RTCDataChannelRemoteHandlerConnection> ServiceWorkerThreadProxy::createRTCDataChannelRemoteHandlerConnection()
{
    ASSERT(isMainThread());
    return m_page->webRTCProvider().createRTCDataChannelRemoteHandlerConnection();
}

std::unique_ptr<FetchLoader> ServiceWorkerThreadProxy::createBlobLoader(FetchLoaderClient& client, const URL& blobURL)
{
    auto loader = makeUnique<FetchLoader>(client, nullptr);
    loader->startLoadingBlobURL(m_document, blobURL);
    if (!loader->isStarted())
        return nullptr;
    return loader;
}

void ServiceWorkerThreadProxy::networkStateChanged(bool isOnLine)
{
    for (auto* proxy : allServiceWorkerThreadProxies())
        proxy->notifyNetworkStateChange(isOnLine);
}

void ServiceWorkerThreadProxy::notifyNetworkStateChange(bool isOnline)
{
    if (m_isTerminatingOrTerminated)
        return;

    postTaskForModeToWorkerOrWorkletGlobalScope([isOnline] (ScriptExecutionContext& context) {
        auto& globalScope = downcast<WorkerGlobalScope>(context);
        globalScope.setIsOnline(isOnline);
        globalScope.eventLoop().queueTask(TaskSource::DOMManipulation, [globalScope = Ref { globalScope }, isOnline] {
            globalScope->dispatchEvent(Event::create(isOnline ? eventNames().onlineEvent : eventNames().offlineEvent, Event::CanBubble::No, Event::IsCancelable::No));
        });
    }, WorkerRunLoop::defaultMode());
}

static inline bool isValidFetch(const ResourceRequest& request, const FetchOptions& options, const URL& serviceWorkerURL, const String& referrer)
{
    // For exotic service workers, do not enforce checks.
    if (!serviceWorkerURL.protocolIsInHTTPFamily())
        return true;

    if (options.mode == FetchOptions::Mode::Navigate) {
        if (!protocolHostAndPortAreEqual(request.url(), serviceWorkerURL)) {
            RELEASE_LOG_ERROR(ServiceWorker, "Should not intercept a navigation load that is not same-origin as the service worker URL");
            RELEASE_ASSERT_WITH_MESSAGE(request.url().host() == serviceWorkerURL.host(), "Hosts do not match");
            RELEASE_ASSERT_WITH_MESSAGE(request.url().protocol() == serviceWorkerURL.protocol(), "Protocols do not match");
            RELEASE_ASSERT_WITH_MESSAGE(request.url().port() == serviceWorkerURL.port(), "Ports do not match");
            return false;
        }
        return true;
    }

    String origin = request.httpOrigin();
    URL url { origin.isEmpty() ? referrer : origin };
    if (url.protocolIsInHTTPFamily() && !protocolHostAndPortAreEqual(url, serviceWorkerURL)) {
        RELEASE_LOG_ERROR(ServiceWorker, "Should not intercept a non navigation load that is not originating from a same-origin context as the service worker URL");
        ASSERT(url.host() == serviceWorkerURL.host());
        ASSERT(url.protocol() == serviceWorkerURL.protocol());
        ASSERT(url.port() == serviceWorkerURL.port());
        return false;
    }
    return true;
}

void ServiceWorkerThreadProxy::startFetch(SWServerConnectionIdentifier connectionIdentifier, FetchIdentifier fetchIdentifier, Ref<ServiceWorkerFetch::Client>&& client, ResourceRequest&& request, String&& referrer, FetchOptions&& options, bool isServiceWorkerNavigationPreloadEnabled, String&& clientIdentifier, String&& resultingClientIdentifier)
{
    ASSERT(!isMainThread());

    callOnMainRunLoop([protectedThis = Ref { *this }] {
        protectedThis->thread().startFetchEventMonitoring();
    });

    postTaskForModeToWorkerOrWorkletGlobalScope([this, protectedThis = Ref { *this }, connectionIdentifier, client = WTFMove(client), request = request.isolatedCopy(), referrer = WTFMove(referrer).isolatedCopy(), options = WTFMove(options).isolatedCopy(), fetchIdentifier, isServiceWorkerNavigationPreloadEnabled, clientIdentifier = WTFMove(clientIdentifier).isolatedCopy(), resultingClientIdentifier = WTFMove(resultingClientIdentifier).isolatedCopy()] (auto& context) mutable {
        if (!isValidFetch(request, options, downcast<ServiceWorkerGlobalScope>(context).contextData().scriptURL, referrer)) {
            client->didNotHandle();
            return;
        }

        std::pair key { connectionIdentifier, fetchIdentifier };
        ASSERT(!m_ongoingFetchTasks.contains(key));
        m_ongoingFetchTasks.add(key, Ref { client });

        thread().queueTaskToFireFetchEvent(WTFMove(client), WTFMove(request), WTFMove(referrer), WTFMove(options), fetchIdentifier, isServiceWorkerNavigationPreloadEnabled, WTFMove(clientIdentifier), WTFMove(resultingClientIdentifier));
    }, WorkerRunLoop::defaultMode());
}

void ServiceWorkerThreadProxy::cancelFetch(SWServerConnectionIdentifier connectionIdentifier, FetchIdentifier fetchIdentifier)
{
    RELEASE_LOG(ServiceWorker, "ServiceWorkerThreadProxy::cancelFetch %llu", fetchIdentifier.toUInt64());

    postTaskForModeToWorkerOrWorkletGlobalScope([this, protectedThis = Ref { *this }, connectionIdentifier, fetchIdentifier] (auto&) {
        auto client = m_ongoingFetchTasks.take({ connectionIdentifier, fetchIdentifier });
        if (!client)
            return;

        if (m_ongoingFetchTasks.isEmpty()) {
            callOnMainRunLoop([protectedThis] {
                protectedThis->thread().stopFetchEventMonitoring();
            });
        }

        client->cancel();
    }, WorkerRunLoop::defaultMode());
}


void ServiceWorkerThreadProxy::convertFetchToDownload(SWServerConnectionIdentifier connectionIdentifier, FetchIdentifier fetchIdentifier)
{
    RELEASE_LOG(ServiceWorker, "ServiceWorkerThreadProxy::convertFetchToDownload %llu", fetchIdentifier.toUInt64());
    ASSERT(!isMainThread());

    postTaskForModeToWorkerOrWorkletGlobalScope([this, protectedThis = Ref { *this }, connectionIdentifier, fetchIdentifier] (auto&) {
        auto client = m_ongoingFetchTasks.take({ connectionIdentifier, fetchIdentifier });
        if (!client)
            return;

        client->convertFetchToDownload();
    }, WorkerRunLoop::defaultMode());
}

void ServiceWorkerThreadProxy::navigationPreloadIsReady(SWServerConnectionIdentifier connectionIdentifier, FetchIdentifier fetchIdentifier, ResourceResponse&& response)
{
    ASSERT(!isMainThread());
    postTaskForModeToWorkerOrWorkletGlobalScope([this, protectedThis = Ref { *this }, connectionIdentifier, fetchIdentifier, responseData = response.crossThreadData()] (auto&) mutable {
        if (auto client = m_ongoingFetchTasks.get({ connectionIdentifier, fetchIdentifier }))
            client->navigationPreloadIsReady(ResourceResponse::fromCrossThreadData(WTFMove(responseData)));
    }, WorkerRunLoop::defaultMode());
}

void ServiceWorkerThreadProxy::navigationPreloadFailed(SWServerConnectionIdentifier connectionIdentifier, FetchIdentifier fetchIdentifier, ResourceError&& error)
{
    ASSERT(!isMainThread());
    postTaskForModeToWorkerOrWorkletGlobalScope([this, protectedThis = Ref { *this }, connectionIdentifier, fetchIdentifier, error = WTFMove(error).isolatedCopy()] (auto&) mutable {
        if (auto client = m_ongoingFetchTasks.get({ connectionIdentifier, fetchIdentifier }))
            client->navigationPreloadFailed(WTFMove(error));
    }, WorkerRunLoop::defaultMode());
}

void ServiceWorkerThreadProxy::continueDidReceiveFetchResponse(SWServerConnectionIdentifier connectionIdentifier, FetchIdentifier fetchIdentifier)
{
    ASSERT(!isMainThread());

    postTaskForModeToWorkerOrWorkletGlobalScope([this, protectedThis = Ref { *this }, connectionIdentifier, fetchIdentifier] (auto&) {
        auto client = m_ongoingFetchTasks.get({ connectionIdentifier, fetchIdentifier });
        if (!client)
            return;

        client->continueDidReceiveResponse();
    }, WorkerRunLoop::defaultMode());
}

void ServiceWorkerThreadProxy::removeFetch(SWServerConnectionIdentifier connectionIdentifier, FetchIdentifier fetchIdentifier)
{
    RELEASE_LOG(ServiceWorker, "ServiceWorkerThreadProxy::removeFetch %llu", fetchIdentifier.toUInt64());

    postTaskForModeToWorkerOrWorkletGlobalScope([this, protectedThis = Ref { *this }, connectionIdentifier, fetchIdentifier] (auto&) {    m_ongoingFetchTasks.remove(std::make_pair(connectionIdentifier, fetchIdentifier));

        if (m_ongoingFetchTasks.isEmpty()) {
            callOnMainRunLoop([protectedThis] {
                protectedThis->thread().stopFetchEventMonitoring();
            });
        }
    }, WorkerRunLoop::defaultMode());
}

void ServiceWorkerThreadProxy::fireMessageEvent(MessageWithMessagePorts&& message, ServiceWorkerOrClientData&& sourceData)
{
    ASSERT(!isMainThread());

    callOnMainRunLoop([protectedThis = Ref { *this }] {
        protectedThis->thread().willPostTaskToFireMessageEvent();
    });

    thread().runLoop().postTask([this, protectedThis = Ref { *this }, message = WTFMove(message), sourceData = WTFMove(sourceData)](auto&) mutable {
        thread().queueTaskToPostMessage(WTFMove(message), WTFMove(sourceData));
    });
}

void ServiceWorkerThreadProxy::fireInstallEvent()
{
    ASSERT(isMainThread());
    thread().willPostTaskToFireInstallEvent();
    thread().runLoop().postTask([this, protectedThis = Ref { *this }](auto&) mutable {
        thread().queueTaskToFireInstallEvent();
    });
}

void ServiceWorkerThreadProxy::fireActivateEvent()
{
    ASSERT(isMainThread());
    thread().willPostTaskToFireActivateEvent();
    thread().runLoop().postTask([this, protectedThis = Ref { *this }](auto&) mutable {
        thread().queueTaskToFireActivateEvent();
    });
}

void ServiceWorkerThreadProxy::didSaveScriptsToDisk(ScriptBuffer&& script, HashMap<URL, ScriptBuffer>&& importedScripts)
{
    ASSERT(!isMainThread());

    thread().runLoop().postTask([script = WTFMove(script), importedScripts = WTFMove(importedScripts)](auto& context) mutable {
        downcast<ServiceWorkerGlobalScope>(context).didSaveScriptsToDisk(WTFMove(script), WTFMove(importedScripts));
    });
}

void ServiceWorkerThreadProxy::firePushEvent(std::optional<Vector<uint8_t>>&& data, CompletionHandler<void(bool)>&& callback)
{
    ASSERT(isMainThread());

    if (m_ongoingFunctionalEventTasks.isEmpty())
        thread().startFunctionalEventMonitoring();

    auto identifier = ++m_functionalEventTasksCounter;
    ASSERT(!m_ongoingFunctionalEventTasks.contains(identifier));
    m_ongoingFunctionalEventTasks.add(identifier, WTFMove(callback));
    bool isPosted = postTaskForModeToWorkerOrWorkletGlobalScope([this, protectedThis = Ref { *this }, identifier, data = WTFMove(data)](auto&) mutable {
        thread().queueTaskToFirePushEvent(WTFMove(data), [this, protectedThis = WTFMove(protectedThis), identifier](bool result) mutable {
            callOnMainThread([this, protectedThis = WTFMove(protectedThis), identifier, result]() mutable {
                if (auto callback = m_ongoingFunctionalEventTasks.take(identifier))
                    callback(result);
                if (m_ongoingFunctionalEventTasks.isEmpty())
                    thread().stopFunctionalEventMonitoring();
            });
        });
    }, WorkerRunLoop::defaultMode());
    if (!isPosted)
        m_ongoingFunctionalEventTasks.take(identifier)(false);
}

void ServiceWorkerThreadProxy::firePushSubscriptionChangeEvent(std::optional<PushSubscriptionData>&& newSubscriptionData, std::optional<PushSubscriptionData>&& oldSubscriptionData)
{
    ASSERT(isMainThread());

    thread().willPostTaskToFirePushSubscriptionChangeEvent();
    thread().runLoop().postTask([this, protectedThis = Ref { *this }, newSubscriptionData = crossThreadCopy(WTFMove(newSubscriptionData)), oldSubscriptionData = crossThreadCopy(WTFMove(oldSubscriptionData))](auto&) mutable {
        thread().queueTaskToFirePushSubscriptionChangeEvent(WTFMove(newSubscriptionData), WTFMove(oldSubscriptionData));
    });
}

void ServiceWorkerThreadProxy::fireNotificationEvent(NotificationData&& data, NotificationEventType eventType, CompletionHandler<void(bool)>&& callback)
{
    ASSERT(isMainThread());

#if ENABLE(NOTIFICATION_EVENT)
    if (m_ongoingFunctionalEventTasks.isEmpty())
        thread().startFunctionalEventMonitoring();

    auto identifier = ++m_functionalEventTasksCounter;
    ASSERT(!m_ongoingFunctionalEventTasks.contains(identifier));
    m_ongoingFunctionalEventTasks.add(identifier, WTFMove(callback));
    bool isPosted = postTaskForModeToWorkerOrWorkletGlobalScope([this, protectedThis = Ref { *this }, identifier, data = WTFMove(data).isolatedCopy(), eventType](auto&) mutable {
        thread().queueTaskToFireNotificationEvent(WTFMove(data), eventType, [this, protectedThis = WTFMove(protectedThis), identifier](bool result) mutable {
            callOnMainThread([this, protectedThis = WTFMove(protectedThis), identifier, result]() mutable {
                if (auto callback = m_ongoingFunctionalEventTasks.take(identifier))
                    callback(result);
                if (m_ongoingFunctionalEventTasks.isEmpty())
                    thread().stopFunctionalEventMonitoring();
            });
        });
    }, WorkerRunLoop::defaultMode());
    if (!isPosted)
        m_ongoingFunctionalEventTasks.take(identifier)(false);
#else
    UNUSED_PARAM(data);
    UNUSED_PARAM(eventType);
    callback(false);
#endif
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
