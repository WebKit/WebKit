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
#include "EventLoop.h"
#include "EventNames.h"
#include "FetchLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "LoaderStrategy.h"
#include "Logging.h"
#include "MessageWithMessagePorts.h"
#include "PlatformStrategies.h"
#include "ServiceWorkerClientData.h"
#include "ServiceWorkerClientIdentifier.h"
#include "Settings.h"
#include "WorkerGlobalScope.h"
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

namespace WebCore {

static URL topOriginURL(const SecurityOrigin& origin)
{
    URL url;
    url.setProtocol(origin.protocol());
    url.setHost(origin.host());
    url.setPort(origin.port());
    return url;
}

static inline UniqueRef<Page> createPageForServiceWorker(PageConfiguration&& configuration, const ServiceWorkerContextData& data, SecurityOrigin::StorageBlockingPolicy storageBlockingPolicy)
{
    auto page = makeUniqueRef<Page>(WTFMove(configuration));

    auto& mainFrame = page->mainFrame();
    mainFrame.loader().initForSynthesizedDocument({ });
    auto document = Document::createNonRenderedPlaceholder(mainFrame, data.scriptURL);
    document->createDOMWindow();

    document->mutableSettings().setStorageBlockingPolicy(storageBlockingPolicy);
    document->storageBlockingStateDidChange();

    auto origin = data.registration.key.topOrigin().securityOrigin();
    origin->setStorageBlockingPolicy(storageBlockingPolicy);

    document->setSiteForCookies(topOriginURL(origin));
    document->setFirstPartyForCookies(topOriginURL(origin));
    document->setDomainForCachePartition(origin->domainForCachePartition());

    if (auto policy = parseReferrerPolicy(data.referrerPolicy, ReferrerPolicySource::HTTPHeader))
        document->setReferrerPolicy(*policy);

    mainFrame.setDocument(WTFMove(document));
    return page;
}

static inline IDBClient::IDBConnectionProxy* idbConnectionProxy(Document& document)
{
#if ENABLE(INDEXED_DATABASE)
    return document.idbConnectionProxy();
#else
    return nullptr;
#endif
}

static HashSet<ServiceWorkerThreadProxy*>& allServiceWorkerThreadProxies()
{
    static NeverDestroyed<HashSet<ServiceWorkerThreadProxy*>> set;
    return set;
}

ServiceWorkerThreadProxy::ServiceWorkerThreadProxy(PageConfiguration&& pageConfiguration, const ServiceWorkerContextData& data, String&& userAgent, CacheStorageProvider& cacheStorageProvider, SecurityOrigin::StorageBlockingPolicy storageBlockingPolicy)
    : m_page(createPageForServiceWorker(WTFMove(pageConfiguration), data, storageBlockingPolicy))
    , m_document(*m_page->mainFrame().document())
    , m_serviceWorkerThread(ServiceWorkerThread::create(data, WTFMove(userAgent), *this, *this, idbConnectionProxy(m_document), m_document->socketProvider()))
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
    m_remoteDebuggable = makeUnique<ServiceWorkerDebuggable>(*this, data);
    m_remoteDebuggable->setRemoteDebuggingAllowed(true);
    m_remoteDebuggable->init();
#endif
}

ServiceWorkerThreadProxy::~ServiceWorkerThreadProxy()
{
    ASSERT(allServiceWorkerThreadProxies().contains(this));
    allServiceWorkerThreadProxies().remove(this);
}

bool ServiceWorkerThreadProxy::postTaskForModeToWorkerGlobalScope(ScriptExecutionContext::Task&& task, const String& mode)
{
    if (m_isTerminatingOrTerminated)
        return false;

    m_serviceWorkerThread->runLoop().postTaskForMode(WTFMove(task), mode);
    return true;
}

void ServiceWorkerThreadProxy::postTaskToLoader(ScriptExecutionContext::Task&& task)
{
    callOnMainThread([task = WTFMove(task), this, protectedThis = makeRef(*this)] () mutable {
        task.performTask(m_document.get());
    });
}

