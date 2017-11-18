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
#include "SWServerRegistration.h"

#if ENABLE(SERVICE_WORKER)

#include "SWServer.h"
#include "SWServerWorker.h"
#include "ServiceWorkerTypes.h"
#include "ServiceWorkerUpdateViaCache.h"

namespace WebCore {

static ServiceWorkerRegistrationIdentifier generateServiceWorkerRegistrationIdentifier()
{
    return generateObjectIdentifier<ServiceWorkerRegistrationIdentifierType>();
}

SWServerRegistration::SWServerRegistration(SWServer& server, const ServiceWorkerRegistrationKey& key, ServiceWorkerUpdateViaCache updateViaCache, const URL& scopeURL, const URL& scriptURL)
    : m_identifier(generateServiceWorkerRegistrationIdentifier())
    , m_registrationKey(key)
    , m_updateViaCache(updateViaCache)
    , m_scopeURL(scopeURL)
    , m_scriptURL(scriptURL)
    , m_server(server)
    , m_creationTime(MonotonicTime::now())
{
    m_scopeURL.removeFragmentIdentifier();
}

SWServerRegistration::~SWServerRegistration()
{
}

SWServerWorker* SWServerRegistration::getNewestWorker()
{
    if (m_installingWorker)
        return m_installingWorker.get();
    if (m_waitingWorker)
        return m_waitingWorker.get();

    return m_activeWorker.get();
}

void SWServerRegistration::updateRegistrationState(ServiceWorkerRegistrationState state, SWServerWorker* worker)
{
    LOG(ServiceWorker, "(%p) Updating registration state to %i with worker %p", this, (int)state, worker);
    
    switch (state) {
    case ServiceWorkerRegistrationState::Installing:
        m_installingWorker = worker;
        break;
    case ServiceWorkerRegistrationState::Waiting:
        m_waitingWorker = worker;
        break;
    case ServiceWorkerRegistrationState::Active:
        m_activeWorker = worker;
        break;
    };

    std::optional<ServiceWorkerData> serviceWorkerData;
    if (worker)
        serviceWorkerData = worker->data();

    forEachConnection([&](auto& connection) {
        connection.updateRegistrationStateInClient(identifier(), state, serviceWorkerData);
    });
}

void SWServerRegistration::updateWorkerState(SWServerWorker& worker, ServiceWorkerState state)
{
    LOG(ServiceWorker, "Updating worker %p state to %i (%p)", &worker, (int)state, this);

    worker.setState(state);

    forEachConnection([&](auto& connection) {
        connection.updateWorkerStateInClient(worker.identifier(), state);
    });
}

void SWServerRegistration::fireUpdateFoundEvent()
{
    forEachConnection([&](auto& connection) {
        connection.fireUpdateFoundEvent(identifier());
    });
}

void SWServerRegistration::forEachConnection(const WTF::Function<void(SWServer::Connection&)>& apply)
{
    for (auto connectionIdentifierWithClients : m_connectionsWithClientRegistrations.values()) {
        if (auto* connection = m_server.getConnection(connectionIdentifierWithClients))
            apply(*connection);
    }
}

ServiceWorkerRegistrationData SWServerRegistration::data() const
{
    std::optional<ServiceWorkerData> installingWorkerData;
    if (m_installingWorker)
        installingWorkerData = m_installingWorker->data();

    std::optional<ServiceWorkerData> waitingWorkerData;
    if (m_waitingWorker)
        waitingWorkerData = m_waitingWorker->data();

    std::optional<ServiceWorkerData> activeWorkerData;
    if (m_activeWorker)
        activeWorkerData = m_activeWorker->data();

    return { m_registrationKey, identifier(), m_scopeURL, m_updateViaCache, WTFMove(installingWorkerData), WTFMove(waitingWorkerData), WTFMove(activeWorkerData) };
}

void SWServerRegistration::addClientServiceWorkerRegistration(SWServerConnectionIdentifier connectionIdentifier)
{
    m_connectionsWithClientRegistrations.add(connectionIdentifier);
}

void SWServerRegistration::removeClientServiceWorkerRegistration(SWServerConnectionIdentifier connectionIdentifier)
{
    m_connectionsWithClientRegistrations.remove(connectionIdentifier);
}

void SWServerRegistration::addClientUsingRegistration(const ServiceWorkerClientIdentifier& clientIdentifier)
{
    auto addResult = m_clientsUsingRegistration.ensure(clientIdentifier.serverConnectionIdentifier, [] {
        return HashSet<uint64_t> { };
    }).iterator->value.add(clientIdentifier.scriptExecutionContextIdentifier);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

void SWServerRegistration::removeClientUsingRegistration(const ServiceWorkerClientIdentifier& clientIdentifier)
{
    auto iterator = m_clientsUsingRegistration.find(clientIdentifier.serverConnectionIdentifier);
    ASSERT(iterator != m_clientsUsingRegistration.end());
    bool wasRemoved = iterator->value.remove(clientIdentifier.scriptExecutionContextIdentifier);
    ASSERT_UNUSED(wasRemoved, wasRemoved);

    if (iterator->value.isEmpty())
        m_clientsUsingRegistration.remove(iterator);
}

// https://w3c.github.io/ServiceWorker/#notify-controller-change
void SWServerRegistration::notifyClientsOfControllerChange()
{
    ASSERT(activeWorker());

    for (auto& item : m_clientsUsingRegistration) {
        if (auto* connection = m_server.getConnection(item.key))
            connection->notifyClientsOfControllerChange(item.value, activeWorker()->data());
    }
}

void SWServerRegistration::unregisterServerConnection(SWServerConnectionIdentifier serverConnectionIdentifier)
{
    m_connectionsWithClientRegistrations.removeAll(serverConnectionIdentifier);
    m_clientsUsingRegistration.remove(serverConnectionIdentifier);
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
