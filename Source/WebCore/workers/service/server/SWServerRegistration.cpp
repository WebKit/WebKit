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
    static uint64_t identifier = 0;
    return makeObjectIdentifier<ServiceWorkerRegistrationIdentifierType>(++identifier);
}

SWServerRegistration::SWServerRegistration(SWServer& server, const ServiceWorkerRegistrationKey& key, ServiceWorkerUpdateViaCache updateViaCache, const URL& scopeURL, const URL& scriptURL)
    : m_identifier(generateServiceWorkerRegistrationIdentifier())
    , m_registrationKey(key)
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

    forEachConnection([&](auto& connection) {
        connection.updateRegistrationStateInClient(identifier(), state, serviceWorkerIdentifier);
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
    for (uint64_t connectionIdentifierWithClients : m_connectionsWithClientRegistrations.values()) {
        if (auto* connection = m_server.getConnection(connectionIdentifierWithClients))
            apply(*connection);
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

void SWServerRegistration::addClientServiceWorkerRegistration(uint64_t connectionIdentifier)
{
    m_connectionsWithClientRegistrations.add(connectionIdentifier);
}

void SWServerRegistration::removeClientServiceWorkerRegistration(uint64_t connectionIdentifier)
{
    m_connectionsWithClientRegistrations.remove(connectionIdentifier);
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
