/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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
#include "NetworkStorageManager.h"

#include "BackgroundFetchChange.h"
#include "BackgroundFetchStoreManager.h"
#include "CacheStorageCache.h"
#include "CacheStorageDiskStore.h"
#include "CacheStorageManager.h"
#include "CacheStorageRegistry.h"
#include "FileSystemStorageHandleRegistry.h"
#include "FileSystemStorageManager.h"
#include "IDBStorageConnectionToClient.h"
#include "IDBStorageManager.h"
#include "IDBStorageRegistry.h"
#include "LocalStorageManager.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkStorageManagerMessages.h"
#include "OriginQuotaManager.h"
#include "OriginStorageManager.h"
#include "ServiceWorkerStorageManager.h"
#include "SessionStorageManager.h"
#include "StorageAreaBase.h"
#include "StorageAreaMapMessages.h"
#include "StorageAreaRegistry.h"
#include "UnifiedOriginStorageLevel.h"
#include "WebsiteDataType.h"
#include <WebCore/DOMCacheEngine.h>
#include <WebCore/IDBRequestData.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/ServiceWorkerContextData.h>
#include <WebCore/StorageEstimate.h>
#include <WebCore/StorageUtilities.h>
#include <WebCore/UniqueIDBDatabaseConnection.h>
#include <WebCore/UniqueIDBDatabaseTransaction.h>
#include <algorithm>
#include <pal/crypto/CryptoDigest.h>
#include <ranges>
#include <wtf/SuspendableWorkQueue.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/Base64.h>
#include <wtf/text/MakeString.h>

#define MESSAGE_CHECK(assertion, connection) MESSAGE_CHECK_BASE(assertion, connection)
#define MESSAGE_CHECK_COMPLETION(assertion, connection, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, connection, completion)

namespace WebKit {

#if PLATFORM(IOS_FAMILY)
static const Seconds defaultBackupExclusionPeriod { 24_h };
#endif

static constexpr double defaultThirdPartyOriginQuotaRatio = 0.1; // third-party_origin_quota / origin_quota
static constexpr uint64_t defaultVolumeCapacityUnit = 1 * GB;
static constexpr auto persistedFileName = "persisted"_s;
static constexpr Seconds originLastModificationTimeUpdateInterval = 30_s;

// FIXME: Remove this if rdar://104754030 is fixed.
static HashMap<String, ThreadSafeWeakPtr<NetworkStorageManager>>& activePaths()
{
    static MainRunLoopNeverDestroyed<HashMap<String, ThreadSafeWeakPtr<NetworkStorageManager>>> pathToManagerMap;
    return pathToManagerMap;
}

static String encode(const String& string, FileSystem::Salt salt)
{
    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    auto utf8String = string.utf8();
    crypto->addBytes(byteCast<uint8_t>(utf8String.span()));
    crypto->addBytes(salt);
    return base64URLEncodeToString(crypto->computeHash());
}

static String originDirectoryPath(const String& rootPath, const WebCore::ClientOrigin& origin, FileSystem::Salt salt)
{
    if (rootPath.isEmpty())
        return emptyString();

    auto encodedTopOrigin = encode(origin.topOrigin.toString(), salt);
    auto encodedOpeningOrigin = encode(origin.clientOrigin.toString(), salt);
    return FileSystem::pathByAppendingComponents(rootPath, std::initializer_list<StringView> { encodedTopOrigin, encodedOpeningOrigin });
}

static String originFilePath(const String& directory)
{
    if (directory.isEmpty())
        return emptyString();

    return FileSystem::pathByAppendingComponent(directory, OriginStorageManager::originFileIdentifier());
}

static bool isEmptyOriginDirectory(const String& directory)
{
    auto children = FileSystem::listDirectory(directory);
    if (children.isEmpty())
        return true;

    if (children.size() > 2)
        return false;

    HashSet<String> invalidFileNames {
        OriginStorageManager::originFileIdentifier()
#if PLATFORM(COCOA)
        , ".DS_Store"_s
#endif
    };
    return std::ranges::all_of(children, [&](auto& child) {
        return invalidFileNames.contains(child);
    });
}

static void deleteEmptyOriginDirectory(const String& directory)
{
    if (directory.isEmpty())
        return;

    if (isEmptyOriginDirectory(directory))
        FileSystem::deleteNonEmptyDirectory(directory);

    FileSystem::deleteEmptyDirectory(directory);
    FileSystem::deleteEmptyDirectory(FileSystem::parentPath(directory));
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(NetworkStorageManager);

String NetworkStorageManager::persistedFilePath(const WebCore::ClientOrigin& origin)
{
    auto directory = originDirectoryPath(m_path, origin, m_salt);
    if (directory.isEmpty())
        return emptyString();

    return FileSystem::pathByAppendingComponent(directory, persistedFileName);
}

Ref<NetworkStorageManager> NetworkStorageManager::create(NetworkProcess& process, PAL::SessionID sessionID, Markable<WTF::UUID> identifier, std::optional<IPC::Connection::UniqueID> connection, const String& path, const String& customLocalStoragePath, const String& customIDBStoragePath, const String& customCacheStoragePath, const String& customServiceWorkerStoragePath, uint64_t defaultOriginQuota, std::optional<double> originQuotaRatio, std::optional<double> totalQuotaRatio, std::optional<uint64_t> standardVolumeCapacity, std::optional<uint64_t> volumeCapacityOverride, UnifiedOriginStorageLevel level, bool storageSiteValidationEnabled)
{
    return adoptRef(*new NetworkStorageManager(process, sessionID, identifier, connection, path, customLocalStoragePath, customIDBStoragePath, customCacheStoragePath, customServiceWorkerStoragePath, defaultOriginQuota, originQuotaRatio, totalQuotaRatio, standardVolumeCapacity, volumeCapacityOverride, level, storageSiteValidationEnabled));
}

static ASCIILiteral queueName(PAL::SessionID sessionID)
{
    if (sessionID.isEphemeral())
        return "com.apple.WebKit.Storage.ephemeral"_s;
    return "com.apple.WebKit.Storage.persistent"_s;
}

NetworkStorageManager::NetworkStorageManager(NetworkProcess& process, PAL::SessionID sessionID, Markable<WTF::UUID> identifier, std::optional<IPC::Connection::UniqueID> connection, const String& path, const String& customLocalStoragePath, const String& customIDBStoragePath, const String& customCacheStoragePath, const String& customServiceWorkerStoragePath, uint64_t defaultOriginQuota, std::optional<double> originQuotaRatio, std::optional<double> totalQuotaRatio, std::optional<uint64_t> standardVolumeCapacity, std::optional<uint64_t> volumeCapacityOverride, UnifiedOriginStorageLevel level, bool storageSiteValidationEnabled)
    : m_process(process)
    , m_sessionID(sessionID)
    , m_queue(SuspendableWorkQueue::create(queueName(sessionID), SuspendableWorkQueue::QOS::Default, SuspendableWorkQueue::ShouldLog::Yes))
    , m_parentConnection(connection)
{
    ASSERT(RunLoop::isMain());

    if (!path.isEmpty()) {
        auto addResult = activePaths().add(path, *this);
        if (!addResult.isNewEntry) {
            if (auto existingManager = addResult.iterator->value.get())
                RELEASE_LOG_ERROR(Storage, "%p - NetworkStorageManager::NetworkStorageManager path for session %" PRIu64 " is already in use by session %" PRIu64, this, m_sessionID.toUInt64(), existingManager->sessionID().toUInt64());
            else
                addResult.iterator->value = *this;
        }
    }
    m_pathNormalizedMainThread = FileSystem::lexicallyNormal(path);
    m_customIDBStoragePathNormalizedMainThread = FileSystem::lexicallyNormal(customIDBStoragePath);

    workQueue().dispatch([this, weakThis = ThreadSafeWeakPtr { *this }, path = path.isolatedCopy(), customLocalStoragePath = crossThreadCopy(customLocalStoragePath), customIDBStoragePath = crossThreadCopy(customIDBStoragePath), customCacheStoragePath = crossThreadCopy(customCacheStoragePath), customServiceWorkerStoragePath = crossThreadCopy(customServiceWorkerStoragePath), defaultOriginQuota, originQuotaRatio, totalQuotaRatio, standardVolumeCapacity, volumeCapacityOverride, level, storageSiteValidationEnabled]() mutable {
        assertIsCurrent(workQueue());

        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        m_defaultOriginQuota = defaultOriginQuota;
        m_originQuotaRatio = originQuotaRatio;
        m_totalQuotaRatio = totalQuotaRatio;
        m_standardVolumeCapacity = standardVolumeCapacity;
        m_volumeCapacityOverride = volumeCapacityOverride;
#if PLATFORM(IOS_FAMILY)
        m_backupExclusionPeriod = defaultBackupExclusionPeriod;
#endif
        setStorageSiteValidationEnabledInternal(storageSiteValidationEnabled);
        m_fileSystemStorageHandleRegistry = FileSystemStorageHandleRegistry::create();
        lazyInitialize(m_storageAreaRegistry, makeUnique<StorageAreaRegistry>());
        lazyInitialize(m_idbStorageRegistry, makeUnique<IDBStorageRegistry>());
        lazyInitialize(m_cacheStorageRegistry, CacheStorageRegistry::create());
        m_unifiedOriginStorageLevel = level;
        m_path = path;
        m_customLocalStoragePath = customLocalStoragePath;
        m_customIDBStoragePath = customIDBStoragePath;
        m_customCacheStoragePath = customCacheStoragePath;
        m_customServiceWorkerStoragePath = customServiceWorkerStoragePath;
        if (!m_path.isEmpty()) {
            auto saltPath = FileSystem::pathByAppendingComponent(m_path, "salt"_s);
            m_salt = valueOrDefault(FileSystem::readOrMakeSalt(saltPath));
        }
        if (shouldManageServiceWorkerRegistrationsByOrigin())
            migrateServiceWorkerRegistrationsToOrigins();
        else
            m_sharedServiceWorkerStorageManager = makeUnique<ServiceWorkerStorageManager>(m_customServiceWorkerStoragePath);
#if PLATFORM(IOS_FAMILY)
        // Exclude LocalStorage directory to reduce backup traffic. See https://webkit.org/b/168388.
        if (m_unifiedOriginStorageLevel == UnifiedOriginStorageLevel::None  && !m_customLocalStoragePath.isEmpty()) {
            FileSystem::makeAllDirectories(m_customLocalStoragePath);
            FileSystem::setExcludedFromBackup(m_customLocalStoragePath, true);
        }
#endif

        IDBStorageManager::createVersionDirectoryIfNeeded(m_customIDBStoragePath);
        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis)] { });
    });
}

NetworkStorageManager::~NetworkStorageManager()
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_closed);
}

RefPtr<NetworkProcess> NetworkStorageManager::protectedProcess() const
{
    return m_process.get();
}

bool NetworkStorageManager::canHandleTypes(OptionSet<WebsiteDataType> types)
{
    return allManagedTypes().containsAny(types);
}

OptionSet<WebsiteDataType> NetworkStorageManager::allManagedTypes()
{
    return {
        WebsiteDataType::LocalStorage,
        WebsiteDataType::SessionStorage,
        WebsiteDataType::FileSystem,
        WebsiteDataType::IndexedDBDatabases,
        WebsiteDataType::DOMCache,
        WebsiteDataType::ServiceWorkerRegistrations
    };
}

void NetworkStorageManager::close(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_closed = true;
    m_connections.forEach([] (auto& connection) {
        connection.removeWorkQueueMessageReceiver(Messages::NetworkStorageManager::messageReceiverName());
    });

    workQueue().dispatch([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)]() mutable {
        assertIsCurrent(workQueue());

        m_originStorageManagers.clear();
        m_fileSystemStorageHandleRegistry = nullptr;
        for (auto&& completionHandler : std::exchange(m_persistCompletionHandlers, { }))
            completionHandler.second(false);
        m_sharedServiceWorkerStorageManager = nullptr;

        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::startReceivingMessageFromConnection(IPC::Connection& connection, const Vector<WebCore::RegistrableDomain>& allowedSites, const SharedPreferencesForWebProcess& preferences)
{
    ASSERT(RunLoop::isMain());

    workQueue().dispatch([this, protectedThis = Ref { *this }, connection = connection.uniqueID(), preferences]() mutable {
        assertIsCurrent(workQueue());
        ASSERT(!m_preferencesForConnections.contains(connection));
        m_preferencesForConnections.add(connection, preferences);

        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis)] { });
    });

    connection.addWorkQueueMessageReceiver(Messages::NetworkStorageManager::messageReceiverName(), m_queue.get(), *this);
    m_connections.add(connection);
    addAllowedSitesForConnection(connection.uniqueID(), allowedSites);
}

