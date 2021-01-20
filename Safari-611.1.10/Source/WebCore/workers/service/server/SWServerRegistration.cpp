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

#include "Logging.h"
#include "SWServer.h"
#include "SWServerToContextConnection.h"
#include "SWServerWorker.h"
#include "ServiceWorkerTypes.h"
#include "ServiceWorkerUpdateViaCache.h"

namespace WebCore {

static ServiceWorkerRegistrationIdentifier generateServiceWorkerRegistrationIdentifier()
{
    return ServiceWorkerRegistrationIdentifier::generate();
}

SWServerRegistration::SWServerRegistration(SWServer& server, const ServiceWorkerRegistrationKey& key, ServiceWorkerUpdateViaCache updateViaCache, const URL& scopeURL, const URL& scriptURL)
    : m_identifier(generateServiceWorkerRegistrationIdentifier())
    , m_registrationKey(key)
    , m_updateViaCache(updateViaCache)
    , m_scopeURL(scopeURL)
    , m_scriptURL(scriptURL)
    , m_server(server)
    , m_creationTime(MonotonicTime::now())
    , m_softUpdateTimer { *this, &SWServerRegistration::softUpdate }
{
    m_scopeURL.removeFragmentIdentifier();
}

SWServerRegistration::~SWServerRegistration()
{
    ASSERT(!m_preInstallationWorker || !m_preInstallationWorker->isRunning());
    ASSERT(!m_installingWorker || !m_installingWorker->isRunning());
    ASSERT(!m_waitingWorker || !m_waitingWorker->isRunning());
    ASSERT(!m_activeWorker || !m_activeWorker->isRunning());
}

SWServerWorker* SWServerRegistration::getNewestWorker()
{
    if (m_installingWorker)
        return m_installingWorker.get();
    if (m_waitingWorker)
        return m_waitingWorker.get();

    return m_activeWorker.get();
}

void SWServerRegistration::setPreInstallationWorker(SWServerWorker* worker)
{
    m_preInstallationWorker = worker;
}

void SWServerRegistration::updateRegistrationState(ServiceWorkerRegistrationState state, SWServerWorker* worker)
{
    LOG(ServiceWorker, "(%p) Updating registration state to %i with worker %p", this, (int)state, worker);
    
    switch (state) {
    case ServiceWorkerRegistrationState::Installing:
        ASSERT(!m_installingWorker || !m_installingWorker->isRunning() || m_waitingWorker == m_installingWorker);
        m_installingWorker = worker;
        break;
    case ServiceWorkerRegistrationState::Waiting:
        ASSERT(!m_waitingWorker || !m_waitingWorker->isRunning() || m_activeWorker == m_waitingWorker);
        m_waitingWorker = worker;
        break;
    case ServiceWorkerRegistrationState::Active:
        ASSERT(!m_activeWorker || !m_activeWorker->isRunning());
        m_activeWorker = worker;
        break;
    };

    Optional<ServiceWorkerData> serviceWorkerData;
    if (worker)
        serviceWorkerData = worker->data();

    forEachConnection([&](auto& connection) {
        connection.updateRegistrationStateInClient(this->identifier(), state, serviceWorkerData);
    });
}

void SWServerRegistration::updateWorkerState(SWServerWorker& worker, ServiceWorkerState state)
{
    LOG(ServiceWorker, "Updating worker %p state to %i (%p)", &worker, (int)state, this);

    worker.setState(state);
}

void SWServerRegistration::setUpdateViaCache(ServiceWorkerUpdateViaCache updateViaCache)
{
    m_updateViaCache = updateViaCache;
    forEachConnection([&](auto& connection) {
        connection.setRegistrationUpdateViaCache(this->identifier(), updateViaCache);
    });
}

void SWServerRegistration::setLastUpdateTime(WallTime time)
{
    m_lastUpdateTime = time;
    forEachConnection([&](auto& connection) {
        connection.setRegistrationLastUpdateTime(this->identifier(), time);
    });
}

void SWServerRegistration::fireUpdateFoundEvent()
{
    forEachConnection([&](auto& connection) {
        connection.fireUpdateFoundEvent(this->identifier());
    });
}

void SWServerRegistration::forEachConnection(const WTF::Function<void(SWServer::Connection&)>& apply)
{
    for (auto connectionIdentifierWithClients : m_connectionsWithClientRegistrations.values()) {
        if (auto* connection = m_server.connection(connectionIdentifierWithClients))
            apply(*connection);
    }
}

ServiceWorkerRegistrationData SWServerRegistration::data() const
{
    Optional<ServiceWorkerData> installingWorkerData;
    if (m_installingWorker)
        installingWorkerData = m_installingWorker->data();

    Optional<ServiceWorkerData> waitingWorkerData;
    if (m_waitingWorker)
        waitingWorkerData = m_waitingWorker->data();

    Optional<ServiceWorkerData> activeWorkerData;
    if (m_activeWorker)
        activeWorkerData = m_activeWorker->data();

    return { m_registrationKey, identifier(), m_scopeURL, m_updateViaCache, m_lastUpdateTime, WTFMove(installingWorkerData), WTFMove(waitingWorkerData), WTFMove(activeWorkerData) };
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
        return HashSet<DocumentIdentifier> { };
    }).iterator->value.add(clientIdentifier.contextIdentifier);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

