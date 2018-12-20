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
#include "SWServer.h"

#if ENABLE(SERVICE_WORKER)

#include "ExceptionCode.h"
#include "ExceptionData.h"
#include "Logging.h"
#include "RegistrationStore.h"
#include "SWOriginStore.h"
#include "SWServerJobQueue.h"
#include "SWServerRegistration.h"
#include "SWServerToContextConnection.h"
#include "SWServerWorker.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerClientType.h"
#include "ServiceWorkerContextData.h"
#include "ServiceWorkerFetchResult.h"
#include "ServiceWorkerJobData.h"
#include <wtf/CompletionHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static Seconds terminationDelay { 10_s };

SWServer::Connection::Connection(SWServer& server)
    : m_server(server)
    , m_identifier(generateObjectIdentifier<SWServerConnectionIdentifierType>())
{
}

HashSet<SWServer*>& SWServer::allServers()
{
    static NeverDestroyed<HashSet<SWServer*>> servers;
    return servers;
}

SWServer::~SWServer()
{
    // Destroy the remaining connections before the SWServer gets destroyed since they have a raw pointer
    // to the server and since they try to unregister clients from the server in their destructor.
    m_connections.clear();

    allServers().remove(this);
}

SWServerWorker* SWServer::workerByID(ServiceWorkerIdentifier identifier) const
{
    auto* worker = SWServerWorker::existingWorkerForIdentifier(identifier);
    ASSERT(!worker || &worker->server() == this);
    return worker;
}

Optional<ServiceWorkerClientData> SWServer::serviceWorkerClientWithOriginByID(const ClientOrigin& clientOrigin, const ServiceWorkerClientIdentifier& clientIdentifier) const
{
    auto iterator = m_clientIdentifiersPerOrigin.find(clientOrigin);
    if (iterator == m_clientIdentifiersPerOrigin.end())
        return WTF::nullopt;

    if (!iterator->value.identifiers.contains(clientIdentifier))
        return WTF::nullopt;

    auto clientIterator = m_clientsById.find(clientIdentifier);
    ASSERT(clientIterator != m_clientsById.end());
    return clientIterator->value;
}

SWServerWorker* SWServer::activeWorkerFromRegistrationID(ServiceWorkerRegistrationIdentifier identifier)
{
    auto* registration = m_registrationsByID.get(identifier);
    return registration ? registration->activeWorker() : nullptr;
}

SWServerRegistration* SWServer::getRegistration(const ServiceWorkerRegistrationKey& registrationKey)
{
    return m_registrations.get(registrationKey);
}

void SWServer::registrationStoreImportComplete()
{
    ASSERT(!m_importCompleted);
    m_importCompleted = true;
    m_originStore->importComplete();

    auto clearCallbacks = WTFMove(m_clearCompletionCallbacks);
    for (auto& callback : clearCallbacks)
        callback();

    performGetOriginsWithRegistrationsCallbacks();
}

void SWServer::registrationStoreDatabaseFailedToOpen()
{
    if (!m_importCompleted)
        registrationStoreImportComplete();
}

void SWServer::addRegistrationFromStore(ServiceWorkerContextData&& data)
{
    // Pages should not have been able to make a new registration to this key while the import was still taking place.
    ASSERT(!m_registrations.contains(data.registration.key));

    auto registration = std::make_unique<SWServerRegistration>(*this, data.registration.key, data.registration.updateViaCache, data.registration.scopeURL, data.scriptURL);
    registration->setLastUpdateTime(data.registration.lastUpdateTime);
    auto registrationPtr = registration.get();
    addRegistration(WTFMove(registration));

    auto worker = SWServerWorker::create(*this, *registrationPtr, data.scriptURL, data.script, data.contentSecurityPolicy, data.workerType, data.serviceWorkerIdentifier, HashMap<URL, ServiceWorkerContextData::ImportedScript> { data.scriptResourceMap });
    registrationPtr->updateRegistrationState(ServiceWorkerRegistrationState::Active, worker.ptr());
    worker->setState(ServiceWorkerState::Activated);
}

void SWServer::addRegistration(std::unique_ptr<SWServerRegistration>&& registration)
{
    auto key = registration->key();
    auto* registrationPtr = registration.get();
    auto addResult1 = m_registrations.add(key, WTFMove(registration));
    ASSERT_UNUSED(addResult1, addResult1.isNewEntry);

    auto addResult2 = m_registrationsByID.add(registrationPtr->identifier(), registrationPtr);
    ASSERT_UNUSED(addResult2, addResult2.isNewEntry);

    m_originStore->add(key.topOrigin());
}

