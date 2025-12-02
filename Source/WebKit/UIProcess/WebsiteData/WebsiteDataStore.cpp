/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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
#include "ITPThirdPartyData.h"
#include "Logging.h"
#include "MockAuthenticatorManager.h"
#include "NetworkProcessConnectionInfo.h"
#include "NetworkProcessMessages.h"
#include "PageLoadState.h"
#include "RestrictedOpenerType.h"
#include "ShouldGrandfatherStatistics.h"
#include "StorageAccessStatus.h"
#include "UnifiedOriginStorageLevel.h"
#include "WebBackForwardCache.h"
#include "WebCookieManagerMessages.h"
#include "WebFrameProxy.h"
#include "WebKit2Initialize.h"
#include "WebKitServiceNames.h"
#include "WebNotificationManagerProxy.h"
#include "WebPageProxy.h"
#include "WebProcessCache.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebPushMessage.h"
#include "WebResourceLoadStatisticsStore.h"
#include "WebsiteData.h"
#include "WebsiteDataFetchOption.h"
#include "WebsiteDataStoreClient.h"
#include "WebsiteDataStoreParameters.h"
#include <WebCore/CredentialStorage.h>
#include <WebCore/DatabaseTracker.h>
#include <WebCore/HTMLMediaElement.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/NotificationResources.h>
#include <WebCore/OriginLock.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SQLiteFileSystem.h>
#include <WebCore/SearchPopupMenu.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/StorageUtilities.h>
#include <WebCore/WebLockRegistry.h>
#include <algorithm>
#include <wtf/CallbackAggregator.h>
#include <wtf/CheckedPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/FileSystem.h>
#include <wtf/ProcessPrivilege.h>
#include <wtf/RunLoop.h>
#include <wtf/text/MakeString.h>

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
#include "ScreenTimeWebsiteDataSupport.h"
#include "WebPrivacyHelpers.h"
#endif

#if ENABLE(WEB_AUTHN)
#include "VirtualAuthenticatorManager.h"
#endif // ENABLE(WEB_AUTHN)

#if HAVE(MODERN_DOWNLOADPROGRESS)
#include <WebKitAdditions/DownloadProgressAdditions.h>
#endif

namespace WebKit {

static bool allowsWebsiteDataRecordsForAllOrigins;
void WebsiteDataStore::allowWebsiteDataRecordsForAllOrigins()
{
    allowsWebsiteDataRecordsForAllOrigins = true;
}

static HashMap<String, PAL::SessionID>& activeGeneralStorageDirectories()
{
    static MainRunLoopNeverDestroyed<HashMap<String, PAL::SessionID>> directoryToSessionMap;
    return directoryToSessionMap;
}

static HashMap<PAL::SessionID, WeakRef<WebsiteDataStore>>& allDataStores()
{
    RELEASE_ASSERT(isUIThread());
    static NeverDestroyed<HashMap<PAL::SessionID, WeakRef<WebsiteDataStore>>> map;
    return map;
}

static String computeMediaKeyFile(const String& mediaKeyDirectory)
{
    return FileSystem::pathByAppendingComponent(mediaKeyDirectory, "SecureStop.plist"_s);
}

WorkQueue& WebsiteDataStore::websiteDataStoreIOQueueSingleton()
{
    static MainRunLoopNeverDestroyed<Ref<WorkQueue>> queue = WorkQueue::create("com.apple.WebKit.WebsiteDataStoreIO"_s);
    return queue.get();
}

void WebsiteDataStore::forEachWebsiteDataStore(NOESCAPE Function<void(WebsiteDataStore&)>&& function)
{
    for (auto& dataStore : allDataStores().values())
        function(Ref { dataStore.get() });
}

Ref<WebsiteDataStore> WebsiteDataStore::createNonPersistent()
{
    Ref result = adoptRef(*new WebsiteDataStore(WebsiteDataStoreConfiguration::create(IsPersistent::No), PAL::SessionID::generateEphemeralSessionID()));
    result->resolveDirectoriesAsynchronously();
    return result;
}

Ref<WebsiteDataStore> WebsiteDataStore::create(Ref<WebsiteDataStoreConfiguration>&& configuration, PAL::SessionID sessionID)
{
    Ref result = adoptRef(*new WebsiteDataStore(WTFMove(configuration), sessionID));
    result->resolveDirectoriesAsynchronously();
    return result;
}

WebsiteDataStore::WebsiteDataStore(Ref<WebsiteDataStoreConfiguration>&& configuration, PAL::SessionID sessionID)
    : m_sessionID(sessionID)
    , m_configuration(WTFMove(configuration))
    , m_trackingPreventionDebugMode(m_configuration->resourceLoadStatisticsDebugModeEnabled())
    , m_queue(WorkQueue::create("com.apple.WebKit.WebsiteDataStore"_s))
#if ENABLE(WEB_AUTHN)
    , m_authenticatorManager(AuthenticatorManager::create())
#endif
#if ENABLE(DEVICE_ORIENTATION)
    , m_deviceOrientationAndMotionAccessController(*this)
#endif
    , m_client(makeUniqueRef<WebsiteDataStoreClient>())
    , m_webLockRegistry(WebCore::LocalWebLockRegistry::create())
{
    RELEASE_LOG(Storage, "%p - WebsiteDataStore::WebsiteDataStore sessionID=%" PRIu64 " identifier=%" PUBLIC_LOG_STRING, this, m_sessionID.toUInt64(), m_configuration->identifier() ? m_configuration->identifier()->toString().utf8().data() : "null"_s);

#if PLATFORM(COCOA)
    determineTrackingPreventionState();
#endif
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
    if (RefPtr networkProcess = m_networkProcess)
        networkProcess->removeSession(*this, std::exchange(m_completionHandlerForRemovalFromNetworkProcess, { }));
    if (m_completionHandlerForRemovalFromNetworkProcess) {
        RunLoop::mainSingleton().dispatch([completionHandler = std::exchange(m_completionHandlerForRemovalFromNetworkProcess, { })]() mutable {
            completionHandler({ });
        });
    }
#if ENABLE(GPU_PROCESS)
    if (RefPtr gpuProcessProxy = GPUProcessProxy::singletonIfCreated())
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
    Ref newDataStore = WebsiteDataStore::create(WebsiteDataStoreConfiguration::create(isPersistent), isPersistent == IsPersistent::Yes ? PAL::SessionID::defaultSessionID() : PAL::SessionID::generateEphemeralSessionID());
    globalDatasStore = newDataStore.ptr();
    protectedDefaultDataStore() = newDataStore.ptr();

    return newDataStore;
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
    for (auto& dataStore : allDataStores().values()) {
        if (dataStore->configuration().identifier() == identifier)
            return dataStore.ptr();
    }

    return nullptr;
}

#if PLATFORM(COCOA)
Ref<WebsiteDataStore> WebsiteDataStore::dataStoreForIdentifier(const WTF::UUID& uuid)
{
    RELEASE_ASSERT(uuid.isValid());

    InitializeWebKit2();
    for (auto& dataStore : allDataStores().values()) {
        if (dataStore->configuration().identifier() == uuid)
            return Ref { dataStore.get() };
    }

    Ref configuration = WebsiteDataStoreConfiguration::create(uuid);
    return WebsiteDataStore::create(WTFMove(configuration), PAL::SessionID::generatePersistentSessionID());
}
#endif

void WebsiteDataStore::registerWithSessionIDMap()
{
    auto result = allDataStores().add(m_sessionID, *this);
    ASSERT_UNUSED(result, result.isNewEntry);
}

WebsiteDataStore* WebsiteDataStore::existingDataStoreForSessionID(PAL::SessionID sessionID)
{
    return allDataStores().get(sessionID);
}

#if HAVE(APP_SSO)
SOAuthorizationCoordinator& WebsiteDataStore::soAuthorizationCoordinator(const WebPageProxy& pageProxy)
{
    RELEASE_ASSERT(pageProxy.protectedPreferences()->isExtensibleSSOEnabled());
    if (!m_soAuthorizationCoordinator)
        lazyInitialize(m_soAuthorizationCoordinator, WTF::makeUnique<SOAuthorizationCoordinator>());

    return *m_soAuthorizationCoordinator;
}
#endif

static Ref<NetworkProcessProxy> networkProcessForSession(PAL::SessionID sessionID)
{
#if ((PLATFORM(GTK) || PLATFORM(WPE)) && !ENABLE(2022_GLIB_API))
    if (sessionID.isEphemeral()) {
        // Reuse a previous persistent session network process for ephemeral sessions.
        for (auto& dataStore : allDataStores().values()) {
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

void WebsiteDataStore::setNetworkProcess(NetworkProcessProxy& networkProcess)
{
    ASSERT(!m_networkProcess);
    m_networkProcess = networkProcess;
}

NetworkProcessProxy& WebsiteDataStore::networkProcess()
{
    if (!m_networkProcess) {
        Ref networkProcess = networkProcessForSession(m_sessionID);
        m_networkProcess = networkProcess.copyRef();
        networkProcess->addSession(*this, NetworkProcessProxy::SendParametersToNetworkProcess::Yes);
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

static std::pair<String, FileSystem::Salt> migrateMediaKeysStorageIfNecessary(const String& directory)
{
    if (directory.isEmpty())
        return { emptyString(), { } };

    static constexpr ASCIILiteral versionName = "v1"_s;
    auto versionDirectory = FileSystem::pathByAppendingComponent(directory, versionName);
    auto saltPath = FileSystem::pathByAppendingComponent(versionDirectory, "salt"_s);
    auto mediaKeysStorageSalt = valueOrDefault(FileSystem::readOrMakeSalt(saltPath));

    auto originDirectoryNames = FileSystem::listDirectory(directory);
    // Migrate existing data to new version directory.
    for (auto name : originDirectoryNames) {
        if (name == versionName)
            continue;

        auto originData = WebCore::SecurityOriginData::fromDatabaseIdentifier(name);
        if (!originData)
            continue;

        auto originDirectory = FileSystem::pathByAppendingComponent(directory, name);
        auto mediaKeysStorageFile = computeMediaKeyFile(originDirectory);
        if (!FileSystem::fileExists(mediaKeysStorageFile)) {
            FileSystem::deleteEmptyDirectory(originDirectory);
            continue;
        }

        auto newOriginDirectoryName = WebCore::StorageUtilities::encodeSecurityOriginForFileName(mediaKeysStorageSalt, *originData);
        auto newOriginDirectory = FileSystem::pathByAppendingComponent(versionDirectory, newOriginDirectoryName);
        if (FileSystem::moveFile(originDirectory, newOriginDirectory))
            WebCore::StorageUtilities::writeOriginToFile(FileSystem::pathByAppendingComponent(newOriginDirectory, "origin"_s), WebCore::ClientOrigin { *originData, *originData });
    }

    return { versionDirectory, mediaKeysStorageSalt };
}

static void resolveDirectories(WebsiteDataStoreConfiguration::Directories& directories, FileSystem::Salt& mediaKeysStorageSalt)
{
    if (!directories.mediaCacheDirectory.isEmpty())
        directories.mediaCacheDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directories.mediaCacheDirectory);

    if (!directories.mediaKeysStorageDirectory.isEmpty()) {
        auto [mediaKeysStorageDirectory, salt] = migrateMediaKeysStorageIfNecessary(directories.mediaKeysStorageDirectory);
        mediaKeysStorageSalt = salt;
        directories.mediaKeysStorageDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(mediaKeysStorageDirectory);
    }

    if (!directories.indexedDBDatabaseDirectory.isEmpty())
        directories.indexedDBDatabaseDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directories.indexedDBDatabaseDirectory);

    if (!directories.alternativeServicesDirectory.isEmpty())
        directories.alternativeServicesDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directories.alternativeServicesDirectory);

    if (!directories.localStorageDirectory.isEmpty())
        directories.localStorageDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directories.localStorageDirectory);

    if (!directories.deviceIdHashSaltsStorageDirectory.isEmpty())
        directories.deviceIdHashSaltsStorageDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directories.deviceIdHashSaltsStorageDirectory);

#if ENABLE(ENCRYPTED_MEDIA)
    if (!directories.mediaKeysHashSaltsStorageDirectory.isEmpty())
        directories.mediaKeysHashSaltsStorageDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directories.mediaKeysHashSaltsStorageDirectory);
#endif

