/*
 * Copyright (C) 2014-2022 Apple Inc. All rights reserved.
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
#include "WebsiteDataStore.h"

#include "APIHTTPCookieStore.h"
#include "APIProcessPoolConfiguration.h"
#include "APIWebsiteDataRecord.h"
#include "AuthenticatorManager.h"
#include "DeviceIdHashSaltStorage.h"
#include "DownloadProxy.h"
#include "GPUProcessProxy.h"
#include "Logging.h"
#include "MockAuthenticatorManager.h"
#include "NetworkProcessConnectionInfo.h"
#include "NetworkProcessMessages.h"
#include "PageLoadState.h"
#include "ShouldGrandfatherStatistics.h"
#include "StorageAccessStatus.h"
#include "UnifiedOriginStorageLevel.h"
#include "WebBackForwardCache.h"
#include "WebKit2Initialize.h"
#include "WebNotificationManagerProxy.h"
#include "WebPageProxy.h"
#include "WebProcessCache.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebsiteData.h"
#include "WebsiteDataStoreClient.h"
#include "WebsiteDataStoreParameters.h"
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/CredentialStorage.h>
#include <WebCore/DatabaseTracker.h>
#include <WebCore/HTMLMediaElement.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/NotificationResources.h>
#include <WebCore/OriginLock.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SearchPopupMenu.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/WebLockRegistry.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CompletionHandler.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/FileSystem.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>

#if OS(DARWIN)
#include <wtf/spi/darwin/OSVariantSPI.h>
#endif

#if HAVE(SEC_KEY_PROXY)
#include "SecKeyProxyStore.h"
#endif

#if HAVE(APP_SSO)
#include "SOAuthorizationCoordinator.h"
#endif

#if PLATFORM(COCOA)
#include "DefaultWebBrowserChecks.h"
#endif

#if ENABLE(WEB_AUTHN)
#include "VirtualAuthenticatorManager.h"
#endif // ENABLE(WEB_AUTHN)

namespace WebKit {

static bool allowsWebsiteDataRecordsForAllOrigins;
void WebsiteDataStore::allowWebsiteDataRecordsForAllOrigins()
{
    allowsWebsiteDataRecordsForAllOrigins = true;
}

static HashMap<String, PAL::SessionID>& activeGeneralStorageDirectories()
{
    static MainThreadNeverDestroyed<HashMap<String, PAL::SessionID>> directoryToSessionMap;
    return directoryToSessionMap;
}

static HashMap<PAL::SessionID, WebsiteDataStore*>& allDataStores()
{
    RELEASE_ASSERT(isUIThread());
    static NeverDestroyed<HashMap<PAL::SessionID, WebsiteDataStore*>> map;
    return map;
}

WorkQueue& WebsiteDataStore::websiteDataStoreIOQueue()
{
    static auto& queue = WorkQueue::create("com.apple.WebKit.WebsiteDataStoreIO").leakRef();
    return queue;
}

void WebsiteDataStore::forEachWebsiteDataStore(Function<void(WebsiteDataStore&)>&& function)
{
    for (auto* dataStore : allDataStores().values())
        function(*dataStore);
}

Ref<WebsiteDataStore> WebsiteDataStore::createNonPersistent()
{
    return adoptRef(*new WebsiteDataStore(WebsiteDataStoreConfiguration::create(IsPersistent::No), PAL::SessionID::generateEphemeralSessionID()));
}

Ref<WebsiteDataStore> WebsiteDataStore::create(Ref<WebsiteDataStoreConfiguration>&& configuration, PAL::SessionID sessionID)
{
    return adoptRef(*new WebsiteDataStore(WTFMove(configuration), sessionID));
}

WebsiteDataStore::WebsiteDataStore(Ref<WebsiteDataStoreConfiguration>&& configuration, PAL::SessionID sessionID)
    : m_sessionID(sessionID)
    , m_resolvedConfiguration(WTFMove(configuration))
    , m_configuration(m_resolvedConfiguration->copy())
    , m_deviceIdHashSaltStorage(DeviceIdHashSaltStorage::create(isPersistent() ? m_configuration->deviceIdHashSaltsStorageDirectory() : String()))
#if ENABLE(TRACKING_PREVENTION)
    , m_trackingPreventionDebugMode(m_resolvedConfiguration->resourceLoadStatisticsDebugModeEnabled())
#endif
    , m_queue(WorkQueue::create("com.apple.WebKit.WebsiteDataStore"))
#if ENABLE(WEB_AUTHN)
    , m_authenticatorManager(makeUniqueRef<AuthenticatorManager>())
#endif
    , m_client(makeUniqueRef<WebsiteDataStoreClient>())
    , m_webLockRegistry(WebCore::LocalWebLockRegistry::create())
{
    RELEASE_LOG(Storage, "%p - WebsiteDataStore::WebsiteDataStore sessionID=%" PRIu64, this, m_sessionID.toUInt64());

    WTF::setProcessPrivileges(allPrivileges());
    registerWithSessionIDMap();
    platformInitialize();

    ASSERT(RunLoop::isMain());

    if (auto directory = m_configuration->generalStorageDirectory(); isPersistent() && !directory.isEmpty()) {
        if (!activeGeneralStorageDirectories().add(directory, m_sessionID).isNewEntry)
            RELEASE_LOG_FAULT(Storage, "GeneralStorageDirectory for session %" PRIu64 " is already in use by session %" PRIu64, m_sessionID.toUInt64(), activeGeneralStorageDirectories().get(directory).toUInt64());
    }
}

WebsiteDataStore::~WebsiteDataStore()
{
    RELEASE_LOG(Storage, "%p - WebsiteDataStore::~WebsiteDataStore sessionID=%" PRIu64, this, m_sessionID.toUInt64());

    ASSERT(RunLoop::isMain());
    RELEASE_ASSERT(m_sessionID.isValid());

    platformDestroy();

    ASSERT(allDataStores().get(m_sessionID) == this);
    if (auto generalStorageDirectory = m_configuration->generalStorageDirectory(); isPersistent() && !generalStorageDirectory.isEmpty())
        activeGeneralStorageDirectories().remove(generalStorageDirectory);
    allDataStores().remove(m_sessionID);
    if (m_networkProcess)
        m_networkProcess->removeSession(*this, std::exchange(m_completionHandlerForRemovalFromNetworkProcess, { }));
    if (m_completionHandlerForRemovalFromNetworkProcess) {
        RunLoop::main().dispatch([completionHandler = std::exchange(m_completionHandlerForRemovalFromNetworkProcess, { })]() mutable {
            completionHandler({ });
        });
    }
#if ENABLE(GPU_PROCESS)
    if (auto* gpuProcessProxy = GPUProcessProxy::singletonIfCreated())
        gpuProcessProxy->removeSession(m_sessionID);
#endif
}

// FIXME: Remove this when rdar://95786441 is resolved.
static RefPtr<WebsiteDataStore>& protectedDefaultDataStore()
{
    static NeverDestroyed<RefPtr<WebsiteDataStore>> globalDefaultDataStore;
    return globalDefaultDataStore.get();
}

static WeakPtr<WebsiteDataStore>& globalDefaultDataStore()
{
    static NeverDestroyed<WeakPtr<WebsiteDataStore>> globalDefaultDataStore;
    return globalDefaultDataStore.get();
}

static IsPersistent defaultDataStoreIsPersistent()
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    // GTK and WPE ports require explicit configuration of a WebsiteDataStore. All default storage
    // locations are relative to the base directories configured by the
    // WebsiteDataStoreConfiguration. The default data store should (probably?) only be used for
    // prewarmed processes and C API tests, and should certainly never be allowed to store anything on disk.
    return IsPersistent::No;
#else
    // Other ports allow general use of the default WebsiteDataStore, and so need to persist data.
    return IsPersistent::Yes;
#endif
}

Ref<WebsiteDataStore> WebsiteDataStore::defaultDataStore()
{
    InitializeWebKit2();
    auto& globalDatasStore = globalDefaultDataStore();
    if (globalDatasStore)
        return Ref { *globalDatasStore };

    auto isPersistent = defaultDataStoreIsPersistent();
    auto newDataStore = adoptRef(new WebsiteDataStore(WebsiteDataStoreConfiguration::create(isPersistent),
        isPersistent == IsPersistent::Yes ? PAL::SessionID::defaultSessionID() : PAL::SessionID::generateEphemeralSessionID()));
    globalDatasStore = newDataStore.get();
    protectedDefaultDataStore() = newDataStore.get();

    return *newDataStore;
}

void WebsiteDataStore::deleteDefaultDataStoreForTesting()
{
    protectedDefaultDataStore() = nullptr;
}

bool WebsiteDataStore::defaultDataStoreExists()
{
    return !!globalDefaultDataStore();
}

RefPtr<WebsiteDataStore> WebsiteDataStore::existingDataStoreForIdentifier(const WTF::UUID& identifier)
{
    for (auto* dataStore : allDataStores().values()) {
        if (dataStore && dataStore->configuration().identifier() == identifier)
            return dataStore;
    }

    return nullptr;
}

#if PLATFORM(COCOA)
Ref<WebsiteDataStore> WebsiteDataStore::dataStoreForIdentifier(const WTF::UUID& uuid)
{
    RELEASE_ASSERT(uuid.isValid());

    InitializeWebKit2();
    for (auto* dataStore : allDataStores().values()) {
        if (dataStore && dataStore->configuration().identifier() == uuid)
            return Ref { *dataStore };
    }

    auto configuration = WebsiteDataStoreConfiguration::create(uuid);
    return WebsiteDataStore::create(WTFMove(configuration), PAL::SessionID::generatePersistentSessionID());
}
#endif

void WebsiteDataStore::registerWithSessionIDMap()
{
    auto result = allDataStores().add(m_sessionID, this);
    ASSERT_UNUSED(result, result.isNewEntry);
}

WebsiteDataStore* WebsiteDataStore::existingDataStoreForSessionID(PAL::SessionID sessionID)
{
    return allDataStores().get(sessionID);
}

#if HAVE(APP_SSO)
SOAuthorizationCoordinator& WebsiteDataStore::soAuthorizationCoordinator(const WebPageProxy& pageProxy)
{
    RELEASE_ASSERT(pageProxy.preferences().isExtensibleSSOEnabled());
    if (!m_soAuthorizationCoordinator)
        m_soAuthorizationCoordinator = WTF::makeUnique<SOAuthorizationCoordinator>();

    return *m_soAuthorizationCoordinator;
}
#endif

static Ref<NetworkProcessProxy> networkProcessForSession(PAL::SessionID sessionID)
{
#if ((PLATFORM(GTK) || PLATFORM(WPE)) && !ENABLE(2022_GLIB_API))
    if (sessionID.isEphemeral()) {
        // Reuse a previous persistent session network process for ephemeral sessions.
        for (auto* dataStore : allDataStores().values()) {
            if (dataStore->isPersistent())
                return dataStore->networkProcess();
        }
    }
    return NetworkProcessProxy::create();
#else
    UNUSED_PARAM(sessionID);
    return NetworkProcessProxy::ensureDefaultNetworkProcess();
#endif
}

NetworkProcessProxy& WebsiteDataStore::networkProcess()
{
    if (!m_networkProcess) {
        m_networkProcess = networkProcessForSession(m_sessionID);
        m_networkProcess->addSession(*this, NetworkProcessProxy::SendParametersToNetworkProcess::Yes);
    }

    return *m_networkProcess;
}

NetworkProcessProxy& WebsiteDataStore::networkProcess() const
{
    return const_cast<WebsiteDataStore&>(*this).networkProcess();
}

Ref<NetworkProcessProxy> WebsiteDataStore::protectedNetworkProcess() const
{
    return networkProcess();
}

void WebsiteDataStore::registerProcess(WebProcessProxy& process)
{
    ASSERT(process.pageCount() || process.provisionalPageCount());
    m_processes.add(process);
}

void WebsiteDataStore::unregisterProcess(WebProcessProxy& process)
{
    ASSERT(!process.pageCount());
    ASSERT(!process.provisionalPageCount());
    m_processes.remove(process);
}

void WebsiteDataStore::resolveDirectoriesIfNecessary()
{
    if (m_hasResolvedDirectories)
        return;
    m_hasResolvedDirectories = true;

    // Resolve directory paths.
    if (!m_configuration->applicationCacheDirectory().isEmpty())
        m_resolvedConfiguration->setApplicationCacheDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->applicationCacheDirectory()));
    if (!m_configuration->mediaCacheDirectory().isEmpty())
        m_resolvedConfiguration->setMediaCacheDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->mediaCacheDirectory()));
    if (!m_configuration->mediaKeysStorageDirectory().isEmpty())
        m_resolvedConfiguration->setMediaKeysStorageDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->mediaKeysStorageDirectory()));
    if (!m_configuration->indexedDBDatabaseDirectory().isEmpty())
        m_resolvedConfiguration->setIndexedDBDatabaseDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->indexedDBDatabaseDirectory()));
    if (!m_configuration->alternativeServicesDirectory().isEmpty())
        m_resolvedConfiguration->setAlternativeServicesDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->alternativeServicesDirectory()));
    if (!m_configuration->localStorageDirectory().isEmpty())
        m_resolvedConfiguration->setLocalStorageDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->localStorageDirectory()));
    if (!m_configuration->deviceIdHashSaltsStorageDirectory().isEmpty())
        m_resolvedConfiguration->setDeviceIdHashSaltsStorageDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->deviceIdHashSaltsStorageDirectory()));
    if (!m_configuration->networkCacheDirectory().isEmpty())
        m_resolvedConfiguration->setNetworkCacheDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->networkCacheDirectory()));
    if (!m_configuration->resourceLoadStatisticsDirectory().isEmpty())
        m_resolvedConfiguration->setResourceLoadStatisticsDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->resourceLoadStatisticsDirectory()));
    if (!m_configuration->serviceWorkerRegistrationDirectory().isEmpty())
        m_resolvedConfiguration->setServiceWorkerRegistrationDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->serviceWorkerRegistrationDirectory()));
    if (!m_configuration->javaScriptConfigurationDirectory().isEmpty())
        m_resolvedConfiguration->setJavaScriptConfigurationDirectory(resolvePathForSandboxExtension(m_configuration->javaScriptConfigurationDirectory()));
    if (!m_configuration->cacheStorageDirectory().isEmpty())
        m_resolvedConfiguration->setCacheStorageDirectory(resolvePathForSandboxExtension(m_configuration->cacheStorageDirectory()));
    if (!m_configuration->hstsStorageDirectory().isEmpty())
        m_resolvedConfiguration->setHSTSStorageDirectory(resolvePathForSandboxExtension(m_configuration->hstsStorageDirectory()));
    if (!m_configuration->generalStorageDirectory().isEmpty())
        m_resolvedConfiguration->setGeneralStorageDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->generalStorageDirectory()));
#if ENABLE(ARKIT_INLINE_PREVIEW)
    if (!m_configuration->modelElementCacheDirectory().isEmpty())
        m_resolvedConfiguration->setModelElementCacheDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->modelElementCacheDirectory()));
#endif
    if (!m_configuration->searchFieldHistoryDirectory().isEmpty())
        m_resolvedConfiguration->setSearchFieldHistoryDirectory(resolveAndCreateReadWriteDirectoryForSandboxExtension(m_configuration->searchFieldHistoryDirectory()));

    // Resolve file paths.
    if (!m_configuration->cookieStorageFile().isEmpty()) {
        auto resolvedCookieDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(FileSystem::parentPath(m_configuration->cookieStorageFile()));
        m_resolvedConfiguration->setCookieStorageFile(FileSystem::pathByAppendingComponent(resolvedCookieDirectory, FileSystem::pathFileName(m_configuration->cookieStorageFile())));
    }

    // Default paths of WebsiteDataStore created with identifier are not under caches or tmp directory,
    // so we need to explicitly exclude them from backup.
    if (m_configuration->identifier()) {
        Vector<String> allCacheDirectories = {
            resolvedApplicationCacheDirectory()
            , resolvedMediaCacheDirectory()
            , resolvedNetworkCacheDirectory()
#if ENABLE(ARKIT_INLINE_PREVIEW)
            , resolvedModelElementCacheDirectory()
#endif
        };
        m_queue->dispatch([directories = crossThreadCopy(WTFMove(allCacheDirectories))]() {
            for (auto& directory : directories)
                FileSystem::setExcludedFromBackup(directory, true);
        });
    }

    // Clear data of deprecated types asynchronously.
    if (auto webSQLDirectory = m_configuration->webSQLDatabaseDirectory(); !webSQLDirectory.isEmpty()) {
        m_queue->dispatch([webSQLDirectory = webSQLDirectory.isolatedCopy()]() {
            WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDirectory)->deleteAllDatabasesImmediately();
            FileSystem::deleteEmptyDirectory(webSQLDirectory);
        });
    }
}

static WebsiteDataStore::ProcessAccessType computeNetworkProcessAccessTypeForDataFetch(OptionSet<WebsiteDataType> dataTypes, bool isNonPersistentStore)
{
    for (auto dataType : dataTypes) {
        if (WebsiteData::ownerProcess(dataType) == WebsiteDataProcessType::Network)
            return isNonPersistentStore ? WebsiteDataStore::ProcessAccessType::OnlyIfLaunched : WebsiteDataStore::ProcessAccessType::Launch;
    }
    return WebsiteDataStore::ProcessAccessType::None;
}

static WebsiteDataStore::ProcessAccessType computeWebProcessAccessTypeForDataFetch(OptionSet<WebsiteDataType> dataTypes, bool /* isNonPersistentStore */)
{
    if (dataTypes.contains(WebsiteDataType::MemoryCache))
        return WebsiteDataStore::ProcessAccessType::OnlyIfLaunched;
    return WebsiteDataStore::ProcessAccessType::None;
}

