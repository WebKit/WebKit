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
#include "LibWebRTCProvider.h"
#include "LoaderStrategy.h"
#include "Logging.h"
#include "MessageWithMessagePorts.h"
#include "PlatformStrategies.h"
#include "ScriptExecutionContextIdentifier.h"
#include "ServiceWorkerClientData.h"
#include "ServiceWorkerGlobalScope.h"
#include "Settings.h"
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
    m_remoteDebuggable->setRemoteDebuggingAllowed(true);
    m_remoteDebuggable->init();
#endif
}

ServiceWorkerThreadProxy::~ServiceWorkerThreadProxy()
{
    ASSERT(allServiceWorkerThreadProxies().contains(this));
    allServiceWorkerThreadProxies().remove(this);

    auto pushTasks = WTFMove(m_ongoingPushTasks);
    for (auto& callback : pushTasks.values())
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

void ServiceWorkerThreadProxy::postTaskToLoader(ScriptExecutionContext::Task&& task)
{
    callOnMainThread([task = WTFMove(task), this, protectedThis = Ref { *this }] () mutable {
        task.performTask(m_document.get());
    });
}

void ServiceWorkerThreadProxy::postMessageToDebugger(const String& message)
{
    RunLoop::main().dispatch([this, protectedThis = Ref { *this }, message = message.isolatedCopy()] {
        // FIXME: Handle terminated case.
        m_inspectorProxy.sendMessageFromWorkerToFrontend(message);
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
    return m_page->libWebRTCProvider().createRTCDataChannelRemoteHandlerConnection();
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

void ServiceWorkerThreadProxy::startFetch(SWServerConnectionIdentifier connectionIdentifier, FetchIdentifier fetchIdentifier, Ref<ServiceWorkerFetch::Client>&& client, ResourceRequest&& request, String&& referrer, FetchOptions&& options, bool isServiceWorkerNavigationPreloadEnabled, String&& clientIdentifier, String&& resultingClientIdentifier)
{
    RELEASE_LOG(ServiceWorker, "ServiceWorkerThreadProxy::startFetch %llu", fetchIdentifier.toUInt64());

    auto key = std::make_pair(connectionIdentifier, fetchIdentifier);

    if (m_ongoingFetchTasks.isEmpty())
        thread().startFetchEventMonitoring();

    ASSERT(!m_ongoingFetchTasks.contains(key));
    m_ongoingFetchTasks.add(key, client.copyRef());
    postTaskForModeToWorkerOrWorkletGlobalScope([this, protectedThis = Ref { *this }, client = WTFMove(client), request = request.isolatedCopy(), referrer = WTFMove(referrer).isolatedCopy(), options = options.isolatedCopy(), fetchIdentifier, isServiceWorkerNavigationPreloadEnabled, clientIdentifier = WTFMove(clientIdentifier).isolatedCopy(), resultingClientIdentifier = WTFMove(resultingClientIdentifier).isolatedCopy()](auto&) mutable {
        thread().queueTaskToFireFetchEvent(WTFMove(client), WTFMove(request), WTFMove(referrer), WTFMove(options), fetchIdentifier, isServiceWorkerNavigationPreloadEnabled, WTFMove(clientIdentifier), WTFMove(resultingClientIdentifier));
    }, WorkerRunLoop::defaultMode());
}

void ServiceWorkerThreadProxy::cancelFetch(SWServerConnectionIdentifier connectionIdentifier, FetchIdentifier fetchIdentifier)
{
    RELEASE_LOG(ServiceWorker, "ServiceWorkerThreadProxy::cancelFetch %llu", fetchIdentifier.toUInt64());

    auto client = m_ongoingFetchTasks.take(std::make_pair(connectionIdentifier, fetchIdentifier));
    if (!client)
        return;

    if (m_ongoingFetchTasks.isEmpty())
        thread().stopFetchEventMonitoring();

    postTaskForModeToWorkerOrWorkletGlobalScope([client = client.releaseNonNull()] (ScriptExecutionContext&) {
        client->cancel();
    }, WorkerRunLoop::defaultMode());
}


void ServiceWorkerThreadProxy::convertFetchToDownload(SWServerConnectionIdentifier connectionIdentifier, FetchIdentifier fetchIdentifier)
{
    RELEASE_LOG(ServiceWorker, "ServiceWorkerThreadProxy::convertFetchToDownload %llu", fetchIdentifier.toUInt64());

    auto client = m_ongoingFetchTasks.take(std::make_pair(connectionIdentifier, fetchIdentifier));
    if (!client)
        return;

    postTaskForModeToWorkerOrWorkletGlobalScope([client = Ref { *client }] (auto&) {
        client->convertFetchToDownload();
    }, WorkerRunLoop::defaultMode());
}

void ServiceWorkerThreadProxy::continueDidReceiveFetchResponse(SWServerConnectionIdentifier connectionIdentifier, FetchIdentifier fetchIdentifier)
{
    auto client = m_ongoingFetchTasks.get(std::make_pair(connectionIdentifier, fetchIdentifier));
    if (!client)
        return;

    postTaskForModeToWorkerOrWorkletGlobalScope([client = Ref { *client }] (ScriptExecutionContext&) {
        client->continueDidReceiveResponse();
    }, WorkerRunLoop::defaultMode());
}

void ServiceWorkerThreadProxy::removeFetch(SWServerConnectionIdentifier connectionIdentifier, FetchIdentifier fetchIdentifier)
{
    RELEASE_LOG(ServiceWorker, "ServiceWorkerThreadProxy::removeFetch %llu", fetchIdentifier.toUInt64());

    m_ongoingFetchTasks.remove(std::make_pair(connectionIdentifier, fetchIdentifier));

    if (m_ongoingFetchTasks.isEmpty())
        thread().stopFetchEventMonitoring();
}

void ServiceWorkerThreadProxy::postMessageToServiceWorker(MessageWithMessagePorts&& message, ServiceWorkerOrClientData&& sourceData)
{
    thread().willPostTaskToFireMessageEvent();
    thread().runLoop().postTask([this, protectedThis = Ref { *this }, message = WTFMove(message), sourceData = WTFMove(sourceData)](auto&) mutable {
        thread().queueTaskToPostMessage(WTFMove(message), WTFMove(sourceData));
    });
}

void ServiceWorkerThreadProxy::fireInstallEvent()
{
    thread().willPostTaskToFireInstallEvent();
    thread().runLoop().postTask([this, protectedThis = Ref { *this }](auto&) mutable {
        thread().queueTaskToFireInstallEvent();
    });
}

void ServiceWorkerThreadProxy::fireActivateEvent()
{
    thread().willPostTaskToFireActivateEvent();
    thread().runLoop().postTask([this, protectedThis = Ref { *this }](auto&) mutable {
        thread().queueTaskToFireActivateEvent();
    });
}

void ServiceWorkerThreadProxy::didSaveScriptsToDisk(ScriptBuffer&& script, HashMap<URL, ScriptBuffer>&& importedScripts)
{
    thread().runLoop().postTask([script = WTFMove(script), importedScripts = WTFMove(importedScripts)](auto& context) mutable {
        downcast<ServiceWorkerGlobalScope>(context).didSaveScriptsToDisk(WTFMove(script), WTFMove(importedScripts));
    });
}

void ServiceWorkerThreadProxy::firePushEvent(std::optional<Vector<uint8_t>>&& data, CompletionHandler<void(bool)>&& callback)
{
    if (m_ongoingPushTasks.isEmpty())
        thread().startPushEventMonitoring();

    auto identifier = ++m_pushTasksCounter;
    ASSERT(!m_ongoingPushTasks.contains(identifier));
    m_ongoingPushTasks.add(identifier, WTFMove(callback));
    bool isPosted = postTaskForModeToWorkerOrWorkletGlobalScope([this, protectedThis = Ref { *this }, identifier, data = WTFMove(data)](auto&) mutable {
        thread().queueTaskToFirePushEvent(WTFMove(data), [this, protectedThis = WTFMove(protectedThis), identifier](bool result) mutable {
            callOnMainThread([this, protectedThis = WTFMove(protectedThis), identifier, result]() mutable {
                if (auto callback = m_ongoingPushTasks.take(identifier))
                    callback(result);
                if (m_ongoingPushTasks.isEmpty())
                    thread().stopPushEventMonitoring();
            });
        });
    }, WorkerRunLoop::defaultMode());
    if (!isPosted)
        m_ongoingPushTasks.take(identifier)(false);
}

void ServiceWorkerThreadProxy::firePushSubscriptionChangeEvent(std::optional<PushSubscriptionData>&& newSubscriptionData, std::optional<PushSubscriptionData>&& oldSubscriptionData)
{
    thread().willPostTaskToFirePushSubscriptionChangeEvent();
    thread().runLoop().postTask([this, protectedThis = Ref { *this }, newSubscriptionData = crossThreadCopy(WTFMove(newSubscriptionData)), oldSubscriptionData = crossThreadCopy(WTFMove(oldSubscriptionData))](auto&) mutable {
        thread().queueTaskToFirePushSubscriptionChangeEvent(WTFMove(newSubscriptionData), WTFMove(oldSubscriptionData));
    });
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
