/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "CacheStorageCache.h"
#include "CacheStorageManager.h"
#include "CacheStorageRegistry.h"
#include "FileSystemStorageHandleRegistry.h"
#include "FileSystemStorageManager.h"
#include "IDBStorageConnectionToClient.h"
#include "IDBStorageManager.h"
#include "IDBStorageRegistry.h"
#include "LocalStorageManager.h"
#include "Logging.h"
#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "NetworkStorageManagerMessages.h"
#include "OriginQuotaManager.h"
#include "OriginStorageManager.h"
#include "SessionStorageManager.h"
#include "StorageAreaBase.h"
#include "StorageAreaMapMessages.h"
#include "StorageAreaRegistry.h"
#include "UnifiedOriginStorageLevel.h"
#include "WebsiteDataType.h"
#include <WebCore/SecurityOriginData.h>
#include <WebCore/StorageUtilities.h>
#include <WebCore/UniqueIDBDatabaseConnection.h>
#include <WebCore/UniqueIDBDatabaseTransaction.h>
#include <pal/crypto/CryptoDigest.h>
#include <wtf/SuspendableWorkQueue.h>
#include <wtf/text/Base64.h>

#if ENABLE(SERVICE_WORKER)
#include "BackgroundFetchChange.h"
#include "BackgroundFetchStoreManager.h"
#include "ServiceWorkerStorageManager.h"
#include <WebCore/ServiceWorkerContextData.h>
#endif

namespace WebKit {

#if PLATFORM(IOS_FAMILY)
static const Seconds defaultBackupExclusionPeriod { 24_h };
#endif

static constexpr double defaultThirdPartyOriginQuotaRatio = 0.1; // third-party_origin_quota / origin_quota
static constexpr uint64_t defaultVolumeCapacityUnit = 1 * GB;
static constexpr auto persistedFileName = "persisted"_s;

// FIXME: Remove this if rdar://104754030 is fixed.
static HashMap<String, ThreadSafeWeakPtr<NetworkStorageManager>>& activePaths()
{
    static MainThreadNeverDestroyed<HashMap<String, ThreadSafeWeakPtr<NetworkStorageManager>>> pathToManagerMap;
    return pathToManagerMap;
}

static String encode(const String& string, FileSystem::Salt salt)
{
    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    auto utf8String = string.utf8();
    crypto->addBytes(utf8String.data(), utf8String.length());
    crypto->addBytes(salt.data(), salt.size());
    auto hash = crypto->computeHash();
    return base64URLEncodeToString(hash.data(), hash.size());
}

static String originDirectoryPath(const String& rootPath, const WebCore::ClientOrigin& origin, FileSystem::Salt salt)
{
    if (rootPath.isEmpty())
        return emptyString();

    auto encodedTopOrigin = encode(origin.topOrigin.toString(), salt);
    auto encodedOpeningOrigin = encode(origin.clientOrigin.toString(), salt);
    return FileSystem::pathByAppendingComponents(rootPath, { encodedTopOrigin, encodedOpeningOrigin });
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

    if (children.size() >= 2)
        return false;

    HashSet<String> invalidFileNames {
        OriginStorageManager::originFileIdentifier()
#if PLATFORM(COCOA)
        , ".DS_Store"_s
#endif
    };
    return WTF::allOf(children, [&] (auto& child) {
        return invalidFileNames.contains(child);
    });
}

static void deleteEmptyOriginDirectory(const String& directory)
{
    if (directory.isEmpty())
        return;

    if (isEmptyOriginDirectory(directory))
        FileSystem::deleteFile(originFilePath(directory));

    FileSystem::deleteEmptyDirectory(directory);
    FileSystem::deleteEmptyDirectory(FileSystem::parentPath(directory));
}

String NetworkStorageManager::persistedFilePath(const WebCore::ClientOrigin& origin)
{
    auto directory = originDirectoryPath(m_path, origin, m_salt);
    if (directory.isEmpty())
        return emptyString();

    return FileSystem::pathByAppendingComponent(directory, persistedFileName);
}

Ref<NetworkStorageManager> NetworkStorageManager::create(NetworkProcess& process, PAL::SessionID sessionID, Markable<WTF::UUID> identifier, IPC::Connection::UniqueID connection, const String& path, const String& customLocalStoragePath, const String& customIDBStoragePath, const String& customCacheStoragePath, const String& customServiceWorkerStoragePath, uint64_t defaultOriginQuota, std::optional<double> originQuotaRatio, std::optional<double> totalQuotaRatio, std::optional<uint64_t> standardVolumeCapacity, std::optional<uint64_t> volumeCapacityOverride, UnifiedOriginStorageLevel level)
{
    return adoptRef(*new NetworkStorageManager(process, sessionID, identifier, connection, path, customLocalStoragePath, customIDBStoragePath, customCacheStoragePath, customServiceWorkerStoragePath, defaultOriginQuota, originQuotaRatio, totalQuotaRatio, standardVolumeCapacity, volumeCapacityOverride, level));
}

NetworkStorageManager::NetworkStorageManager(NetworkProcess& process, PAL::SessionID sessionID, Markable<WTF::UUID> identifier, IPC::Connection::UniqueID connection, const String& path, const String& customLocalStoragePath, const String& customIDBStoragePath, const String& customCacheStoragePath, const String& customServiceWorkerStoragePath, uint64_t defaultOriginQuota, std::optional<double> originQuotaRatio, std::optional<double> totalQuotaRatio, std::optional<uint64_t> standardVolumeCapacity, std::optional<uint64_t> volumeCapacityOverride, UnifiedOriginStorageLevel level)
    : m_process(process)
    , m_sessionID(sessionID)
    , m_queueName(makeString("com.apple.WebKit.Storage.", sessionID.toUInt64(), ".", static_cast<uint64_t>(identifier->data() >> 64), static_cast<uint64_t>(identifier->data())))
    , m_queue(SuspendableWorkQueue::create(m_queueName.utf8().data(), SuspendableWorkQueue::QOS::Default, SuspendableWorkQueue::ShouldLog::Yes))
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

