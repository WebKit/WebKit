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

static ServiceWorkerIdentifier generateServiceWorkerIdentifier()
{
    return generateObjectIdentifier<ServiceWorkerIdentifierType>();
}

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

    ASSERT(m_taskQueue.isEmpty());
    ASSERT(m_taskReplyQueue.isEmpty());

    // For a SWServer to be cleanly shut down its thread must have finished and gone away.
    // At this stage in development of the feature that actually never happens.
    // But once it does start happening, this ASSERT will catch us doing it wrong.
    Locker<Lock> locker(m_taskThreadLock);
    ASSERT(!m_taskThread);
    
    allServers().remove(this);
}

SWServerWorker* SWServer::workerByID(ServiceWorkerIdentifier identifier) const
{
    auto* worker = SWServerWorker::existingWorkerForIdentifier(identifier);
    ASSERT(!worker || &worker->server() == this);
    return worker;
}

SWServerRegistration* SWServer::getRegistration(const ServiceWorkerRegistrationKey& registrationKey)
{
    return m_registrations.get(registrationKey);
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

void SWServer::clearAll()
{
    m_jobQueues.clear();
    while (!m_registrations.isEmpty())
        m_registrations.begin()->value->clear();
    ASSERT(m_registrationsByID.isEmpty());
    m_originStore->clearAll();
}

void SWServer::clear(const SecurityOrigin& origin)
{
    m_originStore->clear(origin);

    // FIXME: We should clear entries in m_registrations, m_jobQueues and m_workersByID.
}

void SWServer::Connection::scheduleJobInServer(const ServiceWorkerJobData& jobData)
{
    LOG(ServiceWorker, "Scheduling ServiceWorker job %s in server", jobData.identifier().loggingString().utf8().data());
    ASSERT(identifier() == jobData.connectionIdentifier());

    m_server.scheduleJob(jobData);
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

SWServer::SWServer(UniqueRef<SWOriginStore>&& originStore, const String& registrationDatabaseDirectory)
    : m_originStore(WTFMove(originStore))
    , m_registrationStore(registrationDatabaseDirectory)
{
    UNUSED_PARAM(registrationDatabaseDirectory);
    allServers().add(this);
    m_taskThread = Thread::create(ASCIILiteral("ServiceWorker Task Thread"), [this] {
        taskThreadEntryPoint();
    });
}

// https://w3c.github.io/ServiceWorker/#schedule-job-algorithm
void SWServer::scheduleJob(const ServiceWorkerJobData& jobData)
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

    connection->rejectJobInClient(jobData.identifier(), exceptionData);
}

void SWServer::resolveRegistrationJob(const ServiceWorkerJobData& jobData, const ServiceWorkerRegistrationData& registrationData, ShouldNotifyWhenResolved shouldNotifyWhenResolved)
{
    LOG(ServiceWorker, "Resolved ServiceWorker job %s in server with registration %s", jobData.identifier().loggingString().utf8().data(), registrationData.identifier.loggingString().utf8().data());
    auto* connection = m_connections.get(jobData.connectionIdentifier());
    if (!connection)
        return;

    connection->resolveRegistrationJobInClient(jobData.identifier(), registrationData, shouldNotifyWhenResolved);
}

void SWServer::resolveUnregistrationJob(const ServiceWorkerJobData& jobData, const ServiceWorkerRegistrationKey& registrationKey, bool unregistrationResult)
{
    auto* connection = m_connections.get(jobData.connectionIdentifier());
    if (!connection)
        return;

    connection->resolveUnregistrationJobInClient(jobData.identifier(), registrationKey, unregistrationResult);
}

void SWServer::startScriptFetch(const ServiceWorkerJobData& jobData)
{
    LOG(ServiceWorker, "Server issuing startScriptFetch for current job %s in client", jobData.identifier().loggingString().utf8().data());
    auto* connection = m_connections.get(jobData.connectionIdentifier());
    if (!connection)
        return;

    connection->startScriptFetchInClient(jobData.identifier());
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

    if (auto* jobQueue = m_jobQueues.get(worker.registrationKey()))
        jobQueue->scriptContextFailedToStart(*jobDataIdentifier, worker.identifier(), message);
}

void SWServer::scriptContextStarted(const std::optional<ServiceWorkerJobDataIdentifier>& jobDataIdentifier, SWServerWorker& worker)
{
    if (!jobDataIdentifier)
        return;

    if (auto* jobQueue = m_jobQueues.get(worker.registrationKey()))
        jobQueue->scriptContextStarted(*jobDataIdentifier, worker.identifier());
}

void SWServer::didFinishInstall(const std::optional<ServiceWorkerJobDataIdentifier>& jobDataIdentifier, SWServerWorker& worker, bool wasSuccessful)
{
    if (!jobDataIdentifier)
        return;

    if (auto* jobQueue = m_jobQueues.get(worker.registrationKey()))
        jobQueue->didFinishInstall(*jobDataIdentifier, worker.identifier(), wasSuccessful);
}

void SWServer::didFinishActivation(SWServerWorker& worker)
{
    if (auto* registration = getRegistration(worker.registrationKey()))
        registration->didFinishActivation(worker.identifier());
}

// https://w3c.github.io/ServiceWorker/#clients-get
std::optional<ServiceWorkerClientData> SWServer::findClientByIdentifier(const ClientOrigin& origin, ServiceWorkerClientIdentifier clientIdentifier)
{
    // FIXME: Support WindowClient additional properties.

    auto iterator = m_clients.find(origin);
    if (iterator == m_clients.end())
        return std::nullopt;

    auto& clients = iterator->value.clients;
    auto position = clients.findMatching([&] (const auto& client) {
        return clientIdentifier == client.identifier;
    });

    return (position != notFound) ? std::make_optional(clients[position].data) : std::nullopt;
}

// https://w3c.github.io/ServiceWorker/#clients-getall
void SWServer::matchAll(SWServerWorker& worker, const ServiceWorkerClientQueryOptions& options, ServiceWorkerClientsMatchAllCallback&& callback)
{
    // FIXME: Support reserved client filtering.
    // FIXME: Support WindowClient additional properties.

    auto clients = m_clients.find(worker.origin())->value.clients;

    if (!options.includeUncontrolled) {
        clients.removeAllMatching([&] (const auto& client) {
            return worker.identifier() != m_clientToControllingWorker.get(client.identifier);
        });
    }
    if (options.type != ServiceWorkerClientType::All) {
        clients.removeAllMatching([&] (const auto& client) {
            return options.type != client.data.type;
        });
    }
    callback(WTFMove(clients));
}

void SWServer::claim(SWServerWorker& worker)
{
    auto& origin = worker.origin();
    auto iterator = m_clients.find(origin);
    if (iterator == m_clients.end())
        return;

    auto& clients = iterator->value.clients;
    for (auto& client : clients) {
        auto* registration = doRegistrationMatching(origin.topOrigin, client.data.url);
        if (!(registration && registration->key() == worker.registrationKey()))
            continue;

        auto result = m_clientToControllingWorker.add(client.identifier, worker.identifier());
        if (!result.isNewEntry) {
            if (result.iterator->value == worker.identifier())
                continue;
            if (auto* controllingRegistration = registrationFromServiceWorkerIdentifier(result.iterator->value))
                controllingRegistration->removeClientUsingRegistration(client.identifier);
            result.iterator->value = worker.identifier();
        }
        registration->controlClient(client.identifier);
    }
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

void SWServer::updateWorker(Connection&, const ServiceWorkerJobDataIdentifier& jobDataIdentifier, SWServerRegistration& registration, const URL& url, const String& script, WorkerType type)
{
    registration.setLastUpdateTime(WallTime::now());

    ServiceWorkerContextData data = { jobDataIdentifier, registration.data(), generateServiceWorkerIdentifier(), script, url, type };

    // Right now we only ever keep up to one connection to one SW context process.
    // And it should always exist if we're calling updateWorker
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
    m_registrationStore.updateRegistration(data);

    auto* connection = SWServerToContextConnection::globalServerToContextConnection();
    ASSERT(connection);

    auto* registration = m_registrations.get(data.registration.key);
    RELEASE_ASSERT(registration);

    auto result = m_runningOrTerminatingWorkers.add(data.serviceWorkerIdentifier, SWServerWorker::create(*this, *registration, connection->identifier(), data.scriptURL, data.script, data.workerType, data.serviceWorkerIdentifier));
    ASSERT(result.isNewEntry);

    result.iterator->value->setState(SWServerWorker::State::Running);

    connection->installServiceWorkerContext(data);
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
    ASSERT_UNUSED(addResult, addResult.isNewEntry);

    worker->setState(SWServerWorker::State::Running);

    auto* connection = SWServerToContextConnection::globalServerToContextConnection();
    ASSERT(connection);
    connection->installServiceWorkerContext(worker->contextData());

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
    auto* connection = SWServerToContextConnection::connectionForIdentifier(worker.contextConnectionIdentifier());
    if (!connection) {
        LOG_ERROR("Request to terminate a worker whose context connection does not exist");
        return;
    }

    ASSERT(m_runningOrTerminatingWorkers.get(worker.identifier()) == &worker);
    ASSERT(!worker.isTerminating());

    worker.setState(SWServerWorker::State::Terminating);

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

    // At this point if no registrations are referencing the worker then it will be destroyed,
    // removing itself from the m_workersByID map.
    auto result = m_runningOrTerminatingWorkers.take(worker.identifier());
    ASSERT_UNUSED(result, result && result->ptr() == &worker);
}

void SWServer::fireInstallEvent(SWServerWorker& worker)
{
    auto* connection = SWServerToContextConnection::connectionForIdentifier(worker.contextConnectionIdentifier());
    if (!connection) {
        LOG_ERROR("Request to fire install event on a worker whose context connection does not exist");
        return;
    }

    connection->fireInstallEvent(worker.identifier());
}

void SWServer::fireActivateEvent(SWServerWorker& worker)
{
    auto* connection = SWServerToContextConnection::connectionForIdentifier(worker.contextConnectionIdentifier());
    if (!connection) {
        LOG_ERROR("Request to fire install event on a worker whose context connection does not exist");
        return;
    }

    connection->fireActivateEvent(worker.identifier());
}

void SWServer::taskThreadEntryPoint()
{
    ASSERT(!isMainThread());

    while (!m_taskQueue.isKilled())
        m_taskQueue.waitForMessage().performTask();

    Locker<Lock> locker(m_taskThreadLock);
    m_taskThread = nullptr;
}

void SWServer::postTask(CrossThreadTask&& task)
{
    m_taskQueue.append(WTFMove(task));
}

void SWServer::postTaskReply(CrossThreadTask&& task)
{
    m_taskReplyQueue.append(WTFMove(task));

    Locker<Lock> locker(m_mainThreadReplyLock);
    if (m_mainThreadReplyScheduled)
        return;

    m_mainThreadReplyScheduled = true;
    callOnMainThread([this] {
        handleTaskRepliesOnMainThread();
    });
}

void SWServer::handleTaskRepliesOnMainThread()
{
    {
        Locker<Lock> locker(m_mainThreadReplyLock);
        m_mainThreadReplyScheduled = false;
    }

    while (auto task = m_taskReplyQueue.tryGetMessage())
        task->performTask();
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

void SWServer::registerServiceWorkerClient(ClientOrigin&& clientOrigin, ServiceWorkerClientIdentifier clientIdentifier, ServiceWorkerClientData&& data, const std::optional<ServiceWorkerIdentifier>& controllingServiceWorkerIdentifier)
{
    auto& clientsData = m_clients.ensure(WTFMove(clientOrigin), [] {
        return Clients { };
    }).iterator->value;

    clientsData.clients.append(ServiceWorkerClientInformation { clientIdentifier, WTFMove(data) });
    if (clientsData.terminateServiceWorkersTimer)
        clientsData.terminateServiceWorkersTimer = nullptr;

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
    auto clientIterator = m_clients.find(clientOrigin);
    ASSERT(clientIterator != m_clients.end());

    auto& clients = clientIterator->value.clients;
    clients.removeFirstMatching([&] (const auto& client) {
        return clientIdentifier == client.identifier;
    });
    if (clients.isEmpty()) {
        ASSERT(!clientIterator->value.terminateServiceWorkersTimer);
        clientIterator->value.terminateServiceWorkersTimer = std::make_unique<Timer>([clientOrigin, this] {
            for (auto& worker : m_runningOrTerminatingWorkers.values()) {
                if (worker->origin() == clientOrigin)
                    terminateWorker(worker);
            }
            m_clients.remove(clientOrigin);
        });
        clientIterator->value.terminateServiceWorkersTimer->startOneShot(terminationDelay);
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

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