void NetworkStorageManager::stopReceivingMessageFromConnection(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());
    
    if (!m_connections.remove(connection))
        return;

    connection.removeWorkQueueMessageReceiver(Messages::NetworkStorageManager::messageReceiverName());
    workQueue().dispatch([this, protectedThis = Ref { *this }, connection = connection.uniqueID()]() mutable {
        assertIsCurrent(workQueue());
        m_idbStorageRegistry->removeConnectionToClient(connection);
        m_originStorageManagers.removeIf([&](auto& entry) {
            auto& manager = entry.value;
            manager->connectionClosed(connection);
            bool shouldRemove = !manager->isActive() && !manager->hasDataInMemory();
            if (shouldRemove) {
                manager->deleteEmptyDirectory();
                deleteEmptyOriginDirectory(manager->path());
            }
            return shouldRemove;
        });
        m_temporaryBlobPathsByConnection.remove(connection);
        if (m_allowedSitesForConnections)
            m_allowedSitesForConnections->remove(connection);

        ASSERT(m_preferencesForConnections.contains(connection));
        m_preferencesForConnections.remove(connection);

        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis)] { });
    });
}

void NetworkStorageManager::updateSharedPreferencesForConnection(IPC::Connection& connection, const SharedPreferencesForWebProcess& preferences)
{
    ASSERT(RunLoop::isMain());

    workQueue().dispatch([this, protectedThis = Ref { *this }, connection = connection.uniqueID(), preferences]() mutable {
        assertIsCurrent(workQueue());
        if (auto iter = m_preferencesForConnections.find(connection); iter != m_preferencesForConnections.end())
            iter->value = preferences;

        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis)] { });
    });
}

#if PLATFORM(IOS_FAMILY)

void NetworkStorageManager::includeOriginInBackupIfNecessary(OriginStorageManager& manager)
{
    if (manager.includedInBackup())
        return;

    auto originFileCreationTimestamp = manager.originFileCreationTimestamp();
    if (!originFileCreationTimestamp)
        return;

    if (WallTime::now() - originFileCreationTimestamp.value() < m_backupExclusionPeriod)
        return;
    
    FileSystem::setExcludedFromBackup(manager.path(), false);
    manager.markIncludedInBackup();
}

#endif

void NetworkStorageManager::writeOriginToFileIfNecessary(const WebCore::ClientOrigin& origin, StorageAreaBase* storageArea)
{
    assertIsCurrent(workQueue());
    CheckedPtr manager = m_originStorageManagers.get(origin);
    if (!manager)
        return;

    if (manager->originFileCreationTimestamp()) {
#if PLATFORM(IOS_FAMILY)
        includeOriginInBackupIfNecessary(*manager);
#endif
        return;
    }

    auto originDirectory = manager->path();
    if (originDirectory.isEmpty())
        return;

    if (storageArea && isEmptyOriginDirectory(originDirectory))
        return;

    auto originFile = originFilePath(originDirectory);
    bool didWrite = WebCore::StorageUtilities::writeOriginToFile(originFile, origin);
    auto timestamp = FileSystem::fileCreationTime(originFile);
    manager->setOriginFileCreationTimestamp(timestamp);
#if PLATFORM(IOS_FAMILY)
    if (didWrite)
        FileSystem::setExcludedFromBackup(originDirectory, true);
    else
        includeOriginInBackupIfNecessary(*manager);
#else
    UNUSED_PARAM(didWrite);
#endif
}

void NetworkStorageManager::spaceGrantedForOrigin(const WebCore::ClientOrigin& origin, uint64_t amount)
{
    assertIsCurrent(workQueue());

    updateLastModificationTimeForOrigin(origin);
    if (!m_totalQuotaRatio)
        return;

    if (!m_totalQuota) {
        std::optional<uint64_t> volumeCapacity;
        if (m_volumeCapacityOverride)
            volumeCapacity = m_volumeCapacityOverride;
        else if (auto capacity = FileSystem::volumeCapacity(m_path))
            volumeCapacity = WTF::roundUpToMultipleOf(defaultVolumeCapacityUnit, *capacity);
        if (volumeCapacity)
            m_totalQuota = *m_totalQuotaRatio * *volumeCapacity;
        else
            return;
    }

    if (m_totalUsage)
        m_totalUsage = *m_totalUsage + amount;

    if (!m_totalUsage || *m_totalUsage > *m_totalQuota)
        schedulePerformEviction();
}

void NetworkStorageManager::schedulePerformEviction()
{
    assertIsCurrent(workQueue());

    if (m_isEvictionScheduled)
        return;

    m_isEvictionScheduled = true;
    prepareForEviction();
}

void NetworkStorageManager::prepareForEviction()
{
    assertIsCurrent(workQueue());

    RunLoop::mainSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }]() mutable {
        auto protectedThis = weakThis.get();
        if (!protectedThis || protectedThis->m_closed || !protectedThis->m_process)
            return;

        protectedThis->protectedProcess()->registrableDomainsWithLastAccessedTime(protectedThis->m_sessionID, [weakThis = WTF::move(weakThis)](auto result) mutable {
            auto protectedThis = weakThis.get();
            if (!protectedThis || protectedThis->m_closed)
                return;

            protectedThis->workQueue().dispatch([weakThis = WTF::move(weakThis), result = crossThreadCopy(WTF::move(result))]() mutable {
                if (auto protectedThis = weakThis.get()) {
                    protectedThis->donePrepareForEviction(WTF::move(result));
                    RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis)] { });
                }
            });
        });
    });
}

WallTime NetworkStorageManager::lastModificationTimeForOrigin(const WebCore::ClientOrigin& origin, OriginStorageManager& manager) const
{
    WallTime lastModificationTime;
    switch (m_unifiedOriginStorageLevel) {
    case UnifiedOriginStorageLevel::None: {
        auto localStoragePath = LocalStorageManager::localStorageFilePath(m_customLocalStoragePath, origin);
        auto localStorageModificationTime = valueOrDefault(FileSystem::fileModificationTime(localStoragePath));
        lastModificationTime = std::max(localStorageModificationTime, lastModificationTime);
        auto idbStoragePath = IDBStorageManager::idbStorageOriginDirectory(m_customIDBStoragePath, origin);
        auto idbStorageModificationTime = valueOrDefault(FileSystem::fileModificationTime(idbStoragePath));
        lastModificationTime = std::max(idbStorageModificationTime, lastModificationTime);
        [[fallthrough]];
    }
    case UnifiedOriginStorageLevel::Basic: {
        auto cacheStoragePath = CacheStorageManager::cacheStorageOriginDirectory(m_customCacheStoragePath, origin);
        auto cacheStorageModificationTime = valueOrDefault(FileSystem::fileModificationTime(cacheStoragePath));
        lastModificationTime = std::max(cacheStorageModificationTime, lastModificationTime);
        [[fallthrough]];
    }
    case UnifiedOriginStorageLevel::Standard: {
        auto originFile = originFilePath(manager.path());
        auto originFileModificationTime = valueOrDefault(FileSystem::fileModificationTime(originFile));
        lastModificationTime = std::max(originFileModificationTime, lastModificationTime);
    }
    }

    return lastModificationTime;
}

void NetworkStorageManager::donePrepareForEviction(const std::optional<HashMap<WebCore::RegistrableDomain, WallTime>>& domainsWithLastAccessedTime)
{
    assertIsCurrent(workQueue());

    HashMap<WebCore::SecurityOriginData, AccessRecord> originRecords;
    uint64_t totalUsage = 0;
    for (auto& origin : getAllOrigins()) {
        auto usage = protect(checkedOriginStorageManager(origin)->quotaManager())->usage();
        totalUsage += usage;
        WallTime accessTime;
        if (domainsWithLastAccessedTime)
            accessTime = domainsWithLastAccessedTime->get(WebCore::RegistrableDomain { origin.topOrigin });
        else
            accessTime = lastModificationTimeForOrigin(origin, checkedOriginStorageManager(origin));

        auto& record = originRecords.ensure(origin.topOrigin, [&] {
            return AccessRecord { };
        }).iterator->value;
        record.usage += usage;
        if (record.lastAccessTime < accessTime)
            record.lastAccessTime = accessTime;

        record.clientOrigins.append(origin.clientOrigin);
        bool removed = removeOriginStorageManagerIfPossible(origin);
        if (!removed)
            record.isActive = true;
        if (!record.isPersisted && persistedInternal(WebCore::ClientOrigin { origin.topOrigin, origin.topOrigin }))
            record.isPersisted = true;
    }

    m_totalUsage = totalUsage;
    performEviction(WTF::move(originRecords));
}

void NetworkStorageManager::performEviction(HashMap<WebCore::SecurityOriginData, AccessRecord>&& originRecords)
{
    assertIsCurrent(workQueue());

    m_isEvictionScheduled = false;
    ASSERT(m_totalQuota);
    if (!m_totalUsage || *m_totalUsage <= *m_totalQuota)
        return;

    Vector<std::pair<WebCore::SecurityOriginData, AccessRecord>> sortedOriginRecords;
    for (auto&& [origin, record] : originRecords)
        sortedOriginRecords.append({ WTF::move(origin), WTF::move(record) });

    std::ranges::sort(sortedOriginRecords, [](auto& a, auto& b) {
        return a.second.lastAccessTime > b.second.lastAccessTime;
    });

    uint64_t deletedOriginCount = 0;
    while (!sortedOriginRecords.isEmpty() && *m_totalUsage > *m_totalQuota) {
        auto [topOrigin, record] = sortedOriginRecords.takeLast();
        if (record.isActive || valueOrDefault(record.isPersisted))
            continue;

        for (auto& clientOrigin : record.clientOrigins) {
            auto origin = WebCore::ClientOrigin { topOrigin, clientOrigin };
            checkedOriginStorageManager(origin)->deleteData(allManagedTypes(), -WallTime::infinity());
            removeOriginStorageManagerIfPossible(origin);
        }

        m_totalUsage = *m_totalUsage - record.usage;
        ++deletedOriginCount;
    }

    UNUSED_PARAM(deletedOriginCount);
    RELEASE_LOG(Storage, "%p - NetworkStorageManager::performEviction evicts %" PRIu64 " origins, current usage %" PRIu64 ", total quota %" PRIu64, this, deletedOriginCount, valueOrDefault(m_totalUsage), *m_totalQuota);
}

OriginQuotaManager::Parameters NetworkStorageManager::originQuotaManagerParameters(const WebCore::ClientOrigin& origin)
{
    OriginQuotaManager::IncreaseQuotaFunction increaseQuotaFunction = [sessionID = m_sessionID, origin, connection = m_parentConnection] (auto identifier, auto currentQuota, auto currentUsage, auto requestedIncrease) mutable {
        if (connection)
            IPC::Connection::send(*connection, Messages::NetworkProcessProxy::IncreaseQuota(sessionID, origin, identifier, currentQuota, currentUsage, requestedIncrease), 0);
    };
    // Use double for multiplication to preserve precision.
    double quota = m_defaultOriginQuota;
    double standardReportedQuota = m_standardVolumeCapacity ? *m_standardVolumeCapacity : 0.0;
    if (m_originQuotaRatio && m_originQuotaRatioEnabled) {
        std::optional<uint64_t> volumeCapacity;
        if (m_volumeCapacityOverride)
            volumeCapacity = m_volumeCapacityOverride;
        else if (auto capacity = FileSystem::volumeCapacity(m_path))
            volumeCapacity = WTF::roundUpToMultipleOf(defaultVolumeCapacityUnit, *capacity);
        if (volumeCapacity) {
            quota = m_originQuotaRatio.value() * volumeCapacity.value();
            increaseQuotaFunction = { };
        }
        standardReportedQuota *= m_originQuotaRatio.value();
    }
    if (origin.topOrigin != origin.clientOrigin) {
        quota *= defaultThirdPartyOriginQuotaRatio;
        standardReportedQuota *= defaultThirdPartyOriginQuotaRatio;
    }
    OriginQuotaManager::NotifySpaceGrantedFunction notifySpaceGrantedFunction = [weakThis = ThreadSafeWeakPtr { *this }, origin](uint64_t spaceRequested) {
        if (auto protectedThis = weakThis.get()) {
            protectedThis->spaceGrantedForOrigin(origin, spaceRequested);
            RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis)] { });
        }
    };
    // Use std::ceil instead of implicit conversion to make result more definitive.
    uint64_t roundedQuota = std::ceil(quota);
    uint64_t roundedStandardReportedQuota = std::ceil(standardReportedQuota);
    return { roundedQuota, roundedStandardReportedQuota, WTF::move(increaseQuotaFunction), WTF::move(notifySpaceGrantedFunction) };
}