void SWServer::removeRegistration(const ServiceWorkerRegistrationKey& key)
{
    auto topOrigin = key.topOrigin();
    auto registration = m_registrations.take(key);
    ASSERT(registration);
    bool wasRemoved = m_registrationsByID.remove(registration->identifier());
    ASSERT_UNUSED(wasRemoved, wasRemoved);

    m_originStore->remove(topOrigin);
    if (m_registrationStore)
        m_registrationStore->removeRegistration(*registration);
}

Vector<ServiceWorkerRegistrationData> SWServer::getRegistrations(const SecurityOriginData& topOrigin, const URL& clientURL)
{
    Vector<SWServerRegistration*> matchingRegistrations;
    for (auto& item : m_registrations) {
        if (!item.value->isUninstalling() && item.key.originIsMatching(topOrigin, clientURL))
            matchingRegistrations.append(item.value.get());
    }
    // The specification mandates that registrations are returned in the insertion order.
    std::sort(matchingRegistrations.begin(), matchingRegistrations.end(), [](auto& a, auto& b) {
        return a->creationTime() < b->creationTime();
    });
    Vector<ServiceWorkerRegistrationData> matchingRegistrationDatas;
    matchingRegistrationDatas.reserveInitialCapacity(matchingRegistrations.size());
    for (auto* registration : matchingRegistrations)
        matchingRegistrationDatas.uncheckedAppend(registration->data());
    return matchingRegistrationDatas;
}

void SWServer::clearAll(CompletionHandler<void()>&& completionHandler)
{
    if (!m_importCompleted) {
        m_clearCompletionCallbacks.append([this, completionHandler = WTFMove(completionHandler)] () mutable {
            ASSERT(m_importCompleted);
            clearAll(WTFMove(completionHandler));
        });
        return;
    }

    m_jobQueues.clear();
    while (!m_registrations.isEmpty())
        m_registrations.begin()->value->clear();
    ASSERT(m_registrationsByID.isEmpty());
    m_pendingContextDatas.clear();
    m_originStore->clearAll();
    if (m_registrationStore)
        m_registrationStore->clearAll(WTFMove(completionHandler));
}

void SWServer::clear(const SecurityOriginData& securityOrigin, CompletionHandler<void()>&& completionHandler)
{
    if (!m_importCompleted) {
        m_clearCompletionCallbacks.append([this, securityOrigin, completionHandler = WTFMove(completionHandler)] () mutable {
            ASSERT(m_importCompleted);
            clear(securityOrigin, WTFMove(completionHandler));
        });
        return;
    }

    m_jobQueues.removeIf([&](auto& keyAndValue) {
        return keyAndValue.key.relatesToOrigin(securityOrigin);
    });

    Vector<SWServerRegistration*> registrationsToRemove;
    for (auto& keyAndValue : m_registrations) {
        if (keyAndValue.key.relatesToOrigin(securityOrigin))
            registrationsToRemove.append(keyAndValue.value.get());
    }

    for (auto& contextDatas : m_pendingContextDatas.values()) {
        contextDatas.removeAllMatching([&](auto& contextData) {
            return contextData.registration.key.relatesToOrigin(securityOrigin);
        });
    }

    if (registrationsToRemove.isEmpty()) {
        completionHandler();
        return;
    }

    // Calling SWServerRegistration::clear() takes care of updating m_registrations, m_originStore and m_registrationStore.
    for (auto* registration : registrationsToRemove)
        registration->clear();

    if (m_registrationStore)
        m_registrationStore->flushChanges(WTFMove(completionHandler));
}

void SWServer::Connection::finishFetchingScriptInServer(const ServiceWorkerFetchResult& result)
{
    m_server.scriptFetchFinished(*this, result);
}

void SWServer::Connection::didResolveRegistrationPromise(const ServiceWorkerRegistrationKey& key)
{
    m_server.didResolveRegistrationPromise(*this, key);
}

void SWServer::Connection::addServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    m_server.addClientServiceWorkerRegistration(*this, identifier);
}

void SWServer::Connection::removeServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    m_server.removeClientServiceWorkerRegistration(*this, identifier);
}