void WebsiteDataStore::fetchData(OptionSet<WebsiteDataType> dataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, Function<void(Vector<WebsiteDataRecord>)>&& completionHandler)
{
    fetchDataAndApply(dataTypes, fetchOptions, WorkQueue::main(), WTFMove(completionHandler));
}

void WebsiteDataStore::fetchDataAndApply(OptionSet<WebsiteDataType> dataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, Ref<WorkQueue>&& queue, Function<void(Vector<WebsiteDataRecord>)>&& apply)
{
    RELEASE_LOG(Storage, "WebsiteDataStore::fetchDataAndApply started to fetch data for session %" PRIu64, m_sessionID.toUInt64());
    class CallbackAggregator final : public ThreadSafeRefCounted<CallbackAggregator, WTF::DestructionThread::MainRunLoop> {
    public:
        static Ref<CallbackAggregator> create(OptionSet<WebsiteDataFetchOption> fetchOptions, Ref<WorkQueue>&& queue, Function<void(Vector<WebsiteDataRecord>)>&& apply, WebsiteDataStore& dataStore)
        {
            return adoptRef(*new CallbackAggregator(fetchOptions, WTFMove(queue), WTFMove(apply), dataStore));
        }

        ~CallbackAggregator()
        {
            ASSERT(RunLoop::isMain());

            auto records = WTF::map(WTFMove(m_websiteDataRecords), [this](auto&& entry) {
                return m_queue.ptr() != &WorkQueue::main() ? crossThreadCopy(WTFMove(entry.value)) : WTFMove(entry.value);
            });
            m_queue->dispatch([apply = WTFMove(m_apply), records = WTFMove(records), sessionID = m_protectedDataStore->sessionID()] () mutable {
                apply(WTFMove(records));
                RELEASE_LOG(Storage, "WebsiteDataStore::fetchDataAndApply finished fetching data for session %" PRIu64, sessionID.toUInt64());
            });
        }

        const OptionSet<WebsiteDataFetchOption>& fetchOptions() const { return m_fetchOptions; }

        void addWebsiteData(WebsiteData&& websiteData)
        {
            if (!RunLoop::isMain()) {
                RunLoop::main().dispatch([protectedThis = Ref { *this }, websiteData = crossThreadCopy(websiteData)]() mutable {
                    protectedThis->addWebsiteData(WTFMove(websiteData));
                });
                return;
            }
            ASSERT(RunLoop::isMain());
            for (auto& entry : websiteData.entries) {
                auto displayName = WebsiteDataRecord::displayNameForOrigin(entry.origin);
                if (!displayName) {
                    if (!allowsWebsiteDataRecordsForAllOrigins)
                        continue;

                    String hostString = entry.origin.host().isEmpty() ? emptyString() : makeString(" ", entry.origin.host());
                    displayName = makeString(entry.origin.protocol(), hostString);
                }

                auto& record = m_websiteDataRecords.add(displayName, WebsiteDataRecord { }).iterator->value;
                if (!record.displayName)
                    record.displayName = WTFMove(displayName);

                record.add(entry.type, entry.origin);

                if (m_fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes)) {
                    if (!record.size)
                        record.size = WebsiteDataRecord::Size { 0, { } };

                    record.size->totalSize += entry.size;
                    record.size->typeSizes.add(static_cast<unsigned>(entry.type), 0).iterator->value += entry.size;
                }
            }

            for (auto& hostName : websiteData.hostNamesWithCookies) {
                auto displayName = WebsiteDataRecord::displayNameForCookieHostName(hostName);
                if (!displayName)
                    continue;

                auto& record = m_websiteDataRecords.add(displayName, WebsiteDataRecord { }).iterator->value;
                if (!record.displayName)
                    record.displayName = WTFMove(displayName);

                record.addCookieHostName(hostName);
            }

            for (auto& hostName : websiteData.hostNamesWithHSTSCache) {
                auto displayName = WebsiteDataRecord::displayNameForHostName(hostName);
                if (!displayName)
                    continue;
                
                auto& record = m_websiteDataRecords.add(displayName, WebsiteDataRecord { }).iterator->value;
                if (!record.displayName)
                    record.displayName = WTFMove(displayName);

                record.addHSTSCacheHostname(hostName);
            }

#if ENABLE(TRACKING_PREVENTION)
            for (const auto& domain : websiteData.registrableDomainsWithResourceLoadStatistics) {
                auto displayName = WebsiteDataRecord::displayNameForHostName(domain.string());
                if (!displayName)
                    continue;

                auto& record = m_websiteDataRecords.add(displayName, WebsiteDataRecord { }).iterator->value;
                if (!record.displayName)
                    record.displayName = WTFMove(displayName);

                record.addResourceLoadStatisticsRegistrableDomain(domain);
            }
#endif
        }

private:
        CallbackAggregator(OptionSet<WebsiteDataFetchOption> fetchOptions, Ref<WorkQueue>&& queue, Function<void(Vector<WebsiteDataRecord>)>&& apply, WebsiteDataStore& dataStore)
            : m_fetchOptions(fetchOptions)
            , m_queue(WTFMove(queue))
            , m_apply(WTFMove(apply))
            , m_protectedDataStore(dataStore)
        {
            ASSERT(RunLoop::isMain());
        }

        const OptionSet<WebsiteDataFetchOption> m_fetchOptions;
        Ref<WorkQueue> m_queue;
        Function<void(Vector<WebsiteDataRecord>)> m_apply;

        HashMap<String, WebsiteDataRecord> m_websiteDataRecords;
        Ref<WebsiteDataStore> m_protectedDataStore;
    };

    auto callbackAggregator = CallbackAggregator::create(fetchOptions, WTFMove(queue), WTFMove(apply), *this);

#if ENABLE(VIDEO)
    if (dataTypes.contains(WebsiteDataType::DiskCache)) {
        m_queue->dispatch([mediaCacheDirectory = m_configuration->mediaCacheDirectory().isolatedCopy(), callbackAggregator] {
            WebsiteData websiteData;
            auto origins = WebCore::HTMLMediaElement::originsInMediaCache(mediaCacheDirectory);
            websiteData.entries = WTF::map(origins, [](auto& origin) {
                return WebsiteData::Entry { origin, WebsiteDataType::DiskCache, 0 };
            });
            callbackAggregator->addWebsiteData(WTFMove(websiteData));
        });
    }
#endif

    auto networkProcessAccessType = computeNetworkProcessAccessTypeForDataFetch(dataTypes, !isPersistent());
    switch (networkProcessAccessType) {
    case ProcessAccessType::Launch:
        networkProcess();
        ASSERT(m_networkProcess);
        FALLTHROUGH;
    case ProcessAccessType::OnlyIfLaunched:
        if (m_networkProcess) {
            m_networkProcess->fetchWebsiteData(m_sessionID, dataTypes, fetchOptions, [callbackAggregator](WebsiteData websiteData) {
                callbackAggregator->addWebsiteData(WTFMove(websiteData));
            });
        }
        break;
    case ProcessAccessType::None:
        break;
    }

    auto webProcessAccessType = computeWebProcessAccessTypeForDataFetch(dataTypes, !isPersistent());
    if (webProcessAccessType != ProcessAccessType::None) {
        for (auto& process : processes()) {
            if (webProcessAccessType == ProcessAccessType::OnlyIfLaunched && process.state() != WebProcessProxy::State::Running)
                continue;
            process.fetchWebsiteData(m_sessionID, dataTypes, [callbackAggregator](WebsiteData websiteData) {
                callbackAggregator->addWebsiteData(WTFMove(websiteData));
            });
        }
    }

    if (dataTypes.contains(WebsiteDataType::DeviceIdHashSalt)) {
        m_deviceIdHashSaltStorage->getDeviceIdHashSaltOrigins([callbackAggregator](auto&& origins) {
            WebsiteData websiteData;
            websiteData.entries = WTF::map(origins, [](auto& origin) {
                return WebsiteData::Entry { origin, WebsiteDataType::DeviceIdHashSalt, 0 };
            });
            callbackAggregator->addWebsiteData(WTFMove(websiteData));
        });
    }

    if (dataTypes.contains(WebsiteDataType::OfflineWebApplicationCache) && isPersistent()) {
        m_queue->dispatch([fetchOptions, applicationCacheDirectory = m_configuration->applicationCacheDirectory().isolatedCopy(), applicationCacheFlatFileSubdirectoryName = m_configuration->applicationCacheFlatFileSubdirectoryName().isolatedCopy(), callbackAggregator] {
            auto storage = WebCore::ApplicationCacheStorage::create(applicationCacheDirectory, applicationCacheFlatFileSubdirectoryName);
            WebsiteData websiteData;
            auto origins = storage->originsWithCache();
            websiteData.entries = WTF::map(origins, [&](auto& origin) {
                uint64_t size = fetchOptions.contains(WebsiteDataFetchOption::ComputeSizes) ? storage->diskUsageForOrigin(origin) : 0;
                return WebsiteData::Entry { origin, WebsiteDataType::OfflineWebApplicationCache, size };
            });
            callbackAggregator->addWebsiteData(WTFMove(websiteData));
        });
    }

    if (dataTypes.contains(WebsiteDataType::MediaKeys) && isPersistent()) {
        m_queue->dispatch([mediaKeysStorageDirectory = m_configuration->mediaKeysStorageDirectory().isolatedCopy(), callbackAggregator] {
            WebsiteData websiteData;
            websiteData.entries = mediaKeyOrigins(mediaKeysStorageDirectory).map([](auto& origin) {
                return WebsiteData::Entry { origin, WebsiteDataType::MediaKeys, 0 };
            });
            callbackAggregator->addWebsiteData(WTFMove(websiteData));
        });
    }
}