    m_queue->dispatch([this, weakThis = ThreadSafeWeakPtr { *this }, path = path.isolatedCopy(), customLocalStoragePath = crossThreadCopy(customLocalStoragePath), customIDBStoragePath = crossThreadCopy(customIDBStoragePath), customCacheStoragePath = crossThreadCopy(customCacheStoragePath), customServiceWorkerStoragePath = crossThreadCopy(customServiceWorkerStoragePath), defaultOriginQuota, originQuotaRatio, totalQuotaRatio, standardVolumeCapacity, volumeCapacityOverride, level]() mutable {
        assertIsCurrent(workQueue());

        auto strongThis = weakThis.get();
        if (!strongThis)
            return;

        m_defaultOriginQuota = defaultOriginQuota;
        m_originQuotaRatio = originQuotaRatio;
        m_totalQuotaRatio = totalQuotaRatio;
        m_standardVolumeCapacity = standardVolumeCapacity;
        m_volumeCapacityOverride = volumeCapacityOverride;
#if PLATFORM(IOS_FAMILY)
        m_backupExclusionPeriod = defaultBackupExclusionPeriod;
#endif
        m_fileSystemStorageHandleRegistry = makeUnique<FileSystemStorageHandleRegistry>();
        m_storageAreaRegistry = makeUnique<StorageAreaRegistry>();
        m_idbStorageRegistry = makeUnique<IDBStorageRegistry>();
        m_cacheStorageRegistry = makeUnique<CacheStorageRegistry>();
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
#if ENABLE(SERVICE_WORKER)
        if (shouldManageServiceWorkerRegistrationsByOrigin())
            migrateServiceWorkerRegistrationsToOrigins();
        else
            m_sharedServiceWorkerStorageManager = makeUnique<ServiceWorkerStorageManager>(m_customServiceWorkerStoragePath);
#endif
#if PLATFORM(IOS_FAMILY)
        // Exclude LocalStorage directory to reduce backup traffic. See https://webkit.org/b/168388.
        if (m_unifiedOriginStorageLevel == UnifiedOriginStorageLevel::None  && !m_customLocalStoragePath.isEmpty()) {
            FileSystem::makeAllDirectories(m_customLocalStoragePath);
            FileSystem::setExcludedFromBackup(m_customLocalStoragePath, true);
        }
#endif

        RunLoop::main().dispatch([strongThis = WTFMove(strongThis)] { });
    });
}

NetworkStorageManager::~NetworkStorageManager()
{
    ASSERT(RunLoop::isMain());
    ASSERT(m_closed);
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
#if ENABLE(SERVICE_WORKER)
        WebsiteDataType::ServiceWorkerRegistrations
#endif
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

    m_queue->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        assertIsCurrent(workQueue());

        m_originStorageManagers.clear();
        m_fileSystemStorageHandleRegistry = nullptr;
        for (auto&& completionHandler : std::exchange(m_persistCompletionHandlers, { }))
            completionHandler.second(false);
#if ENABLE(SERVICE_WORKER)
        m_sharedServiceWorkerStorageManager = nullptr;
#endif

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::startReceivingMessageFromConnection(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());

    connection.addWorkQueueMessageReceiver(Messages::NetworkStorageManager::messageReceiverName(), m_queue.get(), *this);
    m_connections.add(connection);
}