OriginStorageManager& NetworkStorageManager::originStorageManager(const WebCore::ClientOrigin& origin, ShouldWriteOriginFile shouldWriteOriginFile)
{
    assertIsCurrent(workQueue());

    CheckedRef originStorageManager = *m_originStorageManagers.ensure(origin, [&] {
        auto originDirectory = originDirectoryPath(m_path, origin, m_salt);
        auto localStoragePath = LocalStorageManager::localStorageFilePath(m_customLocalStoragePath, origin);
        auto idbStoragePath = IDBStorageManager::idbStorageOriginDirectory(m_customIDBStoragePath, origin);
        auto cacheStoragePath = CacheStorageManager::cacheStorageOriginDirectory(m_customCacheStoragePath, origin);
        CacheStorageManager::copySaltFileToOriginDirectory(m_customCacheStoragePath, cacheStoragePath);
        OriginQuotaManager::IncreaseQuotaFunction increaseQuotaFunction = [sessionID = m_sessionID, origin, connection = m_parentConnection] (auto identifier, auto currentQuota, auto currentUsage, auto requestedIncrease) mutable {
            if (connection)
                IPC::Connection::send(*connection, Messages::NetworkProcessProxy::IncreaseQuota(sessionID, origin, identifier, currentQuota, currentUsage, requestedIncrease), 0);
        };
        return makeUnique<OriginStorageManager>(originQuotaManagerParameters(origin), WTF::move(originDirectory), WTF::move(localStoragePath), WTF::move(idbStoragePath), WTF::move(cacheStoragePath), m_unifiedOriginStorageLevel);
    }).iterator->value;

    if (shouldWriteOriginFile == ShouldWriteOriginFile::Yes)
        writeOriginToFileIfNecessary(origin);

    return originStorageManager.get();
}

bool NetworkStorageManager::removeOriginStorageManagerIfPossible(const WebCore::ClientOrigin& origin)
{
    assertIsCurrent(workQueue());

    auto iterator = m_originStorageManagers.find(origin);
    if (iterator == m_originStorageManagers.end())
        return true;

    auto& manager = iterator->value;
    if (manager->isActive() || manager->hasDataInMemory())
        return false;

    manager->deleteEmptyDirectory();
    deleteEmptyOriginDirectory(manager->path());

    m_originStorageManagers.remove(iterator);
    return true;
}

void NetworkStorageManager::updateLastModificationTimeForOrigin(const WebCore::ClientOrigin& origin)
{
    assertIsCurrent(workQueue());

    auto currentTime = WallTime::now();
    auto iterator = m_lastModificationTimes.find(origin);
    if (iterator == m_lastModificationTimes.end())
        m_lastModificationTimes.set(origin, currentTime);
    else {
        if (currentTime - iterator->value <= originLastModificationTimeUpdateInterval)
            return;
        iterator->value = currentTime;
    }

    m_lastModificationTimes.removeIf([&currentTime](auto& iterator) {
        return currentTime - iterator.value > originLastModificationTimeUpdateInterval;
    });

    // This function must be called when origin is in use, i.e. OriginStorageManager exists.
    CheckedPtr manager = m_originStorageManagers.get(origin);
    ASSERT(manager);

    auto originDirectory = manager->path();
    if (!originDirectory)
        return;

    FileSystem::updateFileModificationTime(originFilePath(originDirectory));
    if (m_unifiedOriginStorageLevel <= UnifiedOriginStorageLevel::Basic)
        FileSystem::updateFileModificationTime(manager->resolvedPath(WebsiteDataType::DOMCache));
    if (m_unifiedOriginStorageLevel == UnifiedOriginStorageLevel::None)
        FileSystem::updateFileModificationTime(manager->resolvedPath(WebsiteDataType::IndexedDBDatabases));
}

bool NetworkStorageManager::persistedInternal(const WebCore::ClientOrigin& origin)
{
    auto persistedFile = persistedFilePath(origin);
    if (persistedFile.isEmpty())
        return false;

    return FileSystem::fileExists(persistedFile);
}

void NetworkStorageManager::persisted(const WebCore::ClientOrigin& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    assertIsCurrent(workQueue());

    completionHandler(persistedInternal(origin));
}

void NetworkStorageManager::fetchRegistrableDomainsForPersist()
{
    ASSERT(RunLoop::isMain());

    if (!m_process)
        return didFetchRegistrableDomainsForPersist({ });

    protectedProcess()->registrableDomainsExemptFromWebsiteDataDeletion(m_sessionID, [weakThis = ThreadSafeWeakPtr { *this }](HashSet<WebCore::RegistrableDomain>&& domains) mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->didFetchRegistrableDomainsForPersist(std::forward<decltype(domains)>(domains));
    });
}

void NetworkStorageManager::didFetchRegistrableDomainsForPersist(HashSet<WebCore::RegistrableDomain>&& domains)
{
    ASSERT(RunLoop::isMain());

    if (m_closed)
        return;

    workQueue().dispatch([weakThis = ThreadSafeWeakPtr { *this }, domains = crossThreadCopy(WTF::move(domains))]() mutable {
        auto protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        assertIsCurrent(protectedThis->workQueue());

        protectedThis->m_domainsExemptFromEviction = WTF::move(domains);
        for (auto&& [origin, completionHandler] : std::exchange(protectedThis->m_persistCompletionHandlers, { }))
            completionHandler(protectedThis->persistOrigin(origin));
    });
}

bool NetworkStorageManager::persistOrigin(const WebCore::ClientOrigin& origin)
{
    assertIsCurrent(workQueue());
    ASSERT(m_domainsExemptFromEviction);

    if (!m_domainsExemptFromEviction->contains(origin.clientRegistrableDomain())) {
        auto persistedFile = persistedFilePath(origin);
        if (!persistedFile.isEmpty())
            FileSystem::deleteFile(persistedFile);
        return false;
    }

    FileSystem::overwriteEntireFile(persistedFilePath(origin), std::span<uint8_t> { });
    return true;
}

void NetworkStorageManager::persist(const WebCore::ClientOrigin& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    assertIsCurrent(workQueue());

    if (origin.topOrigin != origin.clientOrigin)
        return completionHandler(false);

    if (persistedFilePath(origin).isEmpty())
        return completionHandler(false);

    if (m_domainsExemptFromEviction)
        return completionHandler(persistOrigin(origin));

    m_persistCompletionHandlers.append({ origin, WTF::move(completionHandler) });
    RunLoop::mainSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }]() mutable {
        if (auto protectedThis = weakThis.get())
            protectedThis->fetchRegistrableDomainsForPersist();
    });
}

void NetworkStorageManager::estimate(const WebCore::ClientOrigin& origin, CompletionHandler<void(std::optional<WebCore::StorageEstimate>)>&& completionHandler)
{
    assertIsCurrent(workQueue());

    completionHandler(checkedOriginStorageManager(origin)->estimate());
}

void NetworkStorageManager::resetStoragePersistedState(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)]() mutable {
        assertIsCurrent(workQueue());
        for (auto& origin : getAllOrigins()) {
            auto persistedFile = persistedFilePath(origin);
            if (!persistedFile.isEmpty())
                FileSystem::deleteFile(persistedFile);
        }

        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::clearStorageForWebPage(WebPageProxyIdentifier pageIdentifier)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }, pageIdentifier]() mutable {
        assertIsCurrent(workQueue());
        for (auto& manager : m_originStorageManagers.values()) {
            if (auto* sessionStorageManager = manager->existingSessionStorageManager())
                sessionStorageManager->removeNamespace(ObjectIdentifier<StorageNamespaceIdentifierType>(pageIdentifier.toUInt64()));
        }
    });
}

void NetworkStorageManager::cloneSessionStorageForWebPage(WebPageProxyIdentifier fromIdentifier, WebPageProxyIdentifier toIdentifier)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }, fromIdentifier, toIdentifier]() mutable {
        assertIsCurrent(workQueue());
        cloneSessionStorageNamespace(ObjectIdentifier<StorageNamespaceIdentifierType>(fromIdentifier.toUInt64()), ObjectIdentifier<StorageNamespaceIdentifierType>(toIdentifier.toUInt64()));
    });
}

void NetworkStorageManager::cloneSessionStorageNamespace(StorageNamespaceIdentifier fromIdentifier, StorageNamespaceIdentifier toIdentifier)
{
    assertIsCurrent(workQueue());

    for (auto& manager : m_originStorageManagers.values()) {
        if (auto* sessionStorageManager = manager->existingSessionStorageManager())
            sessionStorageManager->cloneStorageArea(fromIdentifier, toIdentifier);
    }
}

void NetworkStorageManager::fetchSessionStorageForWebPage(WebPageProxyIdentifier pageIdentifier, CompletionHandler<void(std::optional<HashMap<WebCore::ClientOrigin, HashMap<String, String>>>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }, pageIdentifier, completionHandler = WTF::move(completionHandler)]() mutable {
        assertIsCurrent(workQueue());

        HashMap<WebCore::ClientOrigin, HashMap<String, String>> sessionStorageMap;
        StorageNamespaceIdentifier storageNameSpaceIdentifier { pageIdentifier.toUInt64() };

        for (auto& [origin, originStorageManager] : m_originStorageManagers) {
            auto* sessionStorageManager = CheckedRef { *originStorageManager }->existingSessionStorageManager();
            if (!sessionStorageManager)
                continue;

            auto storageMap = sessionStorageManager->fetchStorageMap(storageNameSpaceIdentifier);
            if (!storageMap.isEmpty())
                sessionStorageMap.add(origin, WTF::move(storageMap));
        }

        RunLoop::mainSingleton().dispatch([completionHandler = WTF::move(completionHandler), sessionStorageMap = crossThreadCopy(WTF::move(sessionStorageMap))] mutable {
            completionHandler(WTF::move(sessionStorageMap));
        });
    });
}

void NetworkStorageManager::restoreSessionStorageForWebPage(WebPageProxyIdentifier pageIdentifier, HashMap<WebCore::ClientOrigin, HashMap<String, String>>&& sessionStorageMap, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }, pageIdentifier, sessionStorageMap = crossThreadCopy(WTF::move(sessionStorageMap)), completionHandler = WTF::move(completionHandler)]() mutable {
        assertIsCurrent(workQueue());

        bool succeeded = true;
        StorageNamespaceIdentifier storageNameSpaceIdentifier { pageIdentifier.toUInt64() };

        for (auto& [clientOrigin, storageMap] : sessionStorageMap) {
            auto& sessionStorageManager = checkedOriginStorageManager(clientOrigin, ShouldWriteOriginFile::Yes)->sessionStorageManager(*m_storageAreaRegistry);
            auto result = sessionStorageManager.setStorageMap(storageNameSpaceIdentifier, clientOrigin, WTF::move(storageMap));

            if (!result)
                succeeded = false;
        }

        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandler), succeeded] mutable {
            completionHandler(succeeded);
        });
    });
}

void NetworkStorageManager::didIncreaseQuota(WebCore::ClientOrigin&& origin, QuotaIncreaseRequestIdentifier identifier, std::optional<uint64_t> newQuota)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }, origin = crossThreadCopy(WTF::move(origin)), identifier, newQuota]() mutable {
        assertIsCurrent(workQueue());
        if (CheckedPtr manager = m_originStorageManagers.get(origin))
            protect(manager->quotaManager())->didIncreaseQuota(identifier, newQuota);
    });
}