void SWServer::Connection::syncTerminateWorker(ServiceWorkerIdentifier identifier)
{
    if (auto* worker = m_server.workerByID(identifier))
        m_server.syncTerminateWorker(*worker);
}

SWServer::SWServer(UniqueRef<SWOriginStore>&& originStore, String&& registrationDatabaseDirectory, PAL::SessionID sessionID)
    : m_originStore(WTFMove(originStore))
    , m_sessionID(sessionID)
{
    ASSERT(!registrationDatabaseDirectory.isEmpty() || m_sessionID.isEphemeral());
    if (!m_sessionID.isEphemeral())
        m_registrationStore = std::make_unique<RegistrationStore>(*this, WTFMove(registrationDatabaseDirectory));
    else
        registrationStoreImportComplete();

    UNUSED_PARAM(registrationDatabaseDirectory);
    allServers().add(this);
}

// https://w3c.github.io/ServiceWorker/#schedule-job-algorithm
void SWServer::scheduleJob(ServiceWorkerJobData&& jobData)
{
    ASSERT(m_connections.contains(jobData.connectionIdentifier()));

    // FIXME: Per the spec, check if this job is equivalent to the last job on the queue.
    // If it is, stack it along with that job.

    auto& jobQueue = *m_jobQueues.ensure(jobData.registrationKey(), [this, &jobData] {
        return std::make_unique<SWServerJobQueue>(*this, jobData.registrationKey());
    }).iterator->value;

    jobQueue.enqueueJob(jobData);
    if (jobQueue.size() == 1)
        jobQueue.runNextJob();
}

void SWServer::rejectJob(const ServiceWorkerJobData& jobData, const ExceptionData& exceptionData)
{
    LOG(ServiceWorker, "Rejected ServiceWorker job %s in server", jobData.identifier().loggingString().utf8().data());
    auto* connection = m_connections.get(jobData.connectionIdentifier());
    if (!connection)
        return;

    connection->rejectJobInClient(jobData.identifier().jobIdentifier, exceptionData);
}

void SWServer::resolveRegistrationJob(const ServiceWorkerJobData& jobData, const ServiceWorkerRegistrationData& registrationData, ShouldNotifyWhenResolved shouldNotifyWhenResolved)
{
    LOG(ServiceWorker, "Resolved ServiceWorker job %s in server with registration %s", jobData.identifier().loggingString().utf8().data(), registrationData.identifier.loggingString().utf8().data());
    auto* connection = m_connections.get(jobData.connectionIdentifier());
    if (!connection)
        return;

    connection->resolveRegistrationJobInClient(jobData.identifier().jobIdentifier, registrationData, shouldNotifyWhenResolved);
}

void SWServer::resolveUnregistrationJob(const ServiceWorkerJobData& jobData, const ServiceWorkerRegistrationKey& registrationKey, bool unregistrationResult)
{
    auto* connection = m_connections.get(jobData.connectionIdentifier());
    if (!connection)
        return;

    connection->resolveUnregistrationJobInClient(jobData.identifier().jobIdentifier, registrationKey, unregistrationResult);
}

void SWServer::startScriptFetch(const ServiceWorkerJobData& jobData, FetchOptions::Cache cachePolicy)
{
    LOG(ServiceWorker, "Server issuing startScriptFetch for current job %s in client", jobData.identifier().loggingString().utf8().data());
    auto* connection = m_connections.get(jobData.connectionIdentifier());
    ASSERT_WITH_MESSAGE(connection, "If the connection was lost, this job should have been cancelled");
    if (connection)
        connection->startScriptFetchInClient(jobData.identifier().jobIdentifier, jobData.registrationKey(), cachePolicy);
}

void SWServer::scriptFetchFinished(Connection& connection, const ServiceWorkerFetchResult& result)
{
    LOG(ServiceWorker, "Server handling scriptFetchFinished for current job %s in client", result.jobDataIdentifier.loggingString().utf8().data());

    ASSERT(m_connections.contains(result.jobDataIdentifier.connectionIdentifier));

    auto jobQueue = m_jobQueues.get(result.registrationKey);
    if (!jobQueue)
        return;

    jobQueue->scriptFetchFinished(connection, result);
}