void NetworkStorageManager::stopReceivingMessageFromConnection(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());
    
    if (!m_connections.remove(connection))
        return;

    connection.removeWorkQueueMessageReceiver(Messages::NetworkStorageManager::messageReceiverName());
    m_queue->dispatch([this, protectedThis = Ref { *this }, connection = connection.uniqueID()]() mutable {
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

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis)] { });
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
    auto* manager = m_originStorageManagers.get(origin);
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

    RunLoop::main().dispatch([this, weakThis = ThreadSafeWeakPtr { *this }]() mutable {
        auto strongThis = weakThis.get();
        if (!strongThis || m_closed || !m_process)
            return;

        m_process->registrableDomainsWithLastAccessedTime(m_sessionID, [this, weakThis = WTFMove(weakThis)](auto result) mutable {
            auto strongThis = weakThis.get();
            if (!strongThis || m_closed)
                return;

            m_queue->dispatch([weakThis = WTFMove(weakThis), result = crossThreadCopy(WTFMove(result))]() mutable {
                if (auto strongThis = weakThis.get()) {
                    strongThis->donePrepareForEviction(WTFMove(result));
                    RunLoop::main().dispatch([strongThis = WTFMove(strongThis)] { });
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
        FALLTHROUGH;
    }
    case UnifiedOriginStorageLevel::Basic: {
        auto cacheStoragePath = CacheStorageManager::cacheStorageOriginDirectory(m_customCacheStoragePath, origin);
        auto cacheStorageModificationTime = valueOrDefault(FileSystem::fileModificationTime(cacheStoragePath));
        lastModificationTime = std::max(cacheStorageModificationTime, lastModificationTime);
        FALLTHROUGH;
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
        auto usage = originStorageManager(origin).quotaManager().usage();
        totalUsage += usage;
        WallTime accessTime;
        if (domainsWithLastAccessedTime)
            accessTime = domainsWithLastAccessedTime->get(WebCore::RegistrableDomain { origin.topOrigin });
        else
            accessTime = lastModificationTimeForOrigin(origin, originStorageManager(origin));

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
    performEviction(WTFMove(originRecords));
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
        sortedOriginRecords.append({ WTFMove(origin), WTFMove(record) });

    std::sort(sortedOriginRecords.begin(), sortedOriginRecords.end(), [&](const auto& a, const auto& b) {
        return a.second.lastAccessTime > b.second.lastAccessTime;
    });

    uint64_t deletedOriginCount = 0;
    while (!sortedOriginRecords.isEmpty() && *m_totalUsage > *m_totalQuota) {
        auto [topOrigin, record] = sortedOriginRecords.takeLast();
        if (record.isActive || valueOrDefault(record.isPersisted))
            continue;

        for (auto& clientOrigin : record.clientOrigins) {
            auto origin = WebCore::ClientOrigin { topOrigin, clientOrigin };
            originStorageManager(origin).deleteData(allManagedTypes(), -WallTime::infinity());
            removeOriginStorageManagerIfPossible(origin);
        }

        m_totalUsage = *m_totalUsage - record.usage;
        ++deletedOriginCount;
    }

    UNUSED_PARAM(deletedOriginCount);
    RELEASE_LOG(Storage, "%p - NetworkStorageManager::performEviction evicts %" PRIu64 " origins, current usage %" PRIu64 ", total quota %" PRIu64, this, deletedOriginCount, valueOrDefault(m_totalUsage), *m_totalQuota);
}

OriginStorageManager& NetworkStorageManager::originStorageManager(const WebCore::ClientOrigin& origin, ShouldWriteOriginFile shouldWriteOriginFile)
{
    assertIsCurrent(workQueue());

    auto& originStorageManager = *m_originStorageManagers.ensure(origin, [&] {
        auto originDirectory = originDirectoryPath(m_path, origin, m_salt);
        auto localStoragePath = LocalStorageManager::localStorageFilePath(m_customLocalStoragePath, origin);
        auto idbStoragePath = IDBStorageManager::idbStorageOriginDirectory(m_customIDBStoragePath, origin);
        auto cacheStoragePath = CacheStorageManager::cacheStorageOriginDirectory(m_customCacheStoragePath, origin);
        CacheStorageManager::copySaltFileToOriginDirectory(m_customCacheStoragePath, cacheStoragePath);
        OriginQuotaManager::IncreaseQuotaFunction increaseQuotaFunction = [sessionID = m_sessionID, origin, connection = m_parentConnection] (auto identifier, auto currentQuota, auto currentUsage, auto requestedIncrease) mutable {
            IPC::Connection::send(connection, Messages::NetworkProcessProxy::IncreaseQuota(sessionID, origin, identifier, currentQuota, currentUsage, requestedIncrease), 0);
        };
        // Use double for multiplication to preserve precision.
        double quota = m_defaultOriginQuota;
        double standardReportedQuota = m_standardVolumeCapacity ? *m_standardVolumeCapacity : 0.0;
        if (m_originQuotaRatio) {
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
            if (auto strongThis = weakThis.get()) {
                strongThis->spaceGrantedForOrigin(origin, spaceRequested);
                RunLoop::main().dispatch([strongThis = WTFMove(strongThis)] { });
            }
        };
        // Use std::ceil instead of implicit conversion to make result more definitive.
        return makeUnique<OriginStorageManager>(std::ceil(quota), std::ceil(standardReportedQuota), WTFMove(increaseQuotaFunction), WTFMove(notifySpaceGrantedFunction), WTFMove(originDirectory), WTFMove(localStoragePath), WTFMove(idbStoragePath), WTFMove(cacheStoragePath), m_unifiedOriginStorageLevel);
    }).iterator->value;

    if (shouldWriteOriginFile == ShouldWriteOriginFile::Yes)
        writeOriginToFileIfNecessary(origin);

    return originStorageManager;
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
    // This function must be called when origin is in use, i.e. OriginStorageManager exists.
    auto* manager = m_originStorageManagers.get(origin);
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

    m_process->registrableDomainsExemptFromWebsiteDataDeletion(m_sessionID, [weakThis = ThreadSafeWeakPtr { *this }](auto&& domains) mutable {
        if (auto strongThis = weakThis.get())
            strongThis->didFetchRegistrableDomainsForPersist(WTFMove(domains));
    });
}

void NetworkStorageManager::didFetchRegistrableDomainsForPersist(HashSet<WebCore::RegistrableDomain>&& domains)
{
    ASSERT(RunLoop::isMain());

    if (m_closed)
        return;

    m_queue->dispatch([this, weakThis = ThreadSafeWeakPtr { *this }, domains = crossThreadCopy(WTFMove(domains))]() mutable {
        assertIsCurrent(workQueue());

        auto strongThis = weakThis.get();
        if (!strongThis)
            return;

        m_domainsExemptFromEviction = WTFMove(domains);
        for (auto&& [origin, completionHandler] : std::exchange(m_persistCompletionHandlers, { }))
            completionHandler(persistOrigin(origin));
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

    m_persistCompletionHandlers.append({ origin, WTFMove(completionHandler) });
    RunLoop::main().dispatch([weakThis = ThreadSafeWeakPtr { *this }]() mutable {
        if (auto strongThis = weakThis.get())
            strongThis->fetchRegistrableDomainsForPersist();
    });
}

void NetworkStorageManager::estimate(const WebCore::ClientOrigin& origin, CompletionHandler<void(std::optional<WebCore::StorageEstimate>)>&& completionHandler)
{
    assertIsCurrent(workQueue());

    completionHandler(originStorageManager(origin).estimate());
}

void NetworkStorageManager::resetStoragePersistedState(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        assertIsCurrent(workQueue());
        for (auto& origin : getAllOrigins()) {
            auto persistedFile = persistedFilePath(origin);
            if (!persistedFile.isEmpty())
                FileSystem::deleteFile(persistedFile);
        }

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::clearStorageForWebPage(WebPageProxyIdentifier pageIdentifier)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, pageIdentifier]() mutable {
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

    m_queue->dispatch([this, protectedThis = Ref { *this }, fromIdentifier, toIdentifier]() mutable {
        assertIsCurrent(workQueue());
        cloneSessionStorageNamespace(ObjectIdentifier<StorageNamespaceIdentifierType>(fromIdentifier.toUInt64()), ObjectIdentifier<StorageNamespaceIdentifierType>(toIdentifier.toUInt64()));
    });
}

void NetworkStorageManager::didIncreaseQuota(WebCore::ClientOrigin&& origin, QuotaIncreaseRequestIdentifier identifier, std::optional<uint64_t> newQuota)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, origin = crossThreadCopy(WTFMove(origin)), identifier, newQuota]() mutable {
        assertIsCurrent(workQueue());
        if (auto manager = m_originStorageManagers.get(origin))
            manager->quotaManager().didIncreaseQuota(identifier, newQuota);
    });
}

void NetworkStorageManager::fileSystemGetDirectory(IPC::Connection& connection, WebCore::ClientOrigin&& origin, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    completionHandler(originStorageManager(origin).fileSystemStorageManager(*m_fileSystemStorageHandleRegistry).getDirectory(connection.uniqueID()));
}

void NetworkStorageManager::closeHandle(WebCore::FileSystemHandleIdentifier identifier)
{
    ASSERT(!RunLoop::isMain());

    if (auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier))
        handle->close();
}

void NetworkStorageManager::isSameEntry(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier targetIdentifier, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(false);

    completionHandler(handle->isSameEntry(targetIdentifier));
}

void NetworkStorageManager::move(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier destinationIdentifier, const String& newName, CompletionHandler<void(std::optional<FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(FileSystemStorageError::Unknown);

    completionHandler(handle->move(destinationIdentifier, newName));
}

void NetworkStorageManager::getFileHandle(IPC::Connection& connection, WebCore::FileSystemHandleIdentifier identifier, String&& name, bool createIfNecessary, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->getFileHandle(connection.uniqueID(), WTFMove(name), createIfNecessary));
}

void NetworkStorageManager::getDirectoryHandle(IPC::Connection& connection, WebCore::FileSystemHandleIdentifier identifier, String&& name, bool createIfNecessary, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->getDirectoryHandle(connection.uniqueID(), WTFMove(name), createIfNecessary));
}

void NetworkStorageManager::removeEntry(WebCore::FileSystemHandleIdentifier identifier, const String& name, bool deleteRecursively, CompletionHandler<void(std::optional<FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(FileSystemStorageError::Unknown);

    completionHandler(handle->removeEntry(name, deleteRecursively));
}

void NetworkStorageManager::resolve(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier targetIdentifier, CompletionHandler<void(Expected<Vector<String>, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->resolve(targetIdentifier));
}

void NetworkStorageManager::getFile(WebCore::FileSystemHandleIdentifier identifier, CompletionHandler<void(Expected<String, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->path());
}

void NetworkStorageManager::createSyncAccessHandle(WebCore::FileSystemHandleIdentifier identifier, CompletionHandler<void(Expected<FileSystemSyncAccessHandleInfo, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->createSyncAccessHandle());
}

void NetworkStorageManager::closeSyncAccessHandle(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    if (auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier))
        handle->closeSyncAccessHandle(accessHandleIdentifier);

    completionHandler();
}

void NetworkStorageManager::requestNewCapacityForSyncAccessHandle(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, uint64_t newCapacity, CompletionHandler<void(std::optional<uint64_t>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(std::nullopt);

    handle->requestNewCapacityForSyncAccessHandle(accessHandleIdentifier, newCapacity, WTFMove(completionHandler));
}

void NetworkStorageManager::getHandleNames(WebCore::FileSystemHandleIdentifier identifier, CompletionHandler<void(Expected<Vector<String>, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->getHandleNames());
}

void NetworkStorageManager::getHandle(IPC::Connection& connection, WebCore::FileSystemHandleIdentifier identifier, String&& name, CompletionHandler<void(Expected<std::pair<WebCore::FileSystemHandleIdentifier, bool>, FileSystemStorageError>)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));

    completionHandler(handle->getHandle(connection.uniqueID(), WTFMove(name)));
}

void NetworkStorageManager::forEachOriginDirectory(const Function<void(const String&)>& apply)
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
        auto typeSizeMap = originStorageManager(origin).fetchDataTypesInList(targetTypes, shouldComputeSize == ShouldComputeSize::Yes);
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

    m_queue->dispatch([this, protectedThis = Ref { *this }, types, shouldComputeSize, completionHandler = WTFMove(completionHandler)]() mutable {
        auto entries = fetchDataFromDisk(types, shouldComputeSize);
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler), entries = crossThreadCopy(WTFMove(entries))]() mutable {
            completionHandler(WTFMove(entries));
        });
    });
}