void NetworkStorageManager::fileSystemGetDirectory(IPC::Connection& connection, WebCore::ClientOrigin&& origin, CompletionHandler<void(Expected<std::optional<WebCore::FileSystemHandleIdentifier>, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    Ref fileSystemStorageManager = checkedOriginStorageManager(origin)->fileSystemStorageManager(*protectedFileSystemStorageHandleRegistry());
    auto result = fileSystemStorageManager->getDirectory(connection.uniqueID());
    if (result)
        completionHandler(std::optional { result.value() });
    else
        completionHandler(makeUnexpected(result.error()));
}

void NetworkStorageManager::closeHandle(WebCore::FileSystemHandleIdentifier identifier)
{
    ASSERT(!RunLoop::isMain());

    if (RefPtr handle = protectedFileSystemStorageHandleRegistry()->getHandle(identifier))
        handle->close();
}

void NetworkStorageManager::isSameEntry(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier targetIdentifier, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    RefPtr handle = protectedFileSystemStorageHandleRegistry()->getHandle(identifier);
    if (!handle)
        return completionHandler(false);

    completionHandler(handle->isSameEntry(targetIdentifier));
}

void NetworkStorageManager::move(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier destinationIdentifier, const String& newName, CompletionHandler<void(std::optional<FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    RefPtr handle = protectedFileSystemStorageHandleRegistry()->getHandle(identifier);
    if (!handle)
        return completionHandler(FileSystemStorageError::Unknown);

    completionHandler(handle->move(destinationIdentifier, newName));
}

void NetworkStorageManager::getFileHandle(IPC::Connection& connection, WebCore::FileSystemHandleIdentifier identifier, String&& name, bool createIfNecessary, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    RefPtr handle = protectedFileSystemStorageHandleRegistry()->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->getFileHandle(connection.uniqueID(), WTF::move(name), createIfNecessary));
}

void NetworkStorageManager::getDirectoryHandle(IPC::Connection& connection, WebCore::FileSystemHandleIdentifier identifier, String&& name, bool createIfNecessary, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    RefPtr handle = protectedFileSystemStorageHandleRegistry()->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->getDirectoryHandle(connection.uniqueID(), WTF::move(name), createIfNecessary));
}

void NetworkStorageManager::removeEntry(WebCore::FileSystemHandleIdentifier identifier, const String& name, bool deleteRecursively, CompletionHandler<void(std::optional<FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    RefPtr handle = protectedFileSystemStorageHandleRegistry()->getHandle(identifier);
    if (!handle)
        return completionHandler(FileSystemStorageError::Unknown);

    completionHandler(handle->removeEntry(name, deleteRecursively));
}

void NetworkStorageManager::resolve(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier targetIdentifier, CompletionHandler<void(Expected<Vector<String>, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    RefPtr handle = protectedFileSystemStorageHandleRegistry()->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->resolve(targetIdentifier));
}

void NetworkStorageManager::getFile(WebCore::FileSystemHandleIdentifier identifier, CompletionHandler<void(Expected<String, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    RefPtr handle = protectedFileSystemStorageHandleRegistry()->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->path());
}

void NetworkStorageManager::createSyncAccessHandle(WebCore::FileSystemHandleIdentifier identifier, CompletionHandler<void(Expected<FileSystemSyncAccessHandleInfo, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    RefPtr handle = protectedFileSystemStorageHandleRegistry()->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->createSyncAccessHandle());
}

void NetworkStorageManager::closeSyncAccessHandle(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (RefPtr handle = protectedFileSystemStorageHandleRegistry()->getHandle(identifier))
        handle->closeSyncAccessHandle(accessHandleIdentifier);

    completionHandler();
}

void NetworkStorageManager::requestNewCapacityForSyncAccessHandle(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, uint64_t newCapacity, CompletionHandler<void(std::optional<uint64_t>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    RefPtr handle = protectedFileSystemStorageHandleRegistry()->getHandle(identifier);
    if (!handle)
        return completionHandler(std::nullopt);

    handle->requestNewCapacityForSyncAccessHandle(accessHandleIdentifier, newCapacity, WTF::move(completionHandler));
}

void NetworkStorageManager::createWritable(WebCore::FileSystemHandleIdentifier identifier, bool keepExistingData, CompletionHandler<void(Expected<WebCore::FileSystemWritableFileStreamIdentifier, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    RefPtr handle = protectedFileSystemStorageHandleRegistry()->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->createWritable(keepExistingData));
}

void NetworkStorageManager::closeWritable(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemWritableFileStreamIdentifier streamIdentifier, WebCore::FileSystemWriteCloseReason reason, CompletionHandler<void(std::optional<FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    RefPtr handle = protectedFileSystemStorageHandleRegistry()->getHandle(identifier);
    if (!handle)
        return completionHandler(FileSystemStorageError::Unknown);

    completionHandler(handle->closeWritable(streamIdentifier, reason));
}

void NetworkStorageManager::executeCommandForWritable(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemWritableFileStreamIdentifier streamIdentifier, WebCore::FileSystemWriteCommandType type, std::optional<uint64_t> position, std::optional<uint64_t> size, std::span<const uint8_t> dataBytes, bool hasDataError, CompletionHandler<void(std::optional<FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    RefPtr handle = protectedFileSystemStorageHandleRegistry()->getHandle(identifier);
    if (!handle)
        return completionHandler(FileSystemStorageError::Unknown);

    handle->executeCommandForWritable(streamIdentifier, type, position, size, dataBytes, hasDataError, WTF::move(completionHandler));
}

void NetworkStorageManager::getHandleNames(WebCore::FileSystemHandleIdentifier identifier, CompletionHandler<void(Expected<Vector<String>, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    RefPtr handle = protectedFileSystemStorageHandleRegistry()->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->getHandleNames());
}

void NetworkStorageManager::getHandle(IPC::Connection& connection, WebCore::FileSystemHandleIdentifier identifier, String&& name, CompletionHandler<void(Expected<std::optional<std::pair<WebCore::FileSystemHandleIdentifier, bool>>, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    RefPtr handle = protectedFileSystemStorageHandleRegistry()->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    auto result = handle->getHandle(connection.uniqueID(), WTF::move(name));
    if (result)
        completionHandler(std::optional { result.value() });
    else
        completionHandler(makeUnexpected(result.error()));
}

void NetworkStorageManager::forEachOriginDirectory(NOESCAPE const Function<void(const String&)>& apply)
{
    for (auto& topOrigin : FileSystem::listDirectory(m_path)) {
        auto topOriginDirectory = FileSystem::pathByAppendingComponent(m_path, topOrigin);
        auto openingOrigins = FileSystem::listDirectory(topOriginDirectory);
        if (openingOrigins.isEmpty()) {
            FileSystem::deleteEmptyDirectory(topOriginDirectory);
            continue;
        }

        for (auto& openingOrigin : openingOrigins) {
            if (openingOrigin.startsWith('.'))
                continue;

            auto openingOriginDirectory = FileSystem::pathByAppendingComponent(topOriginDirectory, openingOrigin);
            apply(openingOriginDirectory);
        }
    }
}

HashSet<WebCore::ClientOrigin> NetworkStorageManager::getAllOrigins()
{
    assertIsCurrent(workQueue());

    HashSet<WebCore::ClientOrigin> allOrigins;
    for (auto& origin : m_originStorageManagers.keys())
        allOrigins.add(origin);

    forEachOriginDirectory([&](auto directory) {
        if (auto origin = WebCore::StorageUtilities::readOriginFromFile(originFilePath(directory)))
            allOrigins.add(*origin);
    });

    for (auto& origin : LocalStorageManager::originsOfLocalStorageData(m_customLocalStoragePath))
        allOrigins.add(WebCore::ClientOrigin { origin, origin });

    for (auto& origin : IDBStorageManager::originsOfIDBStorageData(m_customIDBStoragePath))
        allOrigins.add(origin);

    for (auto& origin : CacheStorageManager::originsOfCacheStorageData(m_customCacheStoragePath))
        allOrigins.add(origin);

    return allOrigins;
}

static void updateOriginData(HashMap<WebCore::SecurityOriginData, OriginStorageManager::DataTypeSizeMap>& originTypes, const WebCore::SecurityOriginData& origin, const OriginStorageManager::DataTypeSizeMap& newTypeSizeMap)
{
    auto& typeSizeMap = originTypes.add(origin, OriginStorageManager::DataTypeSizeMap { }).iterator->value;
    for (auto [type, size] : newTypeSizeMap) {
        auto& currentSize = typeSizeMap.add(type, 0).iterator->value;
        currentSize += size;
    }
}

Vector<WebsiteData::Entry> NetworkStorageManager::fetchDataFromDisk(OptionSet<WebsiteDataType> targetTypes, ShouldComputeSize shouldComputeSize)
{
    ASSERT(!RunLoop::isMain());

    HashMap<WebCore::SecurityOriginData, OriginStorageManager::DataTypeSizeMap> originTypes;
    for (auto& origin : getAllOrigins()) {
        auto typeSizeMap = checkedOriginStorageManager(origin)->fetchDataTypesInList(targetTypes, shouldComputeSize == ShouldComputeSize::Yes);
        updateOriginData(originTypes, origin.clientOrigin, typeSizeMap);
        if (origin.clientOrigin != origin.topOrigin)
            updateOriginData(originTypes, origin.topOrigin, typeSizeMap);

        removeOriginStorageManagerIfPossible(origin);
    }

    Vector<WebsiteData::Entry> entries;
    for (auto [origin, types] : originTypes) {
        for (auto [type, size] : types)
            entries.append({ WebsiteData::Entry { origin, type, size } });
    }

    return entries;
}

void NetworkStorageManager::fetchData(OptionSet<WebsiteDataType> types, ShouldComputeSize shouldComputeSize, CompletionHandler<void(Vector<WebsiteData::Entry>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }, types, shouldComputeSize, completionHandler = WTF::move(completionHandler)]() mutable {
        auto entries = fetchDataFromDisk(types, shouldComputeSize);
        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandler), entries = crossThreadCopy(WTF::move(entries))]() mutable {
            completionHandler(WTF::move(entries));
        });
    });
}

HashSet<WebCore::ClientOrigin> NetworkStorageManager::deleteDataOnDisk(OptionSet<WebsiteDataType> types, WallTime modifiedSinceTime, NOESCAPE const Function<bool(const WebCore::ClientOrigin&)>& filter)
{
    ASSERT(!RunLoop::isMain());

    HashSet<WebCore::ClientOrigin> deletedOrigins;
    for (auto& origin : getAllOrigins()) {
        if (!filter(origin))
            continue;

        {
            CheckedRef originStorageManager = this->originStorageManager(origin);
            auto existingDataTypes = originStorageManager->fetchDataTypesInList(types, false);
            if (!existingDataTypes.isEmpty()) {
                deletedOrigins.add(origin);
                originStorageManager->deleteData(types, modifiedSinceTime);
            }
        }

        if (types.containsAll(allManagedTypes())) {
            auto persistedFile = persistedFilePath(origin);
            if (!persistedFile.isEmpty())
                FileSystem::deleteFile(persistedFile);
        }

        removeOriginStorageManagerIfPossible(origin);
    }

    return deletedOrigins;
}

