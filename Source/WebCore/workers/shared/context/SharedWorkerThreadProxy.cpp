/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "SharedWorkerThreadProxy.h"

#include "CacheStorageProvider.h"
#include "ErrorEvent.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "LibWebRTCProvider.h"
#include "LoaderStrategy.h"
#include "MessageEvent.h"
#include "MessagePort.h"
#include "Page.h"
#include "PlatformStrategies.h"
#include "RTCDataChannelRemoteHandlerConnection.h"
#include "SharedWorker.h"
#include "SharedWorkerContextManager.h"
#include "SharedWorkerGlobalScope.h"
#include "SharedWorkerThread.h"
#include "WorkerFetchResult.h"
#include "WorkerThread.h"
#include <JavaScriptCore/IdentifiersFactory.h>

namespace WebCore {

static HashSet<SharedWorkerThreadProxy*>& allSharedWorkerThreadProxies()
{
    static NeverDestroyed<HashSet<SharedWorkerThreadProxy*>> set;
    return set;
}

static WorkerParameters generateWorkerParameters(const WorkerFetchResult& workerFetchResult, WorkerOptions&& workerOptions, const String& userAgent, Document& document)
{
    return WorkerParameters {
        workerFetchResult.lastRequestURL,
        workerOptions.name,
        "sharedworker:" + Inspector::IdentifiersFactory::createIdentifier(),
        userAgent,
        platformStrategies()->loaderStrategy()->isOnLine(),
        workerFetchResult.contentSecurityPolicy,
        false,
        workerFetchResult.crossOriginEmbedderPolicy,
        MonotonicTime::now(),
        parseReferrerPolicy(workerFetchResult.referrerPolicy, ReferrerPolicySource::HTTPHeader).value_or(ReferrerPolicy::EmptyString),
        workerOptions.type,
        workerOptions.credentials,
        document.settingsValues()
    };
}

SharedWorkerThreadProxy::SharedWorkerThreadProxy(UniqueRef<Page>&& page, SharedWorkerIdentifier sharedWorkerIdentifier, const ClientOrigin& clientOrigin, WorkerFetchResult&& workerFetchResult, WorkerOptions&& workerOptions, const String& userAgent, CacheStorageProvider& cacheStorageProvider)
    : m_page(WTFMove(page))
    , m_document(*m_page->mainFrame().document())
    , m_workerThread(SharedWorkerThread::create(sharedWorkerIdentifier, generateWorkerParameters(workerFetchResult, WTFMove(workerOptions), userAgent, m_document), WTFMove(workerFetchResult.script), *this, *this, *this, WorkerThreadStartMode::Normal, clientOrigin.topOrigin.securityOrigin(), m_document->idbConnectionProxy(), m_document->socketProvider(), JSC::RuntimeFlags::createAllEnabled()))
    , m_cacheStorageProvider(cacheStorageProvider)
{
    ASSERT(!allSharedWorkerThreadProxies().contains(this));
    allSharedWorkerThreadProxies().add(this);

    static bool addedListener;
    if (!addedListener) {
        platformStrategies()->loaderStrategy()->addOnlineStateChangeListener(&networkStateChanged);
        addedListener = true;
    }
}

SharedWorkerThreadProxy::~SharedWorkerThreadProxy()
{
    ASSERT(allSharedWorkerThreadProxies().contains(this));
    allSharedWorkerThreadProxies().remove(this);
}

SharedWorkerIdentifier SharedWorkerThreadProxy::identifier() const
{
    return m_workerThread->identifier();
}

void SharedWorkerThreadProxy::notifyNetworkStateChange(bool isOnline)
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

void SharedWorkerThreadProxy::postExceptionToWorkerObject(const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
{
    ASSERT(!isMainThread());
    if (!m_workerThread->isInStaticScriptEvaluation())
        return;

    callOnMainThread([sharedWorkerIdentifier = m_workerThread->identifier(), errorMessage = errorMessage.isolatedCopy(), lineNumber, columnNumber, sourceURL = sourceURL.isolatedCopy()] {
        if (auto* connection = SharedWorkerContextManager::singleton().connection())
            connection->postExceptionToWorkerObject(sharedWorkerIdentifier, errorMessage, lineNumber, columnNumber, sourceURL);
    });
}

RefPtr<CacheStorageConnection> SharedWorkerThreadProxy::createCacheStorageConnection()
{
    ASSERT(isMainThread());
    if (!m_cacheStorageConnection)
        m_cacheStorageConnection = m_cacheStorageProvider.createCacheStorageConnection();
    return m_cacheStorageConnection;
}

RefPtr<RTCDataChannelRemoteHandlerConnection> SharedWorkerThreadProxy::createRTCDataChannelRemoteHandlerConnection()
{
    ASSERT(isMainThread());
    return m_page->libWebRTCProvider().createRTCDataChannelRemoteHandlerConnection();
}

void SharedWorkerThreadProxy::postTaskToLoader(ScriptExecutionContext::Task&& task)
{
    callOnMainThread([task = WTFMove(task), protectedThis = Ref { *this }] () mutable {
        task.performTask(protectedThis->m_document.get());
    });
}

bool SharedWorkerThreadProxy::postTaskForModeToWorkerOrWorkletGlobalScope(ScriptExecutionContext::Task&& task, const String& mode)
{
    if (m_isTerminatingOrTerminated)
        return false;

    m_workerThread->runLoop().postTaskForMode(WTFMove(task), mode);
    return true;
}

void SharedWorkerThreadProxy::postMessageToDebugger(const String&)
{

}

void SharedWorkerThreadProxy::setResourceCachingDisabledByWebInspector(bool)
{

}

void SharedWorkerThreadProxy::networkStateChanged(bool isOnLine)
{
    for (auto* proxy : allSharedWorkerThreadProxies())
        proxy->notifyNetworkStateChange(isOnLine);
}

} // namespace WebCore