HashSet<WebCore::ClientOrigin> NetworkStorageManager::deleteDataOnDisk(OptionSet<WebsiteDataType> types, WallTime modifiedSinceTime, const Function<bool(const WebCore::ClientOrigin&)>& filter)
{
    ASSERT(!RunLoop::isMain());

    HashSet<WebCore::ClientOrigin> deletedOrigins;
    for (auto& origin : getAllOrigins()) {
        if (!filter(origin))
            continue;

        auto existingDataTypes = originStorageManager(origin).fetchDataTypesInList(types, false);
        if (!existingDataTypes.isEmpty()) {
            deletedOrigins.add(origin);
            originStorageManager(origin).deleteData(types, modifiedSinceTime);
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

    m_queue->dispatch([this, protectedThis = Ref { *this }, types, origins = crossThreadCopy(origins), completionHandler = WTFMove(completionHandler)]() mutable {
        HashSet<WebCore::SecurityOriginData> originSet;
        originSet.reserveInitialCapacity(origins.size());
        for (auto origin : origins)
            originSet.add(WTFMove(origin));

        deleteDataOnDisk(types, -WallTime::infinity(), [&originSet](auto origin) {
            return originSet.contains(origin.topOrigin) || originSet.contains(origin.clientOrigin);
        });
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::deleteData(OptionSet<WebsiteDataType> types, const WebCore::ClientOrigin& origin, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, types, originToDelete = origin.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        deleteDataOnDisk(types, -WallTime::infinity(), [originToDelete = WTFMove(originToDelete)](auto& origin) {
            return origin == originToDelete;
        });
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::deleteDataModifiedSince(OptionSet<WebsiteDataType> types, WallTime modifiedSinceTime, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, types, modifiedSinceTime, completionHandler = WTFMove(completionHandler)]() mutable {
        deleteDataOnDisk(types, modifiedSinceTime, [](auto&) {
            return true;
        });

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::deleteDataForRegistrableDomains(OptionSet<WebsiteDataType> types, const Vector<WebCore::RegistrableDomain>& domains, CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, types, domains = crossThreadCopy(domains), completionHandler = WTFMove(completionHandler)]() mutable {
        auto deletedOrigins = deleteDataOnDisk(types, -WallTime::infinity(), [&domains](auto& origin) {
            auto domain = WebCore::RegistrableDomain::uncheckedCreateFromHost(origin.clientOrigin.host());
            return domains.contains(domain);
        });

        HashSet<WebCore::RegistrableDomain> deletedDomains;
        for (auto origin : deletedOrigins) {
            auto domain = WebCore::RegistrableDomain::uncheckedCreateFromHost(origin.clientOrigin.host());
            deletedDomains.add(domain);
        }

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler), domains = crossThreadCopy(WTFMove(deletedDomains))]() mutable {
            completionHandler(WTFMove(domains));
        });
    });
}

void NetworkStorageManager::moveData(OptionSet<WebsiteDataType> types, WebCore::SecurityOriginData&& source, WebCore::SecurityOriginData&& target, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, types, source = crossThreadCopy(WTFMove(source)), target = crossThreadCopy(WTFMove(target)), completionHandler = WTFMove(completionHandler)]() mutable {
        auto sourceOrigin = WebCore::ClientOrigin { source, source };
        auto targetOrigin = WebCore::ClientOrigin { target, target };
        
        // Clear existing data of target origin.
        originStorageManager(targetOrigin).deleteData(types, -WallTime::infinity());

        // Move data from source origin to target origin.
        originStorageManager(sourceOrigin).moveData(types, originStorageManager(targetOrigin).resolvedPath(WebsiteDataType::LocalStorage), originStorageManager(targetOrigin).resolvedPath(WebsiteDataType::IndexedDBDatabases));

        removeOriginStorageManagerIfPossible(targetOrigin);
        removeOriginStorageManagerIfPossible(sourceOrigin);

        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void NetworkStorageManager::getOriginDirectory(WebCore::ClientOrigin&& origin, WebsiteDataType type, CompletionHandler<void(const String&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, type, origin = crossThreadCopy(WTFMove(origin)), completionHandler = WTFMove(completionHandler)]() mutable {
        RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), directory = crossThreadCopy(originStorageManager(origin).resolvedPath(type))]() mutable {
            completionHandler(WTFMove(directory));
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
    m_queue->suspend([this, protectedThis = Ref { *this }] {
        assertIsCurrent(workQueue());
        for (auto& manager : m_originStorageManagers.values()) {
            if (auto localStorageManager = manager->existingLocalStorageManager())
                localStorageManager->syncLocalStorage();
            if (auto idbStorageManager = manager->existingIDBStorageManager())
                idbStorageManager->stopDatabaseActivitiesForSuspend();
        }
    }, WTFMove(completionHandler));
}

void NetworkStorageManager::resume()
{
    ASSERT(RunLoop::isMain());

    if (m_sessionID.isEphemeral())
        return;

    RELEASE_LOG(ProcessSuspension, "%p - NetworkStorageManager::resume()", this);
    m_queue->resume();
}

void NetworkStorageManager::handleLowMemoryWarning()
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }] {
        assertIsCurrent(workQueue());
        for (auto& manager : m_originStorageManagers.values()) {
            if (auto localStorageManager = manager->existingLocalStorageManager())
                localStorageManager->handleLowMemoryWarning();
            if (auto idbStorageManager = manager->existingIDBStorageManager())
                idbStorageManager->handleLowMemoryWarning();
        }
    });
}