void NetworkStorageManager::deleteData(OptionSet<WebsiteDataType> types, const Vector<WebCore::SecurityOriginData>& origins, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }, types, origins = crossThreadCopy(origins), completionHandler = WTF::move(completionHandler)]() mutable {
        HashSet<WebCore::SecurityOriginData> originSet;
        originSet.reserveInitialCapacity(origins.size());
        for (auto origin : origins)
            originSet.add(WTF::move(origin));

        deleteDataOnDisk(types, -WallTime::infinity(), [&originSet](auto origin) {
            return originSet.contains(origin.topOrigin) || originSet.contains(origin.clientOrigin);
        });
        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::deleteData(OptionSet<WebsiteDataType> types, const WebCore::ClientOrigin& origin, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }, types, originToDelete = origin.isolatedCopy(), completionHandler = WTF::move(completionHandler)]() mutable {
        deleteDataOnDisk(types, -WallTime::infinity(), [originToDelete = WTF::move(originToDelete)](auto& origin) {
            return origin == originToDelete;
        });
        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::deleteDataModifiedSince(OptionSet<WebsiteDataType> types, WallTime modifiedSinceTime, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }, types, modifiedSinceTime, completionHandler = WTF::move(completionHandler)]() mutable {
        deleteDataOnDisk(types, modifiedSinceTime, [](auto&) {
            return true;
        });

        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::deleteDataForRegistrableDomains(OptionSet<WebsiteDataType> types, const Vector<WebCore::RegistrableDomain>& domains, CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }, types, domains = crossThreadCopy(domains), completionHandler = WTF::move(completionHandler)]() mutable {
        auto deletedOrigins = deleteDataOnDisk(types, -WallTime::infinity(), [&domains](auto& origin) {
            auto domain = WebCore::RegistrableDomain::uncheckedCreateFromHost(origin.clientOrigin.host());
            return domains.contains(domain);
        });

        HashSet<WebCore::RegistrableDomain> deletedDomains;
        for (auto origin : deletedOrigins) {
            auto domain = WebCore::RegistrableDomain::uncheckedCreateFromHost(origin.clientOrigin.host());
            deletedDomains.add(domain);
        }

        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandler), domains = crossThreadCopy(WTF::move(deletedDomains))]() mutable {
            completionHandler(WTF::move(domains));
        });
    });
}

void NetworkStorageManager::moveData(OptionSet<WebsiteDataType> types, WebCore::SecurityOriginData&& source, WebCore::SecurityOriginData&& target, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }, types, source = crossThreadCopy(WTF::move(source)), target = crossThreadCopy(WTF::move(target)), completionHandler = WTF::move(completionHandler)]() mutable {
        auto sourceOrigin = WebCore::ClientOrigin { source, source };
        auto targetOrigin = WebCore::ClientOrigin { target, target };

        {
            CheckedRef targetOriginStorageManager = originStorageManager(targetOrigin);

            // Clear existing data of target origin.
            targetOriginStorageManager->deleteData(types, -WallTime::infinity());

            // Move data from source origin to target origin.
            checkedOriginStorageManager(sourceOrigin)->moveData(types, targetOriginStorageManager->resolvedPath(WebsiteDataType::LocalStorage), targetOriginStorageManager->resolvedPath(WebsiteDataType::IndexedDBDatabases));
        }

        removeOriginStorageManagerIfPossible(targetOrigin);
        removeOriginStorageManagerIfPossible(sourceOrigin);

        RunLoop::mainSingleton().dispatch(WTF::move(completionHandler));
    });
}

void NetworkStorageManager::getOriginDirectory(WebCore::ClientOrigin&& origin, WebsiteDataType type, CompletionHandler<void(const String&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }, type, origin = crossThreadCopy(WTF::move(origin)), completionHandler = WTF::move(completionHandler)]() mutable {
        RunLoop::mainSingleton().dispatch([completionHandler = WTF::move(completionHandler), directory = crossThreadCopy(checkedOriginStorageManager(origin)->resolvedPath(type))]() mutable {
            completionHandler(WTF::move(directory));
        });
        removeOriginStorageManagerIfPossible(origin);
    });
}

void NetworkStorageManager::suspend(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (m_sessionID.isEphemeral())
        return completionHandler();

    RELEASE_LOG(ProcessSuspension, "%p - NetworkStorageManager::suspend()", this);
    workQueue().suspend([this, protectedThis = Ref { *this }] {
        assertIsCurrent(workQueue());
        for (auto& manager : m_originStorageManagers.values()) {
            if (auto localStorageManager = manager->existingLocalStorageManager())
                localStorageManager->syncLocalStorage();
            if (CheckedPtr idbStorageManager = manager->existingIDBStorageManager())
                idbStorageManager->stopDatabaseActivitiesForSuspend();
        }
    }, WTF::move(completionHandler));
}

bool NetworkStorageManager::isSuspended() const
{
    ASSERT(RunLoop::isMain());

    return workQueue().isSuspended();
}

void NetworkStorageManager::resume()
{
    ASSERT(RunLoop::isMain());

    if (m_sessionID.isEphemeral())
        return;

    RELEASE_LOG(ProcessSuspension, "%p - NetworkStorageManager::resume()", this);
    workQueue().resume();
}

void NetworkStorageManager::handleLowMemoryWarning()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }] {
        assertIsCurrent(workQueue());
        for (auto& manager : m_originStorageManagers.values()) {
            if (auto localStorageManager = manager->existingLocalStorageManager())
                localStorageManager->handleLowMemoryWarning();
            if (CheckedPtr idbStorageManager = manager->existingIDBStorageManager())
                idbStorageManager->handleLowMemoryWarning();
        }
    });
}

void NetworkStorageManager::syncLocalStorage(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)]() mutable {
        assertIsCurrent(workQueue());
        for (auto& manager : m_originStorageManagers.values()) {
            if (auto localStorageManager = manager->existingLocalStorageManager())
                localStorageManager->syncLocalStorage();
        }

        RunLoop::mainSingleton().dispatch(WTF::move(completionHandler));
    });
}

void NetworkStorageManager::fetchLocalStorage(CompletionHandler<void(std::optional<HashMap<WebCore::ClientOrigin, HashMap<String, String>>>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)]() mutable {
        assertIsCurrent(workQueue());

        HashMap<WebCore::ClientOrigin, HashMap<String, String>> localStorageMap;

        for (auto& origin : getAllOrigins()) {
            auto& localStorageManager = checkedOriginStorageManager(origin, ShouldWriteOriginFile::No)->localStorageManager(*m_storageAreaRegistry);
            auto storageMap = localStorageManager.fetchStorageMap();

            if (!storageMap.isEmpty())
                localStorageMap.add(origin, WTF::move(storageMap));
        }

        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandler), localStorageMap = crossThreadCopy(WTF::move(localStorageMap))] mutable {
            completionHandler(WTF::move(localStorageMap));
        });
    });
}

void NetworkStorageManager::restoreLocalStorage(HashMap<WebCore::ClientOrigin, HashMap<String, String>>&& localStorageMap, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }, localStorageMap = crossThreadCopy(WTF::move(localStorageMap)), completionHandler = WTF::move(completionHandler)]() mutable {
        assertIsCurrent(workQueue());

        bool succeeded = true;

        for (auto& [clientOrigin, storageMap] : localStorageMap) {
            auto& localStorageManager = checkedOriginStorageManager(clientOrigin, ShouldWriteOriginFile::Yes)->localStorageManager(*m_storageAreaRegistry);
            auto result = localStorageManager.setStorageMap(clientOrigin, WTF::move(storageMap), workQueue());

            if (!result)
                succeeded = false;
        }

        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandler), succeeded] mutable {
            completionHandler(succeeded);
        });
    });
}

void NetworkStorageManager::registerTemporaryBlobFilePaths(IPC::Connection& connection, const Vector<String>& filePaths)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }, connectionID = connection.uniqueID(), filePaths = crossThreadCopy(filePaths)]() mutable {
        assertIsCurrent(workQueue());
        auto& temporaryBlobPaths = m_temporaryBlobPathsByConnection.ensure(connectionID, [] {
            return HashSet<String> { };
        }).iterator->value;
        temporaryBlobPaths.addAll(WTF::move(filePaths));
    });
}

void NetworkStorageManager::requestSpace(const WebCore::ClientOrigin& origin, uint64_t size, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([this, protectedThis = Ref { *this }, origin = crossThreadCopy(origin), size, completionHandler = WTF::move(completionHandler)]() mutable {
        protect(checkedOriginStorageManager(origin)->quotaManager())->requestSpace(size, [completionHandler = WTF::move(completionHandler)](auto decision) mutable {
            RunLoop::mainSingleton().dispatch([completionHandler = WTF::move(completionHandler), decision]() mutable {
                completionHandler(decision == OriginQuotaManager::Decision::Grant);
            });
        });
    });
}

void NetworkStorageManager::resetQuotaForTesting(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    workQueue().dispatch([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)]() mutable {
        assertIsCurrent(workQueue());
        for (auto& manager : m_originStorageManagers.values())
            protect(manager->quotaManager())->resetQuotaForTesting();
        RunLoop::mainSingleton().dispatch(WTF::move(completionHandler));
    });
}

void NetworkStorageManager::resetQuotaUpdatedBasedOnUsageForTesting(WebCore::ClientOrigin&& origin)
{
    assertIsCurrent(workQueue());

    if (CheckedPtr manager = m_originStorageManagers.get(origin))
        protect(manager->quotaManager())->resetQuotaUpdatedBasedOnUsageForTesting();
}

void NetworkStorageManager::setOriginQuotaRatioEnabledForTesting(bool enabled, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    workQueue().dispatch([this, protectedThis = Ref { *this }, enabled, completionHandler = WTF::move(completionHandler)]() mutable {
        assertIsCurrent(workQueue());
        if (m_originQuotaRatioEnabled != enabled) {
            m_originQuotaRatioEnabled = enabled;
            for (auto& [origin, manager] : m_originStorageManagers)
                protect(CheckedRef { *manager }->quotaManager())->updateParametersForTesting(originQuotaManagerParameters(origin));
        }

        RunLoop::mainSingleton().dispatch(WTF::move(completionHandler));
    });
}

#if PLATFORM(IOS_FAMILY)

void NetworkStorageManager::setBackupExclusionPeriodForTesting(Seconds period, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, period, completionHandler = WTF::move(completionHandler)]() mutable {
        m_backupExclusionPeriod = period;
        RunLoop::mainSingleton().dispatch(WTF::move(completionHandler));
    });
}

#endif

void NetworkStorageManager::setStorageSiteValidationEnabledInternal(bool enabled)
{
    assertIsCurrent(workQueue());

    auto currentEnabled = !!m_allowedSitesForConnections;
    if (currentEnabled == enabled)
        return;

    if (enabled)
        m_allowedSitesForConnections = ConnectionSitesMap { };
    else
        m_allowedSitesForConnections = std::nullopt;
}

void NetworkStorageManager::setStorageSiteValidationEnabled(bool enabled)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    workQueue().dispatch([weakThis = ThreadSafeWeakPtr { *this }, enabled]() mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->setStorageSiteValidationEnabledInternal(enabled);
    });
}

void NetworkStorageManager::addAllowedSitesForConnectionInternal(IPC::Connection::UniqueID connection, const Vector<WebCore::RegistrableDomain>& sites)
{
    assertIsCurrent(workQueue());

    if (!m_allowedSitesForConnections)
        return;

    auto& allowedSites = m_allowedSitesForConnections->add(connection,  HashSet<WebCore::RegistrableDomain> { }).iterator->value;
    for (auto& site : sites)
        allowedSites.add(site);
}

void NetworkStorageManager::addAllowedSitesForConnection(IPC::Connection::UniqueID connection, const Vector<WebCore::RegistrableDomain>& sites)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    if (sites.isEmpty())
        return;

    workQueue().dispatch([weakThis = ThreadSafeWeakPtr { *this }, connection, sites = crossThreadCopy(sites)]() mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->addAllowedSitesForConnectionInternal(connection, sites);
    });
}

bool NetworkStorageManager::isSiteAllowedForConnection(IPC::Connection::UniqueID connection, const WebCore::RegistrableDomain& site) const
{
    assertIsCurrent(workQueue());

    if (!m_allowedSitesForConnections)
        return true;

    auto iter = m_allowedSitesForConnections->find(connection);
    if (iter == m_allowedSitesForConnections->end())
        return false;

    return iter->value.contains(site);
}

