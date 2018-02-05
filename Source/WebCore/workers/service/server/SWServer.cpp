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
    m_server.registerConnection(*this);
}

SWServer::Connection::~Connection()
{
    m_server.unregisterConnection(*this);
}

HashSet<SWServer*>& SWServer::allServers()
{
    static NeverDestroyed<HashSet<SWServer*>> servers;
    return servers;
}

SWServer::~SWServer()
{
    RELEASE_ASSERT(m_connections.isEmpty());
    RELEASE_ASSERT(m_registrations.isEmpty());
    RELEASE_ASSERT(m_jobQueues.isEmpty());
    
    allServers().remove(this);
}

SWServerWorker* SWServer::workerByID(ServiceWorkerIdentifier identifier) const
{
    auto* worker = SWServerWorker::existingWorkerForIdentifier(identifier);
    ASSERT(!worker || &worker->server() == this);
    return worker;
}

std::optional<ServiceWorkerClientData> SWServer::serviceWorkerClientWithOriginByID(const ClientOrigin& clientOrigin, const ServiceWorkerClientIdentifier& clientIdentifier) const
{
    auto iterator = m_clientIdentifiersPerOrigin.find(clientOrigin);
    if (iterator == m_clientIdentifiersPerOrigin.end())
        return std::nullopt;

    if (!iterator->value.identifiers.contains(clientIdentifier))
        return std::nullopt;

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

    auto* connection = SWServerToContextConnection::globalServerToContextConnection();
    auto worker = SWServerWorker::create(*this, *registrationPtr, connection ? connection->identifier() : SWServerToContextConnectionIdentifier(), data.scriptURL, data.script, data.contentSecurityPolicy, data.workerType, data.serviceWorkerIdentifier);
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

    m_originStore->add(key.topOrigin().securityOrigin());
}

void SWServer::removeRegistration(const ServiceWorkerRegistrationKey& key)
{
    auto topOrigin = key.topOrigin().securityOrigin();
    auto registration = m_registrations.take(key);
    ASSERT(registration);
    bool wasRemoved = m_registrationsByID.remove(registration->identifier());
    ASSERT_UNUSED(wasRemoved, wasRemoved);

    m_originStore->remove(topOrigin);
    m_registrationStore.removeRegistration(*registration);
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
    m_registrationStore.clearAll(WTFMove(completionHandler));
}

