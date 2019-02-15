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
#include "Frame.h"
#include "FrameLoader.h"
#include "LoaderStrategy.h"
#include "PlatformStrategies.h"
#include "Settings.h"
#include <pal/SessionID.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

namespace WebCore {

URL static inline topOriginURL(const SecurityOrigin& origin)
{
    URL url;
    url.setProtocol(origin.protocol());
    url.setHost(origin.host());
    if (origin.port())
        url.setPort(*origin.port());
    return url;
}

static inline UniqueRef<Page> createPageForServiceWorker(PageConfiguration&& configuration, const ServiceWorkerContextData& data, SecurityOrigin::StorageBlockingPolicy storageBlockingPolicy, PAL::SessionID sessionID)
{
    auto page = makeUniqueRef<Page>(WTFMove(configuration));
    page->setSessionID(sessionID);

    auto& mainFrame = page->mainFrame();
    mainFrame.loader().initForSynthesizedDocument({ });
    auto document = Document::createNonRenderedPlaceholder(&mainFrame, data.scriptURL);
    document->createDOMWindow();

    document->mutableSettings().setStorageBlockingPolicy(storageBlockingPolicy);
    document->storageBlockingStateDidChange();

    auto origin = data.registration.key.topOrigin().securityOrigin();
    origin->setStorageBlockingPolicy(storageBlockingPolicy);

    document->setSiteForCookies(topOriginURL(origin));
    document->setFirstPartyForCookies(data.scriptURL);
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

ServiceWorkerThreadProxy::ServiceWorkerThreadProxy(PageConfiguration&& pageConfiguration, const ServiceWorkerContextData& data, PAL::SessionID sessionID, String&& userAgent, CacheStorageProvider& cacheStorageProvider, SecurityOrigin::StorageBlockingPolicy storageBlockingPolicy)
    : m_page(createPageForServiceWorker(WTFMove(pageConfiguration), data, storageBlockingPolicy, data.sessionID))
    , m_document(*m_page->mainFrame().document())
    , m_serviceWorkerThread(ServiceWorkerThread::create(data, sessionID, WTFMove(userAgent), *this, *this, idbConnectionProxy(m_document), m_document->socketProvider()))
    , m_cacheStorageProvider(cacheStorageProvider)
    , m_sessionID(sessionID)
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
    m_remoteDebuggable = std::make_unique<ServiceWorkerDebuggable>(*this, data);
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

void ServiceWorkerThreadProxy::setResourceCachingDisabled(bool disabled)
{
    postTaskToLoader([this, protectedThis = makeRef(*this), disabled] (ScriptExecutionContext&) {
        ASSERT(isMainThread());
        m_page->setResourceCachingDisabled(disabled);
    });   
}

Ref<CacheStorageConnection> ServiceWorkerThreadProxy::createCacheStorageConnection()
{
    ASSERT(isMainThread());
    if (!m_cacheStorageConnection)
        m_cacheStorageConnection = m_cacheStorageProvider.createCacheStorageConnection(m_sessionID);
    return *m_cacheStorageConnection;
}

std::unique_ptr<FetchLoader> ServiceWorkerThreadProxy::createBlobLoader(FetchLoaderClient& client, const URL& blobURL)
{
    auto loader = std::make_unique<FetchLoader>(client, nullptr);
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
        globalScope.dispatchEvent(Event::create(isOnline ? eventNames().onlineEvent : eventNames().offlineEvent, Event::CanBubble::No, Event::IsCancelable::No));
    }, WorkerRunLoop::defaultMode());
}

void ServiceWorkerThreadProxy::startFetch(SWServerConnectionIdentifier connectionIdentifier, FetchIdentifier fetchIdentifier, Ref<ServiceWorkerFetch::Client>&& client, Optional<ServiceWorkerClientIdentifier>&& clientId, ResourceRequest&& request, String&& referrer, FetchOptions&& options)
{
    auto key = std::make_pair(connectionIdentifier, fetchIdentifier);

    ASSERT(!m_ongoingFetchTasks.contains(key));
    m_ongoingFetchTasks.add(key, client.copyRef());
    thread().postFetchTask(WTFMove(client), WTFMove(clientId), WTFMove(request), WTFMove(referrer), WTFMove(options));
}

void ServiceWorkerThreadProxy::cancelFetch(SWServerConnectionIdentifier connectionIdentifier, FetchIdentifier fetchIdentifier)
{
    auto client = m_ongoingFetchTasks.take(std::make_pair(connectionIdentifier, fetchIdentifier));
    if (!client)
        return;

    postTaskForModeToWorkerGlobalScope([client = WTFMove(client.value())] (ScriptExecutionContext&) {
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
    m_ongoingFetchTasks.remove(std::make_pair(connectionIdentifier, fetchIdentifier));
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