void ServiceWorkerThreadProxy::postMessageToDebugger(const String& message)
{
    RunLoop::main().dispatch([this, protectedThis = makeRef(*this), message = message.isolatedCopy()] {
        // FIXME: Handle terminated case.
        m_inspectorProxy.sendMessageFromWorkerToFrontend(message);
    });
}

void ServiceWorkerThreadProxy::setResourceCachingDisabledByWebInspector(bool disabled)
{
    postTaskToLoader([this, protectedThis = makeRef(*this), disabled] (ScriptExecutionContext&) {
        ASSERT(isMainThread());
        m_page->setResourceCachingDisabledByWebInspector(disabled);
    });   
}

Ref<CacheStorageConnection> ServiceWorkerThreadProxy::createCacheStorageConnection()
{
    ASSERT(isMainThread());
    if (!m_cacheStorageConnection)
        m_cacheStorageConnection = m_cacheStorageProvider.createCacheStorageConnection();
    return *m_cacheStorageConnection;
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

    postTaskForModeToWorkerGlobalScope([isOnline] (ScriptExecutionContext& context) {
        auto& globalScope = downcast<WorkerGlobalScope>(context);
        globalScope.setIsOnline(isOnline);
        globalScope.eventLoop().queueTask(TaskSource::DOMManipulation, [globalScope = makeRef(globalScope), isOnline] {
            globalScope->dispatchEvent(Event::create(isOnline ? eventNames().onlineEvent : eventNames().offlineEvent, Event::CanBubble::No, Event::IsCancelable::No));
        });
    }, WorkerRunLoop::defaultMode());
}

void ServiceWorkerThreadProxy::startFetch(SWServerConnectionIdentifier connectionIdentifier, FetchIdentifier fetchIdentifier, Ref<ServiceWorkerFetch::Client>&& client, Optional<ServiceWorkerClientIdentifier>&& clientId, ResourceRequest&& request, String&& referrer, FetchOptions&& options)
{
    RELEASE_LOG(ServiceWorker, "ServiceWorkerThreadProxy::startFetch %llu", fetchIdentifier.toUInt64());

    auto key = std::make_pair(connectionIdentifier, fetchIdentifier);

    if (m_ongoingFetchTasks.isEmpty())
        thread().startFetchEventMonitoring();

    ASSERT(!m_ongoingFetchTasks.contains(key));
    m_ongoingFetchTasks.add(key, client.copyRef());
    postTaskForModeToWorkerGlobalScope([this, protectedThis = makeRef(*this), client = WTFMove(client), clientId, request = request.isolatedCopy(), referrer = referrer.isolatedCopy(), options = options.isolatedCopy()](auto&) mutable {
        thread().queueTaskToFireFetchEvent(WTFMove(client), WTFMove(clientId), WTFMove(request), WTFMove(referrer), WTFMove(options));
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

    postTaskForModeToWorkerGlobalScope([client = client.releaseNonNull()] (ScriptExecutionContext&) {
        client->cancel();
    }, WorkerRunLoop::defaultMode());
}

void ServiceWorkerThreadProxy::continueDidReceiveFetchResponse(SWServerConnectionIdentifier connectionIdentifier, FetchIdentifier fetchIdentifier)
{
    auto client = m_ongoingFetchTasks.get(std::make_pair(connectionIdentifier, fetchIdentifier));
    if (!client)
        return;

    postTaskForModeToWorkerGlobalScope([client = makeRef(*client)] (ScriptExecutionContext&) {
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
    thread().runLoop().postTask([this, protectedThis = makeRef(*this), message = WTFMove(message), sourceData = WTFMove(sourceData)](auto&) mutable {
        thread().queueTaskToPostMessage(WTFMove(message), WTFMove(sourceData));
    });
}

void ServiceWorkerThreadProxy::fireInstallEvent()
{
    thread().willPostTaskToFireInstallEvent();
    thread().runLoop().postTask([this, protectedThis = makeRef(*this)](auto&) mutable {
        thread().queueTaskToFireInstallEvent();
    });
}

void ServiceWorkerThreadProxy::fireActivateEvent()
{
    thread().willPostTaskToFireActivateEvent();
    thread().runLoop().postTask([this, protectedThis = makeRef(*this)](auto&) mutable {
        thread().queueTaskToFireActivateEvent();
    });
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