void SWServer::scriptContextFailedToStart(const Optional<ServiceWorkerJobDataIdentifier>& jobDataIdentifier, SWServerWorker& worker, const String& message)
{
    if (!jobDataIdentifier)
        return;

    RELEASE_LOG_ERROR(ServiceWorker, "%p - SWServer::scriptContextFailedToStart: Failed to start SW for job %s, error: %s", this, jobDataIdentifier->loggingString().utf8().data(), message.utf8().data());

    auto* jobQueue = m_jobQueues.get(worker.registrationKey());
    if (!jobQueue || !jobQueue->isCurrentlyProcessingJob(*jobDataIdentifier)) {
        // The job which started this worker has been canceled, terminate this worker.
        terminatePreinstallationWorker(worker);
        return;
    }
    jobQueue->scriptContextFailedToStart(*jobDataIdentifier, worker.identifier(), message);
}

void SWServer::scriptContextStarted(const Optional<ServiceWorkerJobDataIdentifier>& jobDataIdentifier, SWServerWorker& worker)
{
    if (!jobDataIdentifier)
        return;

    auto* jobQueue = m_jobQueues.get(worker.registrationKey());
    if (!jobQueue || !jobQueue->isCurrentlyProcessingJob(*jobDataIdentifier)) {
        // The job which started this worker has been canceled, terminate this worker.
        terminatePreinstallationWorker(worker);
        return;
    }
    jobQueue->scriptContextStarted(*jobDataIdentifier, worker.identifier());
}

void SWServer::terminatePreinstallationWorker(SWServerWorker& worker)
{
    worker.terminate();
    auto* registration = getRegistration(worker.registrationKey());
    if (registration && registration->preInstallationWorker() == &worker)
        registration->setPreInstallationWorker(nullptr);
}

void SWServer::didFinishInstall(const Optional<ServiceWorkerJobDataIdentifier>& jobDataIdentifier, SWServerWorker& worker, bool wasSuccessful)
{
    if (!jobDataIdentifier)
        return;

    if (wasSuccessful)
        RELEASE_LOG(ServiceWorker, "%p - SWServer::didFinishInstall: Successfuly finished SW install for job %s", this, jobDataIdentifier->loggingString().utf8().data());
    else
        RELEASE_LOG_ERROR(ServiceWorker, "%p - SWServer::didFinishInstall: Failed SW install for job %s", this, jobDataIdentifier->loggingString().utf8().data());

    if (auto* jobQueue = m_jobQueues.get(worker.registrationKey()))
        jobQueue->didFinishInstall(*jobDataIdentifier, worker.identifier(), wasSuccessful);
}

void SWServer::didFinishActivation(SWServerWorker& worker)
{
    RELEASE_LOG(ServiceWorker, "%p - SWServer::didFinishActivation: Finished activation for service worker %llu", this, worker.identifier().toUInt64());

    auto* registration = getRegistration(worker.registrationKey());
    if (!registration)
        return;

    if (m_registrationStore)
        m_registrationStore->updateRegistration(worker.contextData());
    registration->didFinishActivation(worker.identifier());
}

// https://w3c.github.io/ServiceWorker/#clients-getall
void SWServer::matchAll(SWServerWorker& worker, const ServiceWorkerClientQueryOptions& options, ServiceWorkerClientsMatchAllCallback&& callback)
{
    // FIXME: Support reserved client filtering.
    // FIXME: Support WindowClient additional properties.

    Vector<ServiceWorkerClientData> matchingClients;
    forEachClientForOrigin(worker.origin(), [&](auto& clientData) {
        if (!options.includeUncontrolled) {
            auto registrationIdentifier = m_clientToControllingRegistration.get(clientData.identifier);
            if (worker.data().registrationIdentifier != registrationIdentifier)
                return;
            if (&worker != this->activeWorkerFromRegistrationID(registrationIdentifier))
                return;
        }
        if (options.type != ServiceWorkerClientType::All && options.type != clientData.type)
            return;
        matchingClients.append(clientData);
    });
    callback(WTFMove(matchingClients));
}

void SWServer::forEachClientForOrigin(const ClientOrigin& origin, const WTF::Function<void(ServiceWorkerClientData&)>& apply)
{
    auto iterator = m_clientIdentifiersPerOrigin.find(origin);
    if (iterator == m_clientIdentifiersPerOrigin.end())
        return;

    for (auto& clientIdentifier : iterator->value.identifiers) {
        auto clientIterator = m_clientsById.find(clientIdentifier);
        ASSERT(clientIterator != m_clientsById.end());
        apply(clientIterator->value);
    }
}