void NetworkStorageManager::connectToStorageArea(IPC::Connection& connection, WebCore::StorageType type, StorageAreaMapIdentifier sourceIdentifier, std::optional<StorageNamespaceIdentifier> namespaceIdentifier, const WebCore::ClientOrigin& origin, CompletionHandler<void(std::optional<StorageAreaIdentifier>, HashMap<String, String>, uint64_t)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());
    MESSAGE_CHECK_COMPLETION(isSiteAllowedForConnection(connection.uniqueID(), WebCore::RegistrableDomain { origin.topOrigin }), connection, completionHandler(std::nullopt, { }, StorageAreaBase::nextMessageIdentifier()));

    MESSAGE_CHECK_COMPLETION(isStorageTypeEnabled(connection, type), connection, completionHandler(std::nullopt, { }, StorageAreaBase::nextMessageIdentifier()));

    auto connectionIdentifier = connection.uniqueID();
    // StorageArea may be connected due to LocalStorage prewarming, so do not write origin file eagerly.
    CheckedRef originStorageManager = this->originStorageManager(origin, ShouldWriteOriginFile::No);
    std::optional<StorageAreaIdentifier> resultIdentifier;
    switch (type) {
    case WebCore::StorageType::Local:
        resultIdentifier = originStorageManager->localStorageManager(*m_storageAreaRegistry).connectToLocalStorageArea(connectionIdentifier, sourceIdentifier, origin, m_queue.copyRef());
        break;
    case WebCore::StorageType::TransientLocal:
        resultIdentifier = originStorageManager->localStorageManager(*m_storageAreaRegistry).connectToTransientLocalStorageArea(connectionIdentifier, sourceIdentifier, origin);
        break;
    case WebCore::StorageType::Session:
        if (!namespaceIdentifier)
            return completionHandler(std::nullopt, HashMap<String, String> { }, StorageAreaBase::nextMessageIdentifier());
        resultIdentifier = originStorageManager->sessionStorageManager(*m_storageAreaRegistry).connectToSessionStorageArea(connectionIdentifier, sourceIdentifier, origin, *namespaceIdentifier);
    }

    if (!resultIdentifier)
        return completionHandler(std::nullopt, HashMap<String, String> { }, StorageAreaBase::nextMessageIdentifier());

    if (RefPtr storageArea = m_storageAreaRegistry->getStorageArea(*resultIdentifier)) {
        completionHandler(*resultIdentifier, storageArea->allItems(), StorageAreaBase::nextMessageIdentifier());
        writeOriginToFileIfNecessary(origin, storageArea.get());
        return;
    }

    return completionHandler(*resultIdentifier, HashMap<String, String> { }, StorageAreaBase::nextMessageIdentifier());
}

void NetworkStorageManager::connectToStorageAreaSync(IPC::Connection& connection, WebCore::StorageType type, StorageAreaMapIdentifier sourceIdentifier, std::optional<StorageNamespaceIdentifier> namespaceIdentifier, const WebCore::ClientOrigin& origin, CompletionHandler<void(std::optional<StorageAreaIdentifier>, HashMap<String, String>, uint64_t)>&& completionHandler)
{
    connectToStorageArea(connection, type, sourceIdentifier, namespaceIdentifier, origin, WTF::move(completionHandler));
}

void NetworkStorageManager::cancelConnectToStorageArea(IPC::Connection& connection, WebCore::StorageType type, std::optional<StorageNamespaceIdentifier> namespaceIdentifier, const WebCore::ClientOrigin& origin)
{
    assertIsCurrent(workQueue());
    MESSAGE_CHECK(isSiteAllowedForConnection(connection.uniqueID(), WebCore::RegistrableDomain { origin.topOrigin }), connection);

    auto iterator = m_originStorageManagers.find(origin);
    if (iterator == m_originStorageManagers.end())
        return;

    auto connectionIdentifier = connection.uniqueID();
    CheckedRef originStorageManager = *(iterator->value);
    switch (type) {
    case WebCore::StorageType::Local:
        if (auto localStorageManager = originStorageManager->existingLocalStorageManager())
            localStorageManager->cancelConnectToLocalStorageArea(connectionIdentifier);
        break;
    case WebCore::StorageType::TransientLocal:
        if (auto localStorageManager = originStorageManager->existingLocalStorageManager())
            localStorageManager->cancelConnectToTransientLocalStorageArea(connectionIdentifier);

        break;
    case WebCore::StorageType::Session:
        if (auto sessionStorageManager = originStorageManager->existingSessionStorageManager()) {
            if (!namespaceIdentifier)
                return;
            sessionStorageManager->cancelConnectToSessionStorageArea(connectionIdentifier, *namespaceIdentifier);
        }
    }
}

void NetworkStorageManager::disconnectFromStorageArea(IPC::Connection& connection, StorageAreaIdentifier identifier)
{
    ASSERT(!RunLoop::isMain());

    RefPtr storageArea = m_storageAreaRegistry->getStorageArea(identifier);
    if (!storageArea)
        return;

    MESSAGE_CHECK(isSiteAllowedForConnection(connection.uniqueID(), WebCore::RegistrableDomain { storageArea->origin().topOrigin }), connection);

    CheckedRef originStorageManager = this->originStorageManager(storageArea->origin());
    if (storageArea->storageType() == StorageAreaBase::StorageType::Local)
        originStorageManager->localStorageManager(*m_storageAreaRegistry).disconnectFromStorageArea(connection.uniqueID(), identifier);
    else
        originStorageManager->sessionStorageManager(*m_storageAreaRegistry).disconnectFromStorageArea(connection.uniqueID(), identifier);
}