#if ENABLE(TRACKING_PREVENTION)
void WebsiteDataStore::fetchDataForRegistrableDomains(OptionSet<WebsiteDataType> dataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, Vector<WebCore::RegistrableDomain>&& domains, CompletionHandler<void(Vector<WebsiteDataRecord>&&, HashSet<WebCore::RegistrableDomain>&&)>&& completionHandler)
{
    fetchDataAndApply(dataTypes, fetchOptions, m_queue.copyRef(), [domains = crossThreadCopy(domains), completionHandler = WTFMove(completionHandler)] (auto&& existingDataRecords) mutable {
        ASSERT(!RunLoop::isMain());
        
        Vector<WebsiteDataRecord> matchingDataRecords;
        HashSet<WebCore::RegistrableDomain> domainsWithMatchingDataRecords;
        for (auto&& dataRecord : existingDataRecords) {
            for (auto& domain : domains) {
                if (dataRecord.matches(domain)) {
                    matchingDataRecords.append(WTFMove(dataRecord));
                    domainsWithMatchingDataRecords.add(domain.isolatedCopy());
                    break;
                }
            }
        }
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), matchingDataRecords = WTFMove(matchingDataRecords), domainsWithMatchingDataRecords = WTFMove(domainsWithMatchingDataRecords)] () mutable {
            completionHandler(WTFMove(matchingDataRecords), WTFMove(domainsWithMatchingDataRecords));
        });
    });
}
#endif

static WebsiteDataStore::ProcessAccessType computeNetworkProcessAccessTypeForDataRemoval(OptionSet<WebsiteDataType> dataTypes, bool isNonPersistentStore)
{
    auto processAccessType = WebsiteDataStore::ProcessAccessType::None;
    for (auto dataType : dataTypes) {
        if (dataTypes.contains(WebsiteDataType::MemoryCache))
            processAccessType = WebsiteDataStore::ProcessAccessType::OnlyIfLaunched;
        if (WebsiteData::ownerProcess(dataType) != WebsiteDataProcessType::Network)
            continue;
        if (dataType != WebsiteDataType::Cookies || !isNonPersistentStore)
            return WebsiteDataStore::ProcessAccessType::Launch;
        processAccessType = WebsiteDataStore::ProcessAccessType::OnlyIfLaunched;
    }
    return processAccessType;
}

auto WebsiteDataStore::computeWebProcessAccessTypeForDataRemoval(OptionSet<WebsiteDataType> dataTypes, bool /* isNonPersistentStore */) -> ProcessAccessType
{
    if (dataTypes.contains(WebsiteDataType::MemoryCache))
        return ProcessAccessType::OnlyIfLaunched;
    return ProcessAccessType::None;
}

void WebsiteDataStore::removeData(OptionSet<WebsiteDataType> dataTypes, WallTime modifiedSince, Function<void()>&& completionHandler)
{
    RELEASE_LOG(Storage, "WebsiteDataStore::removeData started to delete data modified since %f for session %" PRIu64, modifiedSince.secondsSinceEpoch().value(), m_sessionID.toUInt64());
    auto callbackAggregator = MainRunLoopCallbackAggregator::create([protectedThis = Ref { *this }, sessionID = m_sessionID, completionHandler = WTFMove(completionHandler)] {
#if RELEASE_LOG_DISABLED
        UNUSED_PARAM(sessionID);
#endif
        RELEASE_LOG(Storage, "WebsiteDataStore::removeData finished deleting modified data for session %" PRIu64, sessionID.toUInt64());
        completionHandler();
    });

#if ENABLE(VIDEO)
    if (dataTypes.contains(WebsiteDataType::DiskCache)) {
        m_queue->dispatch([modifiedSince, mediaCacheDirectory = m_configuration->mediaCacheDirectory().isolatedCopy(), callbackAggregator] {
            WebCore::HTMLMediaElement::clearMediaCache(mediaCacheDirectory, modifiedSince);
        });
    }
#endif

#if ENABLE(TRACKING_PREVENTION)
    bool didNotifyNetworkProcessToDeleteWebsiteData = false;
#endif
    auto networkProcessAccessType = computeNetworkProcessAccessTypeForDataRemoval(dataTypes, !isPersistent());
    switch (networkProcessAccessType) {
    case ProcessAccessType::Launch:
        networkProcess();
        ASSERT(m_networkProcess);
        FALLTHROUGH;
    case ProcessAccessType::OnlyIfLaunched:
        if (m_networkProcess) {
            m_networkProcess->deleteWebsiteData(m_sessionID, dataTypes, modifiedSince, [callbackAggregator] { });
#if ENABLE(TRACKING_PREVENTION)
            didNotifyNetworkProcessToDeleteWebsiteData = true;
#endif
        }
        break;
    case ProcessAccessType::None:
        break;
    }

    auto webProcessAccessType = computeWebProcessAccessTypeForDataRemoval(dataTypes, !isPersistent());
    if (webProcessAccessType != ProcessAccessType::None) {
        for (auto& processPool : processPools()) {
            // Clear back/forward cache first as processes removed from the back/forward cache will likely
            // be added to the WebProcess cache.
            processPool->backForwardCache().removeEntriesForSession(sessionID());
            processPool->webProcessCache().clearAllProcessesForSession(sessionID());
        }

        for (auto& process : processes()) {
            if (webProcessAccessType == ProcessAccessType::OnlyIfLaunched && process.state() != WebProcessProxy::State::Running)
                continue;
            process.deleteWebsiteData(m_sessionID, dataTypes, modifiedSince, [callbackAggregator] { });
        }
    }

    if (dataTypes.contains(WebsiteDataType::DeviceIdHashSalt) || (dataTypes.contains(WebsiteDataType::Cookies)))
        m_deviceIdHashSaltStorage->deleteDeviceIdHashSaltOriginsModifiedSince(modifiedSince, [callbackAggregator] { });

    if (dataTypes.contains(WebsiteDataType::OfflineWebApplicationCache) && isPersistent()) {
        m_queue->dispatch([applicationCacheDirectory = m_configuration->applicationCacheDirectory().isolatedCopy(), applicationCacheFlatFileSubdirectoryName = m_configuration->applicationCacheFlatFileSubdirectoryName().isolatedCopy(), callbackAggregator] {
            auto storage = WebCore::ApplicationCacheStorage::create(applicationCacheDirectory, applicationCacheFlatFileSubdirectoryName);
            storage->deleteAllCaches();
        });
    }

    if (dataTypes.contains(WebsiteDataType::MediaKeys) && isPersistent()) {
        m_queue->dispatch([mediaKeysStorageDirectory = m_configuration->mediaKeysStorageDirectory().isolatedCopy(), callbackAggregator, modifiedSince] {
            removeMediaKeys(mediaKeysStorageDirectory, modifiedSince);
        });
    }

    if (dataTypes.contains(WebsiteDataType::SearchFieldRecentSearches) && isPersistent())
        removeRecentSearches(modifiedSince, [callbackAggregator] { });

#if ENABLE(TRACKING_PREVENTION)
    if (dataTypes.contains(WebsiteDataType::ResourceLoadStatistics)) {
        if (!didNotifyNetworkProcessToDeleteWebsiteData)
            protectedNetworkProcess()->deleteWebsiteData(m_sessionID, dataTypes, modifiedSince, [callbackAggregator] { });

        clearResourceLoadStatisticsInWebProcesses([callbackAggregator] { });
    }
#endif
}

void WebsiteDataStore::removeData(OptionSet<WebsiteDataType> dataTypes, const Vector<WebsiteDataRecord>& dataRecords, Function<void()>&& completionHandler)
{
    RELEASE_LOG(Storage, "WebsiteDataStore::removeData started to delete data for session %" PRIu64, m_sessionID.toUInt64());
    auto callbackAggregator = MainRunLoopCallbackAggregator::create([protectedThis = Ref { *this }, sessionID = m_sessionID, completionHandler = WTFMove(completionHandler)] {
#if RELEASE_LOG_DISABLED
        UNUSED_PARAM(sessionID);
#endif
        RELEASE_LOG(Storage, "WebsiteDataStore::removeData finished deleting data for session %" PRIu64, sessionID.toUInt64());
        completionHandler();
    });

    Vector<WebCore::SecurityOriginData> origins;
    for (const auto& dataRecord : dataRecords) {
        for (auto& origin : dataRecord.origins)
            origins.append(origin);
    }

    if (dataTypes.contains(WebsiteDataType::DiskCache)) {
        HashSet<WebCore::SecurityOriginData> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(crossThreadCopy(origin));
        }
        
#if ENABLE(VIDEO)
        m_queue->dispatch([origins = WTFMove(origins), mediaCacheDirectory = m_configuration->mediaCacheDirectory().isolatedCopy(), callbackAggregator] {
            WebCore::HTMLMediaElement::clearMediaCacheForOrigins(mediaCacheDirectory, origins);
        });
#endif
    }
    
    auto networkProcessAccessType = computeNetworkProcessAccessTypeForDataRemoval(dataTypes, !isPersistent());
    if (networkProcessAccessType != ProcessAccessType::None) {
        auto pools = networkProcessAccessType == ProcessAccessType::Launch ? ensureProcessPools() : processPools();
        for (auto& processPool : pools) {
            switch (networkProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                break;

            case ProcessAccessType::Launch:
                networkProcess();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            Vector<String> cookieHostNames;
            Vector<String> HSTSCacheHostNames;
            Vector<WebCore::RegistrableDomain> registrableDomains;
            for (const auto& dataRecord : dataRecords) {
                for (auto& hostName : dataRecord.cookieHostNames)
                    cookieHostNames.append(hostName);
                for (auto& hostName : dataRecord.HSTSCacheHostNames)
                    HSTSCacheHostNames.append(hostName);
#if ENABLE(TRACKING_PREVENTION)
                registrableDomains.reserveCapacity(registrableDomains.size() + dataRecord.resourceLoadStatisticsRegistrableDomains.size());
                for (auto& registrableDomain : dataRecord.resourceLoadStatisticsRegistrableDomains)
                    registrableDomains.uncheckedAppend(registrableDomain);
#endif
            }

            protectedNetworkProcess()->deleteWebsiteDataForOrigins(m_sessionID, dataTypes, origins, cookieHostNames, HSTSCacheHostNames, registrableDomains, [callbackAggregator, processPool] { });
        }
    }

    auto webProcessAccessType = computeWebProcessAccessTypeForDataRemoval(dataTypes, !isPersistent());
    if (webProcessAccessType != ProcessAccessType::None) {
        for (auto& process : processes()) {
            if (webProcessAccessType == ProcessAccessType::OnlyIfLaunched && process.state() != WebProcessProxy::State::Running)
                continue;
            process.deleteWebsiteDataForOrigins(m_sessionID, dataTypes, origins, [callbackAggregator] { });
        }
    }

    if (dataTypes.contains(WebsiteDataType::DeviceIdHashSalt) || (dataTypes.contains(WebsiteDataType::Cookies))) {
        m_deviceIdHashSaltStorage->deleteDeviceIdHashSaltForOrigins(origins, [callbackAggregator] { });
    }

    if (dataTypes.contains(WebsiteDataType::OfflineWebApplicationCache) && isPersistent()) {
        HashSet<WebCore::SecurityOriginData> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(crossThreadCopy(origin));
        }

        m_queue->dispatch([origins = WTFMove(origins), applicationCacheDirectory = m_configuration->applicationCacheDirectory().isolatedCopy(), applicationCacheFlatFileSubdirectoryName = m_configuration->applicationCacheFlatFileSubdirectoryName().isolatedCopy(), callbackAggregator] {
            auto storage = WebCore::ApplicationCacheStorage::create(applicationCacheDirectory, applicationCacheFlatFileSubdirectoryName);
            for (const auto& origin : origins)
                storage->deleteCacheForOrigin(origin);
        });
    }

    if (dataTypes.contains(WebsiteDataType::MediaKeys) && isPersistent()) {
        HashSet<WebCore::SecurityOriginData> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(crossThreadCopy(origin));
        }

        m_queue->dispatch([mediaKeysStorageDirectory = m_configuration->mediaKeysStorageDirectory().isolatedCopy(), callbackAggregator, origins = WTFMove(origins)] {
            removeMediaKeys(mediaKeysStorageDirectory, origins);
        });
    }
}

