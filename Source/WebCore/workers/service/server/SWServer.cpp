/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#include "AdvancedPrivacyProtections.h"
#include "BackgroundFetchEngine.h"
#include "BackgroundFetchInformation.h"
#include "BackgroundFetchOptions.h"
#include "BackgroundFetchRecordInformation.h"
#include "BackgroundFetchRequest.h"
#include "ExceptionCode.h"
#include "ExceptionData.h"
#include "Logging.h"
#include "NotificationData.h"
#include "SWOriginStore.h"
#include "SWRegistrationStore.h"
#include "SWServerJobQueue.h"
#include "SWServerRegistration.h"
#include "SWServerToContextConnection.h"
#include "SWServerWorker.h"
#include "SecurityOrigin.h"
#include "ServiceWorkerClientType.h"
#include "ServiceWorkerContextData.h"
#include "ServiceWorkerJobData.h"
#include "WorkerFetchResult.h"
#include <wtf/CallbackAggregator.h>
#include <wtf/CompletionHandler.h>
#include <wtf/EnumTraits.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SWServer);
WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(SWServerConnection, SWServer::Connection);

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

Ref<SWServer> SWServer::create(SWServerDelegate& delegate, UniqueRef<SWOriginStore>&& originStore, bool processTerminationDelayEnabled, String&& registrationDatabaseDirectory, PAL::SessionID sessionID, bool shouldRunServiceWorkersOnMainThreadForTesting, bool hasServiceWorkerEntitlement, std::optional<unsigned> overrideServiceWorkerRegistrationCountTestingValue, ServiceWorkerIsInspectable isInspectable)
{
    return adoptRef(*new SWServer(delegate, WTFMove(originStore), processTerminationDelayEnabled, WTFMove(registrationDatabaseDirectory), sessionID, shouldRunServiceWorkersOnMainThreadForTesting, hasServiceWorkerEntitlement, overrideServiceWorkerRegistrationCountTestingValue, isInspectable));
}

SWServer::~SWServer()
{
    // Destroy the remaining connections before the SWServer gets destroyed since they have a raw pointer
    // to the server and since they try to unregister clients from the server in their destructor.
    auto connections = WTFMove(m_connections);
    connections.clear();

    for (auto& callback : std::exchange(m_importCompletedCallbacks, { }))
        callback();

    Vector<Ref<SWServerWorker>> runningWorkers;
    for (auto& worker : m_runningOrTerminatingWorkers.values()) {
        if (worker->isRunning())
            runningWorkers.append(worker);
    }
    for (auto& runningWorker : runningWorkers)
        runningWorker->terminate();
}

RefPtr<SWServerWorker> SWServer::workerByID(ServiceWorkerIdentifier identifier) const
{
    RefPtr worker = SWServerWorker::existingWorkerForIdentifier(identifier);
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
    RefPtr registration = m_registrations.get(identifier);
    return registration ? registration->activeWorker() : nullptr;
}

SWServerRegistration* SWServer::getRegistration(const ServiceWorkerRegistrationKey& registrationKey)
{
    return m_scopeToRegistrationMap.get(registrationKey);
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

void SWServer::addRegistrationFromStore(ServiceWorkerContextData&& data, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isMainThread());

    // Pages should not have been able to make a new registration to this key while the import was still taking place.
    ASSERT(!m_scopeToRegistrationMap.contains(data.registration.key));

    LOG(ServiceWorker, "Adding registration from store for %s", data.registration.key.loggingString().utf8().data());

    auto registrationKey = data.registration.key;
    auto registrableDomain = WebCore::RegistrableDomain(registrationKey.topOrigin());
    validateRegistrationDomain(registrableDomain, ServiceWorkerJobType::Register, m_scopeToRegistrationMap.contains(registrationKey), [this, weakThis = WeakPtr { *this }, data = WTFMove(data), completionHandler = WTFMove(completionHandler)] (bool isValid) mutable {
        ASSERT(isMainThread());
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completionHandler();
        if (m_hasServiceWorkerEntitlement || isValid) {
            Ref registration = SWServerRegistration::create(*this, data.registration.key, data.registration.updateViaCache, data.registration.scopeURL, data.scriptURL, data.serviceWorkerPageIdentifier, WTFMove(data.navigationPreloadState));
            registration->setLastUpdateTime(data.registration.lastUpdateTime);
            addRegistration(registration.copyRef());

            Ref worker = SWServerWorker::create(*this, registration, data.scriptURL, data.script, data.certificateInfo, data.contentSecurityPolicy, data.crossOriginEmbedderPolicy, WTFMove(data.referrerPolicy), data.workerType, data.serviceWorkerIdentifier, WTFMove(data.scriptResourceMap));
            registration->updateRegistrationState(ServiceWorkerRegistrationState::Active, worker.ptr());
            worker->setState(ServiceWorkerState::Activated);
        }
        completionHandler();
    });
}

void SWServer::didSaveWorkerScriptsToDisk(ServiceWorkerIdentifier serviceWorkerIdentifier, ScriptBuffer&& mainScript, MemoryCompactRobinHoodHashMap<URL, ScriptBuffer>&& importedScripts)
{
    if (RefPtr worker = workerByID(serviceWorkerIdentifier))
        worker->didSaveScriptsToDisk(WTFMove(mainScript), WTFMove(importedScripts));
}

void SWServer::addRegistration(Ref<SWServerRegistration>&& registration)
{
    LOG(ServiceWorker, "Adding registration live for %s", registration->key().loggingString().utf8().data());

    if (!m_scopeToRegistrationMap.contains(registration->key()) && !allowLoopbackIPAddress(registration->key().topOrigin().host()))
        m_uniqueRegistrationCount++;

    if (registration->serviceWorkerPageIdentifier())
        m_serviceWorkerPageIdentifierToRegistrationMap.add(*registration->serviceWorkerPageIdentifier(), registration.get());

    m_originStore->add(registration->key().topOrigin());
    auto registrationID = registration->identifier();
    ASSERT(!m_scopeToRegistrationMap.contains(registration->key()));
    m_scopeToRegistrationMap.set(registration->key(), registration.get());
    auto addResult1 = m_registrations.add(registrationID, WTFMove(registration));
    ASSERT_UNUSED(addResult1, addResult1.isNewEntry);
}

void SWServer::removeRegistration(ServiceWorkerRegistrationIdentifier registrationID)
{
    RefPtr registration = m_registrations.take(registrationID);
    ASSERT(registration);

    if (registration->serviceWorkerPageIdentifier())
        m_serviceWorkerPageIdentifierToRegistrationMap.remove(*registration->serviceWorkerPageIdentifier());
    
    auto it = m_scopeToRegistrationMap.find(registration->key());
    if (it != m_scopeToRegistrationMap.end() && it->value.ptr() == registration.get()) {
        m_scopeToRegistrationMap.remove(it);
        if (!SecurityOrigin::isLocalHostOrLoopbackIPAddress(registration->key().topOrigin().host()))
            m_uniqueRegistrationCount--;
    }

    m_originStore->remove(registration->key().topOrigin());
    if (RefPtr store = m_registrationStore)
        store->removeRegistration(registration->key());

    backgroundFetchEngine().remove(*registration);
}