void NetworkStorageManager::setItem(IPC::Connection& connection, StorageAreaIdentifier identifier, StorageAreaImplIdentifier implIdentifier, String&& key, String&& value, String&& urlString, CompletionHandler<void(bool, HashMap<String, String>&&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    bool hasError = false;
    HashMap<String, String> allItems;
    RefPtr storageArea = m_storageAreaRegistry->getStorageArea(identifier);
    if (!storageArea)
        return completionHandler(hasError, WTF::move(allItems));

    MESSAGE_CHECK_COMPLETION(isSiteAllowedForConnection(connection.uniqueID(), WebCore::RegistrableDomain { storageArea->origin().topOrigin }), connection, completionHandler(hasError, WTF::move(allItems)));

    MESSAGE_CHECK_COMPLETION(isStorageAreaTypeEnabled(connection, storageArea->storageType()), connection, completionHandler(true, HashMap<String, String> { }));

    auto result = storageArea->setItem(connection.uniqueID(), implIdentifier, WTF::move(key), WTF::move(value), WTF::move(urlString));
    hasError = !result;
    if (hasError)
        allItems = storageArea->allItems();
    completionHandler(hasError, WTF::move(allItems));

    writeOriginToFileIfNecessary(storageArea->origin(), storageArea.get());
}

void NetworkStorageManager::removeItem(IPC::Connection& connection, StorageAreaIdentifier identifier, StorageAreaImplIdentifier implIdentifier, String&& key, String&& urlString, CompletionHandler<void(bool, HashMap<String, String>&&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    bool hasError = false;
    HashMap<String, String> allItems;
    RefPtr storageArea = m_storageAreaRegistry->getStorageArea(identifier);
    if (!storageArea)
        return completionHandler(hasError, WTF::move(allItems));

    MESSAGE_CHECK_COMPLETION(isSiteAllowedForConnection(connection.uniqueID(), WebCore::RegistrableDomain { storageArea->origin().topOrigin }), connection, completionHandler(hasError, WTF::move(allItems)));

    MESSAGE_CHECK_COMPLETION(isStorageAreaTypeEnabled(connection, storageArea->storageType()), connection, completionHandler(true, HashMap<String, String> { }));

    auto result = storageArea->removeItem(connection.uniqueID(), implIdentifier, WTF::move(key), WTF::move(urlString));
    hasError = !result;
    if (hasError)
        allItems = storageArea->allItems();
    completionHandler(hasError, WTF::move(allItems));

    writeOriginToFileIfNecessary(storageArea->origin(), storageArea.get());
}

void NetworkStorageManager::clear(IPC::Connection& connection, StorageAreaIdentifier identifier, StorageAreaImplIdentifier implIdentifier, String&& urlString, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    RefPtr storageArea = m_storageAreaRegistry->getStorageArea(identifier);
    if (!storageArea)
        return completionHandler();

    MESSAGE_CHECK_COMPLETION(isSiteAllowedForConnection(connection.uniqueID(), WebCore::RegistrableDomain { storageArea->origin().topOrigin }), connection, completionHandler());

    MESSAGE_CHECK_COMPLETION(isStorageAreaTypeEnabled(connection, storageArea->storageType()), connection, completionHandler());

    std::ignore = storageArea->clear(connection.uniqueID(), implIdentifier, WTF::move(urlString));
    completionHandler();

    writeOriginToFileIfNecessary(storageArea->origin(), storageArea.get());
}

void NetworkStorageManager::openDatabase(IPC::Connection& connection, const WebCore::IDBOpenRequestData& requestData)
{
    MESSAGE_CHECK(requestData.requestIdentifier().connectionIdentifier(), connection);
    Ref connectionToClient = m_idbStorageRegistry->ensureConnectionToClient(connection.uniqueID(), *requestData.requestIdentifier().connectionIdentifier());
    checkedOriginStorageManager(requestData.databaseIdentifier().origin())->checkedIDBStorageManager(*m_idbStorageRegistry)->openDatabase(connectionToClient, requestData);
}

void NetworkStorageManager::openDBRequestCancelled(const WebCore::IDBOpenRequestData& requestData)
{
    checkedOriginStorageManager(requestData.databaseIdentifier().origin())->checkedIDBStorageManager(*m_idbStorageRegistry)->openDBRequestCancelled(requestData);
}

void NetworkStorageManager::deleteDatabase(IPC::Connection& connection, const WebCore::IDBOpenRequestData& requestData)
{
    MESSAGE_CHECK(requestData.requestIdentifier().connectionIdentifier(), connection);
    Ref connectionToClient = m_idbStorageRegistry->ensureConnectionToClient(connection.uniqueID(), *requestData.requestIdentifier().connectionIdentifier());
    checkedOriginStorageManager(requestData.databaseIdentifier().origin())->checkedIDBStorageManager(*m_idbStorageRegistry)->deleteDatabase(connectionToClient, requestData);
}

void NetworkStorageManager::establishTransaction(WebCore::IDBDatabaseConnectionIdentifier databaseConnectionIdentifier, const WebCore::IDBTransactionInfo& transactionInfo)
{
    if (RefPtr connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->establishTransaction(transactionInfo);
}

void NetworkStorageManager::databaseConnectionPendingClose(WebCore::IDBDatabaseConnectionIdentifier databaseConnectionIdentifier)
{
    if (RefPtr connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->connectionPendingCloseFromClient();
}

void NetworkStorageManager::databaseConnectionClosed(WebCore::IDBDatabaseConnectionIdentifier databaseConnectionIdentifier)
{
    RefPtr connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier);
    if (!connection)
        return;

    WebCore::IDBDatabaseIdentifier databaseIdentifier;
    if (CheckedPtr database = connection->database()) {
        databaseIdentifier = database->identifier();
        connection->connectionClosedFromClient();
    }

    if (databaseIdentifier.isValid())
        checkedOriginStorageManager(databaseIdentifier.origin())->checkedIDBStorageManager(*m_idbStorageRegistry)->tryCloseDatabase(databaseIdentifier);
}

void NetworkStorageManager::abortOpenAndUpgradeNeeded(WebCore::IDBDatabaseConnectionIdentifier databaseConnectionIdentifier, const std::optional<WebCore::IDBResourceIdentifier>& transactionIdentifier)
{
    if (transactionIdentifier) {
        if (RefPtr transaction = m_idbStorageRegistry->transaction(*transactionIdentifier))
            transaction->abortWithoutCallback();
    }

    if (RefPtr connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->connectionClosedFromClient();
}

void NetworkStorageManager::didFireVersionChangeEvent(WebCore::IDBDatabaseConnectionIdentifier databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& requestIdentifier, const WebCore::IndexedDB::ConnectionClosedOnBehalfOfServer connectionClosed)
{
    if (RefPtr connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->didFireVersionChangeEvent(requestIdentifier, connectionClosed);
}

void NetworkStorageManager::didGenerateIndexKeyForRecord(const WebCore::IDBResourceIdentifier& transactionIdentifier, const WebCore::IDBResourceIdentifier& requestIdentifier, const WebCore::IDBIndexInfo& indexInfo, const WebCore::IDBKeyData& key, const WebCore::IndexKey& indexKey, std::optional<int64_t> recordID)
{
    if (RefPtr transaction = m_idbStorageRegistry->transaction(transactionIdentifier))
        transaction->didGenerateIndexKeyForRecord(requestIdentifier, indexInfo, key, indexKey, recordID);
}

void NetworkStorageManager::abortTransaction(IPC::Connection& connection, const WebCore::IDBResourceIdentifier& transactionIdentifier)
{
    MESSAGE_CHECK(transactionIdentifier.connectionIdentifier(), connection);
    if (RefPtr transaction = m_idbStorageRegistry->transaction(transactionIdentifier))
        transaction->abort();
}

void NetworkStorageManager::commitTransaction(IPC::Connection& connection, const WebCore::IDBResourceIdentifier& transactionIdentifier, uint64_t handledRequestResultsCount)
{
    MESSAGE_CHECK(transactionIdentifier.connectionIdentifier(), connection);
    if (RefPtr transaction = m_idbStorageRegistry->transaction(transactionIdentifier))
        transaction->commit(handledRequestResultsCount);
}

void NetworkStorageManager::didFinishHandlingVersionChangeTransaction(WebCore::IDBDatabaseConnectionIdentifier databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& transactionIdentifier)
{
    if (RefPtr connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->didFinishHandlingVersionChange(transactionIdentifier);
}

RefPtr<WebCore::IDBServer::UniqueIDBDatabaseTransaction> NetworkStorageManager::idbTransaction(const WebCore::IDBRequestData& requestData)
{
    return m_idbStorageRegistry->transaction(requestData.transactionIdentifier());
}

void NetworkStorageManager::createObjectStore(IPC::Connection& connection, const WebCore::IDBRequestData& requestData, const WebCore::IDBObjectStoreInfo& objectStoreInfo)
{
    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;
    MESSAGE_CHECK(transaction->isVersionChange(), connection);

    transaction->createObjectStore(requestData, objectStoreInfo);
}

void NetworkStorageManager::deleteObjectStore(IPC::Connection& connection, const WebCore::IDBRequestData& requestData, const String& objectStoreName)
{
    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;
    MESSAGE_CHECK(transaction->isVersionChange(), connection);

    transaction->deleteObjectStore(requestData, objectStoreName);
}

void NetworkStorageManager::renameObjectStore(IPC::Connection& connection, const WebCore::IDBRequestData& requestData, WebCore::IDBObjectStoreIdentifier objectStoreIdentifier, const String& newName)
{
    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;
    MESSAGE_CHECK(transaction->isVersionChange(), connection);

    transaction->renameObjectStore(requestData, objectStoreIdentifier, newName);
}

void NetworkStorageManager::clearObjectStore(const WebCore::IDBRequestData& requestData, WebCore::IDBObjectStoreIdentifier objectStoreIdentifier)
{
    if (RefPtr transaction = idbTransaction(requestData))
        transaction->clearObjectStore(requestData, objectStoreIdentifier);
}

void NetworkStorageManager::createIndex(IPC::Connection& connection, const WebCore::IDBRequestData& requestData, const WebCore::IDBIndexInfo& indexInfo)
{
    MESSAGE_CHECK(!requestData.requestIdentifier().isEmpty(), connection);
    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;
    MESSAGE_CHECK(transaction->isVersionChange(), connection);

    transaction->createIndex(requestData, indexInfo);
}

void NetworkStorageManager::deleteIndex(IPC::Connection& connection, const WebCore::IDBRequestData& requestData, WebCore::IDBObjectStoreIdentifier objectStoreIdentifier, const String& indexName)
{
    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;
    MESSAGE_CHECK(transaction->isVersionChange(), connection);

    transaction->deleteIndex(requestData, objectStoreIdentifier, indexName);
}

void NetworkStorageManager::renameIndex(IPC::Connection& connection, const WebCore::IDBRequestData& requestData, WebCore::IDBObjectStoreIdentifier objectStoreIdentifier, WebCore::IDBIndexIdentifier indexIdentifier, const String& newName)
{
    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;
    MESSAGE_CHECK(transaction->isVersionChange(), connection);

    transaction->renameIndex(requestData, objectStoreIdentifier, indexIdentifier, newName);
}

void NetworkStorageManager::putOrAdd(IPC::Connection& connection, const WebCore::IDBRequestData& requestData, const WebCore::IDBKeyData& keyData, const WebCore::IDBValue& value, const WebCore::IndexIDToIndexKeyMap& indexKeys, WebCore::IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
    assertIsCurrent(workQueue());
    RefPtr transaction = idbTransaction(requestData);
    if (!transaction)
        return;

    if (value.blobURLs().size() != value.blobFilePaths().size()) {
        RELEASE_LOG_FAULT(IndexedDB, "NetworkStorageManager::putOrAdd: Number of blob URLs doesn't match the number of blob file paths.");
        ASSERT_NOT_REACHED();
        return;
    }

    // Validate temporary blob paths in |value| to make sure they belong to the source process.
    if (!value.blobFilePaths().isEmpty()) {
        auto it = m_temporaryBlobPathsByConnection.find(connection.uniqueID());
        if (it == m_temporaryBlobPathsByConnection.end()) {
            RELEASE_LOG_FAULT(IndexedDB, "NetworkStorageManager::putOrAdd: IDBValue contains blob paths but none are allowed for this process");
            ASSERT_NOT_REACHED();
            return;
        }

        auto& temporaryBlobPathsForConnection = it->value;
        for (auto& blobFilePath : value.blobFilePaths()) {
            if (!temporaryBlobPathsForConnection.remove(blobFilePath)) {
                RELEASE_LOG_FAULT(IndexedDB, "NetworkStorageManager::putOrAdd: Blob path was not created for this WebProcess");
                ASSERT_NOT_REACHED();
                return;
            }
        }
    }

    transaction->putOrAdd(requestData, keyData, value, indexKeys, overwriteMode);
}

void NetworkStorageManager::getRecord(const WebCore::IDBRequestData& requestData, const WebCore::IDBGetRecordData& getRecordData)
{
    if (RefPtr transaction = idbTransaction(requestData))
        transaction->getRecord(requestData, getRecordData);
}

void NetworkStorageManager::getAllRecords(const WebCore::IDBRequestData& requestData, const WebCore::IDBGetAllRecordsData& getAllRecordsData)
{
    if (RefPtr transaction = idbTransaction(requestData))
        transaction->getAllRecords(requestData, getAllRecordsData);
}

void NetworkStorageManager::getCount(const WebCore::IDBRequestData& requestData, const WebCore::IDBKeyRangeData& keyRangeData)
{
    if (RefPtr transaction = idbTransaction(requestData))
        transaction->getCount(requestData, keyRangeData);
}

void NetworkStorageManager::deleteRecord(const WebCore::IDBRequestData& requestData, const WebCore::IDBKeyRangeData& keyRangeData)
{
    if (RefPtr transaction = idbTransaction(requestData))
        transaction->deleteRecord(requestData, keyRangeData);
}

void NetworkStorageManager::openCursor(const WebCore::IDBRequestData& requestData, const WebCore::IDBCursorInfo& cursorInfo)
{
    if (RefPtr transaction = idbTransaction(requestData))
        transaction->openCursor(requestData, cursorInfo);
}

void NetworkStorageManager::iterateCursor(const WebCore::IDBRequestData& requestData, const WebCore::IDBIterateCursorData& cursorData)
{
    if (RefPtr transaction = idbTransaction(requestData))
        transaction->iterateCursor(requestData, cursorData);
}

void NetworkStorageManager::getAllDatabaseNamesAndVersions(IPC::Connection& connection, const WebCore::IDBResourceIdentifier& requestIdentifier, const WebCore::ClientOrigin& origin)
{
    MESSAGE_CHECK(requestIdentifier.connectionIdentifier(), connection);
    Ref connectionToClient = m_idbStorageRegistry->ensureConnectionToClient(connection.uniqueID(), *requestIdentifier.connectionIdentifier());
    auto result = checkedOriginStorageManager(origin)->checkedIDBStorageManager(*m_idbStorageRegistry)->getAllDatabaseNamesAndVersions();
    connectionToClient->didGetAllDatabaseNamesAndVersions(requestIdentifier, WTF::move(result));
}

void NetworkStorageManager::cacheStorageOpenCache(const WebCore::ClientOrigin& origin, const String& cacheName, WebCore::DOMCacheEngine::CacheIdentifierCallback&& callback)
{
    checkedOriginStorageManager(origin)->protectedCacheStorageManager(*m_cacheStorageRegistry, origin, m_queue.copyRef())->openCache(cacheName, WTF::move(callback));
}

void NetworkStorageManager::cacheStorageRemoveCache(WebCore::DOMCacheIdentifier cacheIdentifier, WebCore::DOMCacheEngine::RemoveCacheIdentifierCallback&& callback)
{
    RefPtr cache = m_cacheStorageRegistry->cache(cacheIdentifier);
    if (!cache)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    RefPtr cacheStorageManager = cache->manager();
    if (!cacheStorageManager)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    cacheStorageManager->removeCache(cacheIdentifier, WTF::move(callback));
}

void NetworkStorageManager::cacheStorageAllCaches(const WebCore::ClientOrigin& origin, uint64_t updateCounter, WebCore::DOMCacheEngine::CacheInfosCallback&& callback)
{
    checkedOriginStorageManager(origin)->protectedCacheStorageManager(*m_cacheStorageRegistry, origin, m_queue.copyRef())->allCaches(updateCounter, WTF::move(callback));
}

void NetworkStorageManager::cacheStorageReference(IPC::Connection& connection, WebCore::DOMCacheIdentifier cacheIdentifier)
{
    RefPtr cache = m_cacheStorageRegistry->cache(cacheIdentifier);
    if (!cache)
        return;

    RefPtr cacheStorageManager = cache->manager();
    if (!cacheStorageManager)
        return;

    cacheStorageManager->reference(connection.uniqueID(), cacheIdentifier);
}

void NetworkStorageManager::cacheStorageDereference(IPC::Connection& connection, WebCore::DOMCacheIdentifier cacheIdentifier)
{
    RefPtr cache = m_cacheStorageRegistry->cache(cacheIdentifier);
    if (!cache)
        return;

    RefPtr cacheStorageManager = cache->manager();
    if (!cacheStorageManager)
        return;

    cacheStorageManager->dereference(connection.uniqueID(), cacheIdentifier);
}

void NetworkStorageManager::lockCacheStorage(IPC::Connection& connection, const WebCore::ClientOrigin& origin)
{
    checkedOriginStorageManager(origin)->protectedCacheStorageManager(*m_cacheStorageRegistry, origin, m_queue.copyRef())->lockStorage(connection.uniqueID());
}

void NetworkStorageManager::unlockCacheStorage(IPC::Connection& connection, const WebCore::ClientOrigin& origin)
{
    if (RefPtr cacheStorageManager = checkedOriginStorageManager(origin)->existingCacheStorageManager())
        cacheStorageManager->unlockStorage(connection.uniqueID());
}

void NetworkStorageManager::cacheStorageRetrieveRecords(WebCore::DOMCacheIdentifier cacheIdentifier, WebCore::RetrieveRecordsOptions&& options, WebCore::DOMCacheEngine::CrossThreadRecordsCallback&& callback)
{
    RefPtr cache = m_cacheStorageRegistry->cache(cacheIdentifier);
    if (!cache)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    cache->retrieveRecords(WTF::move(options), WTF::move(callback));
}

void NetworkStorageManager::cacheStorageRemoveRecords(WebCore::DOMCacheIdentifier cacheIdentifier, WebCore::ResourceRequest&& request, WebCore::CacheQueryOptions&& options, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    RefPtr cache = m_cacheStorageRegistry->cache(cacheIdentifier);
    if (!cache)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    cache->removeRecords(WTF::move(request), WTF::move(options), WTF::move(callback));
}

void NetworkStorageManager::cacheStoragePutRecords(IPC::Connection& connection, WebCore::DOMCacheIdentifier cacheIdentifier, Vector<WebCore::DOMCacheEngine::CrossThreadRecord>&& records, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    RefPtr cache = m_cacheStorageRegistry->cache(cacheIdentifier);
    if (!cache)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    for (auto& record : records)
        MESSAGE_CHECK_COMPLETION(record.responseBodySize >= CacheStorageDiskStore::computeRealBodySizeForStorage(record.responseBody), connection, callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal)));

    cache->putRecords(WTF::move(records), WTF::move(callback));
}

void NetworkStorageManager::cacheStorageClearMemoryRepresentation(const WebCore::ClientOrigin& origin, CompletionHandler<void()>&& callback)
{
    assertIsCurrent(workQueue());

    auto iterator = m_originStorageManagers.find(origin);
    if (iterator != m_originStorageManagers.end())
        CheckedRef { *(iterator->value) }->closeCacheStorageManager();

    callback();
}

void NetworkStorageManager::cacheStorageRepresentation(CompletionHandler<void(const String&)>&& callback)
{
    Vector<String> originStrings;
    auto targetTypes = OptionSet<WebsiteDataType> { WebsiteDataType::DOMCache };
    for (auto& origin : getAllOrigins()) {
        {
            CheckedRef originStorageManager = this->originStorageManager(origin);
            auto fetchedTypes = originStorageManager->fetchDataTypesInList(targetTypes, false);

            if (!fetchedTypes.isEmpty()) {
                originStrings.append(makeString("\n{ \"origin\" : { \"topOrigin\" : \""_s,
                    origin.topOrigin.toString(), "\", \"clientOrigin\": \""_s,
                    origin.clientOrigin.toString(), "\" }, \"caches\" : "_s,
                    originStorageManager->protectedCacheStorageManager(*m_cacheStorageRegistry, origin, m_queue.copyRef())->representationString(),
                    '}'
                ));
            }
        }
        removeOriginStorageManagerIfPossible(origin);
    }

    std::ranges::sort(originStrings, codePointCompareLessThan);
    StringBuilder builder;
    builder.append("{ \"path\": \""_s, m_customCacheStoragePath, "\", \"origins\": ["_s);
    ASCIILiteral divider = ""_s;
    for (auto& origin : originStrings) {
        builder.append(divider, origin);
        divider = ","_s;
    }
    builder.append("]}"_s);
    callback(builder.toString());
}

void NetworkStorageManager::dispatchTaskToBackgroundFetchManager(const WebCore::ClientOrigin& origin, Function<void(BackgroundFetchStoreManager*)>&& callback)
{
    ASSERT(RunLoop::isMain());

    if (m_closed) {
        callback(nullptr);
        return;
    }
    workQueue().dispatch([this, protectedThis = Ref { *this }, queue = Ref { m_queue }, origin = crossThreadCopy(origin), callback = WTF::move(callback)]() mutable {
        Ref backgroundFetchManager = checkedOriginStorageManager(origin)->backgroundFetchManager(WTF::move(queue));
        callback(backgroundFetchManager.ptr());
    });
}

void NetworkStorageManager::notifyBackgroundFetchChange(const String& identifier, BackgroundFetchChange change)
{
    if (m_parentConnection)
        IPC::Connection::send(*m_parentConnection, Messages::NetworkProcessProxy::NotifyBackgroundFetchChange(m_sessionID, identifier, change), 0);
}

void NetworkStorageManager::closeServiceWorkerRegistrationFiles(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (m_closed)
        return completionHandler();

    workQueue().dispatch([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)]() mutable {
        assertIsCurrent(workQueue());

        if (m_sharedServiceWorkerStorageManager)
            m_sharedServiceWorkerStorageManager->closeFiles();
        else {
            for (auto& manager : m_originStorageManagers.values())
                manager->serviceWorkerStorageManager().closeFiles();
        }

        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::clearServiceWorkerRegistrations(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (m_closed)
        return completionHandler();

    workQueue().dispatch([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)]() mutable {
        assertIsCurrent(workQueue());

        if (m_sharedServiceWorkerStorageManager)
            m_sharedServiceWorkerStorageManager->clearAllRegistrations();
        else {
            for (auto& origin : getAllOrigins()) {
                checkedOriginStorageManager(origin)->serviceWorkerStorageManager().clearAllRegistrations();
                removeOriginStorageManagerIfPossible(origin);
            }
        }

        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::importServiceWorkerRegistrations(CompletionHandler<void(std::optional<Vector<WebCore::ServiceWorkerContextData>>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (m_closed)
        return completionHandler(std::nullopt);

    workQueue().dispatch([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)]() mutable {
        assertIsCurrent(workQueue());

        std::optional<Vector<WebCore::ServiceWorkerContextData>> result;
        if (m_sharedServiceWorkerStorageManager)
            result = m_sharedServiceWorkerStorageManager->importRegistrations();
        else {
            bool hasResult = false;
            Vector<WebCore::ServiceWorkerContextData> registrations;
            for (auto& origin : getAllOrigins()) {
                if (auto originRegistrations = checkedOriginStorageManager(origin)->serviceWorkerStorageManager().importRegistrations()) {
                    hasResult = true;
                    registrations.appendVector(WTF::move(*originRegistrations));
                }
                removeOriginStorageManagerIfPossible(origin);
            }
            if (hasResult)
                result = WTF::move(registrations);
        }

        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis), result = crossThreadCopy(WTF::move(result)), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(WTF::move(result));
        });
    });
}

void NetworkStorageManager::updateServiceWorkerRegistrations(Vector<WebCore::ServiceWorkerContextData>&& registrationsToUpdate, Vector<WebCore::ServiceWorkerRegistrationKey>&& registrationsToDelete, CompletionHandler<void(std::optional<Vector<WebCore::ServiceWorkerScripts>>)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (m_closed)
        return completionHandler(std::nullopt);

    workQueue().dispatch([this, protectedThis = Ref { *this }, registrationsToUpdate = crossThreadCopy(WTF::move(registrationsToUpdate)), registrationsToDelete = crossThreadCopy(WTF::move(registrationsToDelete)), completionHandler = WTF::move(completionHandler)]() mutable {
        assertIsCurrent(workQueue());

        std::optional<Vector<WebCore::ServiceWorkerScripts>> result;
        if (m_sharedServiceWorkerStorageManager)
            result = m_sharedServiceWorkerStorageManager->updateRegistrations(WTF::move(registrationsToUpdate), WTF::move(registrationsToDelete));
        else
            result = updateServiceWorkerRegistrationsByOrigin(WTF::move(registrationsToUpdate), WTF::move(registrationsToDelete));

        RunLoop::mainSingleton().dispatch([protectedThis = WTF::move(protectedThis), result = crossThreadCopy(WTF::move(result)), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(WTF::move(result));
        });
    });
}

void NetworkStorageManager::migrateServiceWorkerRegistrationsToOrigins()
{
    ASSERT(!RunLoop::isMain());

    auto sharedServiceWorkerStorageManager = makeUnique<ServiceWorkerStorageManager>(m_customServiceWorkerStoragePath);
    auto result = sharedServiceWorkerStorageManager->importRegistrations();
    if (!result)
        return;

    updateServiceWorkerRegistrationsByOrigin(WTF::move(*result), { });
    sharedServiceWorkerStorageManager->clearAllRegistrations();
}

Vector<WebCore::ServiceWorkerScripts> NetworkStorageManager::updateServiceWorkerRegistrationsByOrigin(Vector<WebCore::ServiceWorkerContextData>&& registrationsToUpdate, Vector<WebCore::ServiceWorkerRegistrationKey>&& registrationsToDelete)
{
    ASSERT(!RunLoop::isMain());

    HashMap<WebCore::ClientOrigin, std::pair<Vector<WebCore::ServiceWorkerContextData>, Vector<WebCore::ServiceWorkerRegistrationKey>>> originRegistrations;
    for (auto& registration : registrationsToUpdate) {
        auto origin = registration.registration.key.clientOrigin();
        auto& registrations = originRegistrations.ensure(origin, []() {
            return std::pair<Vector<WebCore::ServiceWorkerContextData>, Vector<WebCore::ServiceWorkerRegistrationKey>> { };
        }).iterator->value.first;
        registrations.append(WTF::move(registration));
    }

    HashMap<WebCore::ClientOrigin, Vector<WebCore::ServiceWorkerRegistrationKey>> originRegistrationsToDelete;
    for (auto&& key : registrationsToDelete) {
        auto origin = key.clientOrigin();
        auto& keys = originRegistrations.ensure(origin, []() {
            return std::pair<Vector<WebCore::ServiceWorkerContextData>, Vector<WebCore::ServiceWorkerRegistrationKey>> { };
        }).iterator->value.second;
        keys.append(WTF::move(key));
    }

    Vector<WebCore::ServiceWorkerScripts> savedScripts;
    for (auto& [origin, registrations] : originRegistrations) {
        auto result = checkedOriginStorageManager(origin)->serviceWorkerStorageManager().updateRegistrations(WTF::move(registrations.first), WTF::move(registrations.second));
        if (result)
            savedScripts.appendVector(WTF::move(*result));
    }

    return savedScripts;
}

bool NetworkStorageManager::shouldManageServiceWorkerRegistrationsByOrigin()
{
    ASSERT(!RunLoop::isMain());

    return m_unifiedOriginStorageLevel >= UnifiedOriginStorageLevel::Standard;
}

RefPtr<FileSystemStorageHandleRegistry> NetworkStorageManager::protectedFileSystemStorageHandleRegistry()
{
    return m_fileSystemStorageHandleRegistry;
}

bool NetworkStorageManager::isStorageTypeEnabled(IPC::Connection& connection, WebCore::StorageType storageType) const
{
    auto preferences = sharedPreferencesForWebProcess(connection);
    if (!preferences)
        return true;

    switch (storageType) {
    case WebCore::StorageType::Local:
    case WebCore::StorageType::TransientLocal:
        return preferences->localStorageEnabled;
    case WebCore::StorageType::Session:
        return preferences->sessionStorageEnabled;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool NetworkStorageManager::isStorageAreaTypeEnabled(IPC::Connection& connection, StorageAreaBase::StorageType storageType) const
{
    auto preferences = sharedPreferencesForWebProcess(connection);
    if (!preferences)
        return true;

    switch (storageType) {
    case StorageAreaBase::StorageType::Local:
        return preferences->localStorageEnabled;
    case StorageAreaBase::StorageType::Session:
        return preferences->sessionStorageEnabled;
    }
    ASSERT_NOT_REACHED();
    return false;
}

std::optional<SharedPreferencesForWebProcess> NetworkStorageManager::sharedPreferencesForWebProcess(IPC::Connection& connection) const
{
    assertIsCurrent(workQueue());

    auto iter = m_preferencesForConnections.find(connection.uniqueID());
    if (iter == m_preferencesForConnections.end())
        return std::nullopt;

    return iter->value;
}

void NetworkStorageManager::queryCacheStorage(WebCore::ClientOrigin&& origin, WebCore::RetrieveRecordsOptions&& options, String&& cacheName, CompletionHandler<void(std::optional<WebCore::DOMCacheEngine::Record>&&)>&& callback)
{
    auto mainThreadCallback = [callback = WTF::move(callback)](std::optional<WebCore::DOMCacheEngine::CrossThreadRecord>&& result) mutable {
        callOnMainRunLoop([callback = WTF::move(callback), result = crossThreadCopy(WTF::move(result))]() mutable {
            if (!result) {
                callback({ });
                return;
            }
            callback(WebCore::DOMCacheEngine::fromCrossThreadRecord(WTF::move(*result)));
        });
    };
    workQueue().dispatch([weakThis = ThreadSafeWeakPtr { *this }, origin = WTF::move(origin).isolatedCopy(), options = WTF::move(options).isolatedCopy(), cacheName = WTF::move(cacheName).isolatedCopy(), callback = WTF::move(mainThreadCallback)]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            callback({ });
            return;
        }

        assertIsCurrent(protectedThis->workQueue());

        RefPtr cacheStorageManager = protectedThis->checkedOriginStorageManager(origin)->existingCacheStorageManager();
        if (!cacheStorageManager) {
            callback({ });
            return;
        }

        cacheStorageManager->query(WTF::move(options), WTF::move(cacheName), WTF::move(callback));
    });
}

} // namespace WebKit

#undef MESSAGE_CHECK_COMPLETION
#undef MESSAGE_CHECK