void NetworkStorageManager::syncLocalStorage(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        assertIsCurrent(workQueue());
        for (auto& manager : m_originStorageManagers.values()) {
            if (auto localStorageManager = manager->existingLocalStorageManager())
                localStorageManager->syncLocalStorage();
        }

        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void NetworkStorageManager::registerTemporaryBlobFilePaths(IPC::Connection& connection, const Vector<String>& filePaths)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, connectionID = connection.uniqueID(), filePaths = crossThreadCopy(filePaths)] {
        assertIsCurrent(workQueue());
        auto& temporaryBlobPaths = m_temporaryBlobPathsByConnection.ensure(connectionID, [] {
            return HashSet<String> { };
        }).iterator->value;
        temporaryBlobPaths.add(filePaths.begin(), filePaths.end());
    });
}

void NetworkStorageManager::requestSpace(const WebCore::ClientOrigin& origin, uint64_t size, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, origin = crossThreadCopy(origin), size, completionHandler = WTFMove(completionHandler)]() mutable {
        originStorageManager(origin).quotaManager().requestSpace(size, [completionHandler = WTFMove(completionHandler)](auto decision) mutable {
            RunLoop::main().dispatch([completionHandler = WTFMove(completionHandler), decision]() mutable {
                completionHandler(decision == OriginQuotaManager::Decision::Grant);
            });
        });
    });
}

