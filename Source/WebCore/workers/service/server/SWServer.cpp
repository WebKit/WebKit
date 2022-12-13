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
#include "NotificationData.h"
#include "RegistrationStore.h"
#include "SWOriginStore.h"
#include "SWServerJobQueue.h"
#include "SWServerRegistration.h"
#include "SWServerToContextConnection.h"
#include "SWServerWorker.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerClientType.h"
#include "ServiceWorkerContextData.h"
#include "ServiceWorkerJobData.h"
#include "WorkerFetchResult.h"
#include <wtf/CompletionHandler.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static const unsigned defaultMaxRegistrationCount = 3;

SWServer::Connection::Connection(SWServer& server, Identifier identifier)
    : m_server(server)
    , m_identifier(identifier)
{
}

SWServer::Connection::~Connection()
{
    for (auto& request : std::exchange(m_registrationReadyRequests, { }))
        request.callback({ });
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

std::optional<ServiceWorkerClientData> SWServer::serviceWorkerClientWithOriginByID(const ClientOrigin& clientOrigin, const ScriptExecutionContextIdentifier& clientIdentifier) const
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

std::optional<ServiceWorkerClientData> SWServer::topLevelServiceWorkerClientFromPageIdentifier(const ClientOrigin& clientOrigin, PageIdentifier pageIdentifier) const
{
    auto iterator = m_clientIdentifiersPerOrigin.find(clientOrigin);
    if (iterator == m_clientIdentifiersPerOrigin.end())
        return { };

    for (auto clientIdentifier : iterator->value.identifiers) {
        auto clientIterator = m_clientsById.find(clientIdentifier);
        if (clientIterator->value->frameType == ServiceWorkerClientFrameType::TopLevel && clientIterator->value->pageIdentifier == pageIdentifier)
            return clientIterator->value;
    }
    return { };
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

void SWServer::whenImportIsCompletedIfNeeded(CompletionHandler<void()>&& callback)
{
    if (m_importCompleted) {
        callback();
        return;
    }
    whenImportIsCompleted(WTFMove(callback));
}

void SWServer::registrationStoreDatabaseFailedToOpen()
{
    LOG(ServiceWorker, "Failed to open SW registration database");
    ASSERT(!m_importCompleted);
    if (!m_importCompleted)
        registrationStoreImportComplete();
}

void SWServer::addRegistrationFromStore(ServiceWorkerContextData&& data)
{
    // Pages should not have been able to make a new registration to this key while the import was still taking place.
    ASSERT(!m_scopeToRegistrationMap.contains(data.registration.key));

    LOG(ServiceWorker, "Adding registration from store for %s", data.registration.key.loggingString().utf8().data());

    auto registrableDomain = WebCore::RegistrableDomain(data.scriptURL);
    validateRegistrationDomain(registrableDomain, ServiceWorkerJobType::Register, m_scopeToRegistrationMap.contains(data.registration.key), [this, weakThis = WeakPtr { *this }, data = WTFMove(data)] (bool isValid) mutable {
        if (!weakThis)
            return;
        if (m_hasServiceWorkerEntitlement || isValid) {
            auto registration = makeUnique<SWServerRegistration>(*this, data.registration.key, data.registration.updateViaCache, data.registration.scopeURL, data.scriptURL, data.serviceWorkerPageIdentifier, WTFMove(data.navigationPreloadState));
            registration->setLastUpdateTime(data.registration.lastUpdateTime);
            auto registrationPtr = registration.get();
            addRegistration(WTFMove(registration));

            auto worker = SWServerWorker::create(*this, *registrationPtr, data.scriptURL, data.script, data.certificateInfo, data.contentSecurityPolicy, data.crossOriginEmbedderPolicy, WTFMove(data.referrerPolicy), data.workerType, data.serviceWorkerIdentifier, WTFMove(data.scriptResourceMap));
            registrationPtr->updateRegistrationState(ServiceWorkerRegistrationState::Active, worker.ptr());
            worker->setState(ServiceWorkerState::Activated);
        }
    });
}

void SWServer::didSaveWorkerScriptsToDisk(ServiceWorkerIdentifier serviceWorkerIdentifier, ScriptBuffer&& mainScript, MemoryCompactRobinHoodHashMap<URL, ScriptBuffer>&& importedScripts)
{
    if (auto worker = workerByID(serviceWorkerIdentifier))
        worker->didSaveScriptsToDisk(WTFMove(mainScript), WTFMove(importedScripts));
}

void SWServer::addRegistration(std::unique_ptr<SWServerRegistration>&& registration)
{
    LOG(ServiceWorker, "Adding registration live for %s", registration->key().loggingString().utf8().data());

    if (!m_scopeToRegistrationMap.contains(registration->key()) && !allowLoopbackIPAddress(registration->key().topOrigin().host))
        m_uniqueRegistrationCount++;

    if (registration->serviceWorkerPageIdentifier())
        m_serviceWorkerPageIdentifierToRegistrationMap.add(*registration->serviceWorkerPageIdentifier(), *registration);

    m_originStore->add(registration->key().topOrigin());
    auto registrationID = registration->identifier();
    ASSERT(!m_scopeToRegistrationMap.contains(registration->key()));
    m_scopeToRegistrationMap.set(registration->key(), *registration);
    auto addResult1 = m_registrations.add(registrationID, WTFMove(registration));
    ASSERT_UNUSED(addResult1, addResult1.isNewEntry);
}

void SWServer::removeRegistration(ServiceWorkerRegistrationIdentifier registrationID)
{
    auto registration = m_registrations.take(registrationID);
    ASSERT(registration);

    if (registration->serviceWorkerPageIdentifier())
        m_serviceWorkerPageIdentifierToRegistrationMap.remove(*registration->serviceWorkerPageIdentifier());
    
    auto it = m_scopeToRegistrationMap.find(registration->key());
    if (it != m_scopeToRegistrationMap.end() && it->value == registration.get()) {
        m_scopeToRegistrationMap.remove(it);
        if (!SecurityOrigin::isLocalHostOrLoopbackIPAddress(registration->key().topOrigin().host))
            m_uniqueRegistrationCount--;
    }

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
    return matchingRegistrations.map([](auto& registration) {
        return registration->data();
    });
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
    if (!m_registrationStore)
        return completionHandler();

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

    if (!m_registrationStore)
        return completionHandler();

    m_registrationStore->flushChanges(WTFMove(completionHandler));
}

void SWServer::Connection::finishFetchingScriptInServer(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, const ServiceWorkerRegistrationKey& registrationKey, WorkerFetchResult&& result)
{
    m_server.scriptFetchFinished(jobDataIdentifier, registrationKey, m_identifier, WTFMove(result));
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

SWServer::SWServer(UniqueRef<SWOriginStore>&& originStore, bool processTerminationDelayEnabled, String&& registrationDatabaseDirectory, PAL::SessionID sessionID, bool shouldRunServiceWorkersOnMainThreadForTesting, bool hasServiceWorkerEntitlement, std::optional<unsigned> overrideServiceWorkerRegistrationCountTestingValue, SoftUpdateCallback&& softUpdateCallback, CreateContextConnectionCallback&& callback, AppBoundDomainsCallback&& appBoundDomainsCallback, AddAllowedFirstPartyForCookiesCallback&& addAllowedFirstPartyForCookiesCallback)
    : m_originStore(WTFMove(originStore))
    , m_sessionID(sessionID)
    , m_isProcessTerminationDelayEnabled(processTerminationDelayEnabled)
    , m_createContextConnectionCallback(WTFMove(callback))
    , m_softUpdateCallback(WTFMove(softUpdateCallback))
    , m_appBoundDomainsCallback(WTFMove(appBoundDomainsCallback))
    , m_addAllowedFirstPartyForCookiesCallback(WTFMove(addAllowedFirstPartyForCookiesCallback))
    , m_shouldRunServiceWorkersOnMainThreadForTesting(shouldRunServiceWorkersOnMainThreadForTesting)
    , m_hasServiceWorkerEntitlement(hasServiceWorkerEntitlement)
    , m_overrideServiceWorkerRegistrationCountTestingValue(overrideServiceWorkerRegistrationCountTestingValue)
{
    RELEASE_LOG_IF(registrationDatabaseDirectory.isEmpty(), ServiceWorker, "No path to store the service worker registrations");
    if (!m_sessionID.isEphemeral())
        m_registrationStore = makeUnique<RegistrationStore>(*this, WTFMove(registrationDatabaseDirectory));
    else
        registrationStoreImportComplete();

    UNUSED_PARAM(registrationDatabaseDirectory);
    allServers().add(this);
}

unsigned SWServer::maxRegistrationCount()
{
    if (m_overrideServiceWorkerRegistrationCountTestingValue)
        return *m_overrideServiceWorkerRegistrationCountTestingValue;

    return defaultMaxRegistrationCount;
}

bool SWServer::allowLoopbackIPAddress(const String& domain)
{
    if (!SecurityOrigin::isLocalHostOrLoopbackIPAddress(domain))
        return false;

    if (m_overrideServiceWorkerRegistrationCountTestingValue)
        return false;

    return true;
}

void SWServer::validateRegistrationDomain(WebCore::RegistrableDomain domain, ServiceWorkerJobType type, bool isRegisteredDomain, CompletionHandler<void(bool)>&& completionHandler)
{
    bool jobTypeAllowed = type != ServiceWorkerJobType::Register || isRegisteredDomain;
    if (m_hasServiceWorkerEntitlement || m_hasReceivedAppBoundDomains) {
        completionHandler(allowLoopbackIPAddress(domain.string()) || jobTypeAllowed || (m_appBoundDomains.contains(domain) && m_uniqueRegistrationCount < maxRegistrationCount()));
        return;
    }

    m_appBoundDomainsCallback([this, weakThis = WeakPtr { *this }, domain = WTFMove(domain), jobTypeAllowed, completionHandler = WTFMove(completionHandler)](auto&& appBoundDomains) mutable {
        if (!weakThis)
            return;
        m_hasReceivedAppBoundDomains = true;
        m_appBoundDomains = WTFMove(appBoundDomains);
        completionHandler(allowLoopbackIPAddress(domain.string()) || jobTypeAllowed || (m_appBoundDomains.contains(domain) && m_uniqueRegistrationCount < maxRegistrationCount()));
    });
}

// https://w3c.github.io/ServiceWorker/#schedule-job-algorithm
void SWServer::scheduleJob(ServiceWorkerJobData&& jobData)
{
    ASSERT(m_connections.contains(jobData.connectionIdentifier()) || jobData.connectionIdentifier() == Process::identifier());

    validateRegistrationDomain(WebCore::RegistrableDomain(jobData.scriptURL), jobData.type, m_scopeToRegistrationMap.contains(jobData.registrationKey()), [this, weakThis = WeakPtr { *this }, jobData = WTFMove(jobData)] (bool isValid) mutable {
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
            rejectJob(jobData, { TypeError, "Job rejected for non app-bound domain"_s });
    });
}

void SWServer::scheduleUnregisterJob(ServiceWorkerJobDataIdentifier jobDataIdentifier, SWServerRegistration& registration, ServiceWorkerOrClientIdentifier contextIdentifier, URL&& clientCreationURL)
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

ResourceRequest SWServer::createScriptRequest(const URL& url, const ServiceWorkerJobData& jobData, SWServerRegistration& registration)
{
    ResourceRequest request { url };

    auto topOrigin = jobData.topOrigin.securityOrigin();
    auto origin = SecurityOrigin::create(jobData.scriptURL);

    request.setDomainForCachePartition(jobData.domainForCachePartition);
    request.setAllowCookies(true);
    request.setFirstPartyForCookies(originURL(topOrigin));

    request.setHTTPHeaderField(HTTPHeaderName::Origin, origin->toString());
    request.setHTTPReferrer(originURL(origin).string());
    request.setHTTPUserAgent(serviceWorkerClientUserAgent(ClientOrigin { jobData.topOrigin, SecurityOrigin::create(jobData.scriptURL)->data() }));
    request.setPriority(ResourceLoadPriority::Low);
    request.setIsAppInitiated(registration.isAppInitiated());

    return request;
}

void SWServer::startScriptFetch(const ServiceWorkerJobData& jobData, SWServerRegistration& registration)
{
    LOG(ServiceWorker, "Server issuing startScriptFetch for current job %s in client", jobData.identifier().loggingString().utf8().data());

    // Set request's cache mode to "no-cache" if any of the following are true:
    // - registration's update via cache mode is not "all".
    // - job's force bypass cache flag is set.
    // - newestWorker is not null, and registration's last update check time is not null and the time difference in seconds calculated by the
    //   current time minus registration's last update check time is greater than 86400.
    bool shouldRefreshCache = registration.updateViaCache() != ServiceWorkerUpdateViaCache::All || (registration.getNewestWorker() && registration.isStale());

    auto* connection = m_connections.get(jobData.connectionIdentifier());
    if (connection) {
        connection->startScriptFetchInClient(jobData.identifier().jobIdentifier, jobData.registrationKey(), shouldRefreshCache ? FetchOptions::Cache::NoCache : FetchOptions::Cache::Default);
        return;
    }
    if (jobData.connectionIdentifier() == Process::identifier()) {
        ASSERT(jobData.type == ServiceWorkerJobType::Update);
        // This is a soft-update job, create directly a network load to fetch the script.
        auto request = createScriptRequest(jobData.scriptURL, jobData, registration);
        request.setHTTPHeaderField(HTTPHeaderName::ServiceWorker, "script"_s);
        m_softUpdateCallback(ServiceWorkerJobData { jobData }, shouldRefreshCache, WTFMove(request), [weakThis = WeakPtr { *this }, jobDataIdentifier = jobData.identifier(), registrationKey = jobData.registrationKey()](auto&& result) {
            std::optional<ProcessIdentifier> requestingProcessIdentifier;
            if (weakThis)
                weakThis->scriptFetchFinished(jobDataIdentifier, registrationKey, requestingProcessIdentifier, WTFMove(result));
        });
        return;
    }
    ASSERT_WITH_MESSAGE(connection, "If the connection was lost, this job should have been cancelled");
}

class RefreshImportedScriptsHandler : public RefCounted<RefreshImportedScriptsHandler> {
public:
    using Callback = CompletionHandler<void(Vector<std::pair<URL, ScriptBuffer>>&&)>;
    static Ref<RefreshImportedScriptsHandler> create(size_t expectedItems, Callback&& callback) { return adoptRef(*new RefreshImportedScriptsHandler(expectedItems, WTFMove(callback))); }

    void add(const URL& url, WorkerFetchResult&& result)
    {
        if (result.error.isNull())
            m_scripts.append(std::make_pair(url, WTFMove(result.script)));

        if (!--m_remainingItems)
            m_callback(std::exchange(m_scripts, { }));
    }

private:
    RefreshImportedScriptsHandler(size_t expectedItems, Callback&& callback)
        : m_remainingItems(expectedItems)
        , m_callback(WTFMove(callback))
    {
    }

    size_t m_remainingItems;
    Callback m_callback;
    Vector<std::pair<URL, ScriptBuffer>> m_scripts;
};

void SWServer::scriptFetchFinished(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, const ServiceWorkerRegistrationKey& registrationKey, const std::optional<ProcessIdentifier>& requestingProcessIdentifier, WorkerFetchResult&& result)
{
    LOG(ServiceWorker, "Server handling scriptFetchFinished for current job %s in client", jobDataIdentifier.loggingString().utf8().data());

    ASSERT(m_connections.contains(jobDataIdentifier.connectionIdentifier) || jobDataIdentifier.connectionIdentifier == Process::identifier());

    auto jobQueue = m_jobQueues.get(registrationKey);
    if (!jobQueue)
        return;

    jobQueue->scriptFetchFinished(jobDataIdentifier, requestingProcessIdentifier, WTFMove(result));
}

void SWServer::refreshImportedScripts(const ServiceWorkerJobData& jobData, SWServerRegistration& registration, const Vector<URL>& urls, const std::optional<ProcessIdentifier>& requestingProcessIdentifier)
{
    RefreshImportedScriptsHandler::Callback callback = [weakThis = WeakPtr { *this }, jobDataIdentifier = jobData.identifier(), registrationKey = jobData.registrationKey(), rpi = requestingProcessIdentifier](auto&& scripts) {
        if (weakThis)
            weakThis->refreshImportedScriptsFinished(jobDataIdentifier, registrationKey, scripts, rpi);
    };
    bool shouldRefreshCache = registration.updateViaCache() == ServiceWorkerUpdateViaCache::None || (registration.getNewestWorker() && registration.isStale());
    auto handler = RefreshImportedScriptsHandler::create(urls.size(), WTFMove(callback));
    for (auto& url : urls) {
        m_softUpdateCallback(ServiceWorkerJobData { jobData }, shouldRefreshCache, createScriptRequest(url, jobData, registration), [handler, url, size = urls.size()](auto&& result) {
            handler->add(url, WTFMove(result));
        });
    }
}

void SWServer::refreshImportedScriptsFinished(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, const ServiceWorkerRegistrationKey& registrationKey, const Vector<std::pair<URL, ScriptBuffer>>& scripts, const std::optional<ProcessIdentifier>& requestingProcessIdentifier)
{
    auto jobQueue = m_jobQueues.get(registrationKey);
    if (!jobQueue)
        return;

    jobQueue->importedScriptsFetchFinished(jobDataIdentifier, scripts, requestingProcessIdentifier);
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
    auto* registration = worker.registration();
    if (registration && registration->preInstallationWorker() == &worker)
        registration->setPreInstallationWorker(nullptr);
}

void SWServer::didFinishInstall(const std::optional<ServiceWorkerJobDataIdentifier>& jobDataIdentifier, SWServerWorker& worker, bool wasSuccessful)
{
    RELEASE_LOG(ServiceWorker, "%p - SWServer::didFinishInstall: Finished install for service worker %llu, success is %d", this, worker.identifier().toUInt64(), wasSuccessful);

    if (!jobDataIdentifier)
        return;

    if (wasSuccessful)
        storeRegistrationForWorker(worker);

    if (auto* jobQueue = m_jobQueues.get(worker.registrationKey()))
        jobQueue->didFinishInstall(*jobDataIdentifier, worker, wasSuccessful);
}

void SWServer::didFinishActivation(SWServerWorker& worker)
{
    RELEASE_LOG(ServiceWorker, "%p - SWServer::didFinishActivation: Finished activation for service worker %llu", this, worker.identifier().toUInt64());

    auto* registration = worker.registration();
    if (!registration)
        return;

    registration->didFinishActivation(worker.identifier());
}

void SWServer::storeRegistrationForWorker(SWServerWorker& worker)
{
    if (m_registrationStore)
        m_registrationStore->updateRegistration(worker.contextData());
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

void SWServer::forEachClientForOrigin(const ClientOrigin& origin, const Function<void(ServiceWorkerClientData&)>& apply)
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

std::optional<ExceptionData> SWServer::claim(SWServerWorker& worker)
{
    auto* registration = worker.registration();
    if (!registration || &worker != registration->activeWorker())
        return ExceptionData { InvalidStateError, "Service worker is not active"_s };

    auto& origin = worker.origin();
    forEachClientForOrigin(origin, [&](auto& clientData) {
        // FIXME: The specification currently doesn't deal properly via Blob URL clients.
        // https://github.com/w3c/ServiceWorker/issues/1554
        URL& clientURLForRegistrationMatching = clientData.url;
        if (clientURLForRegistrationMatching.protocolIsBlob() && clientData.ownerURL.isValid())
            clientURLForRegistrationMatching = clientData.ownerURL;

        if (doRegistrationMatching(origin.topOrigin, clientURLForRegistrationMatching) != registration)
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

void SWServer::updateWorker(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, const std::optional<ProcessIdentifier>& requestingProcessIdentifier, SWServerRegistration& registration, const URL& url, const ScriptBuffer& script, const CertificateInfo& certificateInfo, const ContentSecurityPolicyResponseHeaders& contentSecurityPolicy, const CrossOriginEmbedderPolicy& coep, const String& referrerPolicy, WorkerType type, MemoryCompactRobinHoodHashMap<URL, ServiceWorkerContextData::ImportedScript>&& scriptResourceMap, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier)
{
    tryInstallContextData(requestingProcessIdentifier, ServiceWorkerContextData { jobDataIdentifier, registration.data(), ServiceWorkerIdentifier::generate(), script, certificateInfo, contentSecurityPolicy, coep, referrerPolicy, url, type, false, clientIsAppInitiatedForRegistrableDomain(RegistrableDomain(url)), WTFMove(scriptResourceMap), serviceWorkerPageIdentifier, { } });
}

LastNavigationWasAppInitiated SWServer::clientIsAppInitiatedForRegistrableDomain(const RegistrableDomain& domain)
{
    auto clientsByRegistrableDomainIterator = m_clientsByRegistrableDomain.find(domain);
    if (clientsByRegistrableDomainIterator == m_clientsByRegistrableDomain.end())
        return LastNavigationWasAppInitiated::Yes;

    auto& clientsForRegistrableDomain = clientsByRegistrableDomainIterator->value;
    for (auto& client : clientsForRegistrableDomain) {
        auto data = m_clientsById.find(client);
        ASSERT(data != m_clientsById.end());
        if (data->value->lastNavigationWasAppInitiated == LastNavigationWasAppInitiated::Yes)
            return LastNavigationWasAppInitiated::Yes;
    }

    return LastNavigationWasAppInitiated::No;
}

void SWServer::tryInstallContextData(const std::optional<ProcessIdentifier>& requestingProcessIdentifier, ServiceWorkerContextData&& data)
{
    RegistrableDomain registrableDomain(data.scriptURL);
    auto* connection = contextConnectionForRegistrableDomain(registrableDomain);
    if (!connection) {
        auto firstPartyForCookies = data.registration.key.firstPartyForCookies();
        m_pendingContextDatas.ensure(registrableDomain, [] {
            return Vector<ServiceWorkerContextData> { };
        }).iterator->value.append(WTFMove(data));

        createContextConnection(registrableDomain, data.serviceWorkerPageIdentifier);
        return;
    }

    m_addAllowedFirstPartyForCookiesCallback(connection->webProcessIdentifier(), requestingProcessIdentifier, data.registration.key.firstPartyForCookies());
    installContextData(data);
}

void SWServer::contextConnectionCreated(SWServerToContextConnection& contextConnection)
{
    auto requestingProcessIdentifier = contextConnection.webProcessIdentifier();
    m_addAllowedFirstPartyForCookiesCallback(contextConnection.webProcessIdentifier(), requestingProcessIdentifier, RegistrableDomain(contextConnection.registrableDomain()));

    for (auto& connection : m_connections.values())
        connection->contextConnectionCreated(contextConnection);

    auto pendingContextDatas = m_pendingContextDatas.take(contextConnection.registrableDomain());
    for (auto& data : pendingContextDatas) {
        // FIXME: Add a check that the process this firstPartyForCookies came from was allowed to use it as a firstPartyForCookies.
        m_addAllowedFirstPartyForCookiesCallback(contextConnection.webProcessIdentifier(), requestingProcessIdentifier, data.registration.key.firstPartyForCookies());
        installContextData(data);
    }

    auto serviceWorkerRunRequests = m_serviceWorkerRunRequests.take(contextConnection.registrableDomain());
    for (auto& item : serviceWorkerRunRequests) {
        bool success = runServiceWorker(item.key);
        for (auto& callback : item.value)
            callback(success ? &contextConnection : nullptr);
    }
}

void SWServer::forEachServiceWorker(const Function<bool(const SWServerWorker&)>& apply) const
{
    for (auto& worker : m_runningOrTerminatingWorkers.values()) {
        if (!apply(worker))
            break;
    }
}

void SWServer::terminateContextConnectionWhenPossible(const RegistrableDomain& registrableDomain, ProcessIdentifier processIdentifier)
{
    auto* contextConnection = contextConnectionForRegistrableDomain(registrableDomain);
    if (!contextConnection || contextConnection->webProcessIdentifier() != processIdentifier)
        return;

    contextConnection->terminateWhenPossible();
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
    auto worker = SWServerWorker::create(*this, *registration, data.scriptURL, data.script, data.certificateInfo, data.contentSecurityPolicy, data.crossOriginEmbedderPolicy, String { data.referrerPolicy }, data.workerType, data.serviceWorkerIdentifier, MemoryCompactRobinHoodHashMap<URL, ServiceWorkerContextData::ImportedScript> { data.scriptResourceMap });

    auto* connection = worker->contextConnection();
    ASSERT(connection);

    registration->setPreInstallationWorker(worker.ptr());
    worker->setState(SWServerWorker::State::Running);
    auto userAgent = worker->userAgent();
    auto result = m_runningOrTerminatingWorkers.add(data.serviceWorkerIdentifier, worker.copyRef());
    ASSERT_UNUSED(result, result.isNewEntry);

    connection->installServiceWorkerContext(data, worker->data(), userAgent, worker->workerThreadMode());
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
        worker->whenTerminated([this, weakThis = WeakPtr { *this }, identifier, callback = WTFMove(callback)]() mutable {
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

        createContextConnection(worker->registrableDomain(), worker->serviceWorkerPageIdentifier());
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

    contextConnection->installServiceWorkerContext(worker->contextData(), worker->data(), worker->userAgent(), worker->workerThreadMode());

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
    if (!result) {
        ASSERT(worker.isNotRunning());
        return;
    }

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

void SWServer::updateAppInitiatedValueForWorkers(const ClientOrigin& clientOrigin, LastNavigationWasAppInitiated lastNavigationWasAppInitiated)
{
    for (auto& worker : m_runningOrTerminatingWorkers.values()) {
        if (worker->origin().clientRegistrableDomain() == clientOrigin.clientRegistrableDomain())
            worker->updateAppInitiatedValue(lastNavigationWasAppInitiated);
    }
}

void SWServer::registerServiceWorkerClient(ClientOrigin&& clientOrigin, ServiceWorkerClientData&& data, const std::optional<ServiceWorkerRegistrationIdentifier>& controllingServiceWorkerRegistrationIdentifier, String&& userAgent)
{
    auto clientIdentifier = data.identifier;

    // Update the app-bound value if the new client is app-bound and the current clients for the origin are not marked app-bound.
    if (data.lastNavigationWasAppInitiated == LastNavigationWasAppInitiated::Yes) {
        if (clientIsAppInitiatedForRegistrableDomain(clientOrigin.clientRegistrableDomain()) == LastNavigationWasAppInitiated::No)
            updateAppInitiatedValueForWorkers(clientOrigin, data.lastNavigationWasAppInitiated);
    }

    auto addResult = m_visibleClientIdToInternalClientIdMap.add(data.identifier.object().toString(), clientIdentifier);
    if (!addResult.isNewEntry) {
        ASSERT(m_visibleClientIdToInternalClientIdMap.get(data.identifier.object().toString()) == clientIdentifier);
        ASSERT(m_clientsById.contains(clientIdentifier));
        if (data.isFocused)
            data.focusOrder = ++m_focusOrder;
        m_clientsById.set(clientIdentifier, makeUniqueRef<ServiceWorkerClientData>(WTFMove(data)));
        return;
    }

    ASSERT(!m_clientsById.contains(clientIdentifier));
    m_clientsById.add(clientIdentifier, makeUniqueRef<ServiceWorkerClientData>(WTFMove(data)));

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
        return HashSet<ScriptExecutionContextIdentifier> { };
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

std::optional<SWServer::GatheredClientData> SWServer::gatherClientData(const ClientOrigin& clientOrigin, ScriptExecutionContextIdentifier clientIdentifier)
{
    auto clientDataIterator = m_clientsById.find(clientIdentifier);
    if (clientDataIterator == m_clientsById.end())
        return { };

    auto controllingRegistratioIterator = m_clientToControllingRegistration.find(clientIdentifier);
    if (controllingRegistratioIterator == m_clientToControllingRegistration.end())
        return { };

    auto clientsPerOriginIterator = m_clientIdentifiersPerOrigin.find(clientOrigin);
    if (clientsPerOriginIterator == m_clientIdentifiersPerOrigin.end())
        return { };

    return GatheredClientData {
        clientOrigin,
        clientDataIterator->value,
        controllingRegistratioIterator->value,
        clientsPerOriginIterator->value.userAgent
    };
}

void SWServer::unregisterServiceWorkerClient(const ClientOrigin& clientOrigin, ScriptExecutionContextIdentifier clientIdentifier)
{
    auto clientRegistrableDomain = clientOrigin.clientRegistrableDomain();
    auto appInitiatedValueBefore = clientIsAppInitiatedForRegistrableDomain(clientOrigin.clientRegistrableDomain());

    m_clientsById.remove(clientIdentifier);
    m_visibleClientIdToInternalClientIdMap.remove(clientIdentifier.toString());

    auto clientsByRegistrableDomainIterator = m_clientsByRegistrableDomain.find(clientRegistrableDomain);
    ASSERT(clientsByRegistrableDomainIterator != m_clientsByRegistrableDomain.end());
    auto& clientsForRegistrableDomain = clientsByRegistrableDomainIterator->value;
    clientsForRegistrableDomain.remove(clientIdentifier);
    if (clientsForRegistrableDomain.isEmpty())
        m_clientsByRegistrableDomain.remove(clientsByRegistrableDomainIterator);

    // If the client that's going away is a service worker page then we need to unregister its service worker.
    bool didUnregister = false;
    if (auto registration = m_serviceWorkerPageIdentifierToRegistrationMap.get(clientIdentifier)) {
        removeFromScopeToRegistrationMap(registration->key());
        registration->clear();
        ASSERT(!m_serviceWorkerPageIdentifierToRegistrationMap.contains(clientIdentifier));
        didUnregister = true;
    }

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
                if (worker->isRunning() && worker->origin() == clientOrigin && !worker->shouldContinue())
                    workersToTerminate.append(worker.ptr());
            }
            for (auto* worker : workersToTerminate)
                worker->terminate();

            if (removeContextConnectionIfPossible(clientRegistrableDomain) == ShouldDelayRemoval::Yes) {
                auto iterator = m_clientIdentifiersPerOrigin.find(clientOrigin);
                ASSERT(iterator != m_clientIdentifiersPerOrigin.end());
                iterator->value.terminateServiceWorkersTimer->startOneShot(m_isProcessTerminationDelayEnabled ? defaultTerminationDelay : defaultFunctionalEventDuration);
                return;
            }

            m_clientIdentifiersPerOrigin.remove(clientOrigin);
        });
        auto* contextConnection = contextConnectionForRegistrableDomain(clientRegistrableDomain);
        bool shouldContextConnectionBeTerminatedWhenPossible = contextConnection && contextConnection->shouldTerminateWhenPossible();
        iterator->value.terminateServiceWorkersTimer->startOneShot(m_isProcessTerminationDelayEnabled && !MemoryPressureHandler::singleton().isUnderMemoryPressure() && !shouldContextConnectionBeTerminatedWhenPossible && !didUnregister ? defaultTerminationDelay : 0_s);
    }

    // If the app-bound value changed after this client was removed, we know it was the only app-bound
    // client for its origin, and we should update all workers to reflect this.
    auto appInitiatedValueAfter = clientIsAppInitiatedForRegistrableDomain(clientOrigin.clientRegistrableDomain());
    if (appInitiatedValueBefore != appInitiatedValueAfter)
        updateAppInitiatedValueForWorkers(clientOrigin, appInitiatedValueAfter);

    auto registrationIterator = m_clientToControllingRegistration.find(clientIdentifier);
    if (registrationIterator == m_clientToControllingRegistration.end())
        return;

    if (auto* registration = m_registrations.get(registrationIterator->value))
        registration->removeClientUsingRegistration(clientIdentifier);

    m_clientToControllingRegistration.remove(registrationIterator);
}

std::optional<ServiceWorkerRegistrationIdentifier> SWServer::clientIdentifierToControllingRegistration(ScriptExecutionContextIdentifier clientIdentifier) const
{
    auto registrationIterator = m_clientToControllingRegistration.find(clientIdentifier);
    if (registrationIterator == m_clientToControllingRegistration.end())
        return { };
    return registrationIterator->value;
}

SWServer::ShouldDelayRemoval SWServer::removeContextConnectionIfPossible(const RegistrableDomain& domain)
{
    if (m_clientsByRegistrableDomain.contains(domain))
        return ShouldDelayRemoval::No;

    auto* connection = contextConnectionForRegistrableDomain(domain);
    if (!connection)
        return ShouldDelayRemoval::No;

    for (auto& worker : m_runningOrTerminatingWorkers.values()) {
        if (worker->isRunning() && worker->registrableDomain() == domain && worker->shouldContinue())
            return ShouldDelayRemoval::Yes;
    }

    removeContextConnection(*connection);
    connection->connectionIsNoLongerNeeded();
    return ShouldDelayRemoval::No;
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
    if (m_scopeToRegistrationMap.contains(key) && !allowLoopbackIPAddress(key.topOrigin().host))
        m_uniqueRegistrationCount--;

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

void SWServer::Connection::whenRegistrationReady(const SecurityOriginData& topOrigin, const URL& clientURL, CompletionHandler<void(std::optional<ServiceWorkerRegistrationData>&&)>&& callback)
{
    if (auto* registration = doRegistrationMatching(topOrigin, clientURL)) {
        if (registration->activeWorker()) {
            callback(registration->data());
            return;
        }
    }
    m_registrationReadyRequests.append({ topOrigin, clientURL, WTFMove(callback) });
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

        request.callback(registration.data());
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

    auto registrableDomain = connection.registrableDomain();
    auto serviceWorkerPageIdentifier = connection.serviceWorkerPageIdentifier();

    ASSERT(m_contextConnections.get(registrableDomain) == &connection);

    m_contextConnections.remove(registrableDomain);
    markAllWorkersForRegistrableDomainAsTerminated(registrableDomain);
    if (needsContextConnectionForRegistrableDomain(registrableDomain))
        createContextConnection(registrableDomain, serviceWorkerPageIdentifier);
}

void SWServer::createContextConnection(const RegistrableDomain& registrableDomain, std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier)
{
    ASSERT(!m_contextConnections.contains(registrableDomain));
    if (m_pendingConnectionDomains.contains(registrableDomain))
        return;

    RELEASE_LOG(ServiceWorker, "SWServer::createContextConnection will create a connection");

    std::optional<ProcessIdentifier> requestingProcessIdentifier;
    if (auto it = m_clientsByRegistrableDomain.find(registrableDomain); it != m_clientsByRegistrableDomain.end()) {
        if (!it->value.isEmpty())
            requestingProcessIdentifier = it->value.begin()->processIdentifier();
    }

    m_pendingConnectionDomains.add(registrableDomain);
    m_createContextConnectionCallback(registrableDomain, requestingProcessIdentifier, serviceWorkerPageIdentifier, [this, weakThis = WeakPtr { *this }, registrableDomain, serviceWorkerPageIdentifier] {
        if (!weakThis)
            return;

        RELEASE_LOG(ServiceWorker, "SWServer::createContextConnection should now have created a connection");

        ASSERT(m_pendingConnectionDomains.contains(registrableDomain));
        m_pendingConnectionDomains.remove(registrableDomain);

        if (m_contextConnections.contains(registrableDomain))
            return;

        if (needsContextConnectionForRegistrableDomain(registrableDomain))
            createContextConnection(registrableDomain, serviceWorkerPageIdentifier);
    });
}

bool SWServer::canHandleScheme(StringView scheme) const
{
    if (scheme.isNull())
        return false;
    if (!startsWithLettersIgnoringASCIICase(scheme, "http"_s))
        return false;
    if (scheme.length() == 5 && isASCIIAlphaCaselessEqual(scheme[4], 's'))
        return true;
    return scheme.length() == 4;
}

// https://w3c.github.io/ServiceWorker/#soft-update
void SWServer::softUpdate(SWServerRegistration& registration)
{
    // Let newestWorker be the result of running Get Newest Worker algorithm passing registration as its argument.
    // If newestWorker is null, abort these steps.
    auto* newestWorker = registration.getNewestWorker();
    if (!newestWorker)
        return;

    ServiceWorkerJobData jobData(Process::identifier(), ServiceWorkerIdentifier::generate());
    jobData.scriptURL = registration.scriptURL();
    jobData.topOrigin = registration.key().topOrigin();
    jobData.scopeURL = registration.scopeURLWithoutFragment();
    jobData.workerType = newestWorker->type();
    jobData.type = ServiceWorkerJobType::Update;
    scheduleJob(WTFMove(jobData));
}

void SWServer::processPushMessage(std::optional<Vector<uint8_t>>&& data, URL&& registrationURL, CompletionHandler<void(bool)>&& callback)
{
    whenImportIsCompletedIfNeeded([this, weakThis = WeakPtr { *this }, data = WTFMove(data), registrationURL = WTFMove(registrationURL), callback = WTFMove(callback)]() mutable {
        LOG(Push, "ServiceWorker import is complete, can handle push message now");
        if (!weakThis) {
            callback(false);
            return;
        }

        auto origin = SecurityOriginData::fromURL(registrationURL);
        ServiceWorkerRegistrationKey registrationKey { WTFMove(origin), WTFMove(registrationURL) };
        auto registration = m_scopeToRegistrationMap.get(registrationKey);
        if (!registration) {
            RELEASE_LOG_ERROR(Push, "Cannot process push message: Failed to find SW registration for scope %" PRIVATE_LOG_STRING, registrationKey.scope().string().utf8().data());
            callback(true);
            return;
        }

        auto* worker = registration->activeWorker();
        if (!worker) {
            RELEASE_LOG_ERROR(Push, "Cannot process push message: No active worker for scope %" PRIVATE_LOG_STRING, registrationKey.scope().string().utf8().data());
            callback(true);
            return;
        }

        RELEASE_LOG(Push, "Firing Push event");

        fireFunctionalEvent(*registration, [worker = Ref { *worker }, weakThis = WTFMove(weakThis), data = WTFMove(data), callback = WTFMove(callback)](auto&& connectionOrStatus) mutable {
            if (!connectionOrStatus.has_value()) {
                callback(connectionOrStatus.error() == ShouldSkipEvent::Yes);
                return;
            }

            auto serviceWorkerIdentifier = worker->identifier();

            worker->incrementFunctionalEventCounter();
            auto terminateWorkerTimer = makeUnique<Timer>([worker] {
                RELEASE_LOG_ERROR(ServiceWorker, "Service worker is taking too much time to process a push event");
                worker->decrementFunctionalEventCounter();
            });
            terminateWorkerTimer->startOneShot(weakThis && weakThis->m_isProcessTerminationDelayEnabled ? defaultTerminationDelay : defaultFunctionalEventDuration);
            connectionOrStatus.value()->firePushEvent(serviceWorkerIdentifier, data, [callback = WTFMove(callback), terminateWorkerTimer = WTFMove(terminateWorkerTimer), worker = WTFMove(worker)](bool succeeded) mutable {
                if (!succeeded)
                    RELEASE_LOG(Push, "Push event was not successfully handled");
                if (terminateWorkerTimer->isActive()) {
                    worker->decrementFunctionalEventCounter();
                    terminateWorkerTimer->stop();
                }

                callback(succeeded);
            });
        });
    });
}

void SWServer::processNotificationEvent(NotificationData&& data, NotificationEventType type, CompletionHandler<void(bool)>&& callback)
{
    whenImportIsCompletedIfNeeded([this, weakThis = WeakPtr { *this }, data = WTFMove(data), type, callback = WTFMove(callback)]() mutable {
        if (!weakThis) {
            callback(false);
            return;
        }

        auto origin = SecurityOriginData::fromURL(data.serviceWorkerRegistrationURL);
        ServiceWorkerRegistrationKey registrationKey { WTFMove(origin), URL { data.serviceWorkerRegistrationURL } };
        auto registration = m_scopeToRegistrationMap.get(registrationKey);
        if (!registration) {
            RELEASE_LOG_ERROR(Push, "Cannot process notification event: Failed to find SW registration for scope %" PRIVATE_LOG_STRING, registrationKey.scope().string().utf8().data());
            callback(true);
            return;
        }

        auto* worker = registration->activeWorker();
        if (!worker) {
            RELEASE_LOG_ERROR(Push, "Cannot process notification event: No active worker for scope %" PRIVATE_LOG_STRING, registrationKey.scope().string().utf8().data());
            callback(true);
            return;
        }

        fireFunctionalEvent(*registration, [worker = Ref { *worker }, weakThis = WTFMove(weakThis), data = WTFMove(data), type, callback = WTFMove(callback)](auto&& connectionOrStatus) mutable {
            if (!connectionOrStatus.has_value()) {
                callback(connectionOrStatus.error() == ShouldSkipEvent::Yes);
                return;
            }

            auto serviceWorkerIdentifier = worker->identifier();

            worker->incrementFunctionalEventCounter();
            auto terminateWorkerTimer = makeUnique<Timer>([worker] {
                RELEASE_LOG_ERROR(ServiceWorker, "Service worker is taking too much time to process a notification event");
                worker->decrementFunctionalEventCounter();
            });
            terminateWorkerTimer->startOneShot(weakThis && weakThis->m_isProcessTerminationDelayEnabled ? defaultTerminationDelay : defaultFunctionalEventDuration);
            connectionOrStatus.value()->fireNotificationEvent(serviceWorkerIdentifier, data, type, [callback = WTFMove(callback), terminateWorkerTimer = WTFMove(terminateWorkerTimer), worker = WTFMove(worker)] (bool succeeded) mutable {
                RELEASE_LOG_ERROR_IF(!succeeded, ServiceWorker, "Service Worker notification event handler did not succeed");

                // FIXME: if succeeded is false, should we implement a default action like opening a new page?
                if (terminateWorkerTimer->isActive()) {
                    worker->decrementFunctionalEventCounter();
                    terminateWorkerTimer->stop();
                }

                callback(succeeded);
            });
        });
    });
}

// https://w3c.github.io/ServiceWorker/#fire-functional-event-algorithm, just for push right now.
void SWServer::fireFunctionalEvent(SWServerRegistration& registration, CompletionHandler<void(Expected<SWServerToContextConnection*, ShouldSkipEvent>)>&& callback)
{
    auto* worker = registration.activeWorker();
    if (!worker) {
        callback(makeUnexpected(ShouldSkipEvent::Yes));
        return;
    }

    // FIXME: we should check whether we can skip the event and if skipping do a soft-update.

    worker->whenActivated([this, weakThis = WeakPtr { *this }, callback = WTFMove(callback), registrationIdentifier = registration.identifier(), serviceWorkerIdentifier = worker->identifier()](bool success) mutable {
        if (!weakThis) {
            callback(makeUnexpected(ShouldSkipEvent::No));
            return;
        }

        if (!success) {
            if (auto* registration = m_registrations.get(registrationIdentifier))
                softUpdate(*registration);
            callback(makeUnexpected(ShouldSkipEvent::No));
            return;
        }

        auto* worker = workerByID(serviceWorkerIdentifier);
        if (!worker) {
            callback(makeUnexpected(ShouldSkipEvent::No));
            return;
        }

        if (!worker->contextConnection())
            createContextConnection(worker->registrableDomain(), worker->serviceWorkerPageIdentifier());

        runServiceWorkerIfNecessary(serviceWorkerIdentifier, [callback = WTFMove(callback)](auto* contextConnection) mutable {
            if (!contextConnection) {
                callback(makeUnexpected(ShouldSkipEvent::No));
                return;
            }
            callback(contextConnection);
        });
    });
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