void WebsiteDataStore::setServiceWorkerTimeoutForTesting(Seconds seconds)
{
    protectedNetworkProcess()->sendSync(Messages::NetworkProcess::SetServiceWorkerFetchTimeoutForTesting(seconds), 0);
}

void WebsiteDataStore::resetServiceWorkerTimeoutForTesting()
{
    protectedNetworkProcess()->sendSync(Messages::NetworkProcess::ResetServiceWorkerFetchTimeoutForTesting(), 0);
}

bool WebsiteDataStore::hasServiceWorkerBackgroundActivityForTesting() const
{
#if ENABLE(SERVICE_WORKER)
    return WTF::anyOf(WebProcessPool::allProcessPools(), [](auto& pool) { return pool->hasServiceWorkerBackgroundActivityForTesting(); });
#else
    return false;
#endif
}

#if ENABLE(TRACKING_PREVENTION)
void WebsiteDataStore::setMaxStatisticsEntries(size_t maximumEntryCount, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setMaxStatisticsEntries(m_sessionID, maximumEntryCount, [callbackAggregator] { });
}

void WebsiteDataStore::setPruneEntriesDownTo(size_t pruneTargetCount, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setPruneEntriesDownTo(m_sessionID, pruneTargetCount, [callbackAggregator] { });
}

void WebsiteDataStore::setGrandfatheringTime(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setGrandfatheringTime(m_sessionID, seconds, [callbackAggregator] { });
}

void WebsiteDataStore::setMinimumTimeBetweenDataRecordsRemoval(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setMinimumTimeBetweenDataRecordsRemoval(m_sessionID, seconds, [callbackAggregator] { });
}
    
