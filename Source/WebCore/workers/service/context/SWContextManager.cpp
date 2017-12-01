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

#if ENABLE(SERVICE_WORKER)
#include "SWContextManager.h"
#include "ServiceWorkerClientIdentifier.h"

namespace WebCore {

SWContextManager& SWContextManager::singleton()
{
    static SWContextManager* sharedManager = new SWContextManager;
    return *sharedManager;
}

void SWContextManager::setConnection(std::unique_ptr<Connection>&& connection)
{
    ASSERT(!m_connection);
    m_connection = WTFMove(connection);
}

auto SWContextManager::connection() const -> Connection*
{
    return m_connection.get();
}

void SWContextManager::registerServiceWorkerThreadForInstall(Ref<ServiceWorkerThreadProxy>&& serviceWorkerThreadProxy)
{
    auto serviceWorkerIdentifier = serviceWorkerThreadProxy->identifier();
    auto jobDataIdentifier = serviceWorkerThreadProxy->thread().contextData().jobDataIdentifier;
    auto* threadProxy = serviceWorkerThreadProxy.ptr();
    auto result = m_workerMap.add(serviceWorkerIdentifier, WTFMove(serviceWorkerThreadProxy));
    ASSERT_UNUSED(result, result.isNewEntry);
    
    threadProxy->thread().start([jobDataIdentifier, serviceWorkerIdentifier](const String& exceptionMessage) {
        SWContextManager::singleton().connection()->serviceWorkerStartedWithMessage(jobDataIdentifier, serviceWorkerIdentifier, exceptionMessage);
    });
}

ServiceWorkerThreadProxy* SWContextManager::serviceWorkerThreadProxy(ServiceWorkerIdentifier identifier) const
{
    return m_workerMap.get(identifier);
}

void SWContextManager::postMessageToServiceWorkerGlobalScope(ServiceWorkerIdentifier destination, Ref<SerializedScriptValue>&& message, ServiceWorkerClientIdentifier sourceIdentifier, ServiceWorkerClientData&& sourceData)
{
    auto* serviceWorker = m_workerMap.get(destination);
    ASSERT(serviceWorker);
    ASSERT(!serviceWorker->isTerminatingOrTerminated());

    // FIXME: We should pass valid MessagePortChannels.
    serviceWorker->thread().postMessageToServiceWorkerGlobalScope(WTFMove(message), nullptr, sourceIdentifier, WTFMove(sourceData));
}

void SWContextManager::fireInstallEvent(ServiceWorkerIdentifier identifier)
{
    auto* serviceWorker = m_workerMap.get(identifier);
    if (!serviceWorker)
        return;

    serviceWorker->thread().fireInstallEvent();
}

void SWContextManager::fireActivateEvent(ServiceWorkerIdentifier identifier)
{
    auto* serviceWorker = m_workerMap.get(identifier);
    if (!serviceWorker)
        return;

    serviceWorker->thread().fireActivateEvent();
}

void SWContextManager::terminateWorker(ServiceWorkerIdentifier identifier, Function<void()>&& completionHandler)
{
    auto* serviceWorker = m_workerMap.get(identifier);
    if (!serviceWorker)
        return;

    serviceWorker->setTerminatingOrTerminated(true);

    serviceWorker->thread().stop([this, identifier, completionHandler = WTFMove(completionHandler)] {
        if (auto* connection = SWContextManager::singleton().connection())
            connection->workerTerminated(identifier);

        if (completionHandler)
            completionHandler();
        
        auto worker = m_workerMap.take(identifier);
        ASSERT(worker);
        
        // Spin the runloop before releasing the worker thread proxy, as there would otherwise be
        // a race towards its destruction.
        callOnMainThread([worker = WTFMove(worker)] { });
    });
}

void SWContextManager::forEachServiceWorkerThread(const WTF::Function<void(ServiceWorkerThreadProxy&)>& apply)
{
    for (auto& workerThread : m_workerMap.values())
        apply(*workerThread);
}

} // namespace WebCore

#endif
