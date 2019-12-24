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

#pragma once

#if ENABLE(SERVICE_WORKER)

#include "CacheStorageConnection.h"
#include "Document.h"
#include "FetchIdentifier.h"
#include "Page.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerDebuggable.h"
#include "ServiceWorkerIdentifier.h"
#include "ServiceWorkerInspectorProxy.h"
#include "ServiceWorkerThread.h"
#include "WorkerDebuggerProxy.h"
#include "WorkerLoaderProxy.h"
#include <wtf/HashMap.h>

namespace WebCore {

class CacheStorageProvider;
class FetchLoader;
class FetchLoaderClient;
class PageConfiguration;
class ServiceWorkerInspectorProxy;
struct ServiceWorkerContextData;

class ServiceWorkerThreadProxy final : public ThreadSafeRefCounted<ServiceWorkerThreadProxy>, public WorkerLoaderProxy, public WorkerDebuggerProxy {
public:
    template<typename... Args> static Ref<ServiceWorkerThreadProxy> create(Args&&... args)
    {
        return adoptRef(*new ServiceWorkerThreadProxy(std::forward<Args>(args)...));
    }
    WEBCORE_EXPORT ~ServiceWorkerThreadProxy();

    ServiceWorkerIdentifier identifier() const { return m_serviceWorkerThread->identifier(); }
    ServiceWorkerThread& thread() { return m_serviceWorkerThread.get(); }
    ServiceWorkerInspectorProxy& inspectorProxy() { return m_inspectorProxy; }

    bool isTerminatingOrTerminated() const { return m_isTerminatingOrTerminated; }
    void setAsTerminatingOrTerminated() { m_isTerminatingOrTerminated = true; }

    WEBCORE_EXPORT std::unique_ptr<FetchLoader> createBlobLoader(FetchLoaderClient&, const URL&);

    const URL& scriptURL() const { return m_document->url(); }

    WEBCORE_EXPORT void notifyNetworkStateChange(bool isOnline);

    WEBCORE_EXPORT void startFetch(SWServerConnectionIdentifier, FetchIdentifier, Ref<ServiceWorkerFetch::Client>&&, Optional<ServiceWorkerClientIdentifier>&&, ResourceRequest&&, String&& referrer, FetchOptions&&);
    WEBCORE_EXPORT void cancelFetch(SWServerConnectionIdentifier, FetchIdentifier);
    WEBCORE_EXPORT void continueDidReceiveFetchResponse(SWServerConnectionIdentifier, FetchIdentifier);
    WEBCORE_EXPORT void removeFetch(SWServerConnectionIdentifier, FetchIdentifier);

    void postMessageToServiceWorker(MessageWithMessagePorts&&, ServiceWorkerOrClientData&&);
    void fireInstallEvent();
    void fireActivateEvent();

private:
    WEBCORE_EXPORT ServiceWorkerThreadProxy(PageConfiguration&&, const ServiceWorkerContextData&, String&& userAgent, CacheStorageProvider&, SecurityOrigin::StorageBlockingPolicy);

    WEBCORE_EXPORT static void networkStateChanged(bool isOnLine);

    // WorkerLoaderProxy
    bool postTaskForModeToWorkerGlobalScope(ScriptExecutionContext::Task&&, const String& mode) final;
    void postTaskToLoader(ScriptExecutionContext::Task&&) final;
    Ref<CacheStorageConnection> createCacheStorageConnection() final;

    // WorkerDebuggerProxy
    void postMessageToDebugger(const String&) final;
    void setResourceCachingDisabledByWebInspector(bool) final;

    UniqueRef<Page> m_page;
    Ref<Document> m_document;
    Ref<ServiceWorkerThread> m_serviceWorkerThread;
    CacheStorageProvider& m_cacheStorageProvider;
    RefPtr<CacheStorageConnection> m_cacheStorageConnection;
    bool m_isTerminatingOrTerminated { false };

    ServiceWorkerInspectorProxy m_inspectorProxy;
#if ENABLE(REMOTE_INSPECTOR)
    std::unique_ptr<ServiceWorkerDebuggable> m_remoteDebuggable;
#endif
    HashMap<std::pair<SWServerConnectionIdentifier, FetchIdentifier>, Ref<ServiceWorkerFetch::Client>> m_ongoingFetchTasks;
};

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