void WebsiteDataStore::dumpResourceLoadStatistics(CompletionHandler<void(const String&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    protectedNetworkProcess()->dumpResourceLoadStatistics(m_sessionID, WTFMove(completionHandler));
}

void WebsiteDataStore::isPrevalentResource(const URL& url, CompletionHandler<void(bool isPrevalent)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    protectedNetworkProcess()->isPrevalentResource(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
}

void WebsiteDataStore::isGrandfathered(const URL& url, CompletionHandler<void(bool isPrevalent)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    protectedNetworkProcess()->isGrandfathered(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
}

void WebsiteDataStore::setPrevalentResource(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setPrevalentResource(m_sessionID, WebCore::RegistrableDomain { url }, [callbackAggregator] { });
}

void WebsiteDataStore::setPrevalentResourceForDebugMode(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setPrevalentResourceForDebugMode(m_sessionID, WebCore::RegistrableDomain { url }, [callbackAggregator] { });
}

void WebsiteDataStore::isVeryPrevalentResource(const URL& url, CompletionHandler<void(bool isVeryPrevalent)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    protectedNetworkProcess()->isVeryPrevalentResource(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
}

void WebsiteDataStore::setVeryPrevalentResource(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setVeryPrevalentResource(m_sessionID, WebCore::RegistrableDomain { url }, [callbackAggregator] { });
}

void WebsiteDataStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setShouldClassifyResourcesBeforeDataRecordsRemoval(m_sessionID, value, [callbackAggregator] { });
}

void WebsiteDataStore::setSubframeUnderTopFrameDomain(const URL& subFrameURL, const URL& topFrameURL, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (subFrameURL.protocolIsAbout() || subFrameURL.isEmpty() || topFrameURL.protocolIsAbout() || topFrameURL.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    
    protectedNetworkProcess()->setSubframeUnderTopFrameDomain(m_sessionID, WebCore::RegistrableDomain { subFrameURL }, WebCore::RegistrableDomain { topFrameURL }, [callbackAggregator] { });
}

void WebsiteDataStore::isRegisteredAsSubFrameUnder(const URL& subFrameURL, const URL& topFrameURL, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    protectedNetworkProcess()->isRegisteredAsSubFrameUnder(m_sessionID, WebCore::RegistrableDomain { subFrameURL }, WebCore::RegistrableDomain { topFrameURL }, WTFMove(completionHandler));
}

void WebsiteDataStore::setSubresourceUnderTopFrameDomain(const URL& subresourceURL, const URL& topFrameURL, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (subresourceURL.protocolIsAbout() || subresourceURL.isEmpty() || topFrameURL.protocolIsAbout() || topFrameURL.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setSubresourceUnderTopFrameDomain(m_sessionID, WebCore::RegistrableDomain { subresourceURL }, WebCore::RegistrableDomain { topFrameURL }, [callbackAggregator] { });
}

void WebsiteDataStore::isRegisteredAsSubresourceUnder(const URL& subresourceURL, const URL& topFrameURL, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    protectedNetworkProcess()->isRegisteredAsSubresourceUnder(m_sessionID, WebCore::RegistrableDomain { subresourceURL }, WebCore::RegistrableDomain { topFrameURL }, WTFMove(completionHandler));
}

void WebsiteDataStore::setSubresourceUniqueRedirectTo(const URL& subresourceURL, const URL& urlRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (subresourceURL.protocolIsAbout() || subresourceURL.isEmpty() || urlRedirectedTo.protocolIsAbout() || urlRedirectedTo.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setSubresourceUniqueRedirectTo(m_sessionID, WebCore::RegistrableDomain { subresourceURL }, WebCore::RegistrableDomain { urlRedirectedTo }, [callbackAggregator] { });
}

void WebsiteDataStore::setSubresourceUniqueRedirectFrom(const URL& subresourceURL, const URL& urlRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (subresourceURL.protocolIsAbout() || subresourceURL.isEmpty() || urlRedirectedFrom.protocolIsAbout() || urlRedirectedFrom.isEmpty()) {
        completionHandler();
        return;
    }

    protectedNetworkProcess()->setSubresourceUniqueRedirectFrom(m_sessionID, WebCore::RegistrableDomain { subresourceURL }, WebCore::RegistrableDomain { urlRedirectedFrom }, WTFMove(completionHandler));
}

void WebsiteDataStore::setTopFrameUniqueRedirectTo(const URL& topFrameURL, const URL& urlRedirectedTo, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (topFrameURL.protocolIsAbout() || topFrameURL.isEmpty() || urlRedirectedTo.protocolIsAbout() || urlRedirectedTo.isEmpty()) {
        completionHandler();
        return;
    }

    protectedNetworkProcess()->setTopFrameUniqueRedirectTo(m_sessionID, WebCore::RegistrableDomain { topFrameURL }, WebCore::RegistrableDomain { urlRedirectedTo }, WTFMove(completionHandler));
}

void WebsiteDataStore::setTopFrameUniqueRedirectFrom(const URL& topFrameURL, const URL& urlRedirectedFrom, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (topFrameURL.protocolIsAbout() || topFrameURL.isEmpty() || urlRedirectedFrom.protocolIsAbout() || urlRedirectedFrom.isEmpty()) {
        completionHandler();
        return;
    }

    protectedNetworkProcess()->setTopFrameUniqueRedirectFrom(m_sessionID, WebCore::RegistrableDomain { topFrameURL }, WebCore::RegistrableDomain { urlRedirectedFrom }, WTFMove(completionHandler));
}

void WebsiteDataStore::isRegisteredAsRedirectingTo(const URL& urlRedirectedFrom, const URL& urlRedirectedTo, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    protectedNetworkProcess()->isRegisteredAsRedirectingTo(m_sessionID, WebCore::RegistrableDomain { urlRedirectedFrom }, WebCore::RegistrableDomain { urlRedirectedTo }, WTFMove(completionHandler));
}

void WebsiteDataStore::clearPrevalentResource(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
        
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }

    protectedNetworkProcess()->clearPrevalentResource(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
}

void WebsiteDataStore::resetParametersToDefaultValues(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    protectedNetworkProcess()->resetParametersToDefaultValues(m_sessionID, WTFMove(completionHandler));
}

void WebsiteDataStore::scheduleClearInMemoryAndPersistent(WallTime modifiedSince, ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    protectedNetworkProcess()->scheduleClearInMemoryAndPersistent(m_sessionID, modifiedSince, shouldGrandfather, WTFMove(completionHandler));
}

void WebsiteDataStore::getResourceLoadStatisticsDataSummary(CompletionHandler<void(Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    struct LocalCallbackAggregator : RefCounted<LocalCallbackAggregator> {
        LocalCallbackAggregator(CompletionHandler<void(Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&&)>&& completionHandler)
            : m_completionHandler(WTFMove(completionHandler))
        {
            ASSERT(RunLoop::isMain());
        };

        ~LocalCallbackAggregator()
        {
            ASSERT(RunLoop::isMain());

            m_completionHandler(WTFMove(m_results));
        }

        void addResult(Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&& results)
        {
            m_results.appendVector(WTFMove(results));
        }

        CompletionHandler<void(Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&&)> m_completionHandler;
        Vector<WebResourceLoadStatisticsStore::ThirdPartyData> m_results;
    };
    
    auto localCallbackAggregator = adoptRef(new LocalCallbackAggregator(WTFMove(completionHandler)));
    
    auto wtfCallbackAggregator = WTF::CallbackAggregator::create([this, protectedThis = Ref { *this }, localCallbackAggregator] {
        protectedNetworkProcess()->getResourceLoadStatisticsDataSummary(m_sessionID, [localCallbackAggregator](Vector<WebResourceLoadStatisticsStore::ThirdPartyData>&& data) {
            localCallbackAggregator->addResult(WTFMove(data));
        });
    });
    
    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->sendResourceLoadStatisticsDataImmediately([wtfCallbackAggregator] { });
}

void WebsiteDataStore::scheduleClearInMemoryAndPersistent(ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    protectedNetworkProcess()->scheduleClearInMemoryAndPersistent(m_sessionID, { }, shouldGrandfather, WTFMove(completionHandler));
}

void WebsiteDataStore::scheduleCookieBlockingUpdate(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->scheduleCookieBlockingUpdate(m_sessionID, [callbackAggregator] { });
}

void WebsiteDataStore::scheduleStatisticsAndDataRecordsProcessing(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->scheduleStatisticsAndDataRecordsProcessing(m_sessionID, [callbackAggregator] { });
}

void WebsiteDataStore::statisticsDatabaseHasAllTables(CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    protectedNetworkProcess()->statisticsDatabaseHasAllTables(m_sessionID, WTFMove(completionHandler));
}

void WebsiteDataStore::setLastSeen(const URL& url, Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setLastSeen(m_sessionID, WebCore::RegistrableDomain { url }, seconds, [callbackAggregator] { });
}

void WebsiteDataStore::domainIDExistsInDatabase(int domainID, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    protectedNetworkProcess()->domainIDExistsInDatabase(m_sessionID, domainID, WTFMove(completionHandler));
}

void WebsiteDataStore::mergeStatisticForTesting(const URL& url, const URL& topFrameUrl1, const URL& topFrameUrl2, Seconds lastSeen, bool hadUserInteraction, Seconds mostRecentUserInteraction, bool isGrandfathered, bool isPrevalent, bool isVeryPrevalent, unsigned dataRecordsRemoved, CompletionHandler<void()>&& completionHandler)
{
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->mergeStatisticForTesting(m_sessionID, WebCore::RegistrableDomain { url }, WebCore::RegistrableDomain { topFrameUrl1 }, WebCore::RegistrableDomain { topFrameUrl2 }, lastSeen, hadUserInteraction, mostRecentUserInteraction, isGrandfathered, isPrevalent, isVeryPrevalent, dataRecordsRemoved, [callbackAggregator] { });
}

void WebsiteDataStore::insertExpiredStatisticForTesting(const URL& url, unsigned numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, CompletionHandler<void()>&& completionHandler)
{
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->insertExpiredStatisticForTesting(m_sessionID, WebCore::RegistrableDomain { url }, numberOfOperatingDaysPassed, hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent, [callbackAggregator] { });
}

void WebsiteDataStore::setNotifyPagesWhenDataRecordsWereScanned(bool value, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setNotifyPagesWhenDataRecordsWereScanned(m_sessionID, value, [callbackAggregator] { });
}

void WebsiteDataStore::setResourceLoadStatisticsTimeAdvanceForTesting(Seconds time, CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->setResourceLoadStatisticsTimeAdvanceForTesting(m_sessionID, time, WTFMove(completionHandler));
}

void WebsiteDataStore::setIsRunningResourceLoadStatisticsTest(bool value, CompletionHandler<void()>&& completionHandler)
{
    useExplicitTrackingPreventionState();
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setIsRunningResourceLoadStatisticsTest(m_sessionID, value, [callbackAggregator] { });
}

void WebsiteDataStore::getAllStorageAccessEntries(WebPageProxyIdentifier pageID, CompletionHandler<void(Vector<String>&& domains)>&& completionHandler)
{
    auto page = WebProcessProxy::webPage(pageID);
    if (!page) {
        completionHandler({ });
        return;
    }

    protectedNetworkProcess()->getAllStorageAccessEntries(m_sessionID, WTFMove(completionHandler));
}


void WebsiteDataStore::setTimeToLiveUserInteraction(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setTimeToLiveUserInteraction(m_sessionID, seconds, [callbackAggregator] { });
}

void WebsiteDataStore::logUserInteraction(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->logUserInteraction(m_sessionID, WebCore::RegistrableDomain { url }, [callbackAggregator] { });
}

void WebsiteDataStore::hasHadUserInteraction(const URL& url, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler(false);
        return;
    }

    protectedNetworkProcess()->hasHadUserInteraction(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
}

void WebsiteDataStore::isRelationshipOnlyInDatabaseOnce(const URL& subUrl, const URL& topUrl, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (subUrl.protocolIsAbout() || subUrl.isEmpty() || topUrl.protocolIsAbout() || topUrl.isEmpty()) {
        completionHandler(false);
        return;
    }

    protectedNetworkProcess()->isRelationshipOnlyInDatabaseOnce(m_sessionID, WebCore::RegistrableDomain { subUrl }, WebCore::RegistrableDomain { topUrl }, WTFMove(completionHandler));
}

void WebsiteDataStore::clearUserInteraction(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->clearUserInteraction(m_sessionID, WebCore::RegistrableDomain { url }, [callbackAggregator] { });
}

void WebsiteDataStore::setGrandfathered(const URL& url, bool isGrandfathered, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setGrandfathered(m_sessionID, WebCore::RegistrableDomain { url }, isGrandfathered, [callbackAggregator] { });
}

void WebsiteDataStore::setCrossSiteLoadWithLinkDecorationForTesting(const URL& fromURL, const URL& toURL, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setCrossSiteLoadWithLinkDecorationForTesting(m_sessionID, WebCore::RegistrableDomain { fromURL }, WebCore::RegistrableDomain { toURL }, [callbackAggregator] { });
}

void WebsiteDataStore::resetCrossSiteLoadsWithLinkDecorationForTesting(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->resetCrossSiteLoadsWithLinkDecorationForTesting(m_sessionID, [callbackAggregator] { });
}

void WebsiteDataStore::deleteCookiesForTesting(const URL& url, bool includeHttpOnlyCookies, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->deleteCookiesForTesting(m_sessionID, WebCore::RegistrableDomain { url }, includeHttpOnlyCookies, [callbackAggregator] { });
}

void WebsiteDataStore::hasLocalStorageForTesting(const URL& url, CompletionHandler<void(bool)>&& completionHandler) const
{
    protectedNetworkProcess()->hasLocalStorage(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
}

void WebsiteDataStore::hasIsolatedSessionForTesting(const URL& url, CompletionHandler<void(bool)>&& completionHandler) const
{
    protectedNetworkProcess()->hasIsolatedSession(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
}

void WebsiteDataStore::setResourceLoadStatisticsShouldDowngradeReferrerForTesting(bool enabled, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setShouldDowngradeReferrerForTesting(enabled, [callbackAggregator] { });
}

#if !PLATFORM(COCOA)
WebCore::ThirdPartyCookieBlockingMode WebsiteDataStore::thirdPartyCookieBlockingMode() const
{
    if (!m_thirdPartyCookieBlockingMode)
        m_thirdPartyCookieBlockingMode = WebCore::ThirdPartyCookieBlockingMode::All;
    return *m_thirdPartyCookieBlockingMode;
}
#endif

void WebsiteDataStore::setResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(bool enabled, bool onlyOnSitesWithoutUserInteraction, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    WebCore::ThirdPartyCookieBlockingMode blockingMode = WebCore::ThirdPartyCookieBlockingMode::OnlyAccordingToPerDomainPolicy;
    if (enabled)
        blockingMode = onlyOnSitesWithoutUserInteraction ? WebCore::ThirdPartyCookieBlockingMode::AllOnSitesWithoutUserInteraction : WebCore::ThirdPartyCookieBlockingMode::All;
    setThirdPartyCookieBlockingMode(blockingMode, WTFMove(completionHandler));
}

void WebsiteDataStore::setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode blockingMode, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    if (thirdPartyCookieBlockingMode() != blockingMode) {
        m_thirdPartyCookieBlockingMode = blockingMode;
        for (auto& webProcess : processes())
            webProcess.setThirdPartyCookieBlockingMode(blockingMode, [callbackAggregator] { });
    }

    protectedNetworkProcess()->setThirdPartyCookieBlockingMode(m_sessionID, blockingMode, [callbackAggregator] { });
}

void WebsiteDataStore::setResourceLoadStatisticsShouldEnbleSameSiteStrictEnforcementForTesting(bool enabled, CompletionHandler<void()>&& completionHandler)
{
    auto flag = enabled ? WebCore::SameSiteStrictEnforcementEnabled::Yes : WebCore::SameSiteStrictEnforcementEnabled::No;

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    protectedNetworkProcess()->setShouldEnbleSameSiteStrictEnforcementForTesting(m_sessionID, flag, [callbackAggregator] { });
}

void WebsiteDataStore::setResourceLoadStatisticsFirstPartyWebsiteDataRemovalModeForTesting(bool enabled, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    auto mode = enabled ? WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookies : WebCore::FirstPartyWebsiteDataRemovalMode::None;

    protectedNetworkProcess()->setFirstPartyWebsiteDataRemovalModeForTesting(m_sessionID, mode, [callbackAggregator] { });
}

void WebsiteDataStore::setResourceLoadStatisticsToSameSiteStrictCookiesForTesting(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setToSameSiteStrictCookiesForTesting(m_sessionID, WebCore::RegistrableDomain { url }, [callbackAggregator] { });
}

void WebsiteDataStore::setResourceLoadStatisticsFirstPartyHostCNAMEDomainForTesting(const URL& firstPartyURL, const URL& cnameURL, CompletionHandler<void()>&& completionHandler)
{
    if (cnameURL.host() != "testwebkit.org"_s && cnameURL.host() != "3rdpartytestwebkit.org"_s) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setFirstPartyHostCNAMEDomainForTesting(m_sessionID, firstPartyURL.host().toString(), WebCore::RegistrableDomain { cnameURL }, [callbackAggregator] { });
}

void WebsiteDataStore::setResourceLoadStatisticsThirdPartyCNAMEDomainForTesting(const URL& cnameURL, CompletionHandler<void()>&& completionHandler)
{
    if (cnameURL.host() != "testwebkit.org"_s && cnameURL.host() != "3rdpartytestwebkit.org"_s) {
        completionHandler();
        return;
    }

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setThirdPartyCNAMEDomainForTesting(m_sessionID, WebCore::RegistrableDomain { cnameURL }, [callbackAggregator] { });
}
#endif // ENABLE(TRACKING_PREVENTION)

void WebsiteDataStore::setCachedProcessSuspensionDelayForTesting(Seconds delay)
{
    WebProcessCache::setCachedProcessSuspensionDelayForTesting(delay);
}

void WebsiteDataStore::syncLocalStorage(CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::SyncLocalStorage(), WTFMove(completionHandler));
}

void WebsiteDataStore::storeServiceWorkerRegistrations(CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::StoreServiceWorkerRegistrations(m_sessionID), WTFMove(completionHandler));
}

void WebsiteDataStore::setCacheMaxAgeCapForPrevalentResources(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(TRACKING_PREVENTION)
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setCacheMaxAgeCapForPrevalentResources(m_sessionID, seconds, [callbackAggregator] { });
#else
    UNUSED_PARAM(seconds);
    completionHandler();
#endif
}

void WebsiteDataStore::resetCacheMaxAgeCapForPrevalentResources(CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(TRACKING_PREVENTION)
    protectedNetworkProcess()->resetCacheMaxAgeCapForPrevalentResources(m_sessionID, WTFMove(completionHandler));
#else
    completionHandler();
#endif
}

HashSet<RefPtr<WebProcessPool>> WebsiteDataStore::processPools(size_t limit) const
{
    HashSet<RefPtr<WebProcessPool>> processPools;
    for (auto& process : processes()) {
        if (auto* processPool = process.processPoolIfExists()) {
            processPools.add(processPool);
            if (processPools.size() == limit)
                break;
        }
    }

    if (processPools.isEmpty()) {
        // Check if we're one of the legacy data stores.
        for (auto& processPool : WebProcessPool::allProcessPools()) {
            processPools.add(processPool.ptr());
            if (processPools.size() == limit)
                break;
        }
    }

    return processPools;
}

HashSet<RefPtr<WebProcessPool>> WebsiteDataStore::ensureProcessPools() const
{
    auto processPools = this->processPools();
    if (processPools.isEmpty())
        processPools.add(WebProcessPool::create(API::ProcessPoolConfiguration::create()));
    return processPools;
}

static String computeMediaKeyFile(const String& mediaKeyDirectory)
{
    return FileSystem::pathByAppendingComponent(mediaKeyDirectory, "SecureStop.plist"_s);
}

#if !PLATFORM(COCOA)
void WebsiteDataStore::allowSpecificHTTPSCertificateForHost(const WebCore::CertificateInfo& certificate, const String& host)
{
    protectedNetworkProcess()->send(Messages::NetworkProcess::AllowSpecificHTTPSCertificateForHost(sessionID(), certificate, host), 0);
}
#endif

void WebsiteDataStore::allowTLSCertificateChainForLocalPCMTesting(const WebCore::CertificateInfo& certificate)
{
    protectedNetworkProcess()->send(Messages::NetworkProcess::AllowTLSCertificateChainForLocalPCMTesting(sessionID(), certificate), 0);
}

Vector<WebCore::SecurityOriginData> WebsiteDataStore::mediaKeyOrigins(const String& mediaKeysStorageDirectory)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    Vector<WebCore::SecurityOriginData> origins;

    for (const auto& mediaKeyIdentifier : FileSystem::listDirectory(mediaKeysStorageDirectory)) {
        auto originPath = FileSystem::pathByAppendingComponent(mediaKeysStorageDirectory, mediaKeyIdentifier);
        auto mediaKeyFile = computeMediaKeyFile(originPath);
        if (!FileSystem::fileExists(mediaKeyFile))
            continue;

        if (auto securityOrigin = WebCore::SecurityOriginData::fromDatabaseIdentifier(mediaKeyIdentifier))
            origins.append(*securityOrigin);
    }

    return origins;
}

void WebsiteDataStore::removeMediaKeys(const String& mediaKeysStorageDirectory, WallTime modifiedSince)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    for (const auto& directoryName : FileSystem::listDirectory(mediaKeysStorageDirectory)) {
        auto mediaKeyDirectory = FileSystem::pathByAppendingComponent(mediaKeysStorageDirectory, directoryName);
        auto mediaKeyFile = computeMediaKeyFile(mediaKeyDirectory);

        auto modificationTime = FileSystem::fileModificationTime(mediaKeyFile);
        if (!modificationTime)
            continue;

        if (modificationTime.value() < modifiedSince)
            continue;

        FileSystem::deleteFile(mediaKeyFile);
        FileSystem::deleteEmptyDirectory(mediaKeyDirectory);
    }
}

void WebsiteDataStore::removeMediaKeys(const String& mediaKeysStorageDirectory, const HashSet<WebCore::SecurityOriginData>& origins)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    for (const auto& origin : origins) {
        auto mediaKeyDirectory = FileSystem::pathByAppendingComponent(mediaKeysStorageDirectory, origin.databaseIdentifier());
        auto mediaKeyFile = computeMediaKeyFile(mediaKeyDirectory);

        FileSystem::deleteFile(mediaKeyFile);
        FileSystem::deleteEmptyDirectory(mediaKeyDirectory);
    }
}

void WebsiteDataStore::getNetworkProcessConnection(WebProcessProxy& webProcessProxy, CompletionHandler<void(NetworkProcessConnectionInfo&&)>&& reply, ShouldRetryOnFailure shouldRetryOnFailure)
{
    Ref networkProcessProxy = networkProcess();
    networkProcessProxy->getNetworkProcessConnection(webProcessProxy, [weakThis = WeakPtr { *this }, networkProcessProxy = WeakPtr { networkProcessProxy }, webProcessProxy = WeakPtr { webProcessProxy }, reply = WTFMove(reply), shouldRetryOnFailure] (auto&& connectionInfo) mutable {
        if (UNLIKELY(!connectionInfo.connection)) {
            if (shouldRetryOnFailure == ShouldRetryOnFailure::No || !webProcessProxy) {
                RELEASE_LOG_ERROR(Process, "getNetworkProcessConnection: Failed to get connection to network process, will reply invalid identifier ...");
                reply({ });
                return;
            }

            // Retry on the next RunLoop iteration because we may be inside the WebsiteDataStore destructor.
            RunLoop::main().dispatch([weakThis = WTFMove(weakThis), networkProcessProxy = WTFMove(networkProcessProxy), webProcessProxy = WTFMove(webProcessProxy), reply = WTFMove(reply)] () mutable {
                if (RefPtr<WebsiteDataStore> strongThis = weakThis.get(); strongThis && webProcessProxy) {
                    // Terminate if it is the same network process.
                    if (networkProcessProxy && strongThis->m_networkProcess == networkProcessProxy.get())
                        strongThis->terminateNetworkProcess();
                    RELEASE_LOG_ERROR(Process, "getNetworkProcessConnection: Failed to get connection to network process, will retry ...");
                    strongThis->getNetworkProcessConnection(*webProcessProxy, WTFMove(reply), ShouldRetryOnFailure::No);
                    return;
                }

                RELEASE_LOG_ERROR(Process, "getNetworkProcessConnection: Failed to get connection to network process, will reply invalid identifier ...");
                reply({ });
            });
            return;
        }

        reply(WTFMove(connectionInfo));
    });
}

void WebsiteDataStore::networkProcessDidTerminate(NetworkProcessProxy& networkProcess)
{
    ASSERT(!m_networkProcess || m_networkProcess == &networkProcess);
    m_networkProcess = nullptr;

    if (auto completionHandler = std::exchange(m_completionHandlerForRemovalFromNetworkProcess, { }))
        completionHandler("Network process is terminated"_s);
}

void WebsiteDataStore::terminateNetworkProcess()
{
    if (auto networkProcess = std::exchange(m_networkProcess, nullptr))
        networkProcess->requestTermination();
}

void WebsiteDataStore::sendNetworkProcessPrepareToSuspendForTesting(CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->sendPrepareToSuspend(IsSuspensionImminent::No, 0.0, WTFMove(completionHandler));
}

void WebsiteDataStore::sendNetworkProcessWillSuspendImminentlyForTesting()
{
    protectedNetworkProcess()->sendProcessWillSuspendImminentlyForTesting();
}

void WebsiteDataStore::sendNetworkProcessDidResume()
{
    protectedNetworkProcess()->sendProcessDidResume(ProcessThrottlerClient::ResumeReason::ForegroundActivity);
}

bool WebsiteDataStore::trackingPreventionEnabled() const
{
#if ENABLE(TRACKING_PREVENTION)
    return m_trackingPreventionEnabled;
#else
    return false;
#endif
}

bool WebsiteDataStore::resourceLoadStatisticsDebugMode() const
{
#if ENABLE(TRACKING_PREVENTION)
    return m_trackingPreventionDebugMode;
#else
    return false;
#endif
}

void WebsiteDataStore::setTrackingPreventionEnabled(bool enabled)
{
#if ENABLE(TRACKING_PREVENTION)
    if (enabled == trackingPreventionEnabled())
        return;

    if (enabled) {
        m_trackingPreventionEnabled = true;
        
        resolveDirectoriesIfNecessary();
        
        if (RefPtr networkProcessProxy = m_networkProcess)
            networkProcessProxy->send(Messages::NetworkProcess::SetTrackingPreventionEnabled(m_sessionID, true), 0);
        for (auto& processPool : processPools())
            processPool->sendToAllProcessesForSession(Messages::WebProcess::SetTrackingPreventionEnabled(true), m_sessionID);
        return;
    }

    if (RefPtr networkProcessProxy = m_networkProcess)
        networkProcessProxy->send(Messages::NetworkProcess::SetTrackingPreventionEnabled(m_sessionID, false), 0);
    for (auto& processPool : processPools())
        processPool->sendToAllProcessesForSession(Messages::WebProcess::SetTrackingPreventionEnabled(false), m_sessionID);

    m_trackingPreventionEnabled = false;
#else
    UNUSED_PARAM(enabled);
#endif
}

#if ENABLE(TRACKING_PREVENTION)
void WebsiteDataStore::setStatisticsTestingCallback(Function<void(const String&)>&& callback)
{
    if (callback)
        protectedNetworkProcess()->send(Messages::NetworkProcess::SetResourceLoadStatisticsLogTestingEvent(true), 0);
    
    m_statisticsTestingCallback = WTFMove(callback);
}
#endif

void WebsiteDataStore::setResourceLoadStatisticsDebugMode(bool enabled)
{
    setResourceLoadStatisticsDebugMode(enabled, []() { });
}

void WebsiteDataStore::setResourceLoadStatisticsDebugMode(bool enabled, CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(TRACKING_PREVENTION)
    m_trackingPreventionDebugMode = enabled;

    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setResourceLoadStatisticsDebugMode(m_sessionID, enabled, [callbackAggregator] { });
#else
    UNUSED_PARAM(enabled);
    UNUSED_PARAM(completionHandler);
#endif
}

void WebsiteDataStore::isResourceLoadStatisticsEphemeral(CompletionHandler<void(bool)>&& completionHandler) const
{
#if ENABLE(TRACKING_PREVENTION)
    if (!trackingPreventionEnabled() || !m_sessionID.isEphemeral()) {
        completionHandler(false);
        return;
    }

    protectedNetworkProcess()->isResourceLoadStatisticsEphemeral(m_sessionID, WTFMove(completionHandler));
#else
    completionHandler(false);
#endif
}

void WebsiteDataStore::setPrivateClickMeasurementDebugMode(bool enabled)
{
    protectedNetworkProcess()->setPrivateClickMeasurementDebugMode(sessionID(), enabled);
}

void WebsiteDataStore::storePrivateClickMeasurement(const WebCore::PrivateClickMeasurement& privateClickMeasurement)
{
    protectedNetworkProcess()->send(Messages::NetworkProcess::StorePrivateClickMeasurement(sessionID(), privateClickMeasurement), 0);
}

void WebsiteDataStore::closeDatabases(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::ClosePCMDatabase(m_sessionID), [callbackAggregator] { });

#if ENABLE(TRACKING_PREVENTION)
    protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::CloseITPDatabase(m_sessionID), [callbackAggregator] { });
#endif
}

#if ENABLE(TRACKING_PREVENTION)
void WebsiteDataStore::logTestingEvent(const String& event)
{
    ASSERT(RunLoop::isMain());
    
    if (m_statisticsTestingCallback)
        m_statisticsTestingCallback(event);
}

void WebsiteDataStore::clearResourceLoadStatisticsInWebProcesses(CompletionHandler<void()>&& callback)
{
    if (trackingPreventionEnabled()) {
        for (auto& processPool : processPools())
            processPool->sendToAllProcessesForSession(Messages::WebProcess::ClearResourceLoadStatistics(), m_sessionID);
    }
    callback();
}
#endif

bool WebsiteDataStore::isBlobRegistryPartitioningEnabled() const
{
    return WTF::anyOf(m_processes, [] (const WebProcessProxy& process) {
        return WTF::anyOf(process.pages(), [](const auto& page) {
            return page && page->preferences().blobRegistryTopOriginPartitioningEnabled();
        });
    });
}

void WebsiteDataStore::setAllowsAnySSLCertificateForWebSocket(bool allows)
{
    protectedNetworkProcess()->sendSync(Messages::NetworkProcess::SetAllowsAnySSLCertificateForWebSocket(allows), 0);
}

void WebsiteDataStore::dispatchOnQueue(Function<void()>&& function)
{
    m_queue->dispatch(WTFMove(function));
}

void WebsiteDataStore::setCacheModelSynchronouslyForTesting(CacheModel cacheModel)
{
    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->setCacheModelSynchronouslyForTesting(cacheModel);
}

Vector<WebsiteDataStoreParameters> WebsiteDataStore::parametersFromEachWebsiteDataStore()
{
    return WTF::map(allDataStores(), [](auto& entry) {
        return entry.value->parameters();
    });
}

void WebsiteDataStore::createHandleFromResolvedPathIfPossible(const String& resolvedPath, SandboxExtension::Handle& handle, SandboxExtension::Type type)
{
    if (resolvedPath.isEmpty())
        return;

    if (auto newHandle = SandboxExtension::createHandleWithoutResolvingPath(resolvedPath, type))
        handle = WTFMove(*newHandle);
}

WebsiteDataStoreParameters WebsiteDataStore::parameters()
{
    WebsiteDataStoreParameters parameters;
    resolveDirectoriesIfNecessary();

    auto resourceLoadStatisticsDirectory = resolvedResourceLoadStatisticsDirectory();
    SandboxExtension::Handle resourceLoadStatisticsDirectoryHandle;
    createHandleFromResolvedPathIfPossible(resourceLoadStatisticsDirectory, resourceLoadStatisticsDirectoryHandle);

    auto networkCacheDirectory = resolvedNetworkCacheDirectory();
    SandboxExtension::Handle networkCacheDirectoryExtensionHandle;
    createHandleFromResolvedPathIfPossible(networkCacheDirectory, networkCacheDirectoryExtensionHandle);

    auto hstsStorageDirectory = resolvedHSTSStorageDirectory();
    SandboxExtension::Handle hstsStorageDirectoryExtensionHandle;
    createHandleFromResolvedPathIfPossible(hstsStorageDirectory, hstsStorageDirectoryExtensionHandle);

    bool shouldIncludeLocalhostInResourceLoadStatistics = false;
    auto firstPartyWebsiteDataRemovalMode = WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookies;
    WebCore::RegistrableDomain standaloneApplicationDomain;
    HashSet<WebCore::RegistrableDomain> appBoundDomains;
#if ENABLE(APP_BOUND_DOMAINS)
    if (isAppBoundITPRelaxationEnabled)
        appBoundDomains = valueOrDefault(appBoundDomainsIfInitialized());
#endif
    HashSet<WebCore::RegistrableDomain> managedDomains;
#if ENABLE(MANAGED_DOMAINS)
    auto managedDomainsOptional = managedDomainsIfInitialized();
    if (managedDomainsOptional)
        managedDomains = *managedDomainsOptional;
#endif
    WebCore::RegistrableDomain resourceLoadStatisticsManualPrevalentResource;
    ResourceLoadStatisticsParameters resourceLoadStatisticsParameters = {
        WTFMove(resourceLoadStatisticsDirectory),
        WTFMove(resourceLoadStatisticsDirectoryHandle),
        trackingPreventionEnabled(),
#if ENABLE(TRACKING_PREVENTION)
        isTrackingPreventionStateExplicitlySet(),
        hasStatisticsTestingCallback(),
#else
        false,
        false,
#endif
        shouldIncludeLocalhostInResourceLoadStatistics,
        resourceLoadStatisticsDebugMode(),
#if ENABLE(TRACKING_PREVENTION)
        thirdPartyCookieBlockingMode(),
        WebCore::SameSiteStrictEnforcementEnabled::No,
#endif
        firstPartyWebsiteDataRemovalMode,
        WTFMove(standaloneApplicationDomain),
        WTFMove(appBoundDomains),
        WTFMove(managedDomains),
        WTFMove(resourceLoadStatisticsManualPrevalentResource),
    };

    NetworkSessionCreationParameters networkSessionParameters;
    networkSessionParameters.sessionID = m_sessionID;
    networkSessionParameters.dataStoreIdentifier = configuration().identifier();
    networkSessionParameters.boundInterfaceIdentifier = configuration().boundInterfaceIdentifier();
    networkSessionParameters.allowsCellularAccess = configuration().allowsCellularAccess() ? AllowsCellularAccess::Yes : AllowsCellularAccess::No;
    networkSessionParameters.deviceManagementRestrictionsEnabled = m_configuration->deviceManagementRestrictionsEnabled();
    networkSessionParameters.allLoadsBlockedByDeviceManagementRestrictionsForTesting = m_configuration->allLoadsBlockedByDeviceManagementRestrictionsForTesting();
    networkSessionParameters.webPushDaemonConnectionConfiguration = m_configuration->webPushDaemonConnectionConfiguration();
    networkSessionParameters.networkCacheDirectory = WTFMove(networkCacheDirectory);
    networkSessionParameters.networkCacheDirectoryExtensionHandle = WTFMove(networkCacheDirectoryExtensionHandle);
    networkSessionParameters.hstsStorageDirectory = WTFMove(hstsStorageDirectory);
    networkSessionParameters.hstsStorageDirectoryExtensionHandle = WTFMove(hstsStorageDirectoryExtensionHandle);
    networkSessionParameters.dataConnectionServiceType = m_configuration->dataConnectionServiceType();
    networkSessionParameters.fastServerTrustEvaluationEnabled = m_configuration->fastServerTrustEvaluationEnabled();
    networkSessionParameters.networkCacheSpeculativeValidationEnabled = m_configuration->networkCacheSpeculativeValidationEnabled();
    networkSessionParameters.shouldUseTestingNetworkSession = m_configuration->testingSessionEnabled();
    networkSessionParameters.staleWhileRevalidateEnabled = m_configuration->staleWhileRevalidateEnabled();
    networkSessionParameters.testSpeedMultiplier = m_configuration->testSpeedMultiplier();
    networkSessionParameters.suppressesConnectionTerminationOnSystemChange = m_configuration->suppressesConnectionTerminationOnSystemChange();
    networkSessionParameters.allowsServerPreconnect = m_configuration->allowsServerPreconnect();
    networkSessionParameters.resourceLoadStatisticsParameters = WTFMove(resourceLoadStatisticsParameters);
    networkSessionParameters.requiresSecureHTTPSProxyConnection = m_configuration->requiresSecureHTTPSProxyConnection();
    networkSessionParameters.shouldRunServiceWorkersOnMainThreadForTesting = m_configuration->shouldRunServiceWorkersOnMainThreadForTesting();
    networkSessionParameters.overrideServiceWorkerRegistrationCountTestingValue = m_configuration->overrideServiceWorkerRegistrationCountTestingValue();
    networkSessionParameters.preventsSystemHTTPProxyAuthentication = m_configuration->preventsSystemHTTPProxyAuthentication();
    networkSessionParameters.allowsHSTSWithUntrustedRootCertificate = m_configuration->allowsHSTSWithUntrustedRootCertificate();
    networkSessionParameters.pcmMachServiceName = m_configuration->pcmMachServiceName();
    networkSessionParameters.webPushMachServiceName = m_configuration->webPushMachServiceName();
    networkSessionParameters.webPushPartitionString = m_configuration->webPushPartitionString();
#if !HAVE(NSURLSESSION_WEBSOCKET)
    networkSessionParameters.shouldAcceptInsecureCertificatesForWebSockets = m_configuration->shouldAcceptInsecureCertificatesForWebSockets();
#endif
    networkSessionParameters.isBlobRegistryTopOriginPartitioningEnabled = isBlobRegistryPartitioningEnabled();
    networkSessionParameters.unifiedOriginStorageLevel = m_configuration->unifiedOriginStorageLevel();
    networkSessionParameters.perOriginStorageQuota = perOriginStorageQuota();
    networkSessionParameters.originQuotaRatio = originQuotaRatio();
    networkSessionParameters.totalQuotaRatio = m_configuration->totalQuotaRatio();
    networkSessionParameters.standardVolumeCapacity = m_configuration->standardVolumeCapacity();
    networkSessionParameters.volumeCapacityOverride = m_configuration->volumeCapacityOverride();
    networkSessionParameters.localStorageDirectory = resolvedLocalStorageDirectory();
    createHandleFromResolvedPathIfPossible(networkSessionParameters.localStorageDirectory, networkSessionParameters.localStorageDirectoryExtensionHandle);
    networkSessionParameters.indexedDBDirectory = resolvedIndexedDBDatabaseDirectory();
    createHandleFromResolvedPathIfPossible(networkSessionParameters.indexedDBDirectory, networkSessionParameters.indexedDBDirectoryExtensionHandle);
    networkSessionParameters.generalStorageDirectory = resolvedGeneralStorageDirectory();
    createHandleFromResolvedPathIfPossible(networkSessionParameters.generalStorageDirectory, networkSessionParameters.generalStorageDirectoryHandle);
    networkSessionParameters.cacheStorageDirectory = resolvedCacheStorageDirectory();
    createHandleFromResolvedPathIfPossible(networkSessionParameters.cacheStorageDirectory, networkSessionParameters.cacheStorageDirectoryExtensionHandle);

#if ENABLE(SERVICE_WORKER)
    networkSessionParameters.serviceWorkerRegistrationDirectory = resolvedServiceWorkerRegistrationDirectory();
    createHandleFromResolvedPathIfPossible(networkSessionParameters.serviceWorkerRegistrationDirectory, networkSessionParameters.serviceWorkerRegistrationDirectoryExtensionHandle);
    networkSessionParameters.serviceWorkerProcessTerminationDelayEnabled = m_configuration->serviceWorkerProcessTerminationDelayEnabled();
    networkSessionParameters.inspectionForServiceWorkersAllowed = m_inspectionForServiceWorkersAllowed;
#endif
    parameters.networkSessionParameters = WTFMove(networkSessionParameters);
#if ENABLE(TRACKING_PREVENTION)
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.enabled = m_trackingPreventionEnabled;
#endif
    platformSetNetworkParameters(parameters);
#if PLATFORM(COCOA)
    parameters.networkSessionParameters.appHasRequestedCrossWebsiteTrackingPermission = hasRequestedCrossWebsiteTrackingPermission();
    parameters.networkSessionParameters.useNetworkLoader = useNetworkLoader();
#endif

#if PLATFORM(IOS_FAMILY)
    if (isPersistent()) {
        SandboxExtension::Handle cookieStorageDirectoryExtensionHandle;
        createHandleFromResolvedPathIfPossible(resolvedCookieStorageDirectory(), cookieStorageDirectoryExtensionHandle);
        parameters.cookieStorageDirectoryExtensionHandle = WTFMove(cookieStorageDirectoryExtensionHandle);

        SandboxExtension::Handle containerCachesDirectoryExtensionHandle;
        createHandleFromResolvedPathIfPossible(resolvedContainerCachesNetworkingDirectory(), containerCachesDirectoryExtensionHandle);
        parameters.containerCachesDirectoryExtensionHandle = WTFMove(containerCachesDirectoryExtensionHandle);

        SandboxExtension::Handle parentBundleDirectoryExtensionHandle;
        createHandleFromResolvedPathIfPossible(parentBundleDirectory(), parentBundleDirectoryExtensionHandle, SandboxExtension::Type::ReadOnly);
        parameters.parentBundleDirectoryExtensionHandle = WTFMove(parentBundleDirectoryExtensionHandle);

        if (auto handleAndFilePath = SandboxExtension::createHandleForTemporaryFile(emptyString(), SandboxExtension::Type::ReadWrite))
            parameters.tempDirectoryExtensionHandle = WTFMove(handleAndFilePath->first);
    }
#endif
    
    return parameters;
}

#if HAVE(SEC_KEY_PROXY)
void WebsiteDataStore::addSecKeyProxyStore(Ref<SecKeyProxyStore>&& store)
{
    m_secKeyProxyStores.append(WTFMove(store));
}
#endif

#if ENABLE(WEB_AUTHN)
void WebsiteDataStore::setMockWebAuthenticationConfiguration(WebCore::MockWebAuthenticationConfiguration&& configuration)
{
    if (!m_authenticatorManager->isMock()) {
        m_authenticatorManager = makeUniqueRef<MockAuthenticatorManager>(WTFMove(configuration));
        return;
    }
    static_cast<MockAuthenticatorManager*>(&m_authenticatorManager)->setTestConfiguration(WTFMove(configuration));
}

VirtualAuthenticatorManager& WebsiteDataStore::virtualAuthenticatorManager()
{
    if (!m_authenticatorManager->isVirtual())
        m_authenticatorManager = makeUniqueRef<VirtualAuthenticatorManager>();
    return static_cast<VirtualAuthenticatorManager&>(m_authenticatorManager.get());
}
#endif

API::HTTPCookieStore& WebsiteDataStore::cookieStore()
{
    if (!m_cookieStore)
        m_cookieStore = API::HTTPCookieStore::create(*this);

    return *m_cookieStore;
}

void WebsiteDataStore::resetQuota(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    protectedNetworkProcess()->resetQuota(m_sessionID, [callbackAggregator] { });
}

void WebsiteDataStore::resetStoragePersistedState(CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->resetStoragePersistedState(m_sessionID, WTFMove(completionHandler));
}

#if !PLATFORM(COCOA)
String WebsiteDataStore::defaultCacheStorageDirectory(const String& baseCacheDirectory)
{
    // CacheStorage is really data, not cache, as its lifetime should be controlled by web clients.
    // https://w3c.github.io/ServiceWorker/#cache-lifetimes
    //
    // Keep using baseCacheDirectory for now for compatibility, to avoid leaking existing storage.
    // Soon, it will be migrated to GeneralStorageDirectory.
    return cacheDirectoryFileSystemRepresentation("CacheStorage"_s, baseCacheDirectory);
}

String WebsiteDataStore::defaultGeneralStorageDirectory(const String& baseDataDirectory)
{
#if PLATFORM(PLAYSTATION) || USE(GLIB)
    return websiteDataDirectoryFileSystemRepresentation("storage"_s, baseDataDirectory);
#else
    return websiteDataDirectoryFileSystemRepresentation("Storage"_s, baseDataDirectory);
#endif
}

String WebsiteDataStore::defaultNetworkCacheDirectory(const String& baseCacheDirectory)
{
#if PLATFORM(PLAYSTATION) || USE(GLIB)
    return cacheDirectoryFileSystemRepresentation("WebKitCache"_s, baseCacheDirectory);
#else
    return cacheDirectoryFileSystemRepresentation("NetworkCache"_s, baseCacheDirectory);
#endif
}

String WebsiteDataStore::defaultApplicationCacheDirectory(const String& baseCacheDirectory)
{
#if PLATFORM(PLAYSTATION) || USE(GLIB)
    return cacheDirectoryFileSystemRepresentation("applications"_s, baseCacheDirectory);
#else
    return cacheDirectoryFileSystemRepresentation("ApplicationCache"_s, baseCacheDirectory);
#endif
}

String WebsiteDataStore::defaultMediaCacheDirectory(const String& baseCacheDirectory)
{
    return cacheDirectoryFileSystemRepresentation("MediaCache"_s, baseCacheDirectory);
}

String WebsiteDataStore::defaultIndexedDBDatabaseDirectory(const String& baseDataDirectory)
{
#if PLATFORM(PLAYSTATION)
    return websiteDataDirectoryFileSystemRepresentation("indexeddb"_s, baseDataDirectory);
#elif USE(GLIB)
    return websiteDataDirectoryFileSystemRepresentation(String::fromUTF8("databases" G_DIR_SEPARATOR_S "indexeddb"), baseDataDirectory);
#else
    return websiteDataDirectoryFileSystemRepresentation("IndexedDB"_s, baseDataDirectory);
#endif
}

String WebsiteDataStore::defaultServiceWorkerRegistrationDirectory(const String& baseDataDirectory)
{
#if PLATFORM(PLAYSTATION) || USE(GLIB)
    return websiteDataDirectoryFileSystemRepresentation("serviceworkers"_s, baseDataDirectory);
#else
    return websiteDataDirectoryFileSystemRepresentation("ServiceWorkers"_s, baseDataDirectory);
#endif
}

String WebsiteDataStore::defaultWebSQLDatabaseDirectory(const String& baseDataDirectory)
{
#if PLATFORM(PLAYSTATION)
    return websiteDataDirectoryFileSystemRepresentation("websql"_s, baseDataDirectory);
#elif USE(GLIB)
    return websiteDataDirectoryFileSystemRepresentation("databases"_s, baseDataDirectory);
#else
    return websiteDataDirectoryFileSystemRepresentation("WebSQL"_s, baseDataDirectory);
#endif
}

String WebsiteDataStore::defaultHSTSStorageDirectory(const String& baseCacheDirectory)
{
#if USE(GLIB) && !ENABLE(2022_GLIB_API)
    // Bug: HSTS storage goes in the data directory when baseCacheDirectory is not specified, but
    // it should go in the cache directory. Do not fix this because it would cause the old HSTS
    // cache to be leaked on disk.
    return websiteDataDirectoryFileSystemRepresentation(""_s, baseCacheDirectory);
#else
    return cacheDirectoryFileSystemRepresentation("HSTS"_s, baseCacheDirectory);
#endif
}

String WebsiteDataStore::defaultLocalStorageDirectory(const String& baseDataDirectory)
{
#if PLATFORM(PLAYSTATION)
    return websiteDataDirectoryFileSystemRepresentation("local"_s, baseDataDirectory);
#elif USE(GLIB)
    return websiteDataDirectoryFileSystemRepresentation("localstorage"_s, baseDataDirectory);
#else
    return websiteDataDirectoryFileSystemRepresentation("LocalStorage"_s, baseDataDirectory);
#endif
}

String WebsiteDataStore::defaultMediaKeysStorageDirectory(const String& baseDataDirectory)
{
#if PLATFORM(PLAYSTATION) || USE(GLIB)
    return websiteDataDirectoryFileSystemRepresentation("mediakeys"_s, baseDataDirectory);
#elif OS(WINDOWS)
    return websiteDataDirectoryFileSystemRepresentation("MediaKeyStorage"_s, baseDataDirectory);
#else
    return websiteDataDirectoryFileSystemRepresentation("MediaKeys"_s, baseDataDirectory);
#endif
}

String WebsiteDataStore::defaultAlternativeServicesDirectory(const String& baseCacheDirectory)
{
    return cacheDirectoryFileSystemRepresentation("AlternativeServices"_s, baseCacheDirectory);
}

String WebsiteDataStore::defaultDeviceIdHashSaltsStorageDirectory(const String& baseDataDirectory)
{
#if USE(GLIB)
    return websiteDataDirectoryFileSystemRepresentation("deviceidhashsalts"_s, baseDataDirectory);
#else
    return websiteDataDirectoryFileSystemRepresentation("DeviceIdHashSalts"_s, baseDataDirectory);
#endif
}

String WebsiteDataStore::defaultResourceLoadStatisticsDirectory(const String& baseDataDirectory)
{
#if PLATFORM(PLAYSTATION) || USE(GLIB)
    return websiteDataDirectoryFileSystemRepresentation("itp"_s, baseDataDirectory);
#else
    return websiteDataDirectoryFileSystemRepresentation("ResourceLoadStatistics"_s, baseDataDirectory);
#endif
}

String WebsiteDataStore::defaultJavaScriptConfigurationDirectory(const String&)
{
    // FIXME: This is currently only used on Cocoa ports. If implementing, note that it should not
    // use websiteDataDirectoryFileSystemRepresentation or cacheDirectoryFileSystemRepresentation
    // because it is not data or cache. It is a config file. We need to add a third type of
    // directory configDirectoryFileSystemRepresentation, and the parameter to this function should
    // be renamed accordingly.
    return String();
}

bool WebsiteDataStore::networkProcessHasEntitlementForTesting(const String&)
{
    return false;
}

void WebsiteDataStore::saveRecentSearches(const String& name, const Vector<WebCore::RecentSearch>&)
{
}

void WebsiteDataStore::loadRecentSearches(const String& name, CompletionHandler<void(Vector<WebCore::RecentSearch>&&)>&& completionHandler)
{
    completionHandler({ });
}

void WebsiteDataStore::removeRecentSearches(WallTime, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

std::optional<double> WebsiteDataStore::defaultOriginQuotaRatio()
{
    return std::nullopt;
}

std::optional<double> WebsiteDataStore::defaultTotalQuotaRatio()
{
    return std::nullopt;
}

#endif // !PLATFORM(COCOA)

void WebsiteDataStore::renameOriginInWebsiteData(WebCore::SecurityOriginData&& oldOrigin, WebCore::SecurityOriginData&& newOrigin, OptionSet<WebsiteDataType> dataTypes, CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->renameOriginInWebsiteData(m_sessionID, oldOrigin, newOrigin, dataTypes, WTFMove(completionHandler));
}

void WebsiteDataStore::originDirectoryForTesting(WebCore::ClientOrigin&& origin, OptionSet<WebsiteDataType> type, CompletionHandler<void(const String&)>&& completionHandler)
{
    protectedNetworkProcess()->websiteDataOriginDirectoryForTesting(m_sessionID, WTFMove(origin), type, WTFMove(completionHandler));
}

#if ENABLE(APP_BOUND_DOMAINS)
void WebsiteDataStore::hasAppBoundSession(CompletionHandler<void(bool)>&& completionHandler) const
{
    protectedNetworkProcess()->hasAppBoundSession(m_sessionID, WTFMove(completionHandler));
}

void WebsiteDataStore::clearAppBoundSession(CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->clearAppBoundSession(m_sessionID, WTFMove(completionHandler));
}

void WebsiteDataStore::forwardAppBoundDomainsToITPIfInitialized(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    auto appBoundDomains = appBoundDomainsIfInitialized();
    if (!appBoundDomains)
        return;

    auto propagateAppBoundDomains = [callbackAggregator] (WebsiteDataStore* store, const HashSet<WebCore::RegistrableDomain>& domains) {
        if (!store)
            return;

        if (store->thirdPartyCookieBlockingMode() != WebCore::ThirdPartyCookieBlockingMode::AllExceptBetweenAppBoundDomains)
            store->setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode::AllExceptBetweenAppBoundDomains, [callbackAggregator] { });

        store->setAppBoundDomainsForITP(domains, [callbackAggregator] { });
    };

    propagateAppBoundDomains(globalDefaultDataStore().get(), *appBoundDomains);

    for (auto* store : allDataStores().values())
        propagateAppBoundDomains(store, *appBoundDomains);
}

void WebsiteDataStore::setAppBoundDomainsForITP(const HashSet<WebCore::RegistrableDomain>& domains, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setAppBoundDomainsForResourceLoadStatistics(m_sessionID, domains, [callbackAggregator] { });
}
#endif

#if ENABLE(MANAGED_DOMAINS)
void WebsiteDataStore::forwardManagedDomainsToITPIfInitialized(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
    auto managedDomains = managedDomainsIfInitialized();
    if (!managedDomains || managedDomains->isEmpty())
        return;

    auto propagateManagedDomains = [callbackAggregator] (WebsiteDataStore* store, const HashSet<WebCore::RegistrableDomain>& domains) {
        if (!store)
            return;

        if (store->thirdPartyCookieBlockingMode() != WebCore::ThirdPartyCookieBlockingMode::AllExceptBetweenAppBoundDomains && store->thirdPartyCookieBlockingMode() != WebCore::ThirdPartyCookieBlockingMode::AllExceptManagedDomains)
            store->setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode::AllExceptManagedDomains, [callbackAggregator] { });
        store->setManagedDomainsForITP(domains, [callbackAggregator] { });
    };

    propagateManagedDomains(globalDefaultDataStore().get(), *managedDomains);

    for (auto* store : allDataStores().values())
        propagateManagedDomains(store, *managedDomains);
}

