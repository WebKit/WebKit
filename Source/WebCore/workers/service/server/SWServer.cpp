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
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static const unsigned maxRegistrationCount = 3;

SWServer::Connection::Connection(SWServer& server, Identifier identifier)
    : m_server(server)
    , m_identifier(identifier)
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
    auto connections = WTFMove(m_connections);
    connections.clear();

    for (auto& callback : std::exchange(m_importCompletedCallbacks, { }))
        callback();

    Vector<SWServerWorker*> runningWorkers;
    for (auto& worker : m_runningOrTerminatingWorkers.values()) {
        if (worker->isRunning())
            runningWorkers.append(worker.ptr());
    }
    for (auto& runningWorker : runningWorkers)
        runningWorker->terminate();

    allServers().remove(this);
}

SWServerWorker* SWServer::workerByID(ServiceWorkerIdentifier identifier) const
{
    auto* worker = SWServerWorker::existingWorkerForIdentifier(identifier);
    ASSERT(!worker || worker->server() == this);
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

String SWServer::serviceWorkerClientUserAgent(const ClientOrigin& clientOrigin) const
{
    auto iterator = m_clientIdentifiersPerOrigin.find(clientOrigin);
    if (iterator == m_clientIdentifiersPerOrigin.end())
        return String();
    return iterator->value.userAgent;
}

SWServerWorker* SWServer::activeWorkerFromRegistrationID(ServiceWorkerRegistrationIdentifier identifier)
{
    auto* registration = m_registrations.get(identifier);
    return registration ? registration->activeWorker() : nullptr;
}

SWServerRegistration* SWServer::getRegistration(const ServiceWorkerRegistrationKey& registrationKey)
{
    return m_scopeToRegistrationMap.get(registrationKey).get();
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

    for (auto& callback : std::exchange(m_importCompletedCallbacks, { }))
        callback();
}

void SWServer::whenImportIsCompleted(CompletionHandler<void()>&& callback)
{
    ASSERT(!m_importCompleted);
    m_importCompletedCallbacks.append(WTFMove(callback));
}


void SWServer::registrationStoreDatabaseFailedToOpen()
{
    ASSERT(!m_importCompleted);
    if (!m_importCompleted)
        registrationStoreImportComplete();
}

void SWServer::addRegistrationFromStore(ServiceWorkerContextData&& data)
{
    // Pages should not have been able to make a new registration to this key while the import was still taking place.
    ASSERT(!m_scopeToRegistrationMap.contains(data.registration.key));
    
    auto registrableDomain = WebCore::RegistrableDomain(data.scriptURL);
    validateRegistrationDomain(registrableDomain, [this, weakThis = makeWeakPtr(this), data = WTFMove(data)] (bool isValid) mutable {
        if (!weakThis)
            return;
        if (m_hasServiceWorkerEntitlement || isValid) {
            auto registration = makeUnique<SWServerRegistration>(*this, data.registration.key, data.registration.updateViaCache, data.registration.scopeURL, data.scriptURL);
            registration->setLastUpdateTime(data.registration.lastUpdateTime);
            auto registrationPtr = registration.get();
            addRegistration(WTFMove(registration));

            auto worker = SWServerWorker::create(*this, *registrationPtr, data.scriptURL, data.script, data.certificateInfo, data.contentSecurityPolicy, WTFMove(data.referrerPolicy), data.workerType, data.serviceWorkerIdentifier, WTFMove(data.scriptResourceMap));
            registrationPtr->updateRegistrationState(ServiceWorkerRegistrationState::Active, worker.ptr());
            worker->setState(ServiceWorkerState::Activated);
        }
    });
}

void SWServer::addRegistration(std::unique_ptr<SWServerRegistration>&& registration)
{
    m_originStore->add(registration->key().topOrigin());
    auto registrationID = registration->identifier();
    ASSERT(!m_scopeToRegistrationMap.contains(registration->key()));
    m_scopeToRegistrationMap.set(registration->key(), makeWeakPtr(*registration));
    auto addResult1 = m_registrations.add(registrationID, WTFMove(registration));
    ASSERT_UNUSED(addResult1, addResult1.isNewEntry);
}

void SWServer::removeRegistration(ServiceWorkerRegistrationIdentifier registrationID)
{
    auto registration = m_registrations.take(registrationID);
    ASSERT(registration);
    
    auto it = m_scopeToRegistrationMap.find(registration->key());
    if (it != m_scopeToRegistrationMap.end() && it->value == registration.get())
        m_scopeToRegistrationMap.remove(it);

    m_originStore->remove(registration->key().topOrigin());
    if (m_registrationStore)
        m_registrationStore->removeRegistration(registration->key());
}

Vector<ServiceWorkerRegistrationData> SWServer::getRegistrations(const SecurityOriginData& topOrigin, const URL& clientURL)
{
    Vector<SWServerRegistration*> matchingRegistrations;
    for (auto& item : m_scopeToRegistrationMap) {
        if (item.key.originIsMatching(topOrigin, clientURL)) {
            auto* registration = item.value.get();
            ASSERT(registration);
            if (registration)
                matchingRegistrations.append(registration);
        }
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
    m_pendingContextDatas.clear();
    m_originStore->clearAll();
    if (m_registrationStore)
        m_registrationStore->clearAll(WTFMove(completionHandler));
}

void SWServer::startSuspension(CompletionHandler<void()>&& completionHandler)
{
    if (!m_registrationStore) {
        completionHandler();
        return;
    }
    m_registrationStore->startSuspension(WTFMove(completionHandler));
}

void SWServer::endSuspension()
{
    if (m_registrationStore)
        m_registrationStore->endSuspension();
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
    for (auto& registration : m_registrations.values()) {
        if (registration->key().relatesToOrigin(securityOrigin))
            registrationsToRemove.append(registration.get());
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
    m_server.scriptFetchFinished(result);
}

void SWServer::Connection::didResolveRegistrationPromise(const ServiceWorkerRegistrationKey& key)
{
    m_server.didResolveRegistrationPromise(this, key);
}

void SWServer::Connection::addServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    m_server.addClientServiceWorkerRegistration(*this, identifier);
}

void SWServer::Connection::removeServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    m_server.removeClientServiceWorkerRegistration(*this, identifier);
}

SWServer::SWServer(UniqueRef<SWOriginStore>&& originStore, bool processTerminationDelayEnabled, String&& registrationDatabaseDirectory, PAL::SessionID sessionID, bool hasServiceWorkerEntitlement, SoftUpdateCallback&& softUpdateCallback, CreateContextConnectionCallback&& callback, AppBoundDomainsCallback&& appBoundDomainsCallback)
    : m_originStore(WTFMove(originStore))
    , m_sessionID(sessionID)
    , m_isProcessTerminationDelayEnabled(processTerminationDelayEnabled)
    , m_createContextConnectionCallback(WTFMove(callback))
    , m_softUpdateCallback(WTFMove(softUpdateCallback))
    , m_appBoundDomainsCallback(WTFMove(appBoundDomainsCallback))
    , m_hasServiceWorkerEntitlement(hasServiceWorkerEntitlement)
{
    RELEASE_LOG_IF(registrationDatabaseDirectory.isEmpty() && !m_sessionID.isEphemeral(), ServiceWorker, "No path to store the service worker registrations");
    if (!m_sessionID.isEphemeral())
        m_registrationStore = makeUnique<RegistrationStore>(*this, WTFMove(registrationDatabaseDirectory));
    else
        registrationStoreImportComplete();

    UNUSED_PARAM(registrationDatabaseDirectory);
    allServers().add(this);
}

void SWServer::validateRegistrationDomain(WebCore::RegistrableDomain domain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (m_hasServiceWorkerEntitlement || m_hasReceivedAppBoundDomains) {
        completionHandler(m_appBoundDomains.contains(domain) && m_scopeToRegistrationMap.keys().size() < maxRegistrationCount);
        return;
    }
    
    m_appBoundDomainsCallback([this, weakThis = makeWeakPtr(this), domain = WTFMove(domain), completionHandler = WTFMove(completionHandler)](auto&& appBoundDomains) mutable {
        if (!weakThis)
            return;
        m_hasReceivedAppBoundDomains = true;
        m_appBoundDomains = WTFMove(appBoundDomains);
        completionHandler(m_appBoundDomains.contains(domain) && m_scopeToRegistrationMap.keys().size() < maxRegistrationCount);
    });
}

// https://w3c.github.io/ServiceWorker/#schedule-job-algorithm
void SWServer::scheduleJob(ServiceWorkerJobData&& jobData)
{
    ASSERT(m_connections.contains(jobData.connectionIdentifier()) || jobData.connectionIdentifier() == Process::identifier());

    validateRegistrationDomain(WebCore::RegistrableDomain(jobData.scriptURL), [this, weakThis = makeWeakPtr(this), jobData = WTFMove(jobData)] (bool isValid) mutable {
        if (!weakThis)
            return;
        if (m_hasServiceWorkerEntitlement || isValid) {
            auto& jobQueue = *m_jobQueues.ensure(jobData.registrationKey(), [this, &jobData] {
                return makeUnique<SWServerJobQueue>(*this, jobData.registrationKey());
            }).iterator->value;

            if (!jobQueue.size()) {
                jobQueue.enqueueJob(WTFMove(jobData));
                jobQueue.runNextJob();
                return;
            }
            auto& lastJob = jobQueue.lastJob();
            if (jobData.isEquivalent(lastJob)) {
                // FIXME: Per the spec, check if this job is equivalent to the last job on the queue.
                // If it is, stack it along with that job. For now, we just make sure to not call soft-update too often.
                if (jobData.type == ServiceWorkerJobType::Update && jobData.connectionIdentifier() == Process::identifier())
                    return;
            }
            jobQueue.enqueueJob(WTFMove(jobData));
            if (jobQueue.size() == 1)
                jobQueue.runNextJob();
        } else
            rejectJob(jobData, { TypeError, "Job rejected for non app-bound domain" });
    });
}

void SWServer::scheduleUnregisterJob(ServiceWorkerJobDataIdentifier jobDataIdentifier, SWServerRegistration& registration, DocumentOrWorkerIdentifier contextIdentifier, URL&& clientCreationURL)
{
    ServiceWorkerJobData jobData { jobDataIdentifier, contextIdentifier };
    jobData.clientCreationURL = WTFMove(clientCreationURL);
    jobData.topOrigin = registration.data().key.topOrigin();
    jobData.type = ServiceWorkerJobType::Unregister;
    jobData.scopeURL = registration.data().scopeURL;

    scheduleJob(WTFMove(jobData));
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
    if (!connection) {
        if (shouldNotifyWhenResolved == ShouldNotifyWhenResolved::Yes && jobData.connectionIdentifier() == Process::identifier())
            didResolveRegistrationPromise(nullptr, registrationData.key);
        return;
    }

    connection->resolveRegistrationJobInClient(jobData.identifier().jobIdentifier, registrationData, shouldNotifyWhenResolved);
}

void SWServer::resolveUnregistrationJob(const ServiceWorkerJobData& jobData, const ServiceWorkerRegistrationKey& registrationKey, bool unregistrationResult)
{
    auto* connection = m_connections.get(jobData.connectionIdentifier());
    if (!connection)
        return;

    connection->resolveUnregistrationJobInClient(jobData.identifier().jobIdentifier, registrationKey, unregistrationResult);
}

URL static inline originURL(const SecurityOrigin& origin)
{
    URL url;
    url.setProtocol(origin.protocol());
    url.setHost(origin.host());
    url.setPort(origin.port());
    return url;
}

void SWServer::startScriptFetch(const ServiceWorkerJobData& jobData, bool shouldRefreshCache)
{
    LOG(ServiceWorker, "Server issuing startScriptFetch for current job %s in client", jobData.identifier().loggingString().utf8().data());
    auto* connection = m_connections.get(jobData.connectionIdentifier());
    if (connection) {
        connection->startScriptFetchInClient(jobData.identifier().jobIdentifier, jobData.registrationKey(), shouldRefreshCache ? FetchOptions::Cache::NoCache : FetchOptions::Cache::Default);
        return;
    }
    if (jobData.connectionIdentifier() == Process::identifier()) {
        ASSERT(jobData.type == ServiceWorkerJobType::Update);
        // This is a soft-update job, create directly a network load to fetch the script.
        ResourceRequest request { jobData.scriptURL };

        auto topOrigin = jobData.topOrigin.securityOrigin();
        auto origin = SecurityOrigin::create(jobData.scriptURL);

        request.setDomainForCachePartition(topOrigin->domainForCachePartition());
        request.setAllowCookies(true);
        request.setFirstPartyForCookies(originURL(topOrigin));

        request.setHTTPHeaderField(HTTPHeaderName::Origin, origin->toString());
        request.setHTTPHeaderField(HTTPHeaderName::ServiceWorker, "script"_s);
        request.setHTTPReferrer(originURL(origin).string());
        request.setHTTPUserAgent(serviceWorkerClientUserAgent(ClientOrigin { jobData.topOrigin, SecurityOrigin::create(jobData.scriptURL)->data() }));
        request.setPriority(ResourceLoadPriority::Low);

        m_softUpdateCallback(ServiceWorkerJobData { jobData }, shouldRefreshCache, WTFMove(request), [this, weakThis = makeWeakPtr(this)](auto& result) {
            if (!weakThis)
                return;
            scriptFetchFinished(result);
        });
        return;
    }
    ASSERT_WITH_MESSAGE(connection, "If the connection was lost, this job should have been cancelled");
}

void SWServer::scriptFetchFinished(const ServiceWorkerFetchResult& result)
{
    LOG(ServiceWorker, "Server handling scriptFetchFinished for current job %s in client", result.jobDataIdentifier.loggingString().utf8().data());

    ASSERT(m_connections.contains(result.jobDataIdentifier.connectionIdentifier) || result.jobDataIdentifier.connectionIdentifier == Process::identifier());

    auto jobQueue = m_jobQueues.get(result.registrationKey);
    if (!jobQueue)
        return;

    jobQueue->scriptFetchFinished(result);
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
    auto* registration = worker.registration();
    if (registration && registration->preInstallationWorker() == &worker)
        registration->setPreInstallationWorker(nullptr);
}

void SWServer::didFinishInstall(const Optional<ServiceWorkerJobDataIdentifier>& jobDataIdentifier, SWServerWorker& worker, bool wasSuccessful)
{
    RELEASE_LOG(ServiceWorker, "%p - SWServer::didFinishInstall: Finished install for service worker %llu, success is %d", this, worker.identifier().toUInt64(), wasSuccessful);

    if (!jobDataIdentifier)
        return;

    if (auto* jobQueue = m_jobQueues.get(worker.registrationKey()))
        jobQueue->didFinishInstall(*jobDataIdentifier, worker, wasSuccessful);
}

void SWServer::didFinishActivation(SWServerWorker& worker)
{
    RELEASE_LOG(ServiceWorker, "%p - SWServer::didFinishActivation: Finished activation for service worker %llu", this, worker.identifier().toUInt64());

    auto* registration = worker.registration();
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

Optional<ExceptionData> SWServer::claim(SWServerWorker& worker)
{
    auto* registration = worker.registration();
    if (!registration || &worker != registration->activeWorker())
        return ExceptionData { InvalidStateError, "Service worker is not active"_s };

    auto& origin = worker.origin();
    forEachClientForOrigin(origin, [&](auto& clientData) {
        if (doRegistrationMatching(origin.topOrigin, clientData.url) != registration)
            return;

        auto result = m_clientToControllingRegistration.add(clientData.identifier, registration->identifier());
        if (!result.isNewEntry) {
            auto previousIdentifier = result.iterator->value;
            if (previousIdentifier == registration->identifier())
                return;
            result.iterator->value = registration->identifier();
            if (auto* controllingRegistration = m_registrations.get(previousIdentifier))
                controllingRegistration->removeClientUsingRegistration(clientData.identifier);
        }
        registration->controlClient(clientData.identifier);
    });
    return { };
}

void SWServer::didResolveRegistrationPromise(Connection* connection, const ServiceWorkerRegistrationKey& registrationKey)
{
    ASSERT_UNUSED(connection, !connection || m_connections.contains(connection->identifier()));

    if (auto* jobQueue = m_jobQueues.get(registrationKey))
        jobQueue->didResolveRegistrationPromise();
}

void SWServer::addClientServiceWorkerRegistration(Connection& connection, ServiceWorkerRegistrationIdentifier identifier)
{
    auto* registration = m_registrations.get(identifier);
    if (!registration) {
        LOG_ERROR("Request to add client-side ServiceWorkerRegistration to non-existent server-side registration");
        return;
    }
    
    registration->addClientServiceWorkerRegistration(connection.identifier());
}

void SWServer::removeClientServiceWorkerRegistration(Connection& connection, ServiceWorkerRegistrationIdentifier identifier)
{
    if (auto* registration = m_registrations.get(identifier))
        registration->removeClientServiceWorkerRegistration(connection.identifier());
}

void SWServer::updateWorker(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, SWServerRegistration& registration, const URL& url, const String& script, const CertificateInfo& certificateInfo, const ContentSecurityPolicyResponseHeaders& contentSecurityPolicy, const String& referrerPolicy, WorkerType type, HashMap<URL, ServiceWorkerContextData::ImportedScript>&& scriptResourceMap)
{
    tryInstallContextData(ServiceWorkerContextData { jobDataIdentifier, registration.data(), ServiceWorkerIdentifier::generate(), script, certificateInfo, contentSecurityPolicy, referrerPolicy, url, type, false, WTFMove(scriptResourceMap) });
}

void SWServer::tryInstallContextData(ServiceWorkerContextData&& data)
{
    RegistrableDomain registrableDomain(data.scriptURL);
    auto* connection = contextConnectionForRegistrableDomain(registrableDomain);
    if (!connection) {
        m_pendingContextDatas.ensure(registrableDomain, [] {
            return Vector<ServiceWorkerContextData> { };
        }).iterator->value.append(WTFMove(data));

        createContextConnection(registrableDomain);
        return;
    }
    
    installContextData(data);
}

void SWServer::contextConnectionCreated(SWServerToContextConnection& contextConnection)
{
    for (auto& connection : m_connections.values())
        connection->contextConnectionCreated(contextConnection);

    auto pendingContextDatas = m_pendingContextDatas.take(contextConnection.registrableDomain());
    for (auto& data : pendingContextDatas)
        installContextData(data);

    auto serviceWorkerRunRequests = m_serviceWorkerRunRequests.take(contextConnection.registrableDomain());
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

    auto* registration = m_scopeToRegistrationMap.get(data.registration.key).get();
    auto worker = SWServerWorker::create(*this, *registration, data.scriptURL, data.script, data.certificateInfo, data.contentSecurityPolicy, String { data.referrerPolicy }, data.workerType, data.serviceWorkerIdentifier, HashMap<URL, ServiceWorkerContextData::ImportedScript> { data.scriptResourceMap });

    auto* connection = worker->contextConnection();
    ASSERT(connection);

    registration->setPreInstallationWorker(worker.ptr());
    worker->setState(SWServerWorker::State::Running);
    auto userAgent = worker->userAgent();
    auto result = m_runningOrTerminatingWorkers.add(data.serviceWorkerIdentifier, WTFMove(worker));
    ASSERT_UNUSED(result, result.isNewEntry);

    connection->installServiceWorkerContext(data, userAgent);
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
    
    if (worker->state() == ServiceWorkerState::Redundant) {
        callback(nullptr);
        return;
    }

    if (worker->isTerminating()) {
        worker->whenTerminated([this, weakThis = makeWeakPtr(this), identifier, callback = WTFMove(callback)]() mutable {
            if (!weakThis)
                return callback(nullptr);
            runServiceWorkerIfNecessary(identifier, WTFMove(callback));
        });
        return;
    }

    if (!contextConnection) {
        auto& serviceWorkerRunRequestsForOrigin = m_serviceWorkerRunRequests.ensure(worker->registrableDomain(), [] {
            return HashMap<ServiceWorkerIdentifier, Vector<RunServiceWorkerCallback>> { };
        }).iterator->value;
        serviceWorkerRunRequestsForOrigin.ensure(identifier, [&] {
            return Vector<RunServiceWorkerCallback> { };
        }).iterator->value.append(WTFMove(callback));

        createContextConnection(worker->registrableDomain());
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

    // If the registration for a worker has been removed then the request to run
    // the worker is moot.
    if (!worker->registration())
        return false;

    ASSERT(!worker->isTerminating());
    ASSERT(!m_runningOrTerminatingWorkers.contains(identifier));
    m_runningOrTerminatingWorkers.add(identifier, *worker);

    worker->setState(SWServerWorker::State::Running);

    auto* contextConnection = worker->contextConnection();
    ASSERT(contextConnection);

    contextConnection->installServiceWorkerContext(worker->contextData(), worker->userAgent());

    return true;
}

void SWServer::markAllWorkersForRegistrableDomainAsTerminated(const RegistrableDomain& registrableDomain)
{
    Vector<SWServerWorker*> terminatedWorkers;
    for (auto& worker : m_runningOrTerminatingWorkers.values()) {
        if (worker->registrableDomain() == registrableDomain)
            terminatedWorkers.append(worker.ptr());
    }
    for (auto& terminatedWorker : terminatedWorkers)
        workerContextTerminated(*terminatedWorker);
}

void SWServer::workerContextTerminated(SWServerWorker& worker)
{
    // At this point if no registrations are referencing the worker then it will be destroyed,
    // removing itself from the m_workersByID map.
    auto result = m_runningOrTerminatingWorkers.take(worker.identifier());
    ASSERT_UNUSED(result, result == &worker);

    worker.setState(SWServerWorker::State::NotRunning);

    if (auto* jobQueue = m_jobQueues.get(worker.registrationKey()))
        jobQueue->cancelJobsFromServiceWorker(worker.identifier());
}

void SWServer::fireInstallEvent(SWServerWorker& worker)
{
    auto* contextConnection = worker.contextConnection();
    if (!contextConnection) {
        RELEASE_LOG_ERROR(ServiceWorker, "Request to fire install event on a worker whose context connection does not exist");
        return;
    }

    RELEASE_LOG(ServiceWorker, "%p - SWServer::fireInstallEvent on worker %llu", this, worker.identifier().toUInt64());
    contextConnection->fireInstallEvent(worker.identifier());
}

void SWServer::fireActivateEvent(SWServerWorker& worker)
{
    auto* contextConnection = worker.contextConnection();
    if (!contextConnection) {
        RELEASE_LOG_ERROR(ServiceWorker, "Request to fire activate event on a worker whose context connection does not exist");
        return;
    }

    RELEASE_LOG(ServiceWorker, "%p - SWServer::fireActivateEvent on worker %llu", this, worker.identifier().toUInt64());
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
    ASSERT(isImportCompleted());
    SWServerRegistration* selectedRegistration = nullptr;
    for (auto& pair : m_scopeToRegistrationMap) {
        if (!pair.key.isMatching(topOrigin, clientURL))
            continue;
        if (!selectedRegistration || selectedRegistration->key().scopeLength() < pair.key.scopeLength())
            selectedRegistration = pair.value.get();
    }

    return selectedRegistration;
}

SWServerRegistration* SWServer::registrationFromServiceWorkerIdentifier(ServiceWorkerIdentifier identifier)
{
    auto iterator = m_runningOrTerminatingWorkers.find(identifier);
    if (iterator == m_runningOrTerminatingWorkers.end())
        return nullptr;
    return iterator->value->registration();
}

void SWServer::registerServiceWorkerClient(ClientOrigin&& clientOrigin, ServiceWorkerClientData&& data, const Optional<ServiceWorkerRegistrationIdentifier>& controllingServiceWorkerRegistrationIdentifier, String&& userAgent)
{
    auto clientIdentifier = data.identifier;

    ASSERT(!m_clientsById.contains(clientIdentifier));
    m_clientsById.add(clientIdentifier, WTFMove(data));

    auto& clientIdentifiersForOrigin = m_clientIdentifiersPerOrigin.ensure(clientOrigin, [] {
        return Clients { };
    }).iterator->value;

    ASSERT(!clientIdentifiersForOrigin.identifiers.contains(clientIdentifier));
    clientIdentifiersForOrigin.identifiers.append(clientIdentifier);

    if (!clientIdentifiersForOrigin.userAgent.isNull() && clientIdentifiersForOrigin.userAgent != userAgent)
        RELEASE_LOG_ERROR(ServiceWorker, "%p - SWServer::registerServiceWorkerClient: Service worker has clients using different user agents", this);
    clientIdentifiersForOrigin.userAgent = WTFMove(userAgent);

    clientIdentifiersForOrigin.terminateServiceWorkersTimer = nullptr;

    m_clientsByRegistrableDomain.ensure(clientOrigin.clientRegistrableDomain(), [] {
        return HashSet<ServiceWorkerClientIdentifier> { };
    }).iterator->value.add(clientIdentifier);

    if (!controllingServiceWorkerRegistrationIdentifier)
        return;

    auto* controllingRegistration = m_registrations.get(*controllingServiceWorkerRegistrationIdentifier);
    if (!controllingRegistration || !controllingRegistration->activeWorker())
        return;

    controllingRegistration->addClientUsingRegistration(clientIdentifier);
    ASSERT(!m_clientToControllingRegistration.contains(clientIdentifier));
    m_clientToControllingRegistration.add(clientIdentifier, *controllingServiceWorkerRegistrationIdentifier);
}

void SWServer::unregisterServiceWorkerClient(const ClientOrigin& clientOrigin, ServiceWorkerClientIdentifier clientIdentifier)
{
    auto clientRegistrableDomain = clientOrigin.clientRegistrableDomain();

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
        iterator->value.terminateServiceWorkersTimer = makeUnique<Timer>([clientOrigin, clientRegistrableDomain, this] {
            Vector<SWServerWorker*> workersToTerminate;
            for (auto& worker : m_runningOrTerminatingWorkers.values()) {
                if (worker->isRunning() && worker->origin() == clientOrigin)
                    workersToTerminate.append(worker.ptr());
            }
            for (auto* worker : workersToTerminate)
                worker->terminate();

            if (!m_clientsByRegistrableDomain.contains(clientRegistrableDomain)) {
                if (auto* connection = contextConnectionForRegistrableDomain(clientRegistrableDomain)) {
                    removeContextConnection(*connection);
                    connection->connectionIsNoLongerNeeded();
                }
            }

            m_clientIdentifiersPerOrigin.remove(clientOrigin);
        });
        iterator->value.terminateServiceWorkersTimer->startOneShot(m_isProcessTerminationDelayEnabled && !MemoryPressureHandler::singleton().isUnderMemoryPressure() ? defaultTerminationDelay : 0_s);
    }

    auto clientsByRegistrableDomainIterator = m_clientsByRegistrableDomain.find(clientRegistrableDomain);
    ASSERT(clientsByRegistrableDomainIterator != m_clientsByRegistrableDomain.end());
    auto& clientsForRegistrableDomain = clientsByRegistrableDomainIterator->value;
    clientsForRegistrableDomain.remove(clientIdentifier);
    if (clientsForRegistrableDomain.isEmpty())
        m_clientsByRegistrableDomain.remove(clientsByRegistrableDomainIterator);

    auto registrationIterator = m_clientToControllingRegistration.find(clientIdentifier);
    if (registrationIterator == m_clientToControllingRegistration.end())
        return;

    if (auto* registration = m_registrations.get(registrationIterator->value))
        registration->removeClientUsingRegistration(clientIdentifier);

    m_clientToControllingRegistration.remove(registrationIterator);
}

void SWServer::handleLowMemoryWarning()
{
    // Accelerating the delayed termination of unused service workers due to memory pressure.
    if (m_isProcessTerminationDelayEnabled) {
        for (auto& clients : m_clientIdentifiersPerOrigin.values()) {
            if (clients.terminateServiceWorkersTimer)
                clients.terminateServiceWorkersTimer->startOneShot(0_s);
        }
    }
}

void SWServer::removeFromScopeToRegistrationMap(const ServiceWorkerRegistrationKey& key)
{
    m_scopeToRegistrationMap.remove(key);
}

bool SWServer::needsContextConnectionForRegistrableDomain(const RegistrableDomain& registrableDomain) const
{
    return m_clientsByRegistrableDomain.contains(registrableDomain);
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

void SWServer::Connection::storeRegistrationsOnDisk(CompletionHandler<void()>&& callback)
{
    if (!m_server.m_registrationStore) {
        callback();
        return;
    }
    m_server.m_registrationStore->closeDatabase(WTFMove(callback));
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
    for (auto& key : m_scopeToRegistrationMap.keys()) {
        originsWithRegistrations.add(key.topOrigin());
        originsWithRegistrations.add(SecurityOriginData { key.scope().protocol().toString(), key.scope().host().toString(), key.scope().port() });
    }

    auto callbacks = WTFMove(m_getOriginsWithRegistrationsCallbacks);
    for (auto& callback : callbacks)
        callback(originsWithRegistrations);
}

void SWServer::addContextConnection(SWServerToContextConnection& connection)
{
    RELEASE_LOG(ServiceWorker, "SWServer::addContextConnection");

    ASSERT(!m_contextConnections.contains(connection.registrableDomain()));

    m_contextConnections.add(connection.registrableDomain(), &connection);

    contextConnectionCreated(connection);
}

void SWServer::removeContextConnection(SWServerToContextConnection& connection)
{
    RELEASE_LOG(ServiceWorker, "SWServer::removeContextConnection");

    auto& registrableDomain = connection.registrableDomain();

    ASSERT(m_contextConnections.get(registrableDomain) == &connection);

    m_contextConnections.remove(registrableDomain);
    markAllWorkersForRegistrableDomainAsTerminated(registrableDomain);
    if (needsContextConnectionForRegistrableDomain(registrableDomain))
        createContextConnection(registrableDomain);
}

void SWServer::createContextConnection(const RegistrableDomain& registrableDomain)
{
    ASSERT(!m_contextConnections.contains(registrableDomain));
    if (m_pendingConnectionDomains.contains(registrableDomain))
        return;

    RELEASE_LOG(ServiceWorker, "SWServer::createContextConnection will create a connection");

    m_pendingConnectionDomains.add(registrableDomain);
    m_createContextConnectionCallback(registrableDomain, [this, weakThis = makeWeakPtr(this), registrableDomain] {
        if (!weakThis)
            return;

        RELEASE_LOG(ServiceWorker, "SWServer::createContextConnection should now have created a connection");

        ASSERT(m_pendingConnectionDomains.contains(registrableDomain));
        m_pendingConnectionDomains.remove(registrableDomain);

        if (m_contextConnections.contains(registrableDomain))
            return;

        if (needsContextConnectionForRegistrableDomain(registrableDomain))
            createContextConnection(registrableDomain);
    });
}

bool SWServer::canHandleScheme(StringView scheme) const
{
    if (scheme.isNull())
        return false;
    if (!equalLettersIgnoringASCIICase(scheme.substring(0, 4), "http"))
        return false;
    if (scheme.length() == 5 && isASCIIAlphaCaselessEqual(scheme[4], 's'))
        return true;
    return scheme.length() == 4;
}

// https://w3c.github.io/ServiceWorker/#soft-update
void SWServer::softUpdate(SWServerRegistration& registration)
{
    ServiceWorkerJobData jobData(Process::identifier(), ServiceWorkerIdentifier::generate());
    jobData.scriptURL = registration.scriptURL();
    jobData.topOrigin = registration.key().topOrigin();
    jobData.scopeURL = registration.scopeURLWithoutFragment();
    jobData.type = ServiceWorkerJobType::Update;
    scheduleJob(WTFMove(jobData));
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