    if (!directories.networkCacheDirectory.isEmpty())
        directories.networkCacheDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directories.networkCacheDirectory);

    if (!directories.resourceLoadStatisticsDirectory.isEmpty())
        directories.resourceLoadStatisticsDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directories.resourceLoadStatisticsDirectory);

    if (!directories.serviceWorkerRegistrationDirectory.isEmpty())
        directories.serviceWorkerRegistrationDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directories.serviceWorkerRegistrationDirectory);

    if (!directories.javaScriptConfigurationDirectory.isEmpty())
        directories.javaScriptConfigurationDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directories.javaScriptConfigurationDirectory);

    if (!directories.cacheStorageDirectory.isEmpty())
        directories.cacheStorageDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directories.cacheStorageDirectory);

    if (!directories.hstsStorageDirectory.isEmpty())
        directories.hstsStorageDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directories.hstsStorageDirectory);

    if (!directories.generalStorageDirectory.isEmpty())
        directories.generalStorageDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directories.generalStorageDirectory);

    if (!directories.searchFieldHistoryDirectory.isEmpty())
        directories.searchFieldHistoryDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directories.searchFieldHistoryDirectory);

    if (!directories.generalStorageDirectory.isEmpty())
        directories.generalStorageDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directories.generalStorageDirectory);

#if ENABLE(ARKIT_INLINE_PREVIEW)
    if (!directories.modelElementCacheDirectory.isEmpty())
        directories.modelElementCacheDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directories.modelElementCacheDirectory);
#endif

    if (!directories.cookieStorageFile.isEmpty()) {
        auto resolvedCookieDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(FileSystem::parentPath(directories.cookieStorageFile));
        directories.cookieStorageFile = FileSystem::pathByAppendingComponent(resolvedCookieDirectory, FileSystem::pathFileName(directories.cookieStorageFile));
    }

#if ENABLE(CONTENT_EXTENSIONS)
    if (!directories.resourceMonitorThrottlerDirectory.isEmpty())
        directories.resourceMonitorThrottlerDirectory = resolveAndCreateReadWriteDirectoryForSandboxExtension(directories.resourceMonitorThrottlerDirectory);
#endif
}

const WebsiteDataStoreConfiguration::Directories& WebsiteDataStore::resolvedDirectories() const
{
    ASSERT(RunLoop::isMain());

    Locker resolveLocker { m_resolveDirectoriesLock };
    if (m_resolvedDirectories)
        return *m_resolvedDirectories;

    // Ensure task is dispatched before waiting.
    RELEASE_ASSERT(m_hasDispatchedResolveDirectories);

    while (!m_resolvedDirectories)
        m_resolveDirectoriesCondition.wait(m_resolveDirectoriesLock);

    return *m_resolvedDirectories;
}

FileSystem::Salt WebsiteDataStore::mediaKeysStorageSalt() const
{
    Locker resolveLocker { m_resolveDirectoriesLock };
    RELEASE_ASSERT(m_resolvedDirectories);

    return m_mediaKeysStorageSalt;
}

void WebsiteDataStore::resolveDirectoriesAsynchronously()
{
    ASSERT(RunLoop::isMain());

    RELEASE_ASSERT(!m_hasDispatchedResolveDirectories);
    m_hasDispatchedResolveDirectories = true;

    Ref resolveDirectoriesQueue = WorkQueue::create("com.apple.WebKit.WebsiteDataStore.resolveDirectories"_s, WorkQueue::QOS::UserInteractive);
    resolveDirectoriesQueue->dispatch([this, protectedThis = Ref { *this }, directories = crossThreadCopy(m_configuration->directories()), shouldExcludeCacheDirectories = !!m_configuration->identifier()]() mutable {
        FileSystem::Salt mediaKeysStorageSalt;
        resolveDirectories(directories, mediaKeysStorageSalt);
        // Default paths of WebsiteDataStore created with identifier are not under caches or tmp directory,
        // so we need to explicitly exclude them from backup.
        handleResolvedDirectoriesAsynchronously(directories, shouldExcludeCacheDirectories);
        {
            Locker resolveLocker { m_resolveDirectoriesLock };
            m_resolvedDirectories = crossThreadCopy(WTFMove(directories));
            m_mediaKeysStorageSalt = WTFMove(mediaKeysStorageSalt);
            m_resolveDirectoriesCondition.notifyOne();
        }
        RunLoop::mainSingleton().dispatch([protectedThis = WTFMove(protectedThis)] { });
    });
}

void WebsiteDataStore::handleResolvedDirectoriesAsynchronously(const WebsiteDataStoreConfiguration::Directories& directories, bool shouldExcludeCacheDirectories)
{
    Vector<String> allCacheDirectories;
    if (shouldExcludeCacheDirectories) {
        allCacheDirectories = {
            directories.mediaCacheDirectory.isolatedCopy()
            , directories.networkCacheDirectory.isolatedCopy()
#if ENABLE(ARKIT_INLINE_PREVIEW)
            , directories.modelElementCacheDirectory.isolatedCopy()
#endif
        };
    }

    // Clear data of deprecated types.
    m_queue->dispatch([webSQLDirectory = crossThreadCopy(directories.webSQLDatabaseDirectory), directoriesToExclude = WTFMove(allCacheDirectories)]() {
        if (!webSQLDirectory.isEmpty()) {
            WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDirectory)->deleteAllDatabasesImmediately();
            FileSystem::deleteEmptyDirectory(webSQLDirectory);
        }

        for (auto& directory : directoriesToExclude)
            FileSystem::setExcludedFromBackup(directory, true);
    });
}

void WebsiteDataStore::fetchDomainsWithUserInteraction(CompletionHandler<void(const HashSet<WebCore::RegistrableDomain>&)>&& completionHandler)
{
    if (m_domainsWithUserInteractions)
        return completionHandler(*m_domainsWithUserInteractions);

    bool shouldFetch = m_domainsWithUserInteractionsCompletionHandler.isEmpty();
    m_domainsWithUserInteractionsCompletionHandler.append(WTFMove(completionHandler));

    if (!shouldFetch)
        return;

    protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::FetchWebsitesWithUserInteractions(sessionID()), [this, protectedThis = RefPtr { *this }](HashSet<WebCore::RegistrableDomain>&& domains) {
        domains.addAll(platformAdditionalDomainsWithUserInteraction());
        m_domainsWithUserInteractions = WTFMove(domains);

        for (auto& domain : std::exchange(m_pendingDomainsWithUserInteractions, { }))
            m_domainsWithUserInteractions->add(domain);

        for (auto& completionHandler : std::exchange(m_domainsWithUserInteractionsCompletionHandler, { }))
            completionHandler(*m_domainsWithUserInteractions);
    });
}