void SWServer::claim(SWServerWorker& worker)
{
    auto& origin = worker.origin();
    forEachClientForOrigin(origin, [&](auto& clientData) {
        auto* registration = this->doRegistrationMatching(origin.topOrigin, clientData.url);
        if (!(registration && registration->key() == worker.registrationKey()))
            return;

        auto result = m_clientToControllingRegistration.add(clientData.identifier, registration->identifier());
        if (!result.isNewEntry) {
            auto previousIdentifier = result.iterator->value;
            if (previousIdentifier == registration->identifier())
                return;
            result.iterator->value = registration->identifier();
            if (auto* controllingRegistration = m_registrationsByID.get(previousIdentifier))
                controllingRegistration->removeClientUsingRegistration(clientData.identifier);
        }
        registration->controlClient(clientData.identifier);
    });
}

void SWServer::didResolveRegistrationPromise(Connection& connection, const ServiceWorkerRegistrationKey& registrationKey)
{
    ASSERT_UNUSED(connection, m_connections.contains(connection.identifier()));

    if (auto* jobQueue = m_jobQueues.get(registrationKey))
        jobQueue->didResolveRegistrationPromise();
}

void SWServer::addClientServiceWorkerRegistration(Connection& connection, ServiceWorkerRegistrationIdentifier identifier)
{
    auto* registration = m_registrationsByID.get(identifier);
    if (!registration) {
        LOG_ERROR("Request to add client-side ServiceWorkerRegistration to non-existent server-side registration");
        return;
    }
    
    registration->addClientServiceWorkerRegistration(connection.identifier());
}

void SWServer::removeClientServiceWorkerRegistration(Connection& connection, ServiceWorkerRegistrationIdentifier identifier)
{
    if (auto* registration = m_registrationsByID.get(identifier))
        registration->removeClientServiceWorkerRegistration(connection.identifier());
}

void SWServer::updateWorker(Connection&, const ServiceWorkerJobDataIdentifier& jobDataIdentifier, SWServerRegistration& registration, const URL& url, const String& script, const ContentSecurityPolicyResponseHeaders& contentSecurityPolicy, WorkerType type, HashMap<URL, ServiceWorkerContextData::ImportedScript>&& scriptResourceMap)
{
    tryInstallContextData({ jobDataIdentifier, registration.data(), generateObjectIdentifier<ServiceWorkerIdentifierType>(), script, contentSecurityPolicy, url, type, sessionID(), false, WTFMove(scriptResourceMap) });
}

void SWServer::tryInstallContextData(ServiceWorkerContextData&& data)
{
    auto securityOrigin = SecurityOriginData::fromURL(data.scriptURL);
    auto* connection = SWServerToContextConnection::connectionForOrigin(securityOrigin);
    if (!connection) {
        m_pendingContextDatas.ensure(WTFMove(securityOrigin), [] {
            return Vector<ServiceWorkerContextData> { };
        }).iterator->value.append(WTFMove(data));
        return;
    }
    
    installContextData(data);
}

void SWServer::serverToContextConnectionCreated(SWServerToContextConnection& contextConnection)
{
    auto pendingContextDatas = m_pendingContextDatas.take(contextConnection.securityOrigin());
    for (auto& data : pendingContextDatas)
        installContextData(data);

    auto serviceWorkerRunRequests = m_serviceWorkerRunRequests.take(contextConnection.securityOrigin());
    for (auto& item : serviceWorkerRunRequests) {
        bool success = runServiceWorker(item.key);
        for (auto& callback : item.value)
            callback(success ? &contextConnection : nullptr);
    }
}

void SWServer::installContextData(const ServiceWorkerContextData& data)
{
    ASSERT_WITH_MESSAGE(!data.loadedFromDisk, "Workers we just read from disk should only be launched as needed");

    if (data.jobDataIdentifier) {
        // Abort if the job that scheduled this has been cancelled.
        auto* jobQueue = m_jobQueues.get(data.registration.key);
        if (!jobQueue || !jobQueue->isCurrentlyProcessingJob(*data.jobDataIdentifier))
            return;
    }

    auto* registration = m_registrations.get(data.registration.key);
    RELEASE_ASSERT(registration);

    auto worker = SWServerWorker::create(*this, *registration, data.scriptURL, data.script, data.contentSecurityPolicy, data.workerType, data.serviceWorkerIdentifier, HashMap<URL, ServiceWorkerContextData::ImportedScript> { data.scriptResourceMap });

    auto* connection = worker->contextConnection();
    ASSERT(connection);

    registration->setPreInstallationWorker(worker.ptr());
    worker->setState(SWServerWorker::State::Running);
    auto result = m_runningOrTerminatingWorkers.add(data.serviceWorkerIdentifier, WTFMove(worker));
    ASSERT_UNUSED(result, result.isNewEntry);

    connection->installServiceWorkerContext(data, m_sessionID);
}