void WebsiteDataStore::setManagedDomainsForITP(const HashSet<WebCore::RegistrableDomain>& domains, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->setManagedDomainsForResourceLoadStatistics(m_sessionID, domains, [callbackAggregator] { });
}
#endif

void WebsiteDataStore::updateBundleIdentifierInNetworkProcess(const String& bundleIdentifier, CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->updateBundleIdentifier(bundleIdentifier, [callbackAggregator] { });
}

void WebsiteDataStore::clearBundleIdentifierInNetworkProcess(CompletionHandler<void()>&& completionHandler)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    protectedNetworkProcess()->clearBundleIdentifier([callbackAggregator] { });
}

void WebsiteDataStore::countNonDefaultSessionSets(CompletionHandler<void(size_t)>&& completionHandler)
{
    protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::CountNonDefaultSessionSets(m_sessionID), WTFMove(completionHandler));
}

static bool nextNetworkProcessLaunchShouldFailForTesting { false };

void WebsiteDataStore::makeNextNetworkProcessLaunchFailForTesting()
{
    nextNetworkProcessLaunchShouldFailForTesting = true;
}

bool WebsiteDataStore::shouldMakeNextNetworkProcessLaunchFailForTesting()
{
    return std::exchange(nextNetworkProcessLaunchShouldFailForTesting, false);
}