void NetworkStorageManager::resetQuotaForTesting(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        assertIsCurrent(workQueue());
        for (auto& manager : m_originStorageManagers.values())
            manager->quotaManager().resetQuotaForTesting();
        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

void NetworkStorageManager::resetQuotaUpdatedBasedOnUsageForTesting(WebCore::ClientOrigin&& origin)
{
    ASSERT(RunLoop::isMain());

    m_queue->dispatch([this, protectedThis = Ref { *this }, origin = crossThreadCopy(WTFMove(origin))]() mutable {
        assertIsCurrent(workQueue());
        if (auto manager = m_originStorageManagers.get(origin))
            manager->quotaManager().resetQuotaUpdatedBasedOnUsageForTesting();
    });
}

#if PLATFORM(IOS_FAMILY)

void NetworkStorageManager::setBackupExclusionPeriodForTesting(Seconds period, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    ASSERT(!m_closed);

    m_queue->dispatch([this, protectedThis = Ref { *this }, period, completionHandler = WTFMove(completionHandler)]() mutable {
        m_backupExclusionPeriod = period;
        RunLoop::main().dispatch(WTFMove(completionHandler));
    });
}

#endif

void NetworkStorageManager::connectToStorageArea(IPC::Connection& connection, WebCore::StorageType type, StorageAreaMapIdentifier sourceIdentifier, std::optional<StorageNamespaceIdentifier> namespaceIdentifier, const WebCore::ClientOrigin& origin, CompletionHandler<void(StorageAreaIdentifier, HashMap<String, String>, uint64_t)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto connectionIdentifier = connection.uniqueID();
    // StorageArea may be connected due to LocalStorage prewarming, so do not write origin file eagerly.
    auto& originStorageManager = this->originStorageManager(origin, ShouldWriteOriginFile::No);
    StorageAreaIdentifier resultIdentifier;
    switch (type) {
    case WebCore::StorageType::Local:
        resultIdentifier = originStorageManager.localStorageManager(*m_storageAreaRegistry).connectToLocalStorageArea(connectionIdentifier, sourceIdentifier, origin, m_queue.copyRef());
        break;
    case WebCore::StorageType::TransientLocal:
        resultIdentifier = originStorageManager.localStorageManager(*m_storageAreaRegistry).connectToTransientLocalStorageArea(connectionIdentifier, sourceIdentifier, origin);
        break;
    case WebCore::StorageType::Session:
        if (!namespaceIdentifier)
            return completionHandler(StorageAreaIdentifier { }, HashMap<String, String> { }, StorageAreaBase::nextMessageIdentifier());
        resultIdentifier = originStorageManager.sessionStorageManager(*m_storageAreaRegistry).connectToSessionStorageArea(connectionIdentifier, sourceIdentifier, origin, *namespaceIdentifier);
    }

    if (auto storageArea = m_storageAreaRegistry->getStorageArea(resultIdentifier)) {
        completionHandler(resultIdentifier, storageArea->allItems(), StorageAreaBase::nextMessageIdentifier());
        writeOriginToFileIfNecessary(origin, storageArea);
        return;
    }

    return completionHandler(resultIdentifier, HashMap<String, String> { }, StorageAreaBase::nextMessageIdentifier());
}

void NetworkStorageManager::connectToStorageAreaSync(IPC::Connection& connection, WebCore::StorageType type, StorageAreaMapIdentifier sourceIdentifier, std::optional<StorageNamespaceIdentifier> namespaceIdentifier, const WebCore::ClientOrigin& origin, CompletionHandler<void(StorageAreaIdentifier, HashMap<String, String>, uint64_t)>&& completionHandler)
{
    connectToStorageArea(connection, type, sourceIdentifier, namespaceIdentifier, origin, WTFMove(completionHandler));
}

void NetworkStorageManager::cancelConnectToStorageArea(IPC::Connection& connection, WebCore::StorageType type, std::optional<StorageNamespaceIdentifier> namespaceIdentifier, const WebCore::ClientOrigin& origin)
{
    assertIsCurrent(workQueue());

    auto iterator = m_originStorageManagers.find(origin);
    if (iterator == m_originStorageManagers.end())
        return;

    auto connectionIdentifier = connection.uniqueID();
    switch (type) {
    case WebCore::StorageType::Local:
        if (auto localStorageManager = iterator->value->existingLocalStorageManager())
            localStorageManager->cancelConnectToLocalStorageArea(connectionIdentifier);
        break;
    case WebCore::StorageType::TransientLocal:
        if (auto localStorageManager = iterator->value->existingLocalStorageManager())
            localStorageManager->cancelConnectToTransientLocalStorageArea(connectionIdentifier);
        break;
    case WebCore::StorageType::Session:
        if (auto sessionStorageManager = iterator->value->existingSessionStorageManager()) {
            if (!namespaceIdentifier)
                return;
            sessionStorageManager->cancelConnectToSessionStorageArea(connectionIdentifier, *namespaceIdentifier);
        }
    }
}

void NetworkStorageManager::disconnectFromStorageArea(IPC::Connection& connection, StorageAreaIdentifier identifier)
{
    ASSERT(!RunLoop::isMain());

    auto storageArea = m_storageAreaRegistry->getStorageArea(identifier);
    if (!storageArea)
        return;

    if (storageArea->storageType() == StorageAreaBase::StorageType::Local)
        originStorageManager(storageArea->origin()).localStorageManager(*m_storageAreaRegistry).disconnectFromStorageArea(connection.uniqueID(), identifier);
    else
        originStorageManager(storageArea->origin()).sessionStorageManager(*m_storageAreaRegistry).disconnectFromStorageArea(connection.uniqueID(), identifier);
}

void NetworkStorageManager::cloneSessionStorageNamespace(StorageNamespaceIdentifier fromIdentifier, StorageNamespaceIdentifier toIdentifier)
{
    assertIsCurrent(workQueue());

    for (auto& manager : m_originStorageManagers.values()) {
        if (auto* sessionStorageManager = manager->existingSessionStorageManager())
            sessionStorageManager->cloneStorageArea(fromIdentifier, toIdentifier);
    }
}

void NetworkStorageManager::setItem(IPC::Connection& connection, StorageAreaIdentifier identifier, StorageAreaImplIdentifier implIdentifier, String&& key, String&& value, String&& urlString, CompletionHandler<void(bool, HashMap<String, String>&&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    bool hasError = false;
    HashMap<String, String> allItems;
    auto storageArea = m_storageAreaRegistry->getStorageArea(identifier);
    if (!storageArea)
        return completionHandler(hasError, WTFMove(allItems));

    auto result = storageArea->setItem(connection.uniqueID(), implIdentifier, WTFMove(key), WTFMove(value), WTFMove(urlString));
    hasError = !result;
    if (hasError)
        allItems = storageArea->allItems();
    completionHandler(hasError, WTFMove(allItems));

    writeOriginToFileIfNecessary(storageArea->origin(), storageArea);
}

void NetworkStorageManager::removeItem(IPC::Connection& connection, StorageAreaIdentifier identifier, StorageAreaImplIdentifier implIdentifier, String&& key, String&& urlString, CompletionHandler<void(bool, HashMap<String, String>&&)>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    bool hasError = false;
    HashMap<String, String> allItems;
    auto storageArea = m_storageAreaRegistry->getStorageArea(identifier);
    if (!storageArea)
        return completionHandler(hasError, WTFMove(allItems));
    
    auto result = storageArea->removeItem(connection.uniqueID(), implIdentifier, WTFMove(key), WTFMove(urlString));
    hasError = !result;
    if (hasError)
        allItems = storageArea->allItems();
    completionHandler(hasError, WTFMove(allItems));

    writeOriginToFileIfNecessary(storageArea->origin(), storageArea);
}

void NetworkStorageManager::clear(IPC::Connection& connection, StorageAreaIdentifier identifier, StorageAreaImplIdentifier implIdentifier, String&& urlString, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(!RunLoop::isMain());

    auto storageArea = m_storageAreaRegistry->getStorageArea(identifier);
    if (!storageArea)
        return completionHandler();

    storageArea->clear(connection.uniqueID(), implIdentifier, WTFMove(urlString));
    completionHandler();

    writeOriginToFileIfNecessary(storageArea->origin(), storageArea);
}

void NetworkStorageManager::openDatabase(IPC::Connection& connection, const WebCore::IDBRequestData& requestData)
{
    auto& connectionToClient = m_idbStorageRegistry->ensureConnectionToClient(connection.uniqueID(), requestData.requestIdentifier().connectionIdentifier());
    originStorageManager(requestData.databaseIdentifier().origin()).idbStorageManager(*m_idbStorageRegistry).openDatabase(connectionToClient, requestData);
}

void NetworkStorageManager::openDBRequestCancelled(const WebCore::IDBRequestData& requestData)
{
    originStorageManager(requestData.databaseIdentifier().origin()).idbStorageManager(*m_idbStorageRegistry).openDBRequestCancelled(requestData);
}

void NetworkStorageManager::deleteDatabase(IPC::Connection& connection, const WebCore::IDBRequestData& requestData)
{
    auto& connectionToClient = m_idbStorageRegistry->ensureConnectionToClient(connection.uniqueID(), requestData.requestIdentifier().connectionIdentifier());
    originStorageManager(requestData.databaseIdentifier().origin()).idbStorageManager(*m_idbStorageRegistry).deleteDatabase(connectionToClient, requestData);
}

void NetworkStorageManager::establishTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBTransactionInfo& transactionInfo)
{
    if (auto connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->establishTransaction(transactionInfo);
}

void NetworkStorageManager::databaseConnectionPendingClose(uint64_t databaseConnectionIdentifier)
{
    if (auto connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->connectionPendingCloseFromClient();
}

void NetworkStorageManager::databaseConnectionClosed(uint64_t databaseConnectionIdentifier)
{
    if (auto connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->connectionClosedFromClient();
}

void NetworkStorageManager::abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const std::optional<WebCore::IDBResourceIdentifier>& transactionIdentifier)
{
    if (transactionIdentifier) {
        if (auto transaction = m_idbStorageRegistry->transaction(*transactionIdentifier))
            transaction->abortWithoutCallback();
    }

    if (auto connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->connectionClosedFromClient();
}

void NetworkStorageManager::didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& requestIdentifier, const WebCore::IndexedDB::ConnectionClosedOnBehalfOfServer connectionClosed)
{
    if (auto connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->didFireVersionChangeEvent(requestIdentifier, connectionClosed);
}

void NetworkStorageManager::abortTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier)
{
    if (auto transaction = m_idbStorageRegistry->transaction(transactionIdentifier))
        transaction->abort();
}

void NetworkStorageManager::commitTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, uint64_t pendingRequestCount)
{
    if (auto transaction = m_idbStorageRegistry->transaction(transactionIdentifier))
        transaction->commit(pendingRequestCount);
}

void NetworkStorageManager::didFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& transactionIdentifier)
{
    if (auto connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->didFinishHandlingVersionChange(transactionIdentifier);
}

void NetworkStorageManager::createObjectStore(const WebCore::IDBRequestData& requestData, const WebCore::IDBObjectStoreInfo& objectStoreInfo)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier())) {
        ASSERT(transaction->isVersionChange());
        transaction->createObjectStore(requestData, objectStoreInfo);
    }
}

void NetworkStorageManager::deleteObjectStore(const WebCore::IDBRequestData& requestData, const String& objectStoreName)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier())) {
        ASSERT(transaction->isVersionChange());
        transaction->deleteObjectStore(requestData, objectStoreName);
    }
}

void NetworkStorageManager::renameObjectStore(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& newName)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier())) {
        ASSERT(transaction->isVersionChange());
        transaction->renameObjectStore(requestData, objectStoreIdentifier, newName);
    }
}