void SWServer::runServiceWorkerIfNecessary(ServiceWorkerIdentifier identifier, RunServiceWorkerCallback&& callback)
{
    auto* worker = workerByID(identifier);
    if (!worker) {
        callback(nullptr);
        return;
    }

    auto* contextConnection = worker->contextConnection();
    if (worker->isRunning()) {
        ASSERT(contextConnection);
        callback(contextConnection);
        return;
    }

    if (!contextConnection) {
        auto& serviceWorkerRunRequestsForOrigin = m_serviceWorkerRunRequests.ensure(worker->securityOrigin(), [] {
            return HashMap<ServiceWorkerIdentifier, Vector<RunServiceWorkerCallback>> { };
        }).iterator->value;
        serviceWorkerRunRequestsForOrigin.ensure(identifier, [&] {
            return Vector<RunServiceWorkerCallback> { };
        }).iterator->value.append(WTFMove(callback));
        return;
    }

    bool success = runServiceWorker(identifier);
    callback(success ? contextConnection : nullptr);
}

bool SWServer::runServiceWorker(ServiceWorkerIdentifier identifier)
{
    auto* worker = workerByID(identifier);
    if (!worker)
        return false;

    // If the registration for a working has been removed then the request to run
    // the worker is moot.
    if (!getRegistration(worker->registrationKey()))
        return false;

    auto addResult = m_runningOrTerminatingWorkers.add(identifier, *worker);
    ASSERT_UNUSED(addResult, addResult.isNewEntry || worker->isTerminating());

    worker->setState(SWServerWorker::State::Running);

    auto* contextConnection = worker->contextConnection();
    ASSERT(contextConnection);

    contextConnection->installServiceWorkerContext(worker->contextData(), m_sessionID);

    return true;
}

void SWServer::terminateWorker(SWServerWorker& worker)
{
    terminateWorkerInternal(worker, Asynchronous);
}

void SWServer::syncTerminateWorker(SWServerWorker& worker)
{
    terminateWorkerInternal(worker, Synchronous);
}

void SWServer::terminateWorkerInternal(SWServerWorker& worker, TerminationMode mode)
{
    ASSERT(m_runningOrTerminatingWorkers.get(worker.identifier()) == &worker);
    ASSERT(worker.isRunning());

    RELEASE_LOG(ServiceWorker, "%p - SWServer::terminateWorkerInternal: Terminating service worker %llu", this, worker.identifier().toUInt64());

    worker.setState(SWServerWorker::State::Terminating);

    auto* contextConnection = worker.contextConnection();
    ASSERT(contextConnection);
    if (!contextConnection) {
        LOG_ERROR("Request to terminate a worker whose context connection does not exist");
        workerContextTerminated(worker);
        return;
    }

    switch (mode) {
    case Asynchronous:
        contextConnection->terminateWorker(worker.identifier());
        break;
    case Synchronous:
        contextConnection->syncTerminateWorker(worker.identifier());
        break;
    };
}

void SWServer::markAllWorkersForOriginAsTerminated(const SecurityOriginData& securityOrigin)
{
    Vector<SWServerWorker*> terminatedWorkers;
    for (auto& worker : m_runningOrTerminatingWorkers.values()) {
        if (worker->securityOrigin() == securityOrigin)
            terminatedWorkers.append(worker.ptr());
    }
    for (auto& terminatedWorker : terminatedWorkers)
        workerContextTerminated(*terminatedWorker);
}

void SWServer::workerContextTerminated(SWServerWorker& worker)
{
    worker.setState(SWServerWorker::State::NotRunning);

    if (auto* jobQueue = m_jobQueues.get(worker.registrationKey()))
        jobQueue->cancelJobsFromServiceWorker(worker.identifier());

    // At this point if no registrations are referencing the worker then it will be destroyed,
    // removing itself from the m_workersByID map.
    auto result = m_runningOrTerminatingWorkers.take(worker.identifier());
    ASSERT_UNUSED(result, result && result->ptr() == &worker);
}