void WebsiteDataStore::showServiceWorkerNotification(IPC::Connection& connection, const WebCore::NotificationData& notificationData)
{
    if (m_client->showNotification(notificationData))
        return;

    WebNotificationManagerProxy::sharedServiceWorkerManager().show(*this, connection, notificationData, nullptr);
}

void WebsiteDataStore::cancelServiceWorkerNotification(const WTF::UUID& notificationID)
{
    WebNotificationManagerProxy::sharedServiceWorkerManager().cancel(nullptr, notificationID);
}

void WebsiteDataStore::clearServiceWorkerNotification(const WTF::UUID& notificationID)
{
    Vector<WTF::UUID> notifications = { notificationID };
    WebNotificationManagerProxy::sharedServiceWorkerManager().clearNotifications(nullptr, notifications);
}

void WebsiteDataStore::didDestroyServiceWorkerNotification(const WTF::UUID& notificationID)
{
    WebNotificationManagerProxy::sharedServiceWorkerManager().didDestroyNotification(nullptr, notificationID);
}

void WebsiteDataStore::openWindowFromServiceWorker(const String& urlString, const WebCore::SecurityOriginData& serviceWorkerOrigin, CompletionHandler<void(std::optional<WebCore::PageIdentifier>)>&& callback)
{
    auto innerCallback = [callback = WTFMove(callback)] (WebPageProxy* newPage) mutable {
        if (!newPage) {
            callback(std::nullopt);
            return;
        }

        if (!newPage->pageLoadState().isLoading()) {
            RELEASE_LOG(Loading, "The WKWebView provided in response to a ServiceWorker openWindow request was not in the loading state");
            callback(std::nullopt);
            return;
        }

#if ENABLE(SERVICE_WORKER)
        newPage->setServiceWorkerOpenWindowCompletionCallback(WTFMove(callback));
#endif
    };

    m_client->openWindowFromServiceWorker(urlString, serviceWorkerOrigin, WTFMove(innerCallback));
}