void SWServerRegistration::removeClientUsingRegistration(const ServiceWorkerClientIdentifier& clientIdentifier)
{
    auto iterator = m_clientsUsingRegistration.find(clientIdentifier.serverConnectionIdentifier);
    ASSERT(iterator != m_clientsUsingRegistration.end());
    if (iterator == m_clientsUsingRegistration.end())
        return;

    bool wasRemoved = iterator->value.remove(clientIdentifier.contextIdentifier);
    ASSERT_UNUSED(wasRemoved, wasRemoved);

    if (iterator->value.isEmpty())
        m_clientsUsingRegistration.remove(iterator);

    handleClientUnload();
}

// https://w3c.github.io/ServiceWorker/#notify-controller-change
void SWServerRegistration::notifyClientsOfControllerChange()
{
    ASSERT(activeWorker());

    for (auto& item : m_clientsUsingRegistration) {
        if (auto* connection = m_server.connection(item.key))
            connection->notifyClientsOfControllerChange(item.value, activeWorker()->data());
    }
}

void SWServerRegistration::unregisterServerConnection(SWServerConnectionIdentifier serverConnectionIdentifier)
{
    m_connectionsWithClientRegistrations.removeAll(serverConnectionIdentifier);
    m_clientsUsingRegistration.remove(serverConnectionIdentifier);
}

// https://w3c.github.io/ServiceWorker/#try-clear-registration-algorithm
bool SWServerRegistration::tryClear()
{
    if (hasClientsUsingRegistration())
        return false;

    if (installingWorker() && installingWorker()->hasPendingEvents())
        return false;
    if (waitingWorker() && waitingWorker()->hasPendingEvents())
        return false;
    if (activeWorker() && activeWorker()->hasPendingEvents())
        return false;

    clear();
    return true;
}

// https://w3c.github.io/ServiceWorker/#clear-registration
void SWServerRegistration::clear()
{
    if (m_preInstallationWorker) {
        ASSERT(m_preInstallationWorker->state() == ServiceWorkerState::Redundant);
        m_preInstallationWorker->terminate();
        m_preInstallationWorker = nullptr;
    }

    RefPtr<SWServerWorker> installingWorker = this->installingWorker();
    if (installingWorker) {
        installingWorker->terminate();
        updateRegistrationState(ServiceWorkerRegistrationState::Installing, nullptr);
    }
    RefPtr<SWServerWorker> waitingWorker = this->waitingWorker();
    if (waitingWorker) {
        waitingWorker->terminate();
        updateRegistrationState(ServiceWorkerRegistrationState::Waiting, nullptr);
    }
    RefPtr<SWServerWorker> activeWorker = this->activeWorker();
    if (activeWorker) {
        activeWorker->terminate();
        updateRegistrationState(ServiceWorkerRegistrationState::Active, nullptr);
    }

    if (installingWorker)
        updateWorkerState(*installingWorker, ServiceWorkerState::Redundant);
    if (waitingWorker)
        updateWorkerState(*waitingWorker, ServiceWorkerState::Redundant);
    if (activeWorker)
        updateWorkerState(*activeWorker, ServiceWorkerState::Redundant);

    // Remove scope to registration map[scopeString].
    m_server.removeRegistration(identifier());
}