#if !PLATFORM(COCOA)
HashSet<WebCore::RegistrableDomain> WebsiteDataStore::platformAdditionalDomainsWithUserInteraction() const
{
    return { };
}
#endif

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
    fetchDataAndApply(dataTypes, fetchOptions, WorkQueue::mainSingleton(), WTFMove(completionHandler));
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

        static String loggingString(OptionSet<WebsiteDataType> types) {
            StringBuilder sb;
            for (WebsiteDataType type : types) {
                if (!sb.isEmpty())
                    sb.append(", "_s);
                sb.append(toString(type));
            }
            return sb.toString();
        }

        ~CallbackAggregator()
        {
            ASSERT(RunLoop::isMain());

            auto records = WTF::map(WTFMove(m_websiteDataRecords), [this](auto&& entry) {
                return m_queue.ptr() != &WorkQueue::mainSingleton() ? crossThreadCopy(WTFMove(entry.value)) : WTFMove(entry.value);
            });
            m_queue->dispatch([apply = WTFMove(m_apply), records = WTFMove(records), sessionID = m_protectedDataStore->sessionID()] () mutable {
                OptionSet<WebsiteDataType> allTypes;
                for (auto& record : records)
                    allTypes.add(record.types);
                apply(WTFMove(records));
                RELEASE_LOG(Storage, "WebsiteDataStore::fetchDataAndApply finished fetching data for session %" PRIu64 " ( fetched types: %" PUBLIC_LOG_STRING ")", sessionID.toUInt64(), loggingString(allTypes).utf8().data());
            });
        }

        const OptionSet<WebsiteDataFetchOption>& fetchOptions() const { return m_fetchOptions; }

        void addWebsiteData(WebsiteData&& websiteData)
        {
            if (!RunLoop::isMain()) {
                RunLoop::mainSingleton().dispatch([protectedThis = Ref { *this }, websiteData = crossThreadCopy(websiteData)]() mutable {
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

                    String hostString = entry.origin.host().isEmpty() ? emptyString() : makeString(' ', entry.origin.host());
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

            for (const auto& domain : websiteData.registrableDomainsWithResourceLoadStatistics) {
                auto displayName = WebsiteDataRecord::displayNameForHostName(domain.string());
                if (!displayName)
                    continue;

                auto& record = m_websiteDataRecords.add(displayName, WebsiteDataRecord { }).iterator->value;
                if (!record.displayName)
                    record.displayName = WTFMove(displayName);

                record.addResourceLoadStatisticsRegistrableDomain(domain);
            }
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
        const Ref<WorkQueue> m_queue;
        Function<void(Vector<WebsiteDataRecord>)> m_apply;

        HashMap<String, WebsiteDataRecord> m_websiteDataRecords;
        const Ref<WebsiteDataStore> m_protectedDataStore;
    };

    Ref callbackAggregator = CallbackAggregator::create(fetchOptions, WTFMove(queue), WTFMove(apply), *this);

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
        [[fallthrough]];
    case ProcessAccessType::OnlyIfLaunched:
        if (RefPtr networkProcess = m_networkProcess) {
            networkProcess->fetchWebsiteData(m_sessionID, dataTypes, fetchOptions, [callbackAggregator](WebsiteData websiteData) {
                callbackAggregator->addWebsiteData(WTFMove(websiteData));
            });
        }
        break;
    case ProcessAccessType::None:
        break;
    }

    auto webProcessAccessType = computeWebProcessAccessTypeForDataFetch(dataTypes, !isPersistent());
    if (webProcessAccessType != ProcessAccessType::None) {
        for (Ref process : processes()) {
            if (webProcessAccessType == ProcessAccessType::OnlyIfLaunched && process->state() != WebProcessProxy::State::Running)
                continue;
            process->fetchWebsiteData(m_sessionID, dataTypes, [callbackAggregator](WebsiteData websiteData) {
                callbackAggregator->addWebsiteData(WTFMove(websiteData));
            });
        }
    }

    if (dataTypes.contains(WebsiteDataType::DeviceIdHashSalt)) {
        ensureProtectedDeviceIdHashSaltStorage()->getDeviceIdHashSaltOrigins([callbackAggregator](auto&& origins) {
            WebsiteData websiteData;
            websiteData.entries = WTF::map(origins, [](auto& origin) {
                return WebsiteData::Entry { origin, WebsiteDataType::DeviceIdHashSalt, 0 };
            });
            callbackAggregator->addWebsiteData(WTFMove(websiteData));
        });
    }

    if (dataTypes.contains(WebsiteDataType::MediaKeys) && isPersistent()) {
        auto mediaKeysStorageDirectory = resolvedDirectories().mediaKeysStorageDirectory;
        m_queue->dispatch([mediaKeysStorageDirectory = crossThreadCopy(WTFMove(mediaKeysStorageDirectory)), callbackAggregator] {
            WebsiteData websiteData;
            websiteData.entries = mediaKeysStorageOrigins(mediaKeysStorageDirectory).map([](auto& origin) {
                return WebsiteData::Entry { origin, WebsiteDataType::MediaKeys, 0 };
            });
            callbackAggregator->addWebsiteData(WTFMove(websiteData));
        });
    }

#if ENABLE(SCREEN_TIME)
    if (dataTypes.contains(WebsiteDataType::ScreenTime) && isPersistent()) {
        ScreenTimeWebsiteDataSupport::getScreenTimeURLs(configuration().identifier(), { [callbackAggregator](HashSet<URL> urls) {
            WebsiteData websiteData;
            websiteData.entries = WTF::map(urls, [](auto& url) {
                return WebsiteData::Entry { WebCore::SecurityOriginData::fromURL(url), WebsiteDataType::ScreenTime, 0 };
            });
            callbackAggregator->addWebsiteData(WTFMove(websiteData));
        } });
    }
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    if (dataTypes.contains(WebsiteDataType::MediaKeys)) {
        ensureProtectedMediaKeysHashSaltStorage()->getDeviceIdHashSaltOrigins([callbackAggregator](auto&& origins) {
            WebsiteData websiteData;
            websiteData.entries = WTF::map(origins, [](auto& origin) {
                return WebsiteData::Entry { origin, WebsiteDataType::MediaKeys, 0 };
            });
            callbackAggregator->addWebsiteData(WTFMove(websiteData));
        });
    }
#endif
}

void WebsiteDataStore::fetchDataForRegistrableDomains(OptionSet<WebsiteDataType> dataTypes, OptionSet<WebsiteDataFetchOption> fetchOptions, Vector<WebCore::RegistrableDomain>&& domains, CompletionHandler<void(Vector<WebsiteDataRecord>&&, HashSet<WebCore::RegistrableDomain>&&)>&& completionHandler)
{
    fetchDataAndApply(dataTypes, fetchOptions, Ref { m_queue }, [domains = crossThreadCopy(domains), completionHandler = WTFMove(completionHandler)] (auto&& existingDataRecords) mutable {
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
        RunLoop::mainSingleton().dispatch([completionHandler = WTFMove(completionHandler), matchingDataRecords = WTFMove(matchingDataRecords), domainsWithMatchingDataRecords = WTFMove(domainsWithMatchingDataRecords)] () mutable {
            completionHandler(WTFMove(matchingDataRecords), WTFMove(domainsWithMatchingDataRecords));
        });
    });
}

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

    if (dataTypes.contains(WebsiteDataType::ResourceLoadStatistics))
        return ProcessAccessType::OnlyIfLaunched;

    return ProcessAccessType::None;
}

static WebsiteDataStore::ProcessAccessType computeWebProcessAccessTypeForDataRemovalWithRecords(OptionSet<WebsiteDataType> dataTypes)
{
    // FIXME: We currently don't remove resource load statistics in web process for origin data removal as TestRunner might dead lock.
    if (dataTypes.contains(WebsiteDataType::MemoryCache))
        return WebsiteDataStore::ProcessAccessType::OnlyIfLaunched;

    return WebsiteDataStore::ProcessAccessType::None;
}

HashSet<WebCore::ProcessIdentifier> WebsiteDataStore::activeWebProcesses() const
{
    HashSet<WebCore::ProcessIdentifier> identifiers;
    // m_processes does not include worker processes now, so we iterate all processes.
    for (Ref processPool : WebProcessPool::allProcessPools()) {
        for (Ref process : processPool->processes()) {
            if (process->isPrewarmed() || process->websiteDataStore() != this)
                continue;

            if (process->pageCount() || process->provisionalPageCount() || process->isRunningServiceWorkers())
                identifiers.add(process->coreProcessIdentifier());
        }
    }

    return identifiers;
}

void WebsiteDataStore::removeDataInNetworkProcess(WebsiteDataStore::ProcessAccessType networkProcessAccessType, OptionSet<WebsiteDataType> dataTypes, WallTime modifiedSince, CompletionHandler<void()>&& completionHandler)
{
    RefPtr<NetworkProcessProxy> networkProcess;
    if (networkProcessAccessType == ProcessAccessType::Launch)
        networkProcess = this->networkProcess();
    else if (networkProcessAccessType == ProcessAccessType::OnlyIfLaunched)
        networkProcess = networkProcessIfExists();

    if (!networkProcess)
        return completionHandler();

    networkProcess->deleteWebsiteData(m_sessionID, dataTypes, modifiedSince, activeWebProcesses(), WTFMove(completionHandler));
}

void WebsiteDataStore::removeData(OptionSet<WebsiteDataType> dataTypes, WallTime modifiedSince, Function<void()>&& completionHandler)
{
    RELEASE_LOG(Storage, "WebsiteDataStore::removeData started to delete data modified since %f for session %" PRIu64, modifiedSince.secondsSinceEpoch().value(), m_sessionID.toUInt64());
    Ref callbackAggregator = MainRunLoopCallbackAggregator::create([protectedThis = Ref { *this }, sessionID = m_sessionID, completionHandler = WTFMove(completionHandler), token = m_removeDataTaskCounter.count()] {
#if RELEASE_LOG_DISABLED
        UNUSED_PARAM(sessionID);
#endif
        UNUSED_PARAM(token);
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

    auto webProcessAccessType = computeWebProcessAccessTypeForDataRemoval(dataTypes, !isPersistent());
    auto networkProcessAccessType = computeNetworkProcessAccessTypeForDataRemoval(dataTypes, !isPersistent());
    // Terminate web processes to avoid new data being created during deletion.
    if (webProcessAccessType != ProcessAccessType::None || networkProcessAccessType != ProcessAccessType::None) {
        for (Ref processPool : WebProcessPool::allProcessPools()) {
            // Clear back/forward cache first as processes removed from the back/forward cache will likely
            // be added to the WebProcess cache.
            processPool->backForwardCache().removeEntriesForSession(sessionID());
            processPool->webProcessCache().clearAllProcessesForSession(sessionID());
        }
    }

    if (webProcessAccessType != ProcessAccessType::None) {
        for (Ref process : processes()) {
            if (webProcessAccessType == ProcessAccessType::OnlyIfLaunched && process->state() != WebProcessProxy::State::Running)
                continue;

            process->deleteWebsiteData(m_sessionID, dataTypes, modifiedSince, [callbackAggregator] { });
        }
    }

    removeDataInNetworkProcess(networkProcessAccessType, dataTypes, modifiedSince, [callbackAggregator] { });

    if (dataTypes.contains(WebsiteDataType::DeviceIdHashSalt) || (dataTypes.contains(WebsiteDataType::Cookies)))
        ensureProtectedDeviceIdHashSaltStorage()->deleteDeviceIdHashSaltOriginsModifiedSince(modifiedSince, [callbackAggregator] { });

#if ENABLE(ENCRYPTED_MEDIA)
    if (dataTypes.contains(WebsiteDataType::MediaKeys) || (dataTypes.contains(WebsiteDataType::Cookies)))
        ensureProtectedMediaKeysHashSaltStorage()->deleteDeviceIdHashSaltOriginsModifiedSince(modifiedSince, [callbackAggregator] { });
#endif

    if (dataTypes.contains(WebsiteDataType::MediaKeys) && isPersistent()) {
        auto mediaKeysStorageDirectory = resolvedDirectories().mediaKeysStorageDirectory;
        m_queue->dispatch([mediaKeysStorageDirectory = crossThreadCopy(WTFMove(mediaKeysStorageDirectory)), callbackAggregator, modifiedSince] {
            removeMediaKeysStorage(mediaKeysStorageDirectory, modifiedSince);
        });
    }

    if (dataTypes.contains(WebsiteDataType::SearchFieldRecentSearches) && isPersistent())
        removeRecentSearches(modifiedSince, [callbackAggregator] { });

#if ENABLE(SCREEN_TIME)
    if (dataTypes.contains(WebsiteDataType::ScreenTime) && isPersistent())
        ScreenTimeWebsiteDataSupport::removeScreenTimeDataWithInterval(modifiedSince, configuration());
#endif
}

void WebsiteDataStore::removeData(OptionSet<WebsiteDataType> dataTypes, const Vector<WebsiteDataRecord>& dataRecords, Function<void()>&& completionHandler)
{
    RELEASE_LOG(Storage, "WebsiteDataStore::removeData started to delete data for session %" PRIu64, m_sessionID.toUInt64());
    Ref callbackAggregator = MainRunLoopCallbackAggregator::create([protectedThis = Ref { *this }, sessionID = m_sessionID, completionHandler = WTFMove(completionHandler)] {
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
                registrableDomains.appendRange(dataRecord.resourceLoadStatisticsRegistrableDomains.begin(), dataRecord.resourceLoadStatisticsRegistrableDomains.end());
            }

            protectedNetworkProcess()->deleteWebsiteDataForOrigins(m_sessionID, dataTypes, origins, cookieHostNames, HSTSCacheHostNames, registrableDomains, [callbackAggregator, processPool] { });
        }
    }

    auto webProcessAccessType = computeWebProcessAccessTypeForDataRemovalWithRecords(dataTypes);
    if (webProcessAccessType != ProcessAccessType::None) {
        for (Ref process : processes()) {
            if (webProcessAccessType == ProcessAccessType::OnlyIfLaunched && process->state() != WebProcessProxy::State::Running)
                continue;
            process->deleteWebsiteDataForOrigins(m_sessionID, dataTypes, origins, [callbackAggregator] { });
        }
    }

    if (dataTypes.contains(WebsiteDataType::DeviceIdHashSalt) || (dataTypes.contains(WebsiteDataType::Cookies)))
        ensureProtectedDeviceIdHashSaltStorage()->deleteDeviceIdHashSaltForOrigins(origins, [callbackAggregator] { });

#if ENABLE(ENCRYPTED_MEDIA)
    if (dataTypes.contains(WebsiteDataType::MediaKeys) || (dataTypes.contains(WebsiteDataType::Cookies)))
        ensureProtectedMediaKeysHashSaltStorage()->deleteDeviceIdHashSaltForOrigins(origins, [callbackAggregator] { });
#endif

    if (dataTypes.contains(WebsiteDataType::MediaKeys) && isPersistent()) {
        HashSet<WebCore::SecurityOriginData> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(crossThreadCopy(origin));
        }

        auto mediaKeysStorageDirectory = resolvedDirectories().mediaKeysStorageDirectory;
        m_queue->dispatch([mediaKeysStorageDirectory = crossThreadCopy(WTFMove(mediaKeysStorageDirectory)), salt = mediaKeysStorageSalt(), callbackAggregator, origins = WTFMove(origins)] {
            removeMediaKeysStorage(mediaKeysStorageDirectory, origins, salt);
        });
    }
#if ENABLE(SCREEN_TIME)
    if (dataTypes.contains(WebsiteDataType::ScreenTime) && isPersistent()) {
        HashSet<URL> websitesToRemove;
        for (const auto& dataRecord : dataRecords) {
            if (dataRecord.types.contains(WebsiteDataType::ScreenTime)) {
                for (const auto& origin : dataRecord.origins)
                    websitesToRemove.add(origin.toURL());
            }
        }
        ScreenTimeWebsiteDataSupport::removeScreenTimeData(websitesToRemove, configuration());
    }
#endif
}