void WebsiteDataStore::reportServiceWorkerConsoleMessage(const URL& scriptURL, const WebCore::SecurityOriginData& clientOrigin, MessageSource source, MessageLevel level, const String& message, unsigned long requestIdentifier)
{
    m_client->reportServiceWorkerConsoleMessage(scriptURL, clientOrigin, source, level, message, requestIdentifier);
}

void WebsiteDataStore::workerUpdatedAppBadge(const WebCore::SecurityOriginData& origin, std::optional<uint64_t> badge)
{
    m_client->workerUpdatedAppBadge(origin, badge);
}

bool WebsiteDataStore::hasClientGetDisplayedNotifications() const
{
    return m_client->hasGetDisplayedNotifications();
}

void WebsiteDataStore::getNotifications(const URL& registrationalURL, CompletionHandler<void(Vector<WebCore::NotificationData>&&)>&& completionHandler)
{
    m_client->getDisplayedNotifications(WebCore::SecurityOriginData::fromURL(registrationalURL), WTFMove(completionHandler));
}

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)

void WebsiteDataStore::setEmulatedConditions(std::optional<int64_t>&& bytesPerSecondLimit)
{
    protectedNetworkProcess()->setEmulatedConditions(sessionID(), WTFMove(bytesPerSecondLimit));
}

#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

Ref<DownloadProxy> WebsiteDataStore::createDownloadProxy(Ref<API::DownloadClient>&& client, const WebCore::ResourceRequest& request, WebPageProxy* originatingPage, const FrameInfoData& frameInfo)
{
    return protectedNetworkProcess()->createDownloadProxy(*this, WTFMove(client), request, frameInfo, originatingPage);
}

void WebsiteDataStore::download(const DownloadProxy& downloadProxy, const String& suggestedFilename)
{
    std::optional<NavigatingToAppBoundDomain> isAppBound = NavigatingToAppBoundDomain::No;
    WebCore::ResourceRequest updatedRequest(downloadProxy.request());
    std::optional<WebCore::SecurityOriginData> topOrigin;
    // Request's firstPartyForCookies will be used as Original URL of the download request.
    // We set the value to top level document's URL.
    if (auto* initiatingPage = downloadProxy.originatingPage()) {
#if ENABLE(APP_BOUND_DOMAINS)
        isAppBound = initiatingPage->isTopFrameNavigatingToAppBoundDomain();
#endif

        URL initiatingPageURL = URL { initiatingPage->pageLoadState().url() };
        updatedRequest.setFirstPartyForCookies(initiatingPageURL);
        updatedRequest.setIsSameSite(WebCore::areRegistrableDomainsEqual(initiatingPageURL, downloadProxy.request().url()));
        topOrigin = initiatingPage->pageLoadState().origin();
        if (!updatedRequest.hasHTTPHeaderField(WebCore::HTTPHeaderName::UserAgent))
            updatedRequest.setHTTPUserAgent(initiatingPage->userAgentForURL(downloadProxy.request().url()));
    } else {
        updatedRequest.setFirstPartyForCookies(URL());
        updatedRequest.setIsSameSite(false);
        if (!updatedRequest.hasHTTPHeaderField(WebCore::HTTPHeaderName::UserAgent))
            updatedRequest.setHTTPUserAgent(WebPageProxy::standardUserAgent());
    }
    updatedRequest.setIsTopSite(false);
    protectedNetworkProcess()->send(Messages::NetworkProcess::DownloadRequest(m_sessionID, downloadProxy.downloadID(), updatedRequest, topOrigin, isAppBound, suggestedFilename), 0);
}

void WebsiteDataStore::resumeDownload(const DownloadProxy& downloadProxy, const API::Data& resumeData, const String& path, CallDownloadDidStart callDownloadDidStart)
{
    SandboxExtension::Handle sandboxExtensionHandle;
    if (!path.isEmpty()) {
        if (auto handle = SandboxExtension::createHandle(path, SandboxExtension::Type::ReadWrite))
            sandboxExtensionHandle = WTFMove(*handle);
    }

    protectedNetworkProcess()->send(Messages::NetworkProcess::ResumeDownload(m_sessionID, downloadProxy.downloadID(), resumeData.dataReference(), path, sandboxExtensionHandle, callDownloadDidStart), 0);
}

bool WebsiteDataStore::hasActivePages()
{
    return WTF::anyOf(WebProcessPool::allProcessPools(), [&](auto& pool) {
        return pool->hasPagesUsingWebsiteDataStore(*this);
    });
}

#if HAVE(NW_PROXY_CONFIG)
void WebsiteDataStore::clearProxyConfigData()
{
    protectedNetworkProcess()->send(Messages::NetworkProcess::ClearProxyConfigData(m_sessionID), 0);
}

void WebsiteDataStore::setProxyConfigData(Vector<std::pair<Vector<uint8_t>, WTF::UUID>>&& data)
{
    protectedNetworkProcess()->send(Messages::NetworkProcess::SetProxyConfigData(m_sessionID, WTFMove(data)), 0);
}
#endif // HAVE(NW_PROXY_CONFIG)

void WebsiteDataStore::setCompletionHandlerForRemovalFromNetworkProcess(CompletionHandler<void(String&&)>&& completionHandler)
{
    if (m_completionHandlerForRemovalFromNetworkProcess)
        m_completionHandlerForRemovalFromNetworkProcess("New completion handler is set"_s);

    m_completionHandlerForRemovalFromNetworkProcess = WTFMove(completionHandler);
}

#if ENABLE(SERVICE_WORKER)

void WebsiteDataStore::updateServiceWorkerInspectability()
{
    if (!m_pages.computeSize())
        return;

    bool wasInspectable = m_inspectionForServiceWorkersAllowed;
    m_inspectionForServiceWorkersAllowed = [&] {
        for (auto& page : m_pages) {
            if (page.inspectable())
                return true;
        }
        return false;
    }();

    if (wasInspectable == m_inspectionForServiceWorkersAllowed)
        return;

    if (RefPtr networkProcess = networkProcessIfExists())
        networkProcess->send(Messages::NetworkProcess::SetInspectionForServiceWorkersAllowed(m_sessionID, m_inspectionForServiceWorkersAllowed), 0);
}

#endif // ENABLE(SERVICE_WORKER)

void WebsiteDataStore::addPage(WebPageProxy& page)
{
    m_pages.add(page);

#if ENABLE(SERVICE_WORKER)
    updateServiceWorkerInspectability();
#endif
}

void WebsiteDataStore::removePage(WebPageProxy& page)
{
    m_pages.remove(page);

#if ENABLE(SERVICE_WORKER)
    updateServiceWorkerInspectability();
#endif
}

}