void NetworkStorageManager::clearObjectStore(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->clearObjectStore(requestData, objectStoreIdentifier);
}

void NetworkStorageManager::createIndex(const WebCore::IDBRequestData& requestData, const WebCore::IDBIndexInfo& indexInfo)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->createIndex(requestData, indexInfo);
}

void NetworkStorageManager::deleteIndex(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& indexName)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->deleteIndex(requestData, objectStoreIdentifier, indexName);
}

void NetworkStorageManager::renameIndex(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->renameIndex(requestData, objectStoreIdentifier, indexIdentifier, newName);
}

void NetworkStorageManager::putOrAdd(IPC::Connection& connection, const WebCore::IDBRequestData& requestData, const WebCore::IDBKeyData& keyData, const WebCore::IDBValue& value, WebCore::IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
    assertIsCurrent(workQueue());
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

    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->putOrAdd(requestData, keyData, value, overwriteMode);
}

void NetworkStorageManager::getRecord(const WebCore::IDBRequestData& requestData, const WebCore::IDBGetRecordData& getRecordData)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->getRecord(requestData, getRecordData);
}

void NetworkStorageManager::getAllRecords(const WebCore::IDBRequestData& requestData, const WebCore::IDBGetAllRecordsData& getAllRecordsData)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->getAllRecords(requestData, getAllRecordsData);
}

void NetworkStorageManager::getCount(const WebCore::IDBRequestData& requestData, const WebCore::IDBKeyRangeData& keyRangeData)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->getCount(requestData, keyRangeData);
}

void NetworkStorageManager::deleteRecord(const WebCore::IDBRequestData& requestData, const WebCore::IDBKeyRangeData& keyRangeData)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->deleteRecord(requestData, keyRangeData);
}

void NetworkStorageManager::openCursor(const WebCore::IDBRequestData& requestData, const WebCore::IDBCursorInfo& cursorInfo)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->openCursor(requestData, cursorInfo);
}

void NetworkStorageManager::iterateCursor(const WebCore::IDBRequestData& requestData, const WebCore::IDBIterateCursorData& cursorData)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->iterateCursor(requestData, cursorData);
}

void NetworkStorageManager::getAllDatabaseNamesAndVersions(IPC::Connection& connection, const WebCore::IDBResourceIdentifier& requestIdentifier, const WebCore::ClientOrigin& origin)
{
    auto& connectionToClient = m_idbStorageRegistry->ensureConnectionToClient(connection.uniqueID(), requestIdentifier.connectionIdentifier());
    auto result = originStorageManager(origin).idbStorageManager(*m_idbStorageRegistry).getAllDatabaseNamesAndVersions();
    connectionToClient.didGetAllDatabaseNamesAndVersions(requestIdentifier, WTFMove(result));
}

void NetworkStorageManager::cacheStorageOpenCache(const WebCore::ClientOrigin& origin, const String& cacheName, WebCore::DOMCacheEngine::CacheIdentifierCallback&& callback)
{
    originStorageManager(origin).cacheStorageManager(*m_cacheStorageRegistry, origin, m_queue.copyRef()).openCache(cacheName, WTFMove(callback));
}

void NetworkStorageManager::cacheStorageRemoveCache(WebCore::DOMCacheIdentifier cacheIdentifier, WebCore::DOMCacheEngine::RemoveCacheIdentifierCallback&& callback)
{
    auto* cache = m_cacheStorageRegistry->cache(cacheIdentifier);
    if (!cache)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    auto* cacheStorageManager = cache->manager();
    if (!cacheStorageManager)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    cacheStorageManager->removeCache(cacheIdentifier, WTFMove(callback));
}

void NetworkStorageManager::cacheStorageAllCaches(const WebCore::ClientOrigin& origin, uint64_t updateCounter, WebCore::DOMCacheEngine::CacheInfosCallback&& callback)
{
    originStorageManager(origin).cacheStorageManager(*m_cacheStorageRegistry, origin, m_queue.copyRef()).allCaches(updateCounter, WTFMove(callback));
}

void NetworkStorageManager::cacheStorageReference(IPC::Connection& connection, WebCore::DOMCacheIdentifier cacheIdentifier)
{
    auto* cache = m_cacheStorageRegistry->cache(cacheIdentifier);
    if (!cache)
        return;

    auto* cacheStorageManager = cache->manager();
    if (!cacheStorageManager)
        return;

    cacheStorageManager->reference(connection.uniqueID(), cacheIdentifier);
}

void NetworkStorageManager::cacheStorageDereference(IPC::Connection& connection, WebCore::DOMCacheIdentifier cacheIdentifier)
{
    auto* cache = m_cacheStorageRegistry->cache(cacheIdentifier);
    if (!cache)
        return;

    auto* cacheStorageManager = cache->manager();
    if (!cacheStorageManager)
        return;

    cacheStorageManager->dereference(connection.uniqueID(), cacheIdentifier);
}

void NetworkStorageManager::cacheStorageRetrieveRecords(WebCore::DOMCacheIdentifier cacheIdentifier, WebCore::RetrieveRecordsOptions&& options, WebCore::DOMCacheEngine::CrossThreadRecordsCallback&& callback)
{
    auto* cache = m_cacheStorageRegistry->cache(cacheIdentifier);
    if (!cache)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    cache->retrieveRecords(WTFMove(options), WTFMove(callback));
}

void NetworkStorageManager::cacheStorageRemoveRecords(WebCore::DOMCacheIdentifier cacheIdentifier, WebCore::ResourceRequest&& request, WebCore::CacheQueryOptions&& options, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    auto* cache = m_cacheStorageRegistry->cache(cacheIdentifier);
    if (!cache)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    cache->removeRecords(WTFMove(request), WTFMove(options), WTFMove(callback));
}

