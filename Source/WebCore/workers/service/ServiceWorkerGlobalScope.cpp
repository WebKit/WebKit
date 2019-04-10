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
#include "ServiceWorkerGlobalScope.h"

#if ENABLE(SERVICE_WORKER)

#include "ExtendableEvent.h"
#include "SWContextManager.h"
#include "ServiceWorkerClient.h"
#include "ServiceWorkerClients.h"
#include "ServiceWorkerContainer.h"
#include "ServiceWorkerThread.h"
#include "ServiceWorkerWindowClient.h"
#include "WorkerNavigator.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ServiceWorkerGlobalScope);

Ref<ServiceWorkerGlobalScope> ServiceWorkerGlobalScope::create(const ServiceWorkerContextData& data, const URL& url, Ref<SecurityOrigin>&& origin, const String& identifier, const String& userAgent, bool isOnline, ServiceWorkerThread& thread, const ContentSecurityPolicyResponseHeaders& contentSecurityPolicy, bool shouldBypassMainWorldContentSecurityPolicy, Ref<SecurityOrigin>&& topOrigin, MonotonicTime timeOrigin, IDBClient::IDBConnectionProxy* connectionProxy, SocketProvider* socketProvider, PAL::SessionID sessionID)
{
    auto scope = adoptRef(*new ServiceWorkerGlobalScope { data, url, WTFMove(origin), identifier, userAgent, isOnline, thread, shouldBypassMainWorldContentSecurityPolicy, WTFMove(topOrigin), timeOrigin, connectionProxy, socketProvider, sessionID });
    scope->applyContentSecurityPolicyResponseHeaders(contentSecurityPolicy);
    return scope;
}

ServiceWorkerGlobalScope::ServiceWorkerGlobalScope(const ServiceWorkerContextData& data, const URL& url, Ref<SecurityOrigin>&& origin, const String& identifier, const String& userAgent, bool isOnline, ServiceWorkerThread& thread, bool shouldBypassMainWorldContentSecurityPolicy, Ref<SecurityOrigin>&& topOrigin, MonotonicTime timeOrigin, IDBClient::IDBConnectionProxy* connectionProxy, SocketProvider* socketProvider, PAL::SessionID sessionID)
    : WorkerGlobalScope(url, WTFMove(origin), identifier, userAgent, isOnline, thread, shouldBypassMainWorldContentSecurityPolicy, WTFMove(topOrigin), timeOrigin, connectionProxy, socketProvider, sessionID)
    , m_contextData(crossThreadCopy(data))
    , m_registration(ServiceWorkerRegistration::getOrCreate(*this, navigator().serviceWorker(), WTFMove(m_contextData.registration)))
    , m_clients(ServiceWorkerClients::create())
{
}

ServiceWorkerGlobalScope::~ServiceWorkerGlobalScope() = default;

void ServiceWorkerGlobalScope::skipWaiting(Ref<DeferredPromise>&& promise)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_pendingSkipWaitingPromises.add(requestIdentifier, WTFMove(promise));

    callOnMainThread([workerThread = makeRef(thread()), requestIdentifier]() mutable {
        if (auto* connection = SWContextManager::singleton().connection()) {
            auto identifier = workerThread->identifier();
            connection->skipWaiting(identifier, [workerThread = WTFMove(workerThread), requestIdentifier] {
                workerThread->runLoop().postTask([requestIdentifier](auto& context) {
                    auto& scope = downcast<ServiceWorkerGlobalScope>(context);
                    if (auto promise = scope.m_pendingSkipWaitingPromises.take(requestIdentifier))
                        promise->resolve();
                });
            });
        }
    });
}

EventTargetInterface ServiceWorkerGlobalScope::eventTargetInterface() const
{
    return ServiceWorkerGlobalScopeEventTargetInterfaceType;
}

ServiceWorkerThread& ServiceWorkerGlobalScope::thread()
{
    return static_cast<ServiceWorkerThread&>(WorkerGlobalScope::thread());
}

ServiceWorkerClient* ServiceWorkerGlobalScope::serviceWorkerClient(ServiceWorkerClientIdentifier identifier)
{
    return m_clientMap.get(identifier);
}

void ServiceWorkerGlobalScope::addServiceWorkerClient(ServiceWorkerClient& client)
{
    auto result = m_clientMap.add(client.identifier(), &client);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ServiceWorkerGlobalScope::removeServiceWorkerClient(ServiceWorkerClient& client)
{
    auto isRemoved = m_clientMap.remove(client.identifier());
    ASSERT_UNUSED(isRemoved, isRemoved);
}

// https://w3c.github.io/ServiceWorker/#update-service-worker-extended-events-set-algorithm
void ServiceWorkerGlobalScope::updateExtendedEventsSet(ExtendableEvent* newEvent)
{
    ASSERT(!isMainThread());
    ASSERT(!newEvent || !newEvent->isBeingDispatched());
    bool hadPendingEvents = hasPendingEvents();
    m_extendedEvents.removeAllMatching([](auto& event) {
        return !event->pendingPromiseCount();
    });

    if (newEvent && newEvent->pendingPromiseCount()) {
        m_extendedEvents.append(*newEvent);
        newEvent->whenAllExtendLifetimePromisesAreSettled([this](auto&&) {
            this->updateExtendedEventsSet();
        });
        // Clear out the event's target as it is the WorkerGlobalScope and we do not want to keep it
        // alive unnecessarily.
        newEvent->setTarget(nullptr);
    }

    bool hasPendingEvents = this->hasPendingEvents();
    if (hasPendingEvents == hadPendingEvents)
        return;

    callOnMainThread([threadIdentifier = thread().identifier(), hasPendingEvents] {
        if (auto* connection = SWContextManager::singleton().connection())
            connection->setServiceWorkerHasPendingEvents(threadIdentifier, hasPendingEvents);
    });
}

const ServiceWorkerContextData::ImportedScript* ServiceWorkerGlobalScope::scriptResource(const URL& url) const
{
    auto iterator = m_contextData.scriptResourceMap.find(url);
    return iterator == m_contextData.scriptResourceMap.end() ? nullptr : &iterator->value;
}

void ServiceWorkerGlobalScope::setScriptResource(const URL& url, ServiceWorkerContextData::ImportedScript&& script)
{
    callOnMainThread([threadIdentifier = thread().identifier(), url = url.isolatedCopy(), script = script.isolatedCopy()] {
        if (auto* connection = SWContextManager::singleton().connection())
            connection->setScriptResource(threadIdentifier, url, script);
    });

    m_contextData.scriptResourceMap.set(url, WTFMove(script));
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