DeviceIdHashSaltStorage& WebsiteDataStore::ensureDeviceIdHashSaltStorage()
{
    if (!m_deviceIdHashSaltStorage)
        lazyInitialize(m_deviceIdHashSaltStorage, DeviceIdHashSaltStorage::create(isPersistent() ? m_configuration->deviceIdHashSaltsStorageDirectory() : String()));

    return *m_deviceIdHashSaltStorage;
}

Ref<DeviceIdHashSaltStorage> WebsiteDataStore::ensureProtectedDeviceIdHashSaltStorage()
{
    return ensureDeviceIdHashSaltStorage();
}

#if ENABLE(ENCRYPTED_MEDIA)
DeviceIdHashSaltStorage& WebsiteDataStore::ensureMediaKeysHashSaltStorage()
{
    if (!m_mediaKeysHashSaltStorage)
        lazyInitialize(m_mediaKeysHashSaltStorage, DeviceIdHashSaltStorage::create(isPersistent() ? m_configuration->mediaKeysHashSaltsStorageDirectory() : String()));

    return *m_mediaKeysHashSaltStorage;
}

Ref<DeviceIdHashSaltStorage> WebsiteDataStore::ensureProtectedMediaKeysHashSaltStorage()
{
    return ensureMediaKeysHashSaltStorage();
}
#endif

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
    return std::ranges::any_of(WebProcessPool::allProcessPools(), [](auto& pool) { return pool->hasServiceWorkerBackgroundActivityForTesting(); });
}

void WebsiteDataStore::runningOrTerminatingServiceWorkerCountForTesting(CompletionHandler<void(unsigned)>&& completionHandler)
{
    protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::RunningOrTerminatingServiceWorkerCountForTesting(sessionID()), WTFMove(completionHandler));
}

void WebsiteDataStore::setMaxStatisticsEntries(size_t maximumEntryCount, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    protectedNetworkProcess()->setMaxStatisticsEntries(m_sessionID, maximumEntryCount, WTFMove(completionHandler));
}

void WebsiteDataStore::setPruneEntriesDownTo(size_t pruneTargetCount, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    protectedNetworkProcess()->setPruneEntriesDownTo(m_sessionID, pruneTargetCount, WTFMove(completionHandler));
}

void WebsiteDataStore::setGrandfatheringTime(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    protectedNetworkProcess()->setGrandfatheringTime(m_sessionID, seconds, WTFMove(completionHandler));
}