void SWServer::fireInstallEvent(SWServerWorker& worker)
{
    auto* contextConnection = worker.contextConnection();
    if (!contextConnection) {
        LOG_ERROR("Request to fire install event on a worker whose context connection does not exist");
        return;
    }

    contextConnection->fireInstallEvent(worker.identifier());
}

void SWServer::fireActivateEvent(SWServerWorker& worker)
{
    auto* contextConnection = worker.contextConnection();
    if (!contextConnection) {
        LOG_ERROR("Request to fire install event on a worker whose context connection does not exist");
        return;
    }

    contextConnection->fireActivateEvent(worker.identifier());
}

void SWServer::addConnection(std::unique_ptr<Connection>&& connection)
{
    auto identifier = connection->identifier();
    ASSERT(!m_connections.contains(identifier));
    m_connections.add(identifier, WTFMove(connection));
}

void SWServer::removeConnection(SWServerConnectionIdentifier connectionIdentifier)
{
    ASSERT(m_connections.contains(connectionIdentifier));
    m_connections.remove(connectionIdentifier);

    for (auto& registration : m_registrations.values())
        registration->unregisterServerConnection(connectionIdentifier);

    for (auto& jobQueue : m_jobQueues.values())
        jobQueue->cancelJobsFromConnection(connectionIdentifier);
}

SWServerRegistration* SWServer::doRegistrationMatching(const SecurityOriginData& topOrigin, const URL& clientURL)
{
    SWServerRegistration* selectedRegistration = nullptr;
    for (auto& registration : m_registrations.values()) {
        if (!registration->key().isMatching(topOrigin, clientURL))
            continue;
        if (!selectedRegistration || selectedRegistration->key().scopeLength() < registration->key().scopeLength())
            selectedRegistration = registration.get();
    }

    return (selectedRegistration && !selectedRegistration->isUninstalling()) ? selectedRegistration : nullptr;
}

SWServerRegistration* SWServer::registrationFromServiceWorkerIdentifier(ServiceWorkerIdentifier identifier)
{
    auto iterator = m_runningOrTerminatingWorkers.find(identifier);
    if (iterator == m_runningOrTerminatingWorkers.end())
        return nullptr;

    return m_registrations.get(iterator->value->registrationKey());
}

void SWServer::registerServiceWorkerClient(ClientOrigin&& clientOrigin, ServiceWorkerClientData&& data, const Optional<ServiceWorkerRegistrationIdentifier>& controllingServiceWorkerRegistrationIdentifier)
{
    auto clientIdentifier = data.identifier;

    ASSERT(!m_clientsById.contains(clientIdentifier));
    m_clientsById.add(clientIdentifier, WTFMove(data));

    auto& clientIdentifiersForOrigin = m_clientIdentifiersPerOrigin.ensure(clientOrigin, [] {
        return Clients { };
    }).iterator->value;

    ASSERT(!clientIdentifiersForOrigin.identifiers.contains(clientIdentifier));
    clientIdentifiersForOrigin.identifiers.append(clientIdentifier);
    clientIdentifiersForOrigin.terminateServiceWorkersTimer = nullptr;

    m_clientsBySecurityOrigin.ensure(clientOrigin.clientOrigin, [] {
        return HashSet<ServiceWorkerClientIdentifier> { };
    }).iterator->value.add(clientIdentifier);

    if (!controllingServiceWorkerRegistrationIdentifier)
        return;

    auto* controllingRegistration = m_registrationsByID.get(*controllingServiceWorkerRegistrationIdentifier);
    if (!controllingRegistration || !controllingRegistration->activeWorker())
        return;

    controllingRegistration->addClientUsingRegistration(clientIdentifier);
    ASSERT(!m_clientToControllingRegistration.contains(clientIdentifier));
    m_clientToControllingRegistration.add(clientIdentifier, *controllingServiceWorkerRegistrationIdentifier);
}