void NetworkStorageManager::cacheStoragePutRecords(WebCore::DOMCacheIdentifier cacheIdentifier, Vector<WebCore::DOMCacheEngine::CrossThreadRecord>&& records, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& callback)
{
    auto* cache = m_cacheStorageRegistry->cache(cacheIdentifier);
    if (!cache)
        return callback(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    cache->putRecords(WTFMove(records), WTFMove(callback));
}

void NetworkStorageManager::cacheStorageClearMemoryRepresentation(const WebCore::ClientOrigin& origin, CompletionHandler<void(std::optional<WebCore::DOMCacheEngine::Error>&&)>&& callback)
{
    assertIsCurrent(workQueue());

    auto iterator = m_originStorageManagers.find(origin);
    if (iterator == m_originStorageManagers.end())
        return callback(std::nullopt);

    iterator->value->closeCacheStorageManager();
    callback(std::nullopt);
}

void NetworkStorageManager::cacheStorageRepresentation(CompletionHandler<void(String&&)>&& callback)
{
    Vector<String> originStrings;
    auto targetTypes = OptionSet<WebsiteDataType> { WebsiteDataType::DOMCache };
    for (auto& origin : getAllOrigins()) {
        auto fetchedTypes = originStorageManager(origin).fetchDataTypesInList(targetTypes, false);
        if (!fetchedTypes.isEmpty()) {
            StringBuilder originBuilder;
            originBuilder.append("\n{ \"origin\" : { \"topOrigin\" : \"", origin.topOrigin.toString(), "\", \"clientOrigin\": \"", origin.clientOrigin.toString(), "\" }, \"caches\" : ");
            originBuilder.append(originStorageManager(origin).cacheStorageManager(*m_cacheStorageRegistry, origin, m_queue.copyRef()).representationString());
            originBuilder.append('}');
            originStrings.append(originBuilder.toString());
        }
        removeOriginStorageManagerIfPossible(origin);
    }

    std::sort(originStrings.begin(), originStrings.end(), [](auto& a, auto& b) {
        return codePointCompareLessThan(a, b);
    });
    StringBuilder builder;
    builder.append("{ \"path\": \"", m_customCacheStoragePath, "\", \"origins\": [");
    const char* divider = "";
    for (auto& origin : originStrings) {
        builder.append(divider, origin);
        divider = ",";
    }
    builder.append("]}");
    callback(builder.toString());
}

#if ENABLE(SERVICE_WORKER)

void NetworkStorageManager::dispatchTaskToBackgroundFetchManager(const WebCore::ClientOrigin& origin, Function<void(BackgroundFetchStoreManager*)>&& callback)
{
    ASSERT(RunLoop::isMain());

    if (m_closed) {
        callback(nullptr);
        return;
    }
    m_queue->dispatch([this, protectedThis = Ref { *this }, queue = Ref { m_queue }, origin = crossThreadCopy(origin), callback = WTFMove(callback)]() mutable {
        auto& originStorageManager = this->originStorageManager(origin);
        callback(&originStorageManager.backgroundFetchManager(WTFMove(queue)));
    });
}

void NetworkStorageManager::notifyBackgroundFetchChange(const String& identifier, BackgroundFetchChange change)
{
    IPC::Connection::send(m_parentConnection, Messages::NetworkProcessProxy::NotifyBackgroundFetchChange(m_sessionID, identifier, change), 0);
}

void NetworkStorageManager::closeServiceWorkerRegistrationFiles(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (m_closed)
        return completionHandler();

    m_queue->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        assertIsCurrent(workQueue());

        if (m_sharedServiceWorkerStorageManager)
            m_sharedServiceWorkerStorageManager->closeFiles();
        else {
            for (auto& manager : m_originStorageManagers.values())
                manager->serviceWorkerStorageManager().closeFiles();
        }

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::clearServiceWorkerRegistrations(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (m_closed)
        return completionHandler();

    m_queue->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        assertIsCurrent(workQueue());

        if (m_sharedServiceWorkerStorageManager)
            m_sharedServiceWorkerStorageManager->clearAllRegistrations();
        else {
            for (auto& origin : getAllOrigins()) {
                originStorageManager(origin).serviceWorkerStorageManager().clearAllRegistrations();
                removeOriginStorageManagerIfPossible(origin);
            }
        }

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler();
        });
    });
}

void NetworkStorageManager::importServiceWorkerRegistrations(CompletionHandler<void(std::optional<Vector<WebCore::ServiceWorkerContextData>>)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (m_closed)
        return completionHandler(std::nullopt);

    m_queue->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        assertIsCurrent(workQueue());

        std::optional<Vector<WebCore::ServiceWorkerContextData>> result;
        if (m_sharedServiceWorkerStorageManager)
            result = m_sharedServiceWorkerStorageManager->importRegistrations();
        else {
            bool hasResult = false;
            Vector<WebCore::ServiceWorkerContextData> registrations;
            for (auto& origin : getAllOrigins()) {
                if (auto originRegistrations = originStorageManager(origin).serviceWorkerStorageManager().importRegistrations()) {
                    hasResult = true;
                    registrations.appendVector(WTFMove(*originRegistrations));
                }
                removeOriginStorageManagerIfPossible(origin);
            }
            if (hasResult)
                result = registrations;
        }

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), result = crossThreadCopy(WTFMove(result)), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(result));
        });
    });
}

void NetworkStorageManager::updateServiceWorkerRegistrations(Vector<WebCore::ServiceWorkerContextData>&& registrationsToUpdate, Vector<WebCore::ServiceWorkerRegistrationKey>&& registrationsToDelete, CompletionHandler<void(std::optional<Vector<WebCore::ServiceWorkerScripts>>)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (m_closed)
        return completionHandler(std::nullopt);

    m_queue->dispatch([this, protectedThis = Ref { *this }, registrationsToUpdate = crossThreadCopy(WTFMove(registrationsToUpdate)), registrationsToDelete = crossThreadCopy(WTFMove(registrationsToDelete)), completionHandler = WTFMove(completionHandler)]() mutable {
        assertIsCurrent(workQueue());

        std::optional<Vector<WebCore::ServiceWorkerScripts>> result;
        if (m_sharedServiceWorkerStorageManager)
            result = m_sharedServiceWorkerStorageManager->updateRegistrations(WTFMove(registrationsToUpdate), WTFMove(registrationsToDelete));
        else
            result = updateServiceWorkerRegistrationsByOrigin(WTFMove(registrationsToUpdate), WTFMove(registrationsToDelete));

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), result = crossThreadCopy(WTFMove(result)), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(result));
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

    updateServiceWorkerRegistrationsByOrigin(WTFMove(*result), { });
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
        registrations.append(WTFMove(registration));
    }

    HashMap<WebCore::ClientOrigin, Vector<WebCore::ServiceWorkerRegistrationKey>> originRegistrationsToDelete;
    for (auto&& key : registrationsToDelete) {
        auto origin = key.clientOrigin();
        auto& keys = originRegistrations.ensure(origin, []() {
            return std::pair<Vector<WebCore::ServiceWorkerContextData>, Vector<WebCore::ServiceWorkerRegistrationKey>> { };
        }).iterator->value.second;
        keys.append(WTFMove(key));
    }

    Vector<WebCore::ServiceWorkerScripts> savedScripts;
    for (auto& [origin, registrations] : originRegistrations) {
        auto result = originStorageManager(origin).serviceWorkerStorageManager().updateRegistrations(WTFMove(registrations.first), WTFMove(registrations.second));
        if (result)
            savedScripts.appendVector(WTFMove(*result));
    }

    return savedScripts;
}

bool NetworkStorageManager::shouldManageServiceWorkerRegistrationsByOrigin()
{
    ASSERT(!RunLoop::isMain());

    return m_unifiedOriginStorageLevel >= UnifiedOriginStorageLevel::Standard;
}

#endif // ENABLE(SERVICE_WORKER)

} // namespace WebKit
