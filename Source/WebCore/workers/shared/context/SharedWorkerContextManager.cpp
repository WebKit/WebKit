/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "SharedWorkerContextManager.h"

#include "Logging.h"
#include "SharedWorkerGlobalScope.h"
#include "SharedWorkerThread.h"
#include "SharedWorkerThreadProxy.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

SharedWorkerContextManager& SharedWorkerContextManager::singleton()
{
    static NeverDestroyed<SharedWorkerContextManager> sharedManager;
    return sharedManager;
}

SharedWorkerThreadProxy* SharedWorkerContextManager::sharedWorker(SharedWorkerIdentifier sharedWorkerIdentifier) const
{
    return m_workerMap.get(sharedWorkerIdentifier);
}

void SharedWorkerContextManager::stopSharedWorker(SharedWorkerIdentifier sharedWorkerIdentifier)
{
    auto worker = m_workerMap.take(sharedWorkerIdentifier);
    RELEASE_LOG(SharedWorker, "SharedWorkerContextManager::stopSharedWorker: sharedWorkerIdentifier=%" PRIu64 ", worker=%p", sharedWorkerIdentifier.toUInt64(), worker.get());
    if (!worker)
        return;

    worker->setAsTerminatingOrTerminated();

    // FIXME: We should be able to deal with the thread being unresponsive here.

    auto& thread = worker->thread();
    thread.stop([worker = WTFMove(worker)]() mutable {
        // Spin the runloop before releasing the shared worker thread proxy, as there would otherwise be
        // a race towards its destruction.
        callOnMainThread([worker = WTFMove(worker)] { });
    });
}

void SharedWorkerContextManager::stopAllSharedWorkers()
{
    while (!m_workerMap.isEmpty())
        stopSharedWorker(m_workerMap.begin()->key);
}

void SharedWorkerContextManager::setConnection(std::unique_ptr<Connection>&& connection)
{
    ASSERT(!m_connection || m_connection->isClosed());
    m_connection = WTFMove(connection);
}

auto SharedWorkerContextManager::connection() const -> Connection*
{
    return m_connection.get();
}

void SharedWorkerContextManager::registerSharedWorkerThread(Ref<SharedWorkerThreadProxy>&& proxy)
{
    ASSERT(isMainThread());
    RELEASE_LOG(SharedWorker, "SharedWorkerContextManager::registerSharedWorkerThread: sharedWorkerIdentifier=%" PRIu64, proxy->identifier().toUInt64());

    auto result = m_workerMap.add(proxy->identifier(), proxy.copyRef());
    ASSERT_UNUSED(result, result.isNewEntry);

    proxy->thread().start([](const String& /*exceptionMessage*/) { });
}

void SharedWorkerContextManager::Connection::postConnectEvent(SharedWorkerIdentifier sharedWorkerIdentifier, TransferredMessagePort&& transferredPort, String&& sourceOrigin)
{
    ASSERT(isMainThread());
    auto* proxy = SharedWorkerContextManager::singleton().sharedWorker(sharedWorkerIdentifier);
    RELEASE_LOG(SharedWorker, "SharedWorkerContextManager::Connection::postConnectEvent: sharedWorkerIdentifier=%" PRIu64 ", proxy=%p", sharedWorkerIdentifier.toUInt64(), proxy);
    if (!proxy)
        return;

    proxy->thread().runLoop().postTask([transferredPort = WTFMove(transferredPort), sourceOrigin = WTFMove(sourceOrigin).isolatedCopy()] (auto& scriptExecutionContext) mutable {
        ASSERT(!isMainThread());
        downcast<SharedWorkerGlobalScope>(scriptExecutionContext).postConnectEvent(WTFMove(transferredPort), WTFMove(sourceOrigin));
    });
}

void SharedWorkerContextManager::Connection::terminateSharedWorker(SharedWorkerIdentifier sharedWorkerIdentifier)
{
    ASSERT(isMainThread());
    RELEASE_LOG(SharedWorker, "SharedWorkerContextManager::Connection::terminateSharedWorker: sharedWorkerIdentifier=%" PRIu64, sharedWorkerIdentifier.toUInt64());
    SharedWorkerContextManager::singleton().stopSharedWorker(sharedWorkerIdentifier);
}

} // namespace WebCore
