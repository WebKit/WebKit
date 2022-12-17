/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "SharedWorkerThread.h"

#include "Logging.h"
#include "ServiceWorker.h"
#include "SharedWorkerGlobalScope.h"
#include "WorkerObjectProxy.h"

namespace WebCore {

SharedWorkerThread::SharedWorkerThread(SharedWorkerIdentifier identifier, const WorkerParameters& parameters, const ScriptBuffer& sourceCode, WorkerLoaderProxy& loaderProxy, WorkerDebuggerProxy& debuggerProxy, WorkerObjectProxy& objectProxy, WorkerBadgeProxy& badgeProxy, WorkerThreadStartMode startMode, const SecurityOrigin& topOrigin, IDBClient::IDBConnectionProxy* connectionProxy, SocketProvider* socketProvider, JSC::RuntimeFlags runtimeFlags)
    : WorkerThread(parameters, sourceCode, loaderProxy, debuggerProxy, objectProxy, badgeProxy, startMode, topOrigin, connectionProxy, socketProvider, runtimeFlags)
    , m_identifier(identifier)
    , m_name(parameters.name.isolatedCopy())
{
    RELEASE_LOG(SharedWorker, "%p - SharedWorkerThread::SharedWorkerThread: m_identifier=%" PRIu64, this, m_identifier.toUInt64());
}

Ref<WorkerGlobalScope> SharedWorkerThread::createWorkerGlobalScope(const WorkerParameters& parameters, Ref<SecurityOrigin>&& origin, Ref<SecurityOrigin>&& topOrigin)
{
    RELEASE_LOG(SharedWorker, "%p - SharedWorkerThread::createWorkerGlobalScope: m_identifier=%" PRIu64, this, m_identifier.toUInt64());
    auto scope = SharedWorkerGlobalScope::create(std::exchange(m_name, { }), parameters, WTFMove(origin), *this, WTFMove(topOrigin), idbConnectionProxy(), socketProvider(), WTFMove(m_workerClient));
#if ENABLE(SERVICE_WORKER)
    if (parameters.serviceWorkerData)
        scope->setActiveServiceWorker(ServiceWorker::getOrCreate(scope.get(), ServiceWorkerData { *parameters.serviceWorkerData }));
    scope->updateServiceWorkerClientData();
#endif
    return scope;
}

} // namespace WebCore
