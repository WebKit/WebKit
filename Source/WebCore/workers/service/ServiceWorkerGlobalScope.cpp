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

#include "Document.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "ExtendableEvent.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "JSDOMPromiseDeferred.h"
#include "NotificationEvent.h"
#include "PushEvent.h"
#include "SWContextManager.h"
#include "ServiceWorker.h"
#include "ServiceWorkerClient.h"
#include "ServiceWorkerClients.h"
#include "ServiceWorkerContainer.h"
#include "ServiceWorkerThread.h"
#include "ServiceWorkerWindowClient.h"
#include "WebCoreJSClientData.h"
#include "WorkerNavigator.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ServiceWorkerGlobalScope);

Ref<ServiceWorkerGlobalScope> ServiceWorkerGlobalScope::create(ServiceWorkerContextData&& contextData, ServiceWorkerData&& workerData, const WorkerParameters& params, Ref<SecurityOrigin>&& origin, ServiceWorkerThread& thread, Ref<SecurityOrigin>&& topOrigin, IDBClient::IDBConnectionProxy* connectionProxy, SocketProvider* socketProvider, std::unique_ptr<NotificationClient>&& notificationClient)
{
    auto scope = adoptRef(*new ServiceWorkerGlobalScope { WTFMove(contextData), WTFMove(workerData), params, WTFMove(origin), thread, WTFMove(topOrigin), connectionProxy, socketProvider, WTFMove(notificationClient) });
    scope->addToContextsMap();
    scope->applyContentSecurityPolicyResponseHeaders(params.contentSecurityPolicyResponseHeaders);
    scope->notifyServiceWorkerPageOfCreationIfNecessary();
    return scope;
}

ServiceWorkerGlobalScope::ServiceWorkerGlobalScope(ServiceWorkerContextData&& contextData, ServiceWorkerData&& workerData, const WorkerParameters& params, Ref<SecurityOrigin>&& origin, ServiceWorkerThread& thread, Ref<SecurityOrigin>&& topOrigin, IDBClient::IDBConnectionProxy* connectionProxy, SocketProvider* socketProvider, std::unique_ptr<NotificationClient>&& notificationClient)
    : WorkerGlobalScope(WorkerThreadType::ServiceWorker, params, WTFMove(origin), thread, WTFMove(topOrigin), connectionProxy, socketProvider, nullptr)
    , m_contextData(WTFMove(contextData))
    , m_registration(ServiceWorkerRegistration::getOrCreate(*this, navigator().serviceWorker(), WTFMove(m_contextData.registration)))
    , m_serviceWorker(ServiceWorker::getOrCreate(*this, WTFMove(workerData)))
    , m_clients(ServiceWorkerClients::create())
    , m_notificationClient(WTFMove(notificationClient))
    , m_userGestureTimer(*this, &ServiceWorkerGlobalScope::resetUserGesture)
{
}

ServiceWorkerGlobalScope::~ServiceWorkerGlobalScope()
{
    // We need to remove from the contexts map very early in the destructor so that calling postTask() on this WorkerGlobalScope from another thread is safe.
    removeFromContextsMap();

    // NotificationClient might have some interactions pending with the main thread,
    // so it should also be destroyed there.
    callOnMainThread([notificationClient = WTFMove(m_notificationClient)] { });
}

void ServiceWorkerGlobalScope::dispatchPushEvent(PushEvent& pushEvent)
{
    ASSERT(!m_pushEvent);
    m_pushEvent = &pushEvent;
    dispatchEvent(pushEvent);
    m_pushEvent = nullptr;
}

void ServiceWorkerGlobalScope::notifyServiceWorkerPageOfCreationIfNecessary()
{
    auto serviceWorkerPage = this->serviceWorkerPage();
    if (!serviceWorkerPage)
        return;

    ASSERT(isMainThread());
    serviceWorkerPage->setServiceWorkerGlobalScope(*this);

    Vector<Ref<DOMWrapperWorld>> worlds;
    static_cast<JSVMClientData*>(vm().clientData)->getAllWorlds(worlds);
    for (auto& world : worlds)
        serviceWorkerPage->mainFrame().loader().client().dispatchServiceWorkerGlobalObjectAvailable(world);
}

Page* ServiceWorkerGlobalScope::serviceWorkerPage()
{
    if (!m_contextData.serviceWorkerPageIdentifier)
        return nullptr;

    RELEASE_ASSERT(isMainThread());
    return Page::serviceWorkerPage(*m_contextData.serviceWorkerPageIdentifier);
}

void ServiceWorkerGlobalScope::skipWaiting(Ref<DeferredPromise>&& promise)
{
    uint64_t requestIdentifier = ++m_lastRequestIdentifier;
    m_pendingSkipWaitingPromises.add(requestIdentifier, WTFMove(promise));

    callOnMainThread([workerThread = Ref { thread() }, requestIdentifier]() mutable {
        if (auto* connection = SWContextManager::singleton().connection()) {
            auto identifier = workerThread->identifier();
            connection->skipWaiting(identifier, [workerThread = WTFMove(workerThread), requestIdentifier] {
                workerThread->runLoop().postTask([requestIdentifier](auto& context) {
                    auto& scope = downcast<ServiceWorkerGlobalScope>(context);
                    scope.eventLoop().queueTask(TaskSource::DOMManipulation, [scope = Ref { scope }, requestIdentifier]() mutable {
                        if (auto promise = scope->m_pendingSkipWaitingPromises.take(requestIdentifier))
                            promise->resolve();
                    });
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

// https://w3c.github.io/ServiceWorker/#update-service-worker-extended-events-set-algorithm
void ServiceWorkerGlobalScope::updateExtendedEventsSet(ExtendableEvent* newEvent)
{
    ASSERT(isContextThread());
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

void ServiceWorkerGlobalScope::didSaveScriptsToDisk(ScriptBuffer&& script, HashMap<URL, ScriptBuffer>&& importedScripts)
{
    // These scripts should be identical to the ones we have. However, these are mmap'd so using them helps reduce dirty memory usage.
    updateSourceProviderBuffers(script, importedScripts);

    if (script) {
        ASSERT(m_contextData.script == script);
        m_contextData.script = WTFMove(script);
    }
    for (auto& pair : importedScripts) {
        auto it = m_contextData.scriptResourceMap.find(pair.key);
        if (it == m_contextData.scriptResourceMap.end())
            continue;
        ASSERT(it->value.script == pair.value); // Do a memcmp to make sure the scripts are identical.
        it->value.script = WTFMove(pair.value);
    }
}

void ServiceWorkerGlobalScope::recordUserGesture()
{
    m_isProcessingUserGesture = true;
    m_userGestureTimer.startOneShot(userGestureLifetime);
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
