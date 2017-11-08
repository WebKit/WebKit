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

SWServerRegistration::SWServerRegistration(SWServer& server, const ServiceWorkerRegistrationKey& key, ServiceWorkerUpdateViaCache updateViaCache, const URL& scopeURL, const URL& scriptURL)
    : m_registrationKey(key)
    , m_updateViaCache(updateViaCache)
    , m_scopeURL(scopeURL)
    , m_scriptURL(scriptURL)
    , m_server(server)
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

    std::optional<ServiceWorkerIdentifier> serviceWorkerIdentifier;
    if (worker)
        serviceWorkerIdentifier = worker->identifier();

    for (auto& connectionIdentifierWithClients : m_clientRegistrationsByConnection.keys()) {
        if (auto* connection = m_server.getConnection(connectionIdentifierWithClients))
            connection->updateRegistrationStateInClient(m_registrationKey, state, serviceWorkerIdentifier);
    }
}

void SWServerRegistration::updateWorkerState(SWServerWorker& worker, ServiceWorkerState state)
{
    LOG(ServiceWorker, "Updating worker %p state to %i (%p)", &worker, (int)state, this);

    worker.setState(state);

    for (auto& connectionIdentifierWithClients : m_clientRegistrationsByConnection.keys()) {
        if (auto* connection = m_server.getConnection(connectionIdentifierWithClients))
            connection->updateWorkerStateInClient(worker.identifier(), state);
    }
}

void SWServerRegistration::fireUpdateFoundEvent(uint64_t connectionIdentifier)
{
    // No matter what, we send the event to the connection that scheduled the job. The client registration
    // may not have gotten a chance to register itself yet.
    if (auto* connection = m_server.getConnection(connectionIdentifier))
        connection->fireUpdateFoundEvent(m_registrationKey);

    for (auto& connectionIdentifierWithClients : m_clientRegistrationsByConnection.keys()) {
        if (connectionIdentifierWithClients == connectionIdentifier)
            continue;

        if (auto* connection = m_server.getConnection(connectionIdentifierWithClients))
            connection->fireUpdateFoundEvent(m_registrationKey);
    }
}

// FIXME: This will do away once we correctly update the registration state after install.
void SWServerRegistration::firePostInstallEvents(uint64_t connectionIdentifier)
{
    // No matter what, we send the event to the connection that scheduled the job. The client registration
    // may not have gotten a chance to register itself yet.
    if (auto* connection = m_server.getConnection(connectionIdentifier))
        connection->firePostInstallEvents(m_registrationKey);

    for (auto& connectionIdentifierWithClients : m_clientRegistrationsByConnection.keys()) {
        if (connectionIdentifierWithClients == connectionIdentifier)
            continue;

        if (auto* connection = m_server.getConnection(connectionIdentifierWithClients))
            connection->firePostInstallEvents(m_registrationKey);
    }
}

ServiceWorkerRegistrationData SWServerRegistration::data() const
{
    std::optional<ServiceWorkerIdentifier> installingID;
    if (m_installingWorker)
        installingID = m_installingWorker->identifier();

    std::optional<ServiceWorkerIdentifier> waitingID;
    if (m_waitingWorker)
        waitingID = m_waitingWorker->identifier();

    std::optional<ServiceWorkerIdentifier> activeID;
    if (m_activeWorker)
        activeID = m_activeWorker->identifier();

    return { m_registrationKey, identifier(), m_scopeURL, m_scriptURL, m_updateViaCache, installingID, waitingID, activeID };
}

void SWServerRegistration::addClientServiceWorkerRegistration(uint64_t connectionIdentifier, uint64_t clientRegistrationIdentifier)
{
    auto result = m_clientRegistrationsByConnection.ensure(connectionIdentifier, [] {
        return std::make_unique<HashSet<uint64_t>>();
    });
    
    ASSERT(!result.iterator->value->contains(clientRegistrationIdentifier));
    result.iterator->value->add(clientRegistrationIdentifier);
}

void SWServerRegistration::removeClientServiceWorkerRegistration(uint64_t connectionIdentifier, uint64_t clientRegistrationIdentifier)
{
    auto iterator = m_clientRegistrationsByConnection.find(connectionIdentifier);
    if (iterator == m_clientRegistrationsByConnection.end() || !iterator->value)
        return;
    
    iterator->value->remove(clientRegistrationIdentifier);
    if (iterator->value->isEmpty())
        m_clientRegistrationsByConnection.remove(iterator);
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