void WebsiteDataStore::setMinimumTimeBetweenDataRecordsRemoval(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    protectedNetworkProcess()->setMinimumTimeBetweenDataRecordsRemoval(m_sessionID, seconds, WTFMove(completionHandler));
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
    
    protectedNetworkProcess()->setPrevalentResource(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
}

void WebsiteDataStore::setPrevalentResourceForDebugMode(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    protectedNetworkProcess()->setPrevalentResourceForDebugMode(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
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
    
    protectedNetworkProcess()->setVeryPrevalentResource(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
}

void WebsiteDataStore::setShouldClassifyResourcesBeforeDataRecordsRemoval(bool value, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    protectedNetworkProcess()->setShouldClassifyResourcesBeforeDataRecordsRemoval(m_sessionID, value, WTFMove(completionHandler));
}

void WebsiteDataStore::setSubframeUnderTopFrameDomain(const URL& subFrameURL, const URL& topFrameURL, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (subFrameURL.protocolIsAbout() || subFrameURL.isEmpty() || topFrameURL.protocolIsAbout() || topFrameURL.isEmpty()) {
        completionHandler();
        return;
    }

    protectedNetworkProcess()->setSubframeUnderTopFrameDomain(m_sessionID, WebCore::RegistrableDomain { subFrameURL }, WebCore::RegistrableDomain { topFrameURL }, WTFMove(completionHandler));
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

    protectedNetworkProcess()->setSubresourceUnderTopFrameDomain(m_sessionID, WebCore::RegistrableDomain { subresourceURL }, WebCore::RegistrableDomain { topFrameURL }, WTFMove(completionHandler));
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

    protectedNetworkProcess()->setSubresourceUniqueRedirectTo(m_sessionID, WebCore::RegistrableDomain { subresourceURL }, WebCore::RegistrableDomain { urlRedirectedTo }, WTFMove(completionHandler));
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

void WebsiteDataStore::getResourceLoadStatisticsDataSummary(CompletionHandler<void(Vector<ITPThirdPartyData>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    class LocalCallbackAggregator : public RefCounted<LocalCallbackAggregator> {
    public:
        static Ref<LocalCallbackAggregator> create(CompletionHandler<void(Vector<ITPThirdPartyData>&&)>&& completionHandler)
        {
            return adoptRef(*new LocalCallbackAggregator(WTFMove(completionHandler)));
        }

        ~LocalCallbackAggregator()
        {
            ASSERT(RunLoop::isMain());

            m_completionHandler(WTFMove(m_results));
        }

        void addResult(Vector<ITPThirdPartyData>&& results)
        {
            m_results.appendVector(WTFMove(results));
        }

    private:
        LocalCallbackAggregator(CompletionHandler<void(Vector<ITPThirdPartyData>&&)>&& completionHandler)
            : m_completionHandler(WTFMove(completionHandler))
        {
            ASSERT(RunLoop::isMain());
        }

        CompletionHandler<void(Vector<ITPThirdPartyData>&&)> m_completionHandler;
        Vector<ITPThirdPartyData> m_results;
    };
    
    Ref localCallbackAggregator = LocalCallbackAggregator::create(WTFMove(completionHandler));
    
    Ref wtfCallbackAggregator = CallbackAggregator::create([this, protectedThis = Ref { *this }, localCallbackAggregator] {
        protectedNetworkProcess()->getResourceLoadStatisticsDataSummary(m_sessionID, [localCallbackAggregator](Vector<ITPThirdPartyData>&& data) {
            localCallbackAggregator->addResult(WTFMove(data));
        });
    });
    
    for (Ref processPool : WebProcessPool::allProcessPools())
        processPool->sendResourceLoadStatisticsDataImmediately([wtfCallbackAggregator] { });
}

void WebsiteDataStore::scheduleClearInMemoryAndPersistent(ShouldGrandfatherStatistics shouldGrandfather, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    protectedNetworkProcess()->scheduleClearInMemoryAndPersistent(m_sessionID, { }, shouldGrandfather, WTFMove(completionHandler));
}

void WebsiteDataStore::scheduleCookieBlockingUpdate(CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->scheduleCookieBlockingUpdate(m_sessionID, WTFMove(completionHandler));
}

void WebsiteDataStore::scheduleStatisticsAndDataRecordsProcessing(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    protectedNetworkProcess()->scheduleStatisticsAndDataRecordsProcessing(m_sessionID, WTFMove(completionHandler));
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

    protectedNetworkProcess()->setLastSeen(m_sessionID, WebCore::RegistrableDomain { url }, seconds, WTFMove(completionHandler));
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

    protectedNetworkProcess()->mergeStatisticForTesting(m_sessionID, WebCore::RegistrableDomain { url }, WebCore::RegistrableDomain { topFrameUrl1 }, WebCore::RegistrableDomain { topFrameUrl2 }, lastSeen, hadUserInteraction, mostRecentUserInteraction, isGrandfathered, isPrevalent, isVeryPrevalent, dataRecordsRemoved, WTFMove(completionHandler));
}

void WebsiteDataStore::insertExpiredStatisticForTesting(const URL& url, unsigned numberOfOperatingDaysPassed, bool hadUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent, CompletionHandler<void()>&& completionHandler)
{
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }

    protectedNetworkProcess()->insertExpiredStatisticForTesting(m_sessionID, WebCore::RegistrableDomain { url }, numberOfOperatingDaysPassed, hadUserInteraction, isScheduledForAllButCookieDataRemoval, isPrevalent, WTFMove(completionHandler));
}

void WebsiteDataStore::setResourceLoadStatisticsTimeAdvanceForTesting(Seconds time, CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->setResourceLoadStatisticsTimeAdvanceForTesting(m_sessionID, time, WTFMove(completionHandler));
}

void WebsiteDataStore::setStorageAccessPromptQuirkForTesting(String&& topFrameDomain, Vector<String>&& subFrameDomains, Vector<String>&& triggerPages, CompletionHandler<void()>&& completionHandler)
{
    auto registrableTopFrameDomain = WebCore::RegistrableDomain::fromRawString(WTFMove(topFrameDomain));
    auto registrableTopFrameDomainString = registrableTopFrameDomain.string();
    Vector<WebCore::OrganizationStorageAccessPromptQuirk> quirk { {
        WTFMove(registrableTopFrameDomainString)
        , HashMap<WebCore::RegistrableDomain, Vector<WebCore::RegistrableDomain>> { {
            KeyValuePair { WTFMove(registrableTopFrameDomain),
                subFrameDomains.map([](auto& domain) { return WebCore::RegistrableDomain::fromRawString(String { domain }); })
            },
        } }, {
            triggerPages.map([](auto& page) { return URL { page }; })
        }
    } };

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    StorageAccessPromptQuirkController::sharedSingleton().setCachedListDataForTesting(WTFMove(quirk));
#else
    protectedNetworkProcess()->send(Messages::NetworkProcess::UpdateStorageAccessPromptQuirks(WTFMove(quirk)), 0);
#endif
    completionHandler();
}

void WebsiteDataStore::grantStorageAccessForTesting(String&& topFrameDomain, Vector<String>&& subFrameDomains, CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::GrantStorageAccessForTesting(sessionID(), subFrameDomains.map([](const String& domain) {
        return WebCore::RegistrableDomain::uncheckedCreateFromHost(domain);
    }), WebCore::RegistrableDomain::uncheckedCreateFromHost((topFrameDomain))), WTFMove(completionHandler));
}

void WebsiteDataStore::setIsRunningResourceLoadStatisticsTest(bool value, CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->setIsRunningResourceLoadStatisticsTest(m_sessionID, value, WTFMove(completionHandler));
}

void WebsiteDataStore::getAllStorageAccessEntries(WebPageProxyIdentifier pageID, CompletionHandler<void(Vector<String>&& domains)>&& completionHandler)
{
    if (!WebProcessProxy::webPage(pageID)) {
        completionHandler({ });
        return;
    }

    protectedNetworkProcess()->getAllStorageAccessEntries(m_sessionID, WTFMove(completionHandler));
}


void WebsiteDataStore::setTimeToLiveUserInteraction(Seconds seconds, CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->setTimeToLiveUserInteraction(m_sessionID, seconds, WTFMove(completionHandler));
}

void WebsiteDataStore::didHaveUserInteractionForSiteIsolation(const URL& url)
{
    if (url.protocolIsAbout() || url.isEmpty())
        return;

    WebCore::RegistrableDomain registrableDomain { url };
    if (m_domainsWithUserInteractions)
        m_domainsWithUserInteractions->add(registrableDomain);
    else if (!m_domainsWithUserInteractionsCompletionHandler.isEmpty()) {
        // Currently waiting for the network process's reply.
        // Add this domain to the hash set we get from the network process
        // since the network process may have replied before it had
        // notififed of user interaction by the web content process.
        m_pendingDomainsWithUserInteractions.append(registrableDomain);
    }
}

void WebsiteDataStore::logUserInteraction(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }

    protectedNetworkProcess()->logUserInteraction(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
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
    
    protectedNetworkProcess()->clearUserInteraction(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
}

void WebsiteDataStore::setGrandfathered(const URL& url, bool isGrandfathered, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    if (url.protocolIsAbout() || url.isEmpty()) {
        completionHandler();
        return;
    }
    
    protectedNetworkProcess()->setGrandfathered(m_sessionID, WebCore::RegistrableDomain { url }, isGrandfathered, WTFMove(completionHandler));
}

void WebsiteDataStore::setCrossSiteLoadWithLinkDecorationForTesting(const URL& fromURL, const URL& toURL, bool wasFiltered, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    
    protectedNetworkProcess()->setCrossSiteLoadWithLinkDecorationForTesting(m_sessionID, WebCore::RegistrableDomain { fromURL }, WebCore::RegistrableDomain { toURL }, wasFiltered, WTFMove(completionHandler));
}

void WebsiteDataStore::resetCrossSiteLoadsWithLinkDecorationForTesting(CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->resetCrossSiteLoadsWithLinkDecorationForTesting(m_sessionID, WTFMove(completionHandler));
}

void WebsiteDataStore::deleteCookiesForTesting(const URL& url, bool includeHttpOnlyCookies, CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->deleteCookiesForTesting(m_sessionID, WebCore::RegistrableDomain { url }, includeHttpOnlyCookies, WTFMove(completionHandler));
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
    protectedNetworkProcess()->setShouldDowngradeReferrerForTesting(enabled, WTFMove(completionHandler));
}

#if !PLATFORM(COCOA)
WebCore::ThirdPartyCookieBlockingMode WebsiteDataStore::thirdPartyCookieBlockingMode() const
{
    if (!m_thirdPartyCookieBlockingMode)
        m_thirdPartyCookieBlockingMode = WebCore::ThirdPartyCookieBlockingMode::All;
    return *m_thirdPartyCookieBlockingMode;
}
#endif

void WebsiteDataStore::setResourceLoadStatisticsShouldBlockThirdPartyCookiesForTesting(bool enabled, WebCore::ThirdPartyCookieBlockingMode blockingMode, CompletionHandler<void()>&& completionHandler)
{
    setThirdPartyCookieBlockingMode(blockingMode, WTFMove(completionHandler));
}

void WebsiteDataStore::setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode blockingMode, CompletionHandler<void()>&& completionHandler)
{
    Ref callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
    if (!isOptInCookiePartitioningEnabled() && blockingMode == WebCore::ThirdPartyCookieBlockingMode::AllExceptPartitioned)
        blockingMode = WebCore::ThirdPartyCookieBlockingMode::All;
#endif

    if (thirdPartyCookieBlockingMode() != blockingMode) {
        m_thirdPartyCookieBlockingMode = blockingMode;
        for (Ref webProcess : processes())
            webProcess->setThirdPartyCookieBlockingMode(blockingMode, [callbackAggregator] { });
    }

    protectedNetworkProcess()->setThirdPartyCookieBlockingMode(m_sessionID, blockingMode, [callbackAggregator] { });
}

void WebsiteDataStore::setResourceLoadStatisticsShouldEnbleSameSiteStrictEnforcementForTesting(bool enabled, CompletionHandler<void()>&& completionHandler)
{
    auto flag = enabled ? WebCore::SameSiteStrictEnforcementEnabled::Yes : WebCore::SameSiteStrictEnforcementEnabled::No;

    protectedNetworkProcess()->setShouldEnbleSameSiteStrictEnforcementForTesting(m_sessionID, flag, WTFMove(completionHandler));
}

void WebsiteDataStore::setResourceLoadStatisticsFirstPartyWebsiteDataRemovalModeForTesting(bool enabled, CompletionHandler<void()>&& completionHandler)
{
    auto mode = enabled ? WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookies : WebCore::FirstPartyWebsiteDataRemovalMode::None;

    protectedNetworkProcess()->setFirstPartyWebsiteDataRemovalModeForTesting(m_sessionID, mode, WTFMove(completionHandler));
}

void WebsiteDataStore::setResourceLoadStatisticsToSameSiteStrictCookiesForTesting(const URL& url, CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->setToSameSiteStrictCookiesForTesting(m_sessionID, WebCore::RegistrableDomain { url }, WTFMove(completionHandler));
}

void WebsiteDataStore::setResourceLoadStatisticsFirstPartyHostCNAMEDomainForTesting(const URL& firstPartyURL, const URL& cnameURL, CompletionHandler<void()>&& completionHandler)
{
    if (cnameURL.host() != "testwebkit.org"_s && cnameURL.host() != "3rdpartytestwebkit.org"_s) {
        completionHandler();
        return;
    }

    protectedNetworkProcess()->setFirstPartyHostCNAMEDomainForTesting(m_sessionID, firstPartyURL.host().toString(), WebCore::RegistrableDomain { cnameURL }, WTFMove(completionHandler));
}

void WebsiteDataStore::setResourceLoadStatisticsThirdPartyCNAMEDomainForTesting(const URL& cnameURL, CompletionHandler<void()>&& completionHandler)
{
    if (cnameURL.host() != "testwebkit.org"_s && cnameURL.host() != "3rdpartytestwebkit.org"_s) {
        completionHandler();
        return;
    }

    protectedNetworkProcess()->setThirdPartyCNAMEDomainForTesting(m_sessionID, WebCore::RegistrableDomain { cnameURL }, WTFMove(completionHandler));
}

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
    protectedNetworkProcess()->setCacheMaxAgeCapForPrevalentResources(m_sessionID, seconds, WTFMove(completionHandler));
}

void WebsiteDataStore::resetCacheMaxAgeCapForPrevalentResources(CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->resetCacheMaxAgeCapForPrevalentResources(m_sessionID, WTFMove(completionHandler));
}

HashSet<RefPtr<WebProcessPool>> WebsiteDataStore::processPools(size_t limit) const
{
    HashSet<RefPtr<WebProcessPool>> processPools;
    for (Ref process : processes()) {
        if (RefPtr processPool = process->processPoolIfExists()) {
            processPools.add(WTFMove(processPool));
            if (processPools.size() == limit)
                break;
        }
    }

    if (processPools.isEmpty()) {
        // Check if we're one of the legacy data stores.
        for (Ref processPool : WebProcessPool::allProcessPools()) {
            processPools.add(WTFMove(processPool));
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

Vector<WebCore::SecurityOriginData> WebsiteDataStore::mediaKeysStorageOrigins(const String& mediaKeysStorageDirectory)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    Vector<WebCore::SecurityOriginData> origins;

    for (const auto& originDirectoryName : FileSystem::listDirectory(mediaKeysStorageDirectory)) {
        auto originDirectory = FileSystem::pathByAppendingComponent(mediaKeysStorageDirectory, originDirectoryName);
        auto mediaKeysStorageFile = computeMediaKeyFile(originDirectory);
        if (!FileSystem::fileExists(mediaKeysStorageFile))
            continue;

        auto originFile = FileSystem::pathByAppendingComponent(originDirectory, "origin"_s);
        if (auto origin = WebCore::StorageUtilities::readOriginFromFile(originFile))
            origins.append(origin->clientOrigin);
    }

    return origins;
}

void WebsiteDataStore::removeMediaKeysStorage(const String& mediaKeysStorageDirectory, WallTime modifiedSince)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    for (const auto& originDirectoryName : FileSystem::listDirectory(mediaKeysStorageDirectory)) {
        auto originDirectory = FileSystem::pathByAppendingComponent(mediaKeysStorageDirectory, originDirectoryName);
        auto mediaKeysStorageFile = computeMediaKeyFile(originDirectory);
        auto modificationTime = FileSystem::fileModificationTime(mediaKeysStorageFile);
        if (!modificationTime)
            continue;

        if (modificationTime.value() < modifiedSince)
            continue;

        FileSystem::deleteFile(mediaKeysStorageFile);
        FileSystem::deleteFile(FileSystem::pathByAppendingComponent(originDirectory, "origin"_s));
        FileSystem::deleteEmptyDirectory(originDirectory);
    }
}

void WebsiteDataStore::removeMediaKeysStorage(const String& mediaKeysStorageDirectory, const HashSet<WebCore::SecurityOriginData>& origins, const FileSystem::Salt& salt)
{
    ASSERT(!mediaKeysStorageDirectory.isEmpty());

    for (const auto& origin : origins) {
        auto originDirectoryName = WebCore::StorageUtilities::encodeSecurityOriginForFileName(salt, origin);
        auto originDirectory = FileSystem::pathByAppendingComponent(mediaKeysStorageDirectory, originDirectoryName);
        auto mediaKeysStorageFile = computeMediaKeyFile(originDirectory);
        FileSystem::deleteFile(mediaKeysStorageFile);
        FileSystem::deleteFile(FileSystem::pathByAppendingComponent(originDirectory, "origin"_s));
        FileSystem::deleteEmptyDirectory(originDirectory);
    }
}

void WebsiteDataStore::getNetworkProcessConnection(WebProcessProxy& webProcessProxy, CompletionHandler<void(NetworkProcessConnectionInfo&&)>&& reply, ShouldRetryOnFailure shouldRetryOnFailure)
{
    Ref networkProcessProxy = networkProcess();
    networkProcessProxy->getNetworkProcessConnection(webProcessProxy, [weakThis = WeakPtr { *this }, networkProcessProxy = WeakPtr { networkProcessProxy }, webProcessProxy = WeakPtr { webProcessProxy }, reply = WTFMove(reply), shouldRetryOnFailure] (NetworkProcessConnectionInfo&& connectionInfo) mutable {
        if (!connectionInfo.connection) [[unlikely]] {
            if (shouldRetryOnFailure == ShouldRetryOnFailure::No || !webProcessProxy) {
                RELEASE_LOG_ERROR(Process, "getNetworkProcessConnection: Failed to get connection to network process, will reply invalid identifier ...");
                reply({ });
                return;
            }

            // Retry on the next RunLoop iteration because we may be inside the WebsiteDataStore destructor.
            RunLoop::mainSingleton().dispatch([weakThis = WTFMove(weakThis), networkProcessProxy = WTFMove(networkProcessProxy), weakWebProcessProxy = WTFMove(webProcessProxy), reply = WTFMove(reply)] () mutable {
                RefPtr protectedThis = weakThis.get();
                RefPtr webProcessProxy = weakWebProcessProxy.get();
                if (protectedThis && webProcessProxy) {
                    // Terminate if it is the same network process.
                    if (networkProcessProxy && protectedThis->m_networkProcess == networkProcessProxy.get())
                        protectedThis->terminateNetworkProcess();
                    RELEASE_LOG_ERROR(Process, "getNetworkProcessConnection: Failed to get connection to network process, will retry ...");
                    protectedThis->getNetworkProcessConnection(*webProcessProxy, WTFMove(reply), ShouldRetryOnFailure::No);
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
    if (RefPtr networkProcess = std::exchange(m_networkProcess, nullptr))
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
    protectedNetworkProcess()->sendProcessDidResume(AuxiliaryProcessProxy::ResumeReason::ForegroundActivity);
}

bool WebsiteDataStore::defaultTrackingPreventionEnabled() const
{
#if PLATFORM(COCOA)
    if (auto enabledOverride = m_configuration->defaultTrackingPreventionEnabledOverride())
        return *enabledOverride;

    return doesAppHaveTrackingPreventionEnabled();
#else
    return false;
#endif
}

bool WebsiteDataStore::trackingPreventionEnabled() const
{
    if (m_trackingPreventionEnabled == TrackingPreventionEnabled::Default)
        return defaultTrackingPreventionEnabled();

    return m_trackingPreventionEnabled == TrackingPreventionEnabled::Yes;
}

bool WebsiteDataStore::resourceLoadStatisticsDebugMode() const
{
    return m_trackingPreventionDebugMode;
}

void WebsiteDataStore::setTrackingPreventionEnabled(bool enabled)
{
    auto targetTrackingPreventionEnabled = enabled ? TrackingPreventionEnabled::Yes : TrackingPreventionEnabled::No;
    if (m_trackingPreventionEnabled == targetTrackingPreventionEnabled)
        return;

    bool valueChanged = trackingPreventionEnabled() != enabled;
    m_trackingPreventionEnabled = targetTrackingPreventionEnabled;
    if (!valueChanged)
        return;

    RELEASE_LOG(Storage, "%p - WebsiteDataStore::setTrackingPreventionEnabled: sessionID=%" PRIu64 ", enabled=%d", this, m_sessionID.toUInt64(), enabled);

    if (RefPtr networkProcessProxy = m_networkProcess)
        networkProcessProxy->send(Messages::NetworkProcess::SetTrackingPreventionEnabled(m_sessionID, enabled), 0);

    for (RefPtr processPool : processPools())
        processPool->sendToAllProcessesForSession(Messages::WebProcess::SetTrackingPreventionEnabled(enabled), m_sessionID);
}

void WebsiteDataStore::setStatisticsTestingCallback(Function<void(const String&)>&& callback)
{
    if (callback)
        protectedNetworkProcess()->send(Messages::NetworkProcess::SetResourceLoadStatisticsLogTestingEvent(true), 0);
    
    m_statisticsTestingCallback = WTFMove(callback);
}

void WebsiteDataStore::setResourceLoadStatisticsDebugMode(bool enabled)
{
    setResourceLoadStatisticsDebugMode(enabled, []() { });
}

void WebsiteDataStore::setResourceLoadStatisticsDebugMode(bool enabled, CompletionHandler<void()>&& completionHandler)
{
    m_trackingPreventionDebugMode = enabled;

    protectedNetworkProcess()->setResourceLoadStatisticsDebugMode(m_sessionID, enabled, WTFMove(completionHandler));
}

void WebsiteDataStore::isResourceLoadStatisticsEphemeral(CompletionHandler<void(bool)>&& completionHandler) const
{
    if (!trackingPreventionEnabled() || !m_sessionID.isEphemeral()) {
        completionHandler(false);
        return;
    }

    protectedNetworkProcess()->isResourceLoadStatisticsEphemeral(m_sessionID, WTFMove(completionHandler));
}

void WebsiteDataStore::setPrivateClickMeasurementDebugMode(bool enabled)
{
    protectedNetworkProcess()->setPrivateClickMeasurementDebugMode(sessionID(), enabled);
}

void WebsiteDataStore::storePrivateClickMeasurement(const WebCore::PrivateClickMeasurement& privateClickMeasurement)
{
    protectedNetworkProcess()->send(Messages::NetworkProcess::StorePrivateClickMeasurement(sessionID(), privateClickMeasurement), 0);
}

void WebsiteDataStore::simulatePrivateClickMeasurementConversion(int priority, int triggerData, const URL& sourceURL, const URL& destinationURL)
{
    protectedNetworkProcess()->send(Messages::NetworkProcess::SimulatePrivateClickMeasurementConversion(sessionID(), priority, triggerData, sourceURL, destinationURL), 0);
}

void WebsiteDataStore::setStorageSiteValidationEnabled(bool enabled)
{
    if (m_storageSiteValidationEnabled == enabled)
        return;

    m_storageSiteValidationEnabled = enabled;

    if (RefPtr networkProcess = networkProcessIfExists())
        networkProcess->send(Messages::NetworkProcess::SetStorageSiteValidationEnabled(sessionID(), m_storageSiteValidationEnabled), 0);
}

void WebsiteDataStore::setPersistedSiteURLs(HashSet<URL>&& urls)
{
    m_persistedSiteURLs = WTFMove(urls);

    HashSet<WebCore::RegistrableDomain> domains;
    for (auto& url : m_persistedSiteURLs)
        domains.add(WebCore::RegistrableDomain { url });

    if (RefPtr networkProcess = networkProcessIfExists())
        networkProcess->send(Messages::NetworkProcess::SetPersistedDomains(sessionID(), domains), 0);
}

void WebsiteDataStore::closeDatabases(CompletionHandler<void()>&& completionHandler)
{
    Ref callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));

    Ref networkProcess = this->networkProcess();
    networkProcess->sendWithAsyncReply(Messages::NetworkProcess::ClosePCMDatabase(m_sessionID), [callbackAggregator] { });

    networkProcess->sendWithAsyncReply(Messages::NetworkProcess::CloseITPDatabase(m_sessionID), [callbackAggregator] { });
}

void WebsiteDataStore::logTestingEvent(const String& event)
{
    ASSERT(RunLoop::isMain());
    
    if (m_statisticsTestingCallback)
        m_statisticsTestingCallback(event);
}

void WebsiteDataStore::clearResourceLoadStatisticsInWebProcesses(CompletionHandler<void()>&& callback)
{
    if (trackingPreventionEnabled()) {
        for (RefPtr processPool : processPools())
            processPool->sendToAllProcessesForSession(Messages::WebProcess::ClearResourceLoadStatistics(), m_sessionID);
    }
    callback();
}

void WebsiteDataStore::didAllowPrivateTokenUsageByThirdPartyForTesting(bool wasAllowed, URL&& resourceURL)
{
    m_client->didAllowPrivateTokenUsageByThirdPartyForTesting(wasAllowed, WTFMove(resourceURL));
}

void WebsiteDataStore::setUserAgentStringQuirkForTesting(const String& domain, const String& userAgentString, CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    StorageAccessUserAgentStringQuirkController::sharedSingleton().setCachedListDataForTesting({ { WebCore::RegistrableDomain::uncheckedCreateFromHost(domain), userAgentString } });
#endif
    completionHandler();
}

void WebsiteDataStore::setPrivateTokenIPCForTesting(bool enabled)
{
    protectedNetworkProcess()->send(Messages::NetworkProcess::SetShouldSendPrivateTokenIPCForTesting(sessionID(), enabled), 0);
}

#if ENABLE(OPT_IN_PARTITIONED_COOKIES) && !PLATFORM(COCOA)
bool WebsiteDataStore::isOptInCookiePartitioningEnabled() const
{
    return std::ranges::any_of(m_processes, [](auto& process) {
        return std::ranges::any_of(process.pages(), [](auto& page) {
            return page->preferences().optInPartitionedCookiesEnabled();
        });
    });
}
#endif

void WebsiteDataStore::propagateSettingUpdates()
{
#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
    RefPtr networkProcess = networkProcessIfExists();
    if (!networkProcess)
        return;

    bool enabled = isOptInCookiePartitioningEnabled();
    if (m_isOptInCookiePartitioningEnabled != enabled && trackingPreventionEnabled()) {
        m_isOptInCookiePartitioningEnabled = enabled;
        networkProcess->send(Messages::NetworkProcess::SetOptInCookiePartitioningEnabled(sessionID(), enabled), 0);

        for (Ref webProcess : processes())
            webProcess->setOptInCookiePartitioningEnabled(enabled);

        if (m_isOptInCookiePartitioningEnabled && m_thirdPartyCookieBlockingMode == WebCore::ThirdPartyCookieBlockingMode::All) {
            RELEASE_LOG(Storage, "WebsiteDataStore::propagateSettingUpdates (%p) sessionID=%" PRIu64 ", OptInCookiePartitioning enabled, setting ThirdPartyCookieBlockingMode::AllExceptPartitioned", this, m_sessionID.toUInt64());
            setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode::AllExceptPartitioned, []() { });
        } else if (!m_isOptInCookiePartitioningEnabled && m_thirdPartyCookieBlockingMode == WebCore::ThirdPartyCookieBlockingMode::AllExceptPartitioned) {
            RELEASE_LOG(Storage, "WebsiteDataStore::propagateSettingUpdates (%p) sessionID=%" PRIu64 ", OptInCookiePartitioning disabled, setting ThirdPartyCookieBlockingMode::All", this, m_sessionID.toUInt64());
            setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode::All, []() { });
        }
    }
#endif
}

void WebsiteDataStore::dispatchOnQueue(Function<void()>&& function)
{
    m_queue->dispatch(WTFMove(function));
}

void WebsiteDataStore::setCacheModelSynchronouslyForTesting(CacheModel cacheModel)
{
    for (Ref processPool : WebProcessPool::allProcessPools())
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
    auto& directories = resolvedDirectories();
    auto resourceLoadStatisticsDirectory = directories.resourceLoadStatisticsDirectory;
    SandboxExtension::Handle resourceLoadStatisticsDirectoryHandle;
    createHandleFromResolvedPathIfPossible(resourceLoadStatisticsDirectory, resourceLoadStatisticsDirectoryHandle);

    auto networkCacheDirectory = directories.networkCacheDirectory;
    SandboxExtension::Handle networkCacheDirectoryExtensionHandle;
    createHandleFromResolvedPathIfPossible(networkCacheDirectory, networkCacheDirectoryExtensionHandle);

    auto hstsStorageDirectory = directories.hstsStorageDirectory;
    SandboxExtension::Handle hstsStorageDirectoryExtensionHandle;
    createHandleFromResolvedPathIfPossible(hstsStorageDirectory, hstsStorageDirectoryExtensionHandle);

#if ENABLE(CONTENT_EXTENSIONS)
    auto resourceMonitorThrottlerDirectory = isPersistent() ? directories.resourceMonitorThrottlerDirectory : emptyString();
    SandboxExtension::Handle resourceMonitorThrottlerDirectoryExtensionHandle;
    if (!resourceMonitorThrottlerDirectory.isEmpty())
        createHandleFromResolvedPathIfPossible(resourceMonitorThrottlerDirectory, resourceMonitorThrottlerDirectoryExtensionHandle);
#endif

#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    auto webContentRestrictionsConfigurationFile = m_configuration->webContentRestrictionsConfigurationFile();
    SandboxExtension::Handle webContentRestrictionsConfigurationExtensionHandle;
    if (!webContentRestrictionsConfigurationFile.isEmpty())
        createHandleFromResolvedPathIfPossible(webContentRestrictionsConfigurationFile, webContentRestrictionsConfigurationExtensionHandle, SandboxExtension::Type::ReadOnly);
#endif

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
    HashSet<WebCore::RegistrableDomain> persistedDomains;
    for (auto& url : m_persistedSiteURLs)
        persistedDomains.add(WebCore::RegistrableDomain { url });
    WebCore::RegistrableDomain resourceLoadStatisticsManualPrevalentResource;
    ResourceLoadStatisticsParameters resourceLoadStatisticsParameters = {
        WTFMove(resourceLoadStatisticsDirectory),
        WTFMove(resourceLoadStatisticsDirectoryHandle),
        trackingPreventionEnabled(),
        hasStatisticsTestingCallback(),
        shouldIncludeLocalhostInResourceLoadStatistics,
        resourceLoadStatisticsDebugMode(),
        thirdPartyCookieBlockingMode(),
        WebCore::SameSiteStrictEnforcementEnabled::No,
        firstPartyWebsiteDataRemovalMode,
        WTFMove(standaloneApplicationDomain),
        WTFMove(appBoundDomains),
        WTFMove(managedDomains),
        WTFMove(persistedDomains),
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
#if ENABLE(OPT_IN_PARTITIONED_COOKIES)
    networkSessionParameters.isOptInCookiePartitioningEnabled = isOptInCookiePartitioningEnabled();
#endif
    networkSessionParameters.cookiesVersion = cookiesVersion();
    networkSessionParameters.unifiedOriginStorageLevel = m_configuration->unifiedOriginStorageLevel();
    networkSessionParameters.perOriginStorageQuota = perOriginStorageQuota();
    networkSessionParameters.originQuotaRatio = originQuotaRatio();
    networkSessionParameters.totalQuotaRatio = m_configuration->totalQuotaRatio();
    networkSessionParameters.standardVolumeCapacity = m_configuration->standardVolumeCapacity();
    networkSessionParameters.volumeCapacityOverride = m_configuration->volumeCapacityOverride();
    networkSessionParameters.localStorageDirectory = directories.localStorageDirectory;
    createHandleFromResolvedPathIfPossible(networkSessionParameters.localStorageDirectory, networkSessionParameters.localStorageDirectoryExtensionHandle);
    networkSessionParameters.indexedDBDirectory = directories.indexedDBDatabaseDirectory;
    createHandleFromResolvedPathIfPossible(networkSessionParameters.indexedDBDirectory, networkSessionParameters.indexedDBDirectoryExtensionHandle);
    networkSessionParameters.generalStorageDirectory = directories.generalStorageDirectory;
    createHandleFromResolvedPathIfPossible(networkSessionParameters.generalStorageDirectory, networkSessionParameters.generalStorageDirectoryHandle);
    networkSessionParameters.cacheStorageDirectory = directories.cacheStorageDirectory;
    createHandleFromResolvedPathIfPossible(networkSessionParameters.cacheStorageDirectory, networkSessionParameters.cacheStorageDirectoryExtensionHandle);

    networkSessionParameters.serviceWorkerRegistrationDirectory = directories.serviceWorkerRegistrationDirectory;
    createHandleFromResolvedPathIfPossible(networkSessionParameters.serviceWorkerRegistrationDirectory, networkSessionParameters.serviceWorkerRegistrationDirectoryExtensionHandle);
    networkSessionParameters.serviceWorkerProcessTerminationDelayEnabled = m_configuration->serviceWorkerProcessTerminationDelayEnabled();
    networkSessionParameters.inspectionForServiceWorkersAllowed = m_inspectionForServiceWorkersAllowed;
    networkSessionParameters.storageSiteValidationEnabled = m_storageSiteValidationEnabled;
#if ENABLE(DECLARATIVE_WEB_PUSH)
    networkSessionParameters.isDeclarativeWebPushEnabled = m_configuration->isDeclarativeWebPushEnabled();
#endif
#if HAVE(NW_PROXY_CONFIG)
    networkSessionParameters.proxyConfigData = m_proxyConfigData;
#endif
#if ENABLE(CONTENT_EXTENSIONS)
    networkSessionParameters.resourceMonitorThrottlerDirectory = WTFMove(resourceMonitorThrottlerDirectory);
    networkSessionParameters.resourceMonitorThrottlerDirectoryExtensionHandle = WTFMove(resourceMonitorThrottlerDirectoryExtensionHandle);
#endif
    networkSessionParameters.isLegacyTLSAllowed = m_configuration->legacyTLSEnabled();

#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
    networkSessionParameters.webContentRestrictionsConfigurationFile = WTFMove(webContentRestrictionsConfigurationFile);
    networkSessionParameters.webContentRestrictionsConfigurationExtensionHandle = WTFMove(webContentRestrictionsConfigurationExtensionHandle);
#endif

    parameters.networkSessionParameters = WTFMove(networkSessionParameters);
    parameters.networkSessionParameters.resourceLoadStatisticsParameters.enabled = trackingPreventionEnabled();
    platformSetNetworkParameters(parameters);
#if PLATFORM(COCOA)
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

        if (auto handleAndFilePath = SandboxExtension::createHandleForTemporaryFile(networkingServiceName, SandboxExtension::Type::ReadWrite))
            parameters.tempDirectoryExtensionHandle = WTFMove(handleAndFilePath->first);
        if (auto handleAndFilePath = SandboxExtension::createHandleForTemporaryFile(emptyString(), SandboxExtension::Type::ReadOnly))
            parameters.tempDirectoryRootExtensionHandle = WTFMove(handleAndFilePath->first);
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
    if (RefPtr manager = dynamicDowncast<MockAuthenticatorManager>(m_authenticatorManager))
        manager->setTestConfiguration(WTFMove(configuration));
    else
        m_authenticatorManager = MockAuthenticatorManager::create(WTFMove(configuration));
}

VirtualAuthenticatorManager& WebsiteDataStore::virtualAuthenticatorManager()
{
    if (!is<VirtualAuthenticatorManager>(m_authenticatorManager.get()))
        m_authenticatorManager = VirtualAuthenticatorManager::create();
    return downcast<VirtualAuthenticatorManager>(m_authenticatorManager.get());
}

Ref<AuthenticatorManager> WebsiteDataStore::protectedAuthenticatorManager()
{
    return authenticatorManager();
}

Ref<VirtualAuthenticatorManager> WebsiteDataStore::protectedVirtualAuthenticatorManager()
{
    return virtualAuthenticatorManager();
}
#endif

API::HTTPCookieStore& WebsiteDataStore::cookieStore()
{
    if (!m_cookieStore)
        lazyInitialize(m_cookieStore, API::HTTPCookieStore::create(*this));

    return *m_cookieStore;
}

Ref<API::HTTPCookieStore> WebsiteDataStore::protectedCookieStore()
{
    return cookieStore();
}

void WebsiteDataStore::resetQuota(CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->resetQuota(m_sessionID, WTFMove(completionHandler));
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

#if ENABLE(ENCRYPTED_MEDIA)
String WebsiteDataStore::defaultMediaKeysHashSaltsStorageDirectory(const String& baseDataDirectory)
{
#if USE(GLIB)
    return websiteDataDirectoryFileSystemRepresentation("mediakeyshashsalts"_s, baseDataDirectory);
#else
    return websiteDataDirectoryFileSystemRepresentation("MediaKeysHashSalts"_s, baseDataDirectory);
#endif
}
#endif

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

#if ENABLE(CONTENT_EXTENSIONS)
String WebsiteDataStore::defaultResourceMonitorThrottlerDirectory(const String& baseDataDirectory)
{
#if PLATFORM(PLAYSTATION) || USE(GLIB)
    return websiteDataDirectoryFileSystemRepresentation("resourcemonitorthrottler"_s, baseDataDirectory);
#else
    return websiteDataDirectoryFileSystemRepresentation("ResourceMonitorThrottler"_s, baseDataDirectory);
#endif
}
#endif

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
    Ref callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
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

    propagateAppBoundDomains(protectedDefaultDataStore().get(), *appBoundDomains);

    for (auto& store : allDataStores().values())
        propagateAppBoundDomains(Ref { store.get() }.ptr(), *appBoundDomains);
}

void WebsiteDataStore::setAppBoundDomainsForITP(const HashSet<WebCore::RegistrableDomain>& domains, CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->setAppBoundDomainsForResourceLoadStatistics(m_sessionID, domains, WTFMove(completionHandler));
}
#endif

#if ENABLE(MANAGED_DOMAINS)
void WebsiteDataStore::forwardManagedDomainsToITPIfInitialized(CompletionHandler<void()>&& completionHandler)
{
    Ref callbackAggregator = CallbackAggregator::create(WTFMove(completionHandler));
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

    propagateManagedDomains(protectedDefaultDataStore().get(), *managedDomains);

    for (auto& store : allDataStores().values())
        propagateManagedDomains(Ref { store.get() }.ptr(), *managedDomains);
}

void WebsiteDataStore::setManagedDomainsForITP(const HashSet<WebCore::RegistrableDomain>& domains, CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->setManagedDomainsForResourceLoadStatistics(m_sessionID, domains, WTFMove(completionHandler));
}
#endif

void WebsiteDataStore::updateBundleIdentifierInNetworkProcess(const String& bundleIdentifier, CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->updateBundleIdentifier(bundleIdentifier, WTFMove(completionHandler));
}

void WebsiteDataStore::clearBundleIdentifierInNetworkProcess(CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->clearBundleIdentifier(WTFMove(completionHandler));
}

void WebsiteDataStore::countNonDefaultSessionSets(CompletionHandler<void(uint64_t)>&& completionHandler)
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

bool WebsiteDataStore::showPersistentNotification(IPC::Connection* connection, const WebCore::NotificationData& notificationData)
{
    if (m_client->showNotification(notificationData))
        return true;

    return WebNotificationManagerProxy::serviceWorkerManagerSingleton().showPersistent(*this, connection, notificationData, nullptr);
}

void WebsiteDataStore::cancelServiceWorkerNotification(const WTF::UUID& notificationID)
{
    WebNotificationManagerProxy::serviceWorkerManagerSingleton().cancel(nullptr, notificationID);
}

void WebsiteDataStore::clearServiceWorkerNotification(const WTF::UUID& notificationID)
{
    Vector<WTF::UUID> notifications = { notificationID };
    WebNotificationManagerProxy::serviceWorkerManagerSingleton().clearNotifications(nullptr, notifications);
}

void WebsiteDataStore::didDestroyServiceWorkerNotification(const WTF::UUID& notificationID)
{
    WebNotificationManagerProxy::serviceWorkerManagerSingleton().didDestroyNotification(nullptr, notificationID);
}

void WebsiteDataStore::openWindowFromServiceWorker(const String& urlString, const WebCore::SecurityOriginData& serviceWorkerOrigin, CompletionHandler<void(std::optional<WebCore::PageIdentifier>)>&& callback)
{
    auto innerCallback = [callback = WTFMove(callback)] (WebPageProxy* newPage) mutable {
        if (!newPage) {
            callback(std::nullopt);
            return;
        }

        if (!newPage->protectedPageLoadState()->isLoading()) {
            RELEASE_LOG(Loading, "The WKWebView provided in response to a ServiceWorker openWindow request was not in the loading state");
            callback(std::nullopt);
            return;
        }

        if (RefPtr mainFrame = newPage->mainFrame()) {
            mainFrame->setNavigationCallback([callback = WTFMove(callback)](auto pageID, auto) mutable {
                callback(pageID);
            });
            return;
        }

        callback(std::nullopt);
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

Ref<DownloadProxy> WebsiteDataStore::createDownloadProxy(Ref<API::DownloadClient>&& client, const WebCore::ResourceRequest& request, WebPageProxy* originatingPage, const std::optional<FrameInfoData>& frameInfo)
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
    if (RefPtr initiatingPage = downloadProxy.originatingPage()) {
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

    Vector<uint8_t> downloadProgressAccessToken;
#if HAVE(MODERN_DOWNLOADPROGRESS)
    downloadProgressAccessToken = activityAccessToken();
#endif

    protectedNetworkProcess()->send(Messages::NetworkProcess::ResumeDownload(m_sessionID, downloadProxy.downloadID(), resumeData.span(), path, WTFMove(sandboxExtensionHandle), callDownloadDidStart, downloadProgressAccessToken.span()), 0);
}

bool WebsiteDataStore::hasActivePages()
{
    // FIXME: Drop SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE once <rdar://150855062> is fixed.
    SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE return std::ranges::any_of(WebProcessPool::allProcessPools(), [&](auto& pool) {
        return pool->hasPagesUsingWebsiteDataStore(*this);
    });
}

#if HAVE(NW_PROXY_CONFIG)
void WebsiteDataStore::clearProxyConfigData()
{
    protectedNetworkProcess()->send(Messages::NetworkProcess::ClearProxyConfigData(m_sessionID), 0);
}

void WebsiteDataStore::setProxyConfigData(Vector<std::pair<Vector<uint8_t>, std::optional<WTF::UUID>>>&& data)
{
    m_proxyConfigData = std::nullopt;
    protectedNetworkProcess()->send(Messages::NetworkProcess::SetProxyConfigData(m_sessionID, data), 0);
    m_proxyConfigData = WTFMove(data);
}
#endif // HAVE(NW_PROXY_CONFIG)

void WebsiteDataStore::setCompletionHandlerForRemovalFromNetworkProcess(CompletionHandler<void(String&&)>&& completionHandler)
{
    if (m_completionHandlerForRemovalFromNetworkProcess)
        m_completionHandlerForRemovalFromNetworkProcess("New completion handler is set"_s);

    m_completionHandlerForRemovalFromNetworkProcess = WTFMove(completionHandler);
}

void WebsiteDataStore::setOriginQuotaRatioEnabledForTesting(bool enabled, CompletionHandler<void()>&& completionHandler)
{
    RefPtr networkProcess = networkProcessIfExists();
    if (!networkProcess)
        return completionHandler();

    networkProcess->sendWithAsyncReply(Messages::NetworkProcess::SetOriginQuotaRatioEnabledForTesting(m_sessionID, enabled), WTFMove(completionHandler));
}

void WebsiteDataStore::getAppBadgeForTesting(CompletionHandler<void(std::optional<uint64_t>)>&& completionHandler)
{
    RefPtr networkProcess = networkProcessIfExists();
    if (!networkProcess)
        return completionHandler(std::nullopt);

    networkProcess->sendWithAsyncReply(Messages::NetworkProcess::GetAppBadgeForTesting(m_sessionID), WTFMove(completionHandler));
}

void WebsiteDataStore::updateServiceWorkerInspectability()
{
    if (!m_pages.computeSize())
        return;

    bool wasInspectable = m_inspectionForServiceWorkersAllowed;
    m_inspectionForServiceWorkersAllowed = [&] {
#if ENABLE(REMOTE_INSPECTOR)
        for (Ref page : m_pages) {
            if (page->inspectable())
                return true;
        }
#endif // ENABLE(REMOTE_INSPECTOR)
        return false;
    }();

    if (wasInspectable == m_inspectionForServiceWorkersAllowed)
        return;

    if (RefPtr networkProcess = networkProcessIfExists())
        networkProcess->send(Messages::NetworkProcess::SetInspectionForServiceWorkersAllowed(m_sessionID, m_inspectionForServiceWorkersAllowed), 0);
}

void WebsiteDataStore::addPage(WebPageProxy& page)
{
    m_pages.add(page);

    updateServiceWorkerInspectability();
}

void WebsiteDataStore::removePage(WebPageProxy& page)
{
    m_pages.remove(page);

    updateServiceWorkerInspectability();
}

void WebsiteDataStore::processPushMessage(WebPushMessage&& pushMessage, CompletionHandler<void(bool)>&& completionHandler)
{
#if ENABLE(DECLARATIVE_WEB_PUSH)
    bool isDeclarative = !!pushMessage.notificationPayload;
    auto innerHandler = [this, protectedThis = Ref { *this }, isDeclarative, pushMessageCopy = pushMessage, completionHandler = WTFMove(completionHandler)](bool handled, std::optional<WebCore::NotificationPayload>&& resultPayload) mutable {
        if (!isDeclarative || !m_configuration->isDeclarativeWebPushEnabled()) {
            completionHandler(handled);
            return;
        }

        // There was a proposed payload going in, so we require there be one to display now.
        RELEASE_ASSERT(resultPayload);
        pushMessageCopy.notificationPayload = WTFMove(*resultPayload);

        handled = showPersistentNotification(nullptr, pushMessageCopy.notificationPayloadToCoreData());

        if (pushMessageCopy.notificationPayload->appBadge)
            m_client->workerUpdatedAppBadge(WebCore::SecurityOriginData::fromURL(pushMessageCopy.registrationURL), *pushMessageCopy.notificationPayload->appBadge);
        completionHandler(handled);
    };

    // For immutable, declarative push messages, display the payload right now.
    if (pushMessage.notificationPayload && !pushMessage.notificationPayload->isMutable && m_configuration->isDeclarativeWebPushEnabled()) {
        innerHandler(true, WTFMove(pushMessage.notificationPayload));
        return;
    }
#else
    auto innerHandler = [completionHandler = WTFMove(completionHandler)] (bool handled, std::optional<WebCore::NotificationPayload>&&) mutable {
        completionHandler(handled);
    };
#endif // ENABLE(DECLARATIVE_WEB_PUSH)

    RELEASE_LOG(Push, "Sending push message to network process to handle");
    protectedNetworkProcess()->processPushMessage(sessionID(), WTFMove(pushMessage), WTFMove(innerHandler));
}

RestrictedOpenerType WebsiteDataStore::openerTypeForDomain(const WebCore::RegistrableDomain& domain) const
{
    if (!m_restrictedOpenerTypesForTesting.isEmpty()) [[unlikely]] {
        auto it = m_restrictedOpenerTypesForTesting.find(domain);
        return it == m_restrictedOpenerTypesForTesting.end() ? RestrictedOpenerType::Unrestricted : it->value;
    }

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    return RestrictedOpenerDomainsController::singleton().lookup(domain);
#else
    return RestrictedOpenerType::Unrestricted;
#endif
}

void WebsiteDataStore::setRestrictedOpenerTypeForDomainForTesting(const WebCore::RegistrableDomain& domain, RestrictedOpenerType type)
{
    if (domain.isEmpty())
        return;

    m_restrictedOpenerTypesForTesting.set(domain, type);
}

void WebsiteDataStore::fetchLocalStorage(CompletionHandler<void(std::optional<HashMap<WebCore::ClientOrigin, HashMap<String, String>>>&&)>&& completionHandler)
{
    if (RefPtr networkProcess = networkProcessIfExists())
        networkProcess->fetchLocalStorage(m_sessionID, WTFMove(completionHandler));
    else
        protectedNetworkProcess()->fetchLocalStorage(m_sessionID, WTFMove(completionHandler));
}

void WebsiteDataStore::restoreLocalStorage(HashMap<WebCore::ClientOrigin, HashMap<String, String>>&& localStorage, CompletionHandler<void(bool)>&& completionHandler)
{
    protectedNetworkProcess()->restoreLocalStorage(m_sessionID, WTFMove(localStorage), WTFMove(completionHandler));
}

#if ENABLE(WEB_PUSH_NOTIFICATIONS)
bool WebsiteDataStore::builtInNotificationsEnabled() const
{
    if (!m_pages.computeSize())
        return defaultBuiltInNotificationsEnabled();

    for (Ref page : m_pages) {
        if (page->protectedPreferences()->builtInNotificationsEnabled())
            return true;
    }
    return false;
}
#endif

#if ENABLE(CONTENT_EXTENSIONS)
void WebsiteDataStore::resetResourceMonitorThrottlerForTesting(CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->resetResourceMonitorThrottlerForTesting(m_sessionID, WTFMove(completionHandler));
}
#endif

void WebsiteDataStore::setCookies(Vector<WebCore::Cookie>&& cookies, CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->sendWithAsyncReply(Messages::WebCookieManager::SetCookie(m_sessionID, WTFMove(cookies), ++m_cookiesVersion), WTFMove(completionHandler));
}

void WebsiteDataStore::setStorageAccessPermissionForTesting(bool granted, WebPageProxyIdentifier webPageProxyID, const String& topFrame, const String& subFrame, CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::SetStorageAccessPermissionForTesting(m_sessionID, granted, webPageProxyID, WebCore::RegistrableDomain(URL(topFrame)), WebCore::RegistrableDomain(URL(subFrame))), WTFMove(completionHandler));
}

void WebsiteDataStore::clearStorageAccessForTesting(CompletionHandler<void()>&& completionHandler)
{
    protectedNetworkProcess()->sendWithAsyncReply(Messages::NetworkProcess::ClearStorageAccessForTesting(m_sessionID), WTFMove(completionHandler));
}

void WebsiteDataStore::isStorageSuspendedForTesting(CompletionHandler<void(bool)>&& completionHandler) const
{
    protectedNetworkProcess()->isStorageSuspendedForTesting(m_sessionID, WTFMove(completionHandler));
}

} // namespace WebKit