void SWServer::unregisterServiceWorkerClient(const ClientOrigin& clientOrigin, ServiceWorkerClientIdentifier clientIdentifier)
{
    bool wasRemoved = m_clientsById.remove(clientIdentifier);
    ASSERT_UNUSED(wasRemoved, wasRemoved);

    auto iterator = m_clientIdentifiersPerOrigin.find(clientOrigin);
    ASSERT(iterator != m_clientIdentifiersPerOrigin.end());

    auto& clientIdentifiers = iterator->value.identifiers;
    clientIdentifiers.removeFirstMatching([&] (const auto& identifier) {
        return clientIdentifier == identifier;
    });
    if (clientIdentifiers.isEmpty()) {
        ASSERT(!iterator->value.terminateServiceWorkersTimer);
        iterator->value.terminateServiceWorkersTimer = std::make_unique<Timer>([clientOrigin, this] {
            Vector<SWServerWorker*> workersToTerminate;
            for (auto& worker : m_runningOrTerminatingWorkers.values()) {
                if (worker->isRunning() && worker->origin() == clientOrigin)
                    workersToTerminate.append(worker.ptr());
            }
            for (auto* worker : workersToTerminate)
                terminateWorker(*worker);

            if (!m_clientsBySecurityOrigin.contains(clientOrigin.clientOrigin)) {
                if (auto* connection = SWServerToContextConnection::connectionForOrigin(clientOrigin.clientOrigin))
                    connection->connectionMayNoLongerBeNeeded();
            }

            m_clientIdentifiersPerOrigin.remove(clientOrigin);
        });
        iterator->value.terminateServiceWorkersTimer->startOneShot(m_shouldDisableServiceWorkerProcessTerminationDelay ? 0_s : terminationDelay);
    }

    auto clientsBySecurityOriginIterator = m_clientsBySecurityOrigin.find(clientOrigin.clientOrigin);
    ASSERT(clientsBySecurityOriginIterator != m_clientsBySecurityOrigin.end());
    auto& clientsForSecurityOrigin = clientsBySecurityOriginIterator->value;
    clientsForSecurityOrigin.remove(clientIdentifier);
    if (clientsForSecurityOrigin.isEmpty())
        m_clientsBySecurityOrigin.remove(clientsBySecurityOriginIterator);

    auto registrationIterator = m_clientToControllingRegistration.find(clientIdentifier);
    if (registrationIterator == m_clientToControllingRegistration.end())
        return;

    if (auto* registration = m_registrationsByID.get(registrationIterator->value))
        registration->removeClientUsingRegistration(clientIdentifier);

    m_clientToControllingRegistration.remove(registrationIterator);
}

bool SWServer::needsServerToContextConnectionForOrigin(const SecurityOriginData& securityOrigin) const
{
    return m_clientsBySecurityOrigin.contains(securityOrigin);
}

void SWServer::resolveRegistrationReadyRequests(SWServerRegistration& registration)
{
    for (auto& connection : m_connections.values())
        connection->resolveRegistrationReadyRequests(registration);
}

void SWServer::Connection::whenRegistrationReady(uint64_t registrationReadyRequestIdentifier, const SecurityOriginData& topOrigin, const URL& clientURL)
{
    if (auto* registration = doRegistrationMatching(topOrigin, clientURL)) {
        if (registration->activeWorker()) {
            registrationReady(registrationReadyRequestIdentifier, registration->data());
            return;
        }
    }
    m_registrationReadyRequests.append({ topOrigin, clientURL, registrationReadyRequestIdentifier });
}

void SWServer::Connection::resolveRegistrationReadyRequests(SWServerRegistration& registration)
{
    m_registrationReadyRequests.removeAllMatching([&](auto& request) {
        if (!registration.key().isMatching(request.topOrigin, request.clientURL))
            return false;

        this->registrationReady(request.identifier, registration.data());
        return true;
    });
}

void SWServer::getOriginsWithRegistrations(Function<void(const HashSet<SecurityOriginData>&)>&& callback)
{
    m_getOriginsWithRegistrationsCallbacks.append(WTFMove(callback));

    if (m_importCompleted)
        performGetOriginsWithRegistrationsCallbacks();
}

void SWServer::performGetOriginsWithRegistrationsCallbacks()
{
    ASSERT(isMainThread());
    ASSERT(m_importCompleted);

    if (m_getOriginsWithRegistrationsCallbacks.isEmpty())
        return;

    HashSet<SecurityOriginData> originsWithRegistrations;
    for (auto& key : m_registrations.keys()) {
        originsWithRegistrations.add(key.topOrigin());
        originsWithRegistrations.add(SecurityOriginData { key.scope().protocol().toString(), key.scope().host().toString(), key.scope().port() });
    }

    auto callbacks = WTFMove(m_getOriginsWithRegistrationsCallbacks);
    for (auto& callback : callbacks)
        callback(originsWithRegistrations);
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
