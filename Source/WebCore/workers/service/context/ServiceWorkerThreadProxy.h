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
#include "Page.h"
#include "ServiceWorkerThread.h"
#include "WorkerLoaderProxy.h"
#include <wtf/HashMap.h>

namespace WebCore {
class CacheStorageProvider;
class PageConfiguration;
struct ServiceWorkerContextData;

class ServiceWorkerThreadProxy final : public ThreadSafeRefCounted<ServiceWorkerThreadProxy>, public WorkerLoaderProxy {
public:
    WEBCORE_EXPORT static Ref<ServiceWorkerThreadProxy> create(uint64_t serverConnectionIdentifier, const ServiceWorkerContextData&, PAL::SessionID, CacheStorageProvider&);

    uint64_t identifier() const { return m_serviceWorkerThread->identifier(); }
    ServiceWorkerThread& thread() { return m_serviceWorkerThread.get(); }

private:
    ServiceWorkerThreadProxy(uint64_t serverConnectionIdentifier, const ServiceWorkerContextData&, PAL::SessionID, CacheStorageProvider&);
    bool postTaskForModeToWorkerGlobalScope(ScriptExecutionContext::Task&&, const String& mode) final;
    void postTaskToLoader(ScriptExecutionContext::Task&&) final;
    Ref<CacheStorageConnection> createCacheStorageConnection() final;

    Ref<ServiceWorkerThread> m_serviceWorkerThread;
    CacheStorageProvider& m_cacheStorageProvider;
    RefPtr<CacheStorageConnection> m_cacheStorageConnection;
    PAL::SessionID m_sessionID;
};

} // namespace WebKit

#endif // ENABLE(SERVICE_WORKER)