// https://w3c.github.io/ServiceWorker/#try-activate-algorithm
void SWServerRegistration::tryActivate()
{
    // If registration's waiting worker is null, return.
    if (!waitingWorker())
        return;
    // If registration's active worker is not null and registration's active worker's state is activating, return.
    if (activeWorker() && activeWorker()->state() == ServiceWorkerState::Activating)
        return;

    // Invoke Activate with registration if either of the following is true:
    // - registration's active worker is null.
    // - The result of running Service Worker Has No Pending Events with registration's active worker is true,
    //   and no service worker client is using registration or registration's waiting worker's skip waiting flag is set.
    if (!activeWorker() || (!activeWorker()->hasPendingEvents() && (!hasClientsUsingRegistration() || waitingWorker()->isSkipWaitingFlagSet())))
        activate();
}

// https://w3c.github.io/ServiceWorker/#activate
void SWServerRegistration::activate()
{
    // If registration's waiting worker is null, abort these steps.
    if (!waitingWorker())
        return;

    // If registration's active worker is not null, then:
    if (auto* worker = activeWorker()) {
        // Terminate registration's active worker.
        worker->terminate();
        // Run the Update Worker State algorithm passing registration's active worker and redundant as the arguments.
        updateWorkerState(*worker, ServiceWorkerState::Redundant);
    }
    // Run the Update Registration State algorithm passing registration, "active" and registration's waiting worker as the arguments.
    updateRegistrationState(ServiceWorkerRegistrationState::Active, waitingWorker());
    // Run the Update Registration State algorithm passing registration, "waiting" and null as the arguments.
    updateRegistrationState(ServiceWorkerRegistrationState::Waiting, nullptr);
    // Run the Update Worker State algorithm passing registration's active worker and activating as the arguments.
    updateWorkerState(*activeWorker(), ServiceWorkerState::Activating);
    // FIXME: For each service worker client whose creation URL matches registration's scope url...

    // The registration now has an active worker so we need to check if there are any ready promises that were waiting for this.
    m_server.resolveRegistrationReadyRequests(*this);

    // For each service worker client who is using registration:
    // - Set client's active worker to registration's active worker.

    // - Invoke Notify Controller Change algorithm with client as the argument.
    notifyClientsOfControllerChange();

    // FIXME: Invoke Run Service Worker algorithm with activeWorker as the argument.

    // Queue a task to fire the activate event.
    ASSERT(activeWorker());
    m_server.fireActivateEvent(*activeWorker());
}

// https://w3c.github.io/ServiceWorker/#activate (post activate event steps).
void SWServerRegistration::didFinishActivation(ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    if (!activeWorker() || activeWorker()->identifier() != serviceWorkerIdentifier)
        return;

    // Run the Update Worker State algorithm passing registration's active worker and activated as the arguments.
    updateWorkerState(*activeWorker(), ServiceWorkerState::Activated);
}

// https://w3c.github.io/ServiceWorker/#on-client-unload-algorithm
void SWServerRegistration::handleClientUnload()
{
    if (hasClientsUsingRegistration())
        return;
    if (isUnregistered() && tryClear())
        return;
    tryActivate();
}

bool SWServerRegistration::isUnregistered() const
{
    return m_server.getRegistration(key()) != this;
}

void SWServerRegistration::controlClient(ServiceWorkerClientIdentifier identifier)
{
    ASSERT(activeWorker());

    addClientUsingRegistration(identifier);

    HashSet<DocumentIdentifier> identifiers;
    identifiers.add(identifier.contextIdentifier);
    m_server.connection(identifier.serverConnectionIdentifier)->notifyClientsOfControllerChange(identifiers, activeWorker()->data());
}

bool SWServerRegistration::shouldSoftUpdate(const FetchOptions& options) const
{
    if (options.mode == FetchOptions::Mode::Navigate)
        return true;

    return WebCore::isNonSubresourceRequest(options.destination) && isStale();
}

void SWServerRegistration::softUpdate()
{
    m_server.softUpdate(*this);
}

void SWServerRegistration::scheduleSoftUpdate()
{
    // To avoid scheduling many updates during a single page load, we do soft updates on a 1 second delay and keep delaying
    // as long as soft update requests keep coming. This seems to match Chrome's behavior.
    if (m_softUpdateTimer.isActive())
        return;

    RELEASE_LOG(ServiceWorker, "SWServerRegistration::softUpdateIfNeeded");
    m_softUpdateTimer.startOneShot(softUpdateDelay);
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