void SWServer::clear(const SecurityOrigin& origin, CompletionHandler<void()>&& completionHandler)
{
    if (!m_importCompleted) {
        m_clearCompletionCallbacks.append([this, origin = makeRef(origin), completionHandler = WTFMove(completionHandler)] () mutable {
            ASSERT(m_importCompleted);
            clear(origin, WTFMove(completionHandler));
        });
        return;
    }

    m_jobQueues.removeIf([&](auto& keyAndValue) {
        return keyAndValue.key.relatesToOrigin(origin);
    });

    Vector<SWServerRegistration*> registrationsToRemove;
    for (auto& keyAndValue : m_registrations) {
        if (keyAndValue.key.relatesToOrigin(origin))
            registrationsToRemove.append(keyAndValue.value.get());
    }

    m_pendingContextDatas.removeAllMatching([&](auto& contextData) {
        return contextData.registration.key.relatesToOrigin(origin);
    });

    if (registrationsToRemove.isEmpty()) {
        completionHandler();
        return;
    }

    // Calling SWServerRegistration::clear() takes care of updating m_registrations, m_originStore and m_registrationStore.
    for (auto* registration : registrationsToRemove)
        registration->clear();

    m_registrationStore.flushChanges(WTFMove(completionHandler));
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
    , m_registrationStore(*this, WTFMove(registrationDatabaseDirectory))
    , m_sessionID(sessionID)
{
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

void SWServer::scriptContextFailedToStart(const std::optional<ServiceWorkerJobDataIdentifier>& jobDataIdentifier, SWServerWorker& worker, const String& message)
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

void SWServer::scriptContextStarted(const std::optional<ServiceWorkerJobDataIdentifier>& jobDataIdentifier, SWServerWorker& worker)
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

void SWServer::didFinishInstall(const std::optional<ServiceWorkerJobDataIdentifier>& jobDataIdentifier, SWServerWorker& worker, bool wasSuccessful)
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

    if (auto* registration = getRegistration(worker.registrationKey()))
        registration->didFinishActivation(worker.identifier());
}

// https://w3c.github.io/ServiceWorker/#clients-getall
void SWServer::matchAll(SWServerWorker& worker, const ServiceWorkerClientQueryOptions& options, ServiceWorkerClientsMatchAllCallback&& callback)
{
    // FIXME: Support reserved client filtering.
    // FIXME: Support WindowClient additional properties.

    Vector<ServiceWorkerClientData> matchingClients;
    forEachClientForOrigin(worker.origin(), [&](auto& clientData) {
        if (!options.includeUncontrolled && worker.identifier() != m_clientToControllingWorker.get(clientData.identifier))
            return;
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
        auto* registration = doRegistrationMatching(origin.topOrigin, clientData.url);
        if (!(registration && registration->key() == worker.registrationKey()))
            return;

        auto result = m_clientToControllingWorker.add(clientData.identifier, worker.identifier());
        if (!result.isNewEntry) {
            auto previousIdentifier = result.iterator->value;
            if (previousIdentifier == worker.identifier())
                return;
            result.iterator->value = worker.identifier();
            if (auto* controllingRegistration = registrationFromServiceWorkerIdentifier(previousIdentifier))
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
    auto* registration = m_registrationsByID.get(identifier);
    if (!registration) {
        LOG_ERROR("Request to remove client-side ServiceWorkerRegistration from non-existent server-side registration");
        return;
    }
    
    registration->removeClientServiceWorkerRegistration(connection.identifier());
}

void SWServer::updateWorker(Connection&, const ServiceWorkerJobDataIdentifier& jobDataIdentifier, SWServerRegistration& registration, const URL& url, const String& script, const ContentSecurityPolicyResponseHeaders& contentSecurityPolicy, WorkerType type)
{
    tryInstallContextData({ jobDataIdentifier, registration.data(), generateObjectIdentifier<ServiceWorkerIdentifierType>(), script, contentSecurityPolicy, url, type, false });
}

void SWServer::tryInstallContextData(ServiceWorkerContextData&& data)
{
    // Right now we only ever keep up to one connection to one SW context process.
    // And it should always exist if we're trying to install context data.
    auto* connection = SWServerToContextConnection::globalServerToContextConnection();
    if (!connection) {
        m_pendingContextDatas.append(WTFMove(data));
        return;
    }
    
    installContextData(data);
}

void SWServer::serverToContextConnectionCreated()
{
    auto* connection = SWServerToContextConnection::globalServerToContextConnection();
    ASSERT(connection);

    auto pendingContextDatas = WTFMove(m_pendingContextDatas);
    for (auto& data : pendingContextDatas)
        installContextData(data);

    auto serviceWorkerRunRequests = WTFMove(m_serviceWorkerRunRequests);
    for (auto& item : serviceWorkerRunRequests) {
        bool success = runServiceWorker(item.key);
        for (auto& callback : item.value)
            callback(success, *connection);
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

    m_registrationStore.updateRegistration(data);

    auto* connection = SWServerToContextConnection::globalServerToContextConnection();
    ASSERT(connection);

    auto* registration = m_registrations.get(data.registration.key);
    RELEASE_ASSERT(registration);

    auto worker = SWServerWorker::create(*this, *registration, connection->identifier(), data.scriptURL, data.script, data.contentSecurityPolicy, data.workerType, data.serviceWorkerIdentifier);

    registration->setPreInstallationWorker(worker.ptr());
    worker->setState(SWServerWorker::State::Running);
    auto result = m_runningOrTerminatingWorkers.add(data.serviceWorkerIdentifier, WTFMove(worker));
    ASSERT_UNUSED(result, result.isNewEntry);

    connection->installServiceWorkerContext(data, m_sessionID);
}

void SWServer::runServiceWorkerIfNecessary(ServiceWorkerIdentifier identifier, RunServiceWorkerCallback&& callback)
{
    auto* connection = SWServerToContextConnection::globalServerToContextConnection();
    if (auto* worker = m_runningOrTerminatingWorkers.get(identifier)) {
        if (worker->isRunning()) {
            ASSERT(connection);
            callback(true, *connection);
            return;
        }
    }

    if (!connection) {
        m_serviceWorkerRunRequests.ensure(identifier, [&] {
            return Vector<RunServiceWorkerCallback> { };
        }).iterator->value.append(WTFMove(callback));
        return;
    }

    callback(runServiceWorker(identifier), *connection);
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

    auto* connection = SWServerToContextConnection::globalServerToContextConnection();
    ASSERT(connection);

    // When re-running a service worker after a context process crash, the connection identifier may have changed
    // so we update it here.
    worker->setContextConnectionIdentifier(connection->identifier());

    connection->installServiceWorkerContext(worker->contextData(), m_sessionID);

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

    auto contextConnectionIdentifier = worker.contextConnectionIdentifier();
    ASSERT(contextConnectionIdentifier);
    if (!contextConnectionIdentifier) {
        LOG_ERROR("Request to terminate a worker whose contextConnectionIdentifier is invalid");
        workerContextTerminated(worker);
        return;
    }

    auto* connection = SWServerToContextConnection::connectionForIdentifier(*contextConnectionIdentifier);
    ASSERT(connection);
    if (!connection) {
        LOG_ERROR("Request to terminate a worker whose context connection does not exist");
        workerContextTerminated(worker);
        return;
    }

    switch (mode) {
    case Asynchronous:
        connection->terminateWorker(worker.identifier());
        break;
    case Synchronous:
        connection->syncTerminateWorker(worker.identifier());
        break;
    };
}

void SWServer::markAllWorkersAsTerminated()
{
    while (!m_runningOrTerminatingWorkers.isEmpty())
        workerContextTerminated(m_runningOrTerminatingWorkers.begin()->value);
}

void SWServer::workerContextTerminated(SWServerWorker& worker)
{
    worker.setState(SWServerWorker::State::NotRunning);
    worker.setContextConnectionIdentifier(std::nullopt);

    if (auto* jobQueue = m_jobQueues.get(worker.registrationKey()))
        jobQueue->cancelJobsFromServiceWorker(worker.identifier());

    // At this point if no registrations are referencing the worker then it will be destroyed,
    // removing itself from the m_workersByID map.
    auto result = m_runningOrTerminatingWorkers.take(worker.identifier());
    ASSERT_UNUSED(result, result && result->ptr() == &worker);
}

void SWServer::fireInstallEvent(SWServerWorker& worker)
{
    auto contextConnectionIdentifier = worker.contextConnectionIdentifier();
    if (!contextConnectionIdentifier) {
        LOG_ERROR("Request to fire install event on a worker whose contextConnectionIdentifier is invalid");
        return;
    }
    auto* connection = SWServerToContextConnection::connectionForIdentifier(*contextConnectionIdentifier);
    if (!connection) {
        LOG_ERROR("Request to fire install event on a worker whose context connection does not exist");
        return;
    }

    connection->fireInstallEvent(worker.identifier());
}

void SWServer::fireActivateEvent(SWServerWorker& worker)
{
    auto contextConnectionIdentifier = worker.contextConnectionIdentifier();
    if (!contextConnectionIdentifier) {
        LOG_ERROR("Request to fire install event on a worker whose contextConnectionIdentifier is invalid");
        return;
    }

    auto* connection = SWServerToContextConnection::connectionForIdentifier(*contextConnectionIdentifier);
    if (!connection) {
        LOG_ERROR("Request to fire install event on a worker whose context connection does not exist");
        return;
    }

    connection->fireActivateEvent(worker.identifier());
}

void SWServer::registerConnection(Connection& connection)
{
    auto result = m_connections.add(connection.identifier(), nullptr);
    ASSERT(result.isNewEntry);
    result.iterator->value = &connection;
}

void SWServer::unregisterConnection(Connection& connection)
{
    ASSERT(m_connections.get(connection.identifier()) == &connection);
    m_connections.remove(connection.identifier());

    for (auto& registration : m_registrations.values())
        registration->unregisterServerConnection(connection.identifier());

    for (auto& jobQueue : m_jobQueues.values())
        jobQueue->cancelJobsFromConnection(connection.identifier());
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

void SWServer::setClientActiveWorker(ServiceWorkerClientIdentifier clientIdentifier, ServiceWorkerIdentifier serviceWorkerIdentifier)
{
    m_clientToControllingWorker.set(clientIdentifier, serviceWorkerIdentifier);
}

SWServerRegistration* SWServer::registrationFromServiceWorkerIdentifier(ServiceWorkerIdentifier identifier)
{
    auto iterator = m_runningOrTerminatingWorkers.find(identifier);
    if (iterator == m_runningOrTerminatingWorkers.end())
        return nullptr;

    return m_registrations.get(iterator->value->registrationKey());
}

void SWServer::registerServiceWorkerClient(ClientOrigin&& clientOrigin, ServiceWorkerClientData&& data, const std::optional<ServiceWorkerIdentifier>& controllingServiceWorkerIdentifier)
{
    auto clientIdentifier = data.identifier;
    auto addResult = m_clientsById.add(clientIdentifier, WTFMove(data));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);

    auto& clientIdentifiersForOrigin = m_clientIdentifiersPerOrigin.ensure(WTFMove(clientOrigin), [] {
        return Clients { };
    }).iterator->value;
    clientIdentifiersForOrigin.identifiers.append(clientIdentifier);
    clientIdentifiersForOrigin.terminateServiceWorkersTimer = nullptr;

    if (!controllingServiceWorkerIdentifier)
        return;

    if (auto* controllingRegistration = registrationFromServiceWorkerIdentifier(*controllingServiceWorkerIdentifier)) {
        controllingRegistration->addClientUsingRegistration(clientIdentifier);
        auto result = m_clientToControllingWorker.add(clientIdentifier, *controllingServiceWorkerIdentifier);
        ASSERT_UNUSED(result, result.isNewEntry);
    }
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
            for (auto& worker : m_runningOrTerminatingWorkers.values()) {
                if (worker->isRunning() && worker->origin() == clientOrigin)
                    terminateWorker(worker);
            }
            m_clientIdentifiersPerOrigin.remove(clientOrigin);
        });
        iterator->value.terminateServiceWorkersTimer->startOneShot(terminationDelay);
    }

    auto workerIterator = m_clientToControllingWorker.find(clientIdentifier);
    if (workerIterator == m_clientToControllingWorker.end())
        return;

    if (auto* controllingRegistration = registrationFromServiceWorkerIdentifier(workerIterator->value))
        controllingRegistration->removeClientUsingRegistration(clientIdentifier);

    m_clientToControllingWorker.remove(workerIterator);
}

void SWServer::resolveRegistrationReadyRequests(SWServerRegistration& registration)
{
    for (auto* connection : m_connections.values())
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

        registrationReady(request.identifier, registration.data());
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
        originsWithRegistrations.add(SecurityOriginData { key.scope().protocol().toString(), key.scope().host(), key.scope().port() });
    }

    auto callbacks = WTFMove(m_getOriginsWithRegistrationsCallbacks);
    for (auto& callback : callbacks)
        callback(originsWithRegistrations);
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