Vector<ServiceWorkerRegistrationData> SWServer::getRegistrations(const SecurityOriginData& topOrigin, const URL& clientURL)
{
    Vector<Ref<SWServerRegistration>> matchingRegistrations;
    for (auto& item : m_scopeToRegistrationMap) {
        if (item.key.originIsMatching(topOrigin, clientURL)) {
            auto& registration = item.value.get();
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

void SWServer::storeRegistrationsOnDisk(CompletionHandler<void()>&& completionHandler)
{
    RefPtr store = m_registrationStore;
    if (!store)
        return completionHandler();

    store->closeFiles(WTFMove(completionHandler));
}

void SWServer::clearAll(CompletionHandler<void()>&& completionHandler)
{
    if (!m_importCompleted) {
        m_clearCompletionCallbacks.append([this, completionHandler = WTFMove(completionHandler)] () mutable {
            ASSERT(m_importCompleted);
            Ref { *this }->clearAll(WTFMove(completionHandler));
        });
        return;
    }

    m_jobQueues.clear();
    while (!m_registrations.isEmpty())
        m_registrations.begin()->value->clear();
    m_pendingContextDatas.clear();
    m_originStore->clearAll();

    RefPtr store = m_registrationStore;
    if (!store)
        return completionHandler();

    store->clearAll(WTFMove(completionHandler));
}

void SWServer::clear(const SecurityOriginData& securityOrigin, CompletionHandler<void()>&& completionHandler)
{
    clearInternal([securityOrigin](auto& key) {
        return key.relatesToOrigin(securityOrigin);
    }, WTFMove(completionHandler));
}

void SWServer::clear(const ClientOrigin& origin, CompletionHandler<void()>&& completionHandler)
{
    clearInternal([origin](auto& key) {
        return key.topOrigin() == origin.topOrigin && origin.clientOrigin == SecurityOriginData::fromURL(key.scope());
    }, WTFMove(completionHandler));
}

void SWServer::clearInternal(Function<bool(const ServiceWorkerRegistrationKey&)>&& matches, CompletionHandler<void()>&& completionHandler)
{
    if (!m_importCompleted) {
        m_clearCompletionCallbacks.append([this, matches = WTFMove(matches), completionHandler = WTFMove(completionHandler)] () mutable {
            ASSERT(m_importCompleted);
            Ref { *this }->clearInternal(WTFMove(matches), WTFMove(completionHandler));
        });
        return;
    }

    m_jobQueues.removeIf([&](auto& keyAndValue) {
        return matches(keyAndValue.key);
    });

    Vector<Ref<SWServerRegistration>> registrationsToRemove;
    for (auto& registration : m_registrations.values()) {
        if (matches(registration->key()))
            registrationsToRemove.append(registration.get());
    }

    for (auto& contextDatas : m_pendingContextDatas.values()) {
        contextDatas.removeAllMatching([&](auto& contextData) {
            return matches(contextData.registration.key);
        });
    }

    if (registrationsToRemove.isEmpty()) {
        completionHandler();
        return;
    }

    // Calling SWServerRegistration::clear() takes care of updating m_registrations, m_originStore and m_registrationStore.
    for (auto& registration : registrationsToRemove)
        registration->clear(); // Will destroy the registration.

    RefPtr store = m_registrationStore;
    if (!store)
        return completionHandler();

    store->flushChanges(WTFMove(completionHandler));
}

void SWServer::Connection::finishFetchingScriptInServer(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, const ServiceWorkerRegistrationKey& registrationKey, WorkerFetchResult&& result)
{
    protectedServer()->scriptFetchFinished(jobDataIdentifier, registrationKey, m_identifier, WTFMove(result));
}

void SWServer::Connection::didResolveRegistrationPromise(const ServiceWorkerRegistrationKey& key)
{
    protectedServer()->didResolveRegistrationPromise(this, key);
}

RefPtr<SWServerRegistration> SWServer::Connection::doRegistrationMatching(const SecurityOriginData& topOrigin, const URL& clientURL)
{
    return m_server->doRegistrationMatching(topOrigin, clientURL);
}

void SWServer::Connection::addServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    protectedServer()->addClientServiceWorkerRegistration(*this, identifier);
}

void SWServer::Connection::removeServiceWorkerRegistrationInServer(ServiceWorkerRegistrationIdentifier identifier)
{
    protectedServer()->removeClientServiceWorkerRegistration(*this, identifier);
}

SWServer::SWServer(SWServerDelegate& delegate, UniqueRef<SWOriginStore>&& originStore, bool processTerminationDelayEnabled, String&& registrationDatabaseDirectory, PAL::SessionID sessionID, bool shouldRunServiceWorkersOnMainThreadForTesting, bool hasServiceWorkerEntitlement, std::optional<unsigned> overrideServiceWorkerRegistrationCountTestingValue, ServiceWorkerIsInspectable inspectable)
    : m_delegate(delegate)
    , m_originStore(WTFMove(originStore))
    , m_sessionID(sessionID)
    , m_isProcessTerminationDelayEnabled(processTerminationDelayEnabled)
    , m_shouldRunServiceWorkersOnMainThreadForTesting(shouldRunServiceWorkersOnMainThreadForTesting)
    , m_hasServiceWorkerEntitlement(hasServiceWorkerEntitlement)
    , m_overrideServiceWorkerRegistrationCountTestingValue(overrideServiceWorkerRegistrationCountTestingValue)
    , m_isInspectable(inspectable)
{
    RELEASE_LOG_IF(registrationDatabaseDirectory.isEmpty(), ServiceWorker, "No path to store the service worker registrations");

    if (RefPtr store = m_delegate->createRegistrationStore(*this)) {
        m_registrationStore = store;
        store->importRegistrations([weakThis = WeakPtr { *this }](auto&& result) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            if (!result) {
                protectedThis->registrationStoreDatabaseFailedToOpen();
                return;
            }

            Ref callbackAggregator = CallbackAggregator::create([weakThis = WTFMove(weakThis)]() mutable {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->registrationStoreImportComplete();
            });
            for (auto&& data : WTFMove(result.value()))
                protectedThis->addRegistrationFromStore(WTFMove(data), [callbackAggregator] { });
        });
    } else
        registrationStoreImportComplete();

    UNUSED_PARAM(registrationDatabaseDirectory);
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

    m_delegate->appBoundDomains([this, weakThis = WeakPtr { *this }, domain = WTFMove(domain), jobTypeAllowed, completionHandler = WTFMove(completionHandler)](HashSet<RegistrableDomain>&& appBoundDomains) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
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

    auto registrationKey = jobData.registrationKey();
    validateRegistrationDomain(WebCore::RegistrableDomain(jobData.topOrigin), jobData.type, m_scopeToRegistrationMap.contains(registrationKey), [weakThis = WeakPtr { *this }, jobData = WTFMove(jobData)] (bool isValid) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (protectedThis->m_hasServiceWorkerEntitlement || isValid) {
            auto& jobQueue = *protectedThis->m_jobQueues.ensure(jobData.registrationKey(), [&] {
                return makeUnique<SWServerJobQueue>(*protectedThis, jobData.registrationKey());
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
            protectedThis->rejectJob(jobData, { ExceptionCode::TypeError, "Job rejected for non app-bound domain"_s });
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
    CheckedPtr connection = m_connections.get(jobData.connectionIdentifier());
    if (!connection)
        return;

    connection->rejectJobInClient(jobData.identifier().jobIdentifier, exceptionData);
}

void SWServer::resolveRegistrationJob(const ServiceWorkerJobData& jobData, const ServiceWorkerRegistrationData& registrationData, ShouldNotifyWhenResolved shouldNotifyWhenResolved)
{
    LOG(ServiceWorker, "Resolved ServiceWorker job %s in server with registration %s", jobData.identifier().loggingString().utf8().data(), registrationData.identifier.loggingString().utf8().data());
    CheckedPtr connection = m_connections.get(jobData.connectionIdentifier());
    if (!connection) {
        if (shouldNotifyWhenResolved == ShouldNotifyWhenResolved::Yes && jobData.connectionIdentifier() == Process::identifier())
            didResolveRegistrationPromise(nullptr, registrationData.key);
        return;
    }

    connection->resolveRegistrationJobInClient(jobData.identifier().jobIdentifier, registrationData, shouldNotifyWhenResolved);
}

void SWServer::resolveUnregistrationJob(const ServiceWorkerJobData& jobData, const ServiceWorkerRegistrationKey& registrationKey, bool unregistrationResult)
{
    CheckedPtr connection = m_connections.get(jobData.connectionIdentifier());
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

    CheckedPtr connection = m_connections.get(jobData.connectionIdentifier());
    if (connection) {
        connection->startScriptFetchInClient(jobData.identifier().jobIdentifier, jobData.registrationKey(), shouldRefreshCache ? FetchOptions::Cache::NoCache : FetchOptions::Cache::Default);
        return;
    }
    if (jobData.connectionIdentifier() == Process::identifier()) {
        ASSERT(jobData.type == ServiceWorkerJobType::Update);
        // This is a soft-update job, create directly a network load to fetch the script.
        auto request = createScriptRequest(jobData.scriptURL, jobData, registration);
        request.setHTTPHeaderField(HTTPHeaderName::ServiceWorker, "script"_s);
        m_delegate->softUpdate(ServiceWorkerJobData { jobData }, shouldRefreshCache, WTFMove(request), [weakThis = WeakPtr { *this }, jobDataIdentifier = jobData.identifier(), registrationKey = jobData.registrationKey()](WorkerFetchResult&& result) {
            std::optional<ProcessIdentifier> requestingProcessIdentifier;
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->scriptFetchFinished(jobDataIdentifier, registrationKey, requestingProcessIdentifier, WTFMove(result));
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

    if (CheckedPtr jobQueue = m_jobQueues.get(registrationKey))
        jobQueue->scriptFetchFinished(jobDataIdentifier, requestingProcessIdentifier, WTFMove(result));
}

void SWServer::refreshImportedScripts(const ServiceWorkerJobData& jobData, SWServerRegistration& registration, const Vector<URL>& urls, const std::optional<ProcessIdentifier>& requestingProcessIdentifier)
{
    RefreshImportedScriptsHandler::Callback callback = [weakThis = WeakPtr { *this }, jobDataIdentifier = jobData.identifier(), registrationKey = jobData.registrationKey(), rpi = requestingProcessIdentifier](auto&& scripts) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->refreshImportedScriptsFinished(jobDataIdentifier, registrationKey, scripts, rpi);
    };
    bool shouldRefreshCache = registration.updateViaCache() == ServiceWorkerUpdateViaCache::None || (registration.getNewestWorker() && registration.isStale());
    auto handler = RefreshImportedScriptsHandler::create(urls.size(), WTFMove(callback));
    for (auto& url : urls) {
        m_delegate->softUpdate(ServiceWorkerJobData { jobData }, shouldRefreshCache, createScriptRequest(url, jobData, registration), [handler, url, size = urls.size()](WorkerFetchResult&& result) {
            handler->add(url, WTFMove(result));
        });
    }
}

void SWServer::refreshImportedScriptsFinished(const ServiceWorkerJobDataIdentifier& jobDataIdentifier, const ServiceWorkerRegistrationKey& registrationKey, const Vector<std::pair<URL, ScriptBuffer>>& scripts, const std::optional<ProcessIdentifier>& requestingProcessIdentifier)
{
    if (CheckedPtr jobQueue = m_jobQueues.get(registrationKey))
        jobQueue->importedScriptsFetchFinished(jobDataIdentifier, scripts, requestingProcessIdentifier);
}

void SWServer::scriptContextFailedToStart(const std::optional<ServiceWorkerJobDataIdentifier>& jobDataIdentifier, SWServerWorker& worker, const String& message)
{
    if (!jobDataIdentifier)
        return;

    RELEASE_LOG_ERROR(ServiceWorker, "%p - SWServer::scriptContextFailedToStart: Failed to start SW for job %s, error: %s", this, jobDataIdentifier->loggingString().utf8().data(), message.utf8().data());

    CheckedPtr jobQueue = m_jobQueues.get(worker.registrationKey());
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

    CheckedPtr jobQueue = m_jobQueues.get(worker.registrationKey());
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
    RefPtr registration = worker.registration();
    if (registration && registration->preInstallationWorker() == &worker)
        registration->setPreInstallationWorker(nullptr);
}

void SWServer::didFinishInstall(const std::optional<ServiceWorkerJobDataIdentifier>& jobDataIdentifier, SWServerWorker& worker, bool wasSuccessful)
{
    RELEASE_LOG(ServiceWorker, "%p - SWServer::didFinishInstall: Finished install for service worker %" PRIu64 ", success is %d", this, worker.identifier().toUInt64(), wasSuccessful);

    if (!jobDataIdentifier)
        return;

    if (wasSuccessful)
        storeRegistrationForWorker(worker);

    if (CheckedPtr jobQueue = m_jobQueues.get(worker.registrationKey()))
        jobQueue->didFinishInstall(*jobDataIdentifier, worker, wasSuccessful);
}

void SWServer::didFinishActivation(SWServerWorker& worker)
{
    RELEASE_LOG(ServiceWorker, "%p - SWServer::didFinishActivation: Finished activation for service worker %" PRIu64, this, worker.identifier().toUInt64());

    if (RefPtr registration = worker.registration())
        registration->didFinishActivation(worker.identifier());
}

void SWServer::storeRegistrationForWorker(SWServerWorker& worker)
{
    if (RefPtr store = m_registrationStore)
        store->updateRegistration(worker.contextData());
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
            if (&worker != this->activeWorkerFromRegistrationID(worker.data().registrationIdentifier))
                return;
        }
        if (options.type != ServiceWorkerClientType::All && options.type != clientData.type)
            return;
        matchingClients.append(clientData);
    });
    callback(WTFMove(matchingClients));
}

template<typename ClientDataType, typename ClientsByIDType>
void forEachClientForOriginImpl(const Vector<ScriptExecutionContextIdentifier>& identifiers, ClientsByIDType& clientsById, const Function<void(ClientDataType&)>& apply)
{
    for (auto& clientIdentifier : identifiers) {
        auto clientIterator = clientsById.find(clientIdentifier);
        ASSERT(clientIterator != clientsById.end());
        apply(clientIterator->value);
    }
}

void SWServer::forEachClientForOrigin(const ClientOrigin& origin, const Function<void(const ServiceWorkerClientData&)>& apply) const
{
    auto iterator = m_clientIdentifiersPerOrigin.find(origin);
    if (iterator != m_clientIdentifiersPerOrigin.end())
        forEachClientForOriginImpl(iterator->value.identifiers, m_clientsById, apply);
}

void SWServer::forEachClientForOrigin(const ClientOrigin& origin, const Function<void(ServiceWorkerClientData&)>& apply)
{
    auto iterator = m_clientIdentifiersPerOrigin.find(origin);
    if (iterator != m_clientIdentifiersPerOrigin.end())
        forEachClientForOriginImpl(iterator->value.identifiers, m_clientsById, apply);
}

std::optional<ExceptionData> SWServer::claim(SWServerWorker& worker)
{
    RefPtr registration = worker.registration();
    if (!registration || &worker != registration->activeWorker())
        return ExceptionData { ExceptionCode::InvalidStateError, "Service worker is not active"_s };

    auto& origin = worker.origin();
    forEachClientForOrigin(origin, [&](auto& clientData) {
        // FIXME: The specification currently doesn't deal properly via Blob URL clients.
        // https://github.com/w3c/ServiceWorker/issues/1554
        URL& clientURLForRegistrationMatching = clientData.url;
        if (clientURLForRegistrationMatching.protocolIsBlob() && clientData.ownerURL.isValid())
            clientURLForRegistrationMatching = clientData.ownerURL;

        if (doRegistrationMatching(origin.topOrigin, clientURLForRegistrationMatching) != registration.get())
            return;

        auto result = m_clientToControllingRegistration.add(clientData.identifier, registration->identifier());
        if (!result.isNewEntry) {
            auto previousIdentifier = result.iterator->value;
            if (previousIdentifier == registration->identifier())
                return;
            result.iterator->value = registration->identifier();
            if (RefPtr controllingRegistration = m_registrations.get(previousIdentifier))
                controllingRegistration->removeClientUsingRegistration(clientData.identifier); // May destroy the registration.
        }
        registration->controlClient(clientData.identifier);
    });
    return { };
}

void SWServer::didResolveRegistrationPromise(Connection* connection, const ServiceWorkerRegistrationKey& registrationKey)
{
    ASSERT_UNUSED(connection, !connection || m_connections.contains(connection->identifier()));

    if (CheckedPtr jobQueue = m_jobQueues.get(registrationKey))
        jobQueue->didResolveRegistrationPromise();
}

void SWServer::addClientServiceWorkerRegistration(Connection& connection, ServiceWorkerRegistrationIdentifier identifier)
{
    RefPtr registration = m_registrations.get(identifier);
    if (!registration) {
        LOG_ERROR("Request to add client-side ServiceWorkerRegistration to non-existent server-side registration");
        return;
    }
    
    registration->addClientServiceWorkerRegistration(connection.identifier());
}

void SWServer::removeClientServiceWorkerRegistration(Connection& connection, ServiceWorkerRegistrationIdentifier identifier)
{
    if (RefPtr registration = m_registrations.get(identifier))
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
    RegistrableDomain registrableDomain(data.registration.key.topOrigin());
    CheckedPtr connection = contextConnectionForRegistrableDomain(registrableDomain);
    if (!connection) {
        auto firstPartyForCookies = data.registration.key.firstPartyForCookies();
        m_pendingContextDatas.ensure(registrableDomain, [] {
            return Vector<ServiceWorkerContextData> { };
        }).iterator->value.append(WTFMove(data));

        createContextConnection(registrableDomain, data.serviceWorkerPageIdentifier);
        return;
    }

    m_delegate->addAllowedFirstPartyForCookies(connection->webProcessIdentifier(), requestingProcessIdentifier, data.registration.key.firstPartyForCookies());
    installContextData(data);
}

void SWServer::contextConnectionCreated(SWServerToContextConnection& contextConnection)
{
    auto requestingProcessIdentifier = contextConnection.webProcessIdentifier();
    m_delegate->addAllowedFirstPartyForCookies(contextConnection.webProcessIdentifier(), requestingProcessIdentifier, RegistrableDomain(contextConnection.registrableDomain()));

    for (auto& connection : m_connections.values())
        CheckedRef { *connection }->contextConnectionCreated(contextConnection);

    contextConnection.setInspectable(m_isInspectable);

    auto pendingContextDatas = m_pendingContextDatas.take(contextConnection.registrableDomain());
    for (auto& data : pendingContextDatas) {
        m_delegate->addAllowedFirstPartyForCookies(contextConnection.webProcessIdentifier(), requestingProcessIdentifier, data.registration.key.firstPartyForCookies());
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
    for (Ref worker : m_runningOrTerminatingWorkers.values()) {
        if (!apply(worker))
            break;
    }
}

void SWServer::terminateContextConnectionWhenPossible(const RegistrableDomain& registrableDomain, ProcessIdentifier processIdentifier)
{
    CheckedPtr contextConnection = contextConnectionForRegistrableDomain(registrableDomain);
    if (!contextConnection || contextConnection->webProcessIdentifier() != processIdentifier)
        return;

    contextConnection->terminateWhenPossible();
}

OptionSet<AdvancedPrivacyProtections> SWServer::advancedPrivacyProtectionsFromClient(const ClientOrigin& origin) const
{
    OptionSet<AdvancedPrivacyProtections> result;
    forEachClientForOrigin(origin, [&result](auto& clientData) {
        result.add(clientData.advancedPrivacyProtections);
    });
    return result;
}

void SWServer::installContextData(const ServiceWorkerContextData& data)
{
    ASSERT_WITH_MESSAGE(!data.loadedFromDisk, "Workers we just read from disk should only be launched as needed");

    if (data.jobDataIdentifier) {
        // Abort if the job that scheduled this has been cancelled.
        CheckedPtr jobQueue = m_jobQueues.get(data.registration.key);
        if (!jobQueue || !jobQueue->isCurrentlyProcessingJob(*data.jobDataIdentifier))
            return;
    }

    RefPtr registration = m_scopeToRegistrationMap.get(data.registration.key);
    Ref worker = SWServerWorker::create(*this, *registration, data.scriptURL, data.script, data.certificateInfo, data.contentSecurityPolicy, data.crossOriginEmbedderPolicy, String { data.referrerPolicy }, data.workerType, data.serviceWorkerIdentifier, MemoryCompactRobinHoodHashMap<URL, ServiceWorkerContextData::ImportedScript> { data.scriptResourceMap });

    CheckedPtr connection = worker->contextConnection();
    ASSERT(connection);

    registration->setPreInstallationWorker(worker.ptr());
    worker->setState(SWServerWorker::State::Running);
    auto userAgent = worker->userAgent();
    auto result = m_runningOrTerminatingWorkers.add(data.serviceWorkerIdentifier, worker.copyRef());
    ASSERT_UNUSED(result, result.isNewEntry);

    connection->installServiceWorkerContext(data, worker->data(), userAgent, worker->workerThreadMode(), advancedPrivacyProtectionsFromClient(worker->registrationKey().clientOrigin()));
}

void SWServer::runServiceWorkerIfNecessary(ServiceWorkerIdentifier identifier, RunServiceWorkerCallback&& callback)
{
    RefPtr worker = workerByID(identifier);
    if (!worker) {
        callback(nullptr);
        return;
    }
    runServiceWorkerIfNecessary(*worker, WTFMove(callback));
}

void SWServer::runServiceWorkerIfNecessary(SWServerWorker& worker, RunServiceWorkerCallback&& callback)
{
    CheckedPtr contextConnection = worker.contextConnection();
    if (worker.isRunning()) {
        ASSERT(contextConnection);
        callback(contextConnection.get());
        return;
    }
    
    if (worker.state() == ServiceWorkerState::Redundant) {
        callback(nullptr);
        return;
    }

    if (worker.isTerminating()) {
        worker.whenTerminated([weakThis = WeakPtr { *this }, identifier = worker.identifier(), callback = WTFMove(callback)]() mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return callback(nullptr);
            protectedThis->runServiceWorkerIfNecessary(identifier, WTFMove(callback));
        });
        return;
    }

    if (!contextConnection) {
        auto& serviceWorkerRunRequestsForOrigin = m_serviceWorkerRunRequests.ensure(worker.topRegistrableDomain(), [] {
            return HashMap<ServiceWorkerIdentifier, Vector<RunServiceWorkerCallback>> { };
        }).iterator->value;
        serviceWorkerRunRequestsForOrigin.ensure(worker.identifier(), [&] {
            return Vector<RunServiceWorkerCallback> { };
        }).iterator->value.append(WTFMove(callback));

        createContextConnection(worker.topRegistrableDomain(), worker.serviceWorkerPageIdentifier());
        return;
    }

    bool success = runServiceWorker(worker.identifier());
    callback(success ? contextConnection.get() : nullptr);
}

bool SWServer::runServiceWorker(ServiceWorkerIdentifier identifier)
{
    RefPtr worker = workerByID(identifier);
    if (!worker)
        return false;

    return runServiceWorker(*worker);
}

bool SWServer::runServiceWorker(SWServerWorker& worker)
{
    // If the registration for a worker has been removed then the request to run the worker is moot.
    if (!worker.registration())
        return false;

    ASSERT(!worker.isTerminating());
    ASSERT(!m_runningOrTerminatingWorkers.contains(worker.identifier()));
    m_runningOrTerminatingWorkers.add(worker.identifier(), worker);

    worker.setState(SWServerWorker::State::Running);

    CheckedPtr contextConnection = worker.contextConnection();
    ASSERT(contextConnection);

    contextConnection->installServiceWorkerContext(worker.contextData(), worker.data(), worker.userAgent(), worker.workerThreadMode(), advancedPrivacyProtectionsFromClient(worker.registrationKey().clientOrigin()));

    return true;
}

void SWServer::markAllWorkersForRegistrableDomainAsTerminated(const RegistrableDomain& registrableDomain)
{
    Vector<Ref<SWServerWorker>> terminatedWorkers;
    for (auto& worker : m_runningOrTerminatingWorkers.values()) {
        if (worker->topRegistrableDomain() == registrableDomain)
            terminatedWorkers.append(worker);
    }
    for (auto& terminatedWorker : terminatedWorkers)
        workerContextTerminated(terminatedWorker);
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

    if (CheckedPtr jobQueue = m_jobQueues.get(worker.registrationKey()))
        jobQueue->cancelJobsFromServiceWorker(worker.identifier());
}

void SWServer::fireInstallEvent(SWServerWorker& worker)
{
    CheckedPtr contextConnection = worker.contextConnection();
    if (!contextConnection) {
        RELEASE_LOG_ERROR(ServiceWorker, "Request to fire install event on a worker whose context connection does not exist");
        return;
    }

    RELEASE_LOG(ServiceWorker, "%p - SWServer::fireInstallEvent on worker %" PRIu64, this, worker.identifier().toUInt64());
    contextConnection->fireInstallEvent(worker.identifier());
}

void SWServer::runServiceWorkerAndFireActivateEvent(SWServerWorker& worker)
{
    runServiceWorkerIfNecessary(worker, [worker = Ref { worker }](auto* contextConnection) {
        if (!contextConnection) {
            RELEASE_LOG_ERROR(ServiceWorker, "Request to fire activate event on a worker whose context connection does not exist");
            return;
        }

        if (worker->state() != ServiceWorkerState::Activating)
            return;

        RELEASE_LOG(ServiceWorker, "SWServer::runServiceWorkerAndFireActivateEvent on worker %" PRIu64, worker->identifier().toUInt64());
        worker->markActivateEventAsFired();
        contextConnection->fireActivateEvent(worker->identifier());
    });
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
        Ref { registration }->unregisterServerConnection(connectionIdentifier);

    for (auto& jobQueue : m_jobQueues.values())
        CheckedRef { *jobQueue }->cancelJobsFromConnection(connectionIdentifier);
}

RefPtr<SWServerRegistration> SWServer::doRegistrationMatching(const SecurityOriginData& topOrigin, const URL& clientURL)
{
    ASSERT(isImportCompleted());
    RefPtr<SWServerRegistration> selectedRegistration;
    for (auto& pair : m_scopeToRegistrationMap) {
        if (!pair.key.isMatching(topOrigin, clientURL))
            continue;
        if (!selectedRegistration || selectedRegistration->key().scopeLength() < pair.key.scopeLength())
            selectedRegistration = pair.value.ptr();
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
    for (Ref worker : m_runningOrTerminatingWorkers.values()) {
        if (worker->origin().clientRegistrableDomain() == clientOrigin.clientRegistrableDomain())
            worker->updateAppInitiatedValue(lastNavigationWasAppInitiated);
    }
}

void SWServer::registerServiceWorkerClient(ClientOrigin&& clientOrigin, ServiceWorkerClientData&& data, const std::optional<ServiceWorkerRegistrationIdentifier>& controllingServiceWorkerRegistrationIdentifier, String&& userAgent, IsBeingCreatedClient isBeingCreatedClient)
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

    ASSERT(!m_clientPendingMessagesById.contains(clientIdentifier));
    if (isBeingCreatedClient == IsBeingCreatedClient::Yes)
        m_clientPendingMessagesById.add(clientIdentifier, Vector<ServiceWorkerClientPendingMessage> { });

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

    RefPtr controllingRegistration = m_registrations.get(*controllingServiceWorkerRegistrationIdentifier);
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
    m_clientPendingMessagesById.remove(clientIdentifier);
    m_visibleClientIdToInternalClientIdMap.remove(clientIdentifier.toString());

    auto clientsByRegistrableDomainIterator = m_clientsByRegistrableDomain.find(clientRegistrableDomain);
    ASSERT(clientsByRegistrableDomainIterator != m_clientsByRegistrableDomain.end());
    auto& clientsForRegistrableDomain = clientsByRegistrableDomainIterator->value;
    clientsForRegistrableDomain.remove(clientIdentifier);
    if (clientsForRegistrableDomain.isEmpty())
        m_clientsByRegistrableDomain.remove(clientsByRegistrableDomainIterator);

    // If the client that's going away is a service worker page then we need to unregister its service worker.
    bool didUnregister = false;
    if (RefPtr registration = m_serviceWorkerPageIdentifierToRegistrationMap.get(clientIdentifier)) {
        removeFromScopeToRegistrationMap(registration->key());
        if (RefPtr preinstallingServiceWorker = registration->preInstallationWorker()) {
            if (CheckedPtr jobQueue = m_jobQueues.get(registration->key()))
                jobQueue->cancelJobsFromServiceWorker(preinstallingServiceWorker->identifier());
        }
        registration->clear(); // Will destroy the registration.
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
            Vector<Ref<SWServerWorker>> workersToTerminate;
            for (auto& worker : m_runningOrTerminatingWorkers.values()) {
                if (worker->isRunning() && worker->origin() == clientOrigin && !worker->shouldContinue())
                    workersToTerminate.append(worker);
            }
            for (auto& worker : workersToTerminate)
                worker->terminate();

            if (removeContextConnectionIfPossible(clientRegistrableDomain) == ShouldDelayRemoval::Yes) {
                auto iterator = m_clientIdentifiersPerOrigin.find(clientOrigin);
                ASSERT(iterator != m_clientIdentifiersPerOrigin.end());
                iterator->value.terminateServiceWorkersTimer->startOneShot(m_isProcessTerminationDelayEnabled ? defaultTerminationDelay : defaultFunctionalEventDuration);
                return;
            }

            m_clientIdentifiersPerOrigin.remove(clientOrigin);
        });
        CheckedPtr contextConnection = contextConnectionForRegistrableDomain(clientRegistrableDomain);
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

    if (RefPtr registration = m_registrations.get(registrationIterator->value))
        registration->removeClientUsingRegistration(clientIdentifier); // May destroy the registration.

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

    WeakPtr connection = contextConnectionForRegistrableDomain(domain);
    if (!connection)
        return ShouldDelayRemoval::No;

    for (Ref worker : m_runningOrTerminatingWorkers.values()) {
        if (worker->isRunning() && worker->topRegistrableDomain() == domain && worker->shouldContinue())
            return ShouldDelayRemoval::Yes;
    }

    removeContextConnection(*connection);
    connection->connectionIsNoLongerNeeded(); // May destroy the connection object.
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
    if (m_scopeToRegistrationMap.contains(key) && !allowLoopbackIPAddress(key.topOrigin().host()))
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
        CheckedRef { *connection }->resolveRegistrationReadyRequests(registration);
}

void SWServer::Connection::whenRegistrationReady(const SecurityOriginData& topOrigin, const URL& clientURL, CompletionHandler<void(std::optional<ServiceWorkerRegistrationData>&&)>&& callback)
{
    if (RefPtr registration = doRegistrationMatching(topOrigin, clientURL)) {
        if (registration->activeWorker()) {
            callback(registration->data());
            return;
        }
    }
    m_registrationReadyRequests.append({ topOrigin, clientURL, WTFMove(callback) });
}

void SWServer::Connection::storeRegistrationsOnDisk(CompletionHandler<void()>&& callback)
{
    protectedServer()->storeRegistrationsOnDisk(WTFMove(callback));
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

void SWServer::getAllOrigins(CompletionHandler<void(HashSet<ClientOrigin>&&)>&& callback)
{
    whenImportIsCompletedIfNeeded([weakThis = WeakPtr { *this }, callback = WTFMove(callback)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            callback({ });
            return;
        }
        HashSet<ClientOrigin> clientOrigins;
        for (auto& key : protectedThis->m_scopeToRegistrationMap.keys())
            clientOrigins.add(key.clientOrigin());
        callback(WTFMove(clientOrigins));
    });
}

void SWServer::addContextConnection(SWServerToContextConnection& connection)
{
    RELEASE_LOG(ServiceWorker, "SWServer::addContextConnection %" PRIu64, connection.identifier().toUInt64());

    ASSERT(!m_contextConnections.contains(connection.registrableDomain()));

    m_contextConnections.add(connection.registrableDomain(), connection);

    contextConnectionCreated(connection);
}

void SWServer::removeContextConnection(SWServerToContextConnection& connection)
{
    RELEASE_LOG(ServiceWorker, "SWServer::removeContextConnection %" PRIu64, connection.identifier().toUInt64());

    auto registrableDomain = connection.registrableDomain();
    auto serviceWorkerPageIdentifier = connection.serviceWorkerPageIdentifier();

    ASSERT(m_contextConnections.get(registrableDomain) == &connection);

    m_contextConnections.remove(registrableDomain);
    markAllWorkersForRegistrableDomainAsTerminated(registrableDomain);
    if (needsContextConnectionForRegistrableDomain(registrableDomain))
        createContextConnection(registrableDomain, serviceWorkerPageIdentifier);
}

SWServerToContextConnection* SWServer::contextConnectionForRegistrableDomain(const RegistrableDomain& domain)
{
    return m_contextConnections.get(domain);
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
    m_delegate->createContextConnection(registrableDomain, requestingProcessIdentifier, serviceWorkerPageIdentifier, [weakThis = WeakPtr { *this }, registrableDomain, serviceWorkerPageIdentifier] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RELEASE_LOG(ServiceWorker, "SWServer::createContextConnection should now have created a connection");

        ASSERT(protectedThis->m_pendingConnectionDomains.contains(registrableDomain));
        protectedThis->m_pendingConnectionDomains.remove(registrableDomain);

        if (protectedThis->m_contextConnections.contains(registrableDomain))
            return;

        if (protectedThis->needsContextConnectionForRegistrableDomain(registrableDomain))
            protectedThis->createContextConnection(registrableDomain, serviceWorkerPageIdentifier);
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
    RefPtr newestWorker = registration.getNewestWorker();
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

void SWServer::processPushMessage(std::optional<Vector<uint8_t>>&& data, std::optional<NotificationPayload>&& notificationPayload, URL&& registrationURL, CompletionHandler<void(bool, std::optional<NotificationPayload>&&)>&& callback)
{
    whenImportIsCompletedIfNeeded([weakThis = WeakPtr { *this }, data = WTFMove(data), notificationPayload = WTFMove(notificationPayload), registrationURL = WTFMove(registrationURL), callback = WTFMove(callback)]() mutable {
        LOG(Push, "ServiceWorker import is complete, can handle push message now");
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            callback(false, WTFMove(notificationPayload));
            return;
        }

        auto origin = SecurityOriginData::fromURL(registrationURL);
        ServiceWorkerRegistrationKey registrationKey { WTFMove(origin), WTFMove(registrationURL) };
        RefPtr registration = protectedThis->m_scopeToRegistrationMap.get(registrationKey);
        if (!registration) {
            RELEASE_LOG_ERROR(Push, "Cannot process push message: Failed to find SW registration for scope %" SENSITIVE_LOG_STRING, registrationKey.scope().string().utf8().data());
            callback(true, WTFMove(notificationPayload));
            return;
        }

        RefPtr worker = registration->activeWorker();
        if (!worker) {
            RELEASE_LOG_ERROR(Push, "Cannot process push message: No active worker for scope %" SENSITIVE_LOG_STRING, registrationKey.scope().string().utf8().data());
            callback(true, WTFMove(notificationPayload));
            return;
        }

        RELEASE_LOG(Push, "Firing push event");
        protectedThis->fireFunctionalEvent(*registration, [worker = worker.releaseNonNull(), weakThis = WTFMove(weakThis), data = WTFMove(data), notificationPayload = WTFMove(notificationPayload), callback = WTFMove(callback)](auto&& connectionOrStatus) mutable {
            if (!connectionOrStatus.has_value()) {
                callback(connectionOrStatus.error() == ShouldSkipEvent::Yes, WTFMove(notificationPayload));
                return;
            }

            auto serviceWorkerIdentifier = worker->identifier();

            worker->incrementFunctionalEventCounter();
            auto terminateWorkerTimer = makeUnique<Timer>([worker] {
                RELEASE_LOG_ERROR(ServiceWorker, "Service worker is taking too much time to process a `push` event");
                worker->decrementFunctionalEventCounter();
            });
            terminateWorkerTimer->startOneShot(weakThis && weakThis->m_isProcessTerminationDelayEnabled ? defaultTerminationDelay : defaultFunctionalEventDuration);
            connectionOrStatus.value()->firePushEvent(serviceWorkerIdentifier, data, WTFMove(notificationPayload), [callback = WTFMove(callback), terminateWorkerTimer = WTFMove(terminateWorkerTimer), worker = WTFMove(worker)](bool succeeded, std::optional<NotificationPayload>&& resultPayload) mutable {
                if (!succeeded)
                    RELEASE_LOG(Push, "Push event was not successfully handled");
                if (terminateWorkerTimer->isActive()) {
                    worker->decrementFunctionalEventCounter();
                    terminateWorkerTimer->stop();
                }

                callback(succeeded, WTFMove(resultPayload));
            });
        });
    });
}

void SWServer::processNotificationEvent(NotificationData&& data, NotificationEventType type, CompletionHandler<void(bool)>&& callback)
{
    whenImportIsCompletedIfNeeded([weakThis = WeakPtr { *this }, data = WTFMove(data), type, callback = WTFMove(callback)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            callback(false);
            return;
        }

        auto origin = SecurityOriginData::fromURL(data.serviceWorkerRegistrationURL);
        ServiceWorkerRegistrationKey registrationKey { WTFMove(origin), URL { data.serviceWorkerRegistrationURL } };
        RefPtr registration = protectedThis->m_scopeToRegistrationMap.get(registrationKey);
        if (!registration) {
            RELEASE_LOG_ERROR(Push, "Cannot process notification event: Failed to find SW registration for scope %" SENSITIVE_LOG_STRING, registrationKey.scope().string().utf8().data());
            callback(true);
            return;
        }

        RefPtr worker = registration->activeWorker();
        if (!worker) {
            RELEASE_LOG_ERROR(Push, "Cannot process notification event: No active worker for scope %" SENSITIVE_LOG_STRING, registrationKey.scope().string().utf8().data());
            callback(true);
            return;
        }

        protectedThis->fireFunctionalEvent(*registration, [worker = worker.releaseNonNull(), weakThis = WTFMove(weakThis), data = WTFMove(data), type, callback = WTFMove(callback)](auto&& connectionOrStatus) mutable {
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

void SWServer::fireBackgroundFetchEvent(SWServerRegistration& registration, BackgroundFetchInformation&& info, CompletionHandler<void()>&& callback)
{
    RefPtr worker = registration.activeWorker();
    if (!worker) {
        RELEASE_LOG_ERROR(ServiceWorker, "Cannot process background fetch update message: no active worker for scope %" PRIVATE_LOG_STRING, registration.key().scope().string().utf8().data());
        callback();
        return;
    }

    fireFunctionalEvent(registration, [weakThis = WeakPtr { *this }, worker = worker.releaseNonNull(), info = WTFMove(info), callback = WTFMove(callback)](auto&& connectionOrStatus) mutable {
        if (!connectionOrStatus.has_value()) {
            callback();
            return;
        }

        auto serviceWorkerIdentifier = worker->identifier();

        worker->incrementFunctionalEventCounter();
        auto terminateWorkerTimer = makeUnique<Timer>([worker] {
            RELEASE_LOG_ERROR(ServiceWorker, "Service worker is taking too much time to process a background fetch event");
            worker->decrementFunctionalEventCounter();
        });
        terminateWorkerTimer->startOneShot(weakThis && weakThis->m_isProcessTerminationDelayEnabled ? defaultTerminationDelay : defaultFunctionalEventDuration);
        connectionOrStatus.value()->fireBackgroundFetchEvent(serviceWorkerIdentifier, info, [terminateWorkerTimer = WTFMove(terminateWorkerTimer), worker = WTFMove(worker), callback = WTFMove(callback)](bool succeeded) mutable {
            RELEASE_LOG_ERROR_IF(!succeeded, ServiceWorker, "Background fetch event was not successfully handled");
            if (terminateWorkerTimer->isActive()) {
                worker->decrementFunctionalEventCounter();
                terminateWorkerTimer->stop();
            }
            callback();
        });
    });
}

void SWServer::fireBackgroundFetchClickEvent(SWServerRegistration& registration, BackgroundFetchInformation&& info)
{
    RefPtr worker = registration.activeWorker();
    if (!worker) {
        RELEASE_LOG_ERROR(ServiceWorker, "Cannot process background fetch click message: no active worker for scope %" PRIVATE_LOG_STRING, registration.key().scope().string().utf8().data());
        return;
    }

    fireFunctionalEvent(registration, [weakThis = WeakPtr { *this }, worker = worker.releaseNonNull(), info = WTFMove(info)](auto&& connectionOrStatus) mutable {
        if (!connectionOrStatus.has_value())
            return;

        auto serviceWorkerIdentifier = worker->identifier();

        worker->incrementFunctionalEventCounter();
        auto terminateWorkerTimer = makeUnique<Timer>([worker] {
            RELEASE_LOG_ERROR(ServiceWorker, "Service worker is taking too much time to process a background fetch click event");
            worker->decrementFunctionalEventCounter();
        });
        terminateWorkerTimer->startOneShot(weakThis && weakThis->m_isProcessTerminationDelayEnabled ? defaultTerminationDelay : defaultFunctionalEventDuration);
        connectionOrStatus.value()->fireBackgroundFetchClickEvent(serviceWorkerIdentifier, info, [terminateWorkerTimer = WTFMove(terminateWorkerTimer), worker = WTFMove(worker)](bool succeeded) mutable {
            RELEASE_LOG_ERROR_IF(!succeeded, ServiceWorker, "Background fetch clickevent was not successfully handled");
            if (terminateWorkerTimer->isActive()) {
                worker->decrementFunctionalEventCounter();
                terminateWorkerTimer->stop();
            }
        });
    });
}

// https://w3c.github.io/ServiceWorker/#fire-functional-event-algorithm, just for push right now.
void SWServer::fireFunctionalEvent(SWServerRegistration& registration, CompletionHandler<void(Expected<SWServerToContextConnection*, ShouldSkipEvent>)>&& callback)
{
    RefPtr worker = registration.activeWorker();
    if (!worker) {
        callback(makeUnexpected(ShouldSkipEvent::Yes));
        return;
    }

    // FIXME: we should check whether we can skip the event and if skipping do a soft-update.

    RELEASE_LOG(ServiceWorker, "SWServer::fireFunctionalEvent serviceWorkerID=%" PRIu64 ", state=%hhu", worker->identifier().toUInt64(), enumToUnderlyingType(worker->state()));

    worker->whenActivated([weakThis = WeakPtr { *this }, callback = WTFMove(callback), registrationIdentifier = registration.identifier(), serviceWorkerIdentifier = worker->identifier()](bool success) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            callback(makeUnexpected(ShouldSkipEvent::No));
            return;
        }

        if (!success) {
            if (RefPtr registration = protectedThis->m_registrations.get(registrationIdentifier))
                protectedThis->softUpdate(*registration);
            callback(makeUnexpected(ShouldSkipEvent::No));
            return;
        }

        RefPtr worker = protectedThis->workerByID(serviceWorkerIdentifier);
        if (!worker) {
            callback(makeUnexpected(ShouldSkipEvent::No));
            return;
        }

        if (!worker->contextConnection())
            protectedThis->createContextConnection(worker->topRegistrableDomain(), worker->serviceWorkerPageIdentifier());

        protectedThis->runServiceWorkerIfNecessary(serviceWorkerIdentifier, [callback = WTFMove(callback)](auto* contextConnection) mutable {
            if (!contextConnection) {
                callback(makeUnexpected(ShouldSkipEvent::No));
                return;
            }
            callback(contextConnection);
        });
    });
}

void SWServer::postMessageToServiceWorkerClient(ScriptExecutionContextIdentifier destinationContextIdentifier, const MessageWithMessagePorts& message, ServiceWorkerIdentifier sourceIdentifier, const String& sourceOrigin, const Function<void(ScriptExecutionContextIdentifier, const MessageWithMessagePorts&, const ServiceWorkerData&, const String&)>& callbackIfClientIsReady)
{
    RefPtr sourceServiceWorker = workerByID(sourceIdentifier);
    if (!sourceServiceWorker)
        return;

    auto iterator = m_clientPendingMessagesById.find(destinationContextIdentifier);
    if (iterator == m_clientPendingMessagesById.end()) {
        callbackIfClientIsReady(destinationContextIdentifier, message, sourceServiceWorker->data(), sourceOrigin);
        return;
    }
    iterator->value.append({ message, sourceServiceWorker->data(), sourceOrigin });
}

Vector<ServiceWorkerClientPendingMessage> SWServer::releaseServiceWorkerClientPendingMessage(ScriptExecutionContextIdentifier contextIdentifier)
{
    return m_clientPendingMessagesById.take(contextIdentifier);
}

void SWServer::setInspectable(ServiceWorkerIsInspectable inspectable)
{
    if (m_isInspectable == inspectable)
        return;

    m_isInspectable = inspectable;

    for (auto& connection : m_contextConnections.values())
        CheckedRef { connection.get() }->setInspectable(inspectable);
}

SWServerRegistration* SWServer::getRegistration(ServiceWorkerRegistrationIdentifier identifier)
{
    return m_registrations.get(identifier);
}

void SWServer::Connection::startBackgroundFetch(ServiceWorkerRegistrationIdentifier registrationIdentifier, const String& backgroundFetchIdentifier, Vector<BackgroundFetchRequest>&& requests, BackgroundFetchOptions&& options, ExceptionOrBackgroundFetchInformationCallback&& callback)
{
    RefPtr registration = protectedServer()->getRegistration(registrationIdentifier);
    if (!registration) {
        callback(makeUnexpected(ExceptionData { ExceptionCode::InvalidStateError, "No registration found"_s }));
        return;
    }

    protectedServer()->requestBackgroundFetchPermission({ registration->key().topOrigin(), SecurityOriginData::fromURL(registration->key().scope()) }, [weakServer = WeakPtr { server() }, registrationIdentifier, backgroundFetchIdentifier, requests = WTFMove(requests), options = WTFMove(options), callback = WTFMove(callback)](bool result) mutable {
        RefPtr server = weakServer.get();
        if (!server || !result) {
            callback(makeUnexpected(ExceptionData { ExceptionCode::NotAllowedError, "Background fetch permission is denied"_s }));
            return;
        }

        RefPtr registration = server->getRegistration(registrationIdentifier);
        if (!registration) {
            callback(makeUnexpected(ExceptionData { ExceptionCode::InvalidStateError, "No registration found"_s }));
            return;
        }
        if (!registration->activeWorker()) {
            callback(makeUnexpected(ExceptionData { ExceptionCode::TypeError, "No active worker"_s }));
            return;
        }

        server->backgroundFetchEngine().startBackgroundFetch(*registration, backgroundFetchIdentifier, WTFMove(requests), WTFMove(options), WTFMove(callback));
    });
}

BackgroundFetchEngine& SWServer::backgroundFetchEngine()
{
    if (!m_backgroundFetchEngine)
        m_backgroundFetchEngine = BackgroundFetchEngine::create(*this);
    return *m_backgroundFetchEngine;
}

void SWServer::Connection::backgroundFetchInformation(ServiceWorkerRegistrationIdentifier registrationIdentifier, const String& backgroundFetchIdentifier, BackgroundFetchEngine::ExceptionOrBackgroundFetchInformationCallback&& callback)
{
    RefPtr registration = server().getRegistration(registrationIdentifier);
    if (!registration) {
        callback(makeUnexpected(ExceptionData { ExceptionCode::InvalidStateError, "No registration found"_s }));
        return;
    }

    protectedServer()->backgroundFetchEngine().backgroundFetchInformation(*registration, backgroundFetchIdentifier, WTFMove(callback));
}

void SWServer::Connection::backgroundFetchIdentifiers(ServiceWorkerRegistrationIdentifier registrationIdentifier, BackgroundFetchEngine::BackgroundFetchIdentifiersCallback&& callback)
{
    RefPtr registration = server().getRegistration(registrationIdentifier);
    if (!registration) {
        callback({ });
        return;
    }

    protectedServer()->backgroundFetchEngine().backgroundFetchIdentifiers(*registration, WTFMove(callback));
}

void SWServer::Connection::abortBackgroundFetch(ServiceWorkerRegistrationIdentifier registrationIdentifier, const String& backgroundFetchIdentifier, BackgroundFetchEngine::AbortBackgroundFetchCallback&& callback)
{
    RefPtr registration = server().getRegistration(registrationIdentifier);
    if (!registration) {
        callback(false);
        return;
    }

    protectedServer()->backgroundFetchEngine().abortBackgroundFetch(*registration, backgroundFetchIdentifier, WTFMove(callback));
}

void SWServer::Connection::matchBackgroundFetch(ServiceWorkerRegistrationIdentifier registrationIdentifier, const String& backgroundFetchIdentifier, RetrieveRecordsOptions&& options, BackgroundFetchEngine::MatchBackgroundFetchCallback&& callback)
{
    RefPtr registration = server().getRegistration(registrationIdentifier);
    if (!registration) {
        callback({ });
        return;
    }

    protectedServer()->backgroundFetchEngine().matchBackgroundFetch(*registration, backgroundFetchIdentifier, WTFMove(options), WTFMove(callback));
}

void SWServer::Connection::retrieveRecordResponse(BackgroundFetchRecordIdentifier recordIdentifier, BackgroundFetchEngine::RetrieveRecordResponseCallback&& callback)
{
    protectedServer()->backgroundFetchEngine().retrieveRecordResponse(recordIdentifier, WTFMove(callback));
}

void SWServer::Connection::retrieveRecordResponseBody(BackgroundFetchRecordIdentifier recordIdentifier, BackgroundFetchEngine::RetrieveRecordResponseBodyCallback&& callback)
{
    protectedServer()->backgroundFetchEngine().retrieveRecordResponseBody(recordIdentifier, WTFMove(callback));
}

} // namespace WebCore
