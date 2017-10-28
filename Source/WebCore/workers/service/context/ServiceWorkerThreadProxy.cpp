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
#include "FrameLoader.h"
#include "MainFrame.h"
#include <pal/SessionID.h>
#include <wtf/RunLoop.h>

namespace WebCore {

Ref<ServiceWorkerThreadProxy> ServiceWorkerThreadProxy::create(PageConfiguration&& pageConfiguration, uint64_t serverConnectionIdentifier, const ServiceWorkerContextData& data, PAL::SessionID sessionID, CacheStorageProvider& cacheStorageProvider)
{
    auto serviceWorker = adoptRef(*new ServiceWorkerThreadProxy { WTFMove(pageConfiguration), serverConnectionIdentifier, data, sessionID, cacheStorageProvider });
    serviceWorker->m_serviceWorkerThread->start();
    return serviceWorker;
}

static inline UniqueRef<Page> createPageForServiceWorker(PageConfiguration&& configuration, const URL& url)
{
    auto page = makeUniqueRef<Page>(WTFMove(configuration));
    auto& mainFrame = page->mainFrame();
    mainFrame.loader().initForSynthesizedDocument({ });
    auto document = Document::createNonRenderedPlaceholder(&mainFrame, url);
    document->createDOMWindow();
    mainFrame.setDocument(WTFMove(document));
    return page;
}

ServiceWorkerThreadProxy::ServiceWorkerThreadProxy(PageConfiguration&& pageConfiguration, uint64_t serverConnectionIdentifier, const ServiceWorkerContextData& data, PAL::SessionID sessionID, CacheStorageProvider& cacheStorageProvider)
    : m_page(createPageForServiceWorker(WTFMove(pageConfiguration), data.scriptURL))
    , m_document(*m_page->mainFrame().document())
    , m_serviceWorkerThread(ServiceWorkerThread::create(serverConnectionIdentifier, data, sessionID, *this))
    , m_cacheStorageProvider(cacheStorageProvider)
    , m_sessionID(sessionID)
{
    m_serviceWorkerThread->start();
}

bool ServiceWorkerThreadProxy::postTaskForModeToWorkerGlobalScope(ScriptExecutionContext::Task&& task, const String& mode)
{
    // FIXME: Handle termination case.
    m_serviceWorkerThread->runLoop().postTaskForMode(WTFMove(task), mode);
    return true;
}

void ServiceWorkerThreadProxy::postTaskToLoader(ScriptExecutionContext::Task&& task)
{
    RunLoop::main().dispatch([task = WTFMove(task), this, protectedThis = makeRef(*this)] () mutable {
        task.performTask(m_document.get());
    });
}

Ref<CacheStorageConnection> ServiceWorkerThreadProxy::createCacheStorageConnection()
{
    ASSERT(isMainThread());
    if (!m_cacheStorageConnection)
        m_cacheStorageConnection = m_cacheStorageProvider.createCacheStorageConnection(m_sessionID);
    return *m_cacheStorageConnection;
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
