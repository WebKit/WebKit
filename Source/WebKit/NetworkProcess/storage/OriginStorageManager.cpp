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
#include "OriginStorageManager.h"

#include "CacheStorageCache.h"
#include "CacheStorageManager.h"
#include "CacheStorageRegistry.h"
#include "FileSystemStorageHandleRegistry.h"
#include "FileSystemStorageManager.h"
#include "IDBStorageManager.h"
#include "IDBStorageRegistry.h"
#include "LocalStorageManager.h"
#include "Logging.h"
#include "MemoryStorageArea.h"
#include "SessionStorageManager.h"
#include "StorageAreaRegistry.h"
#include "UnifiedOriginStorageLevel.h"
#include "WebsiteDataType.h"
#include <WebCore/SQLiteFileSystem.h>
#include <WebCore/StorageEstimate.h>
#include <wtf/FileSystem.h>

namespace WebKit {

static constexpr auto originFileName = "origin"_s;
enum class OriginStorageManager::StorageBucketMode : bool { BestEffort, Persistent };

class OriginStorageManager::StorageBucket {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class StorageType : uint8_t {
        FileSystem,
        LocalStorage,
        SessionStorage,
        IndexedDB,
        CacheStorage
    };
    std::optional<StorageType> toStorageType(WebsiteDataType) const;
    String toStorageIdentifier(StorageType) const;
    StorageBucket(const String& rootPath, const String& identifier, const String& localStoragePath, const String& idbStoragePath, const String& cacheStoragePath, UnifiedOriginStorageLevel);
    StorageBucketMode mode() const { return m_mode; }
    void setMode(StorageBucketMode mode) { m_mode = mode; }
    void connectionClosed(IPC::Connection::UniqueID);
    String typeStoragePath(StorageType) const;
    FileSystemStorageManager& fileSystemStorageManager(FileSystemStorageHandleRegistry&, FileSystemStorageManager::QuotaCheckFunction&&);
    FileSystemStorageManager* existingFileSystemStorageManager() { return m_fileSystemStorageManager.get(); }
    LocalStorageManager& localStorageManager(StorageAreaRegistry&);
    LocalStorageManager* existingLocalStorageManager() { return m_localStorageManager.get(); }
    SessionStorageManager& sessionStorageManager(StorageAreaRegistry&);
    SessionStorageManager* existingSessionStorageManager() { return m_sessionStorageManager.get(); }
    IDBStorageManager& idbStorageManager(IDBStorageRegistry&, IDBStorageManager::QuotaCheckFunction&&);
    IDBStorageManager* existingIDBStorageManager() { return m_idbStorageManager.get(); }
    CacheStorageManager& cacheStorageManager(CacheStorageRegistry&, const WebCore::ClientOrigin&, CacheStorageManager::QuotaCheckFunction&&, Ref<WorkQueue>&&);
    CacheStorageManager* existingCacheStorageManager() { return m_cacheStorageManager.get(); }
    void closeCacheStorageManager();
    bool isActive() const;
    bool isEmpty();
    DataTypeSizeMap fetchDataTypesInList(OptionSet<WebsiteDataType>, bool shouldComputeSize);
    void deleteData(OptionSet<WebsiteDataType>, WallTime);
    void moveData(OptionSet<WebsiteDataType>, const String& localStoragePath, const String& idbStoragePath);
    void deleteEmptyDirectory();
    String resolvedLocalStoragePath();
    String resolvedIDBStoragePath();
    String resolvedCacheStoragePath();
    String resolvedPath(WebsiteDataType);

private:
    OptionSet<WebsiteDataType> fetchDataTypesInListFromMemory(OptionSet<WebsiteDataType>);
    DataTypeSizeMap fetchDataTypesInListFromDisk(OptionSet<WebsiteDataType>, bool shouldComputeSize);
    void deleteFileSystemStorageData(WallTime);
    void deleteLocalStorageData(WallTime);
    void deleteSessionStorageData();
    void deleteIDBStorageData(WallTime);
    void deleteCacheStorageData(WallTime);

    String m_rootPath;
    String m_identifier;
    StorageBucketMode m_mode { StorageBucketMode::BestEffort };
    std::unique_ptr<FileSystemStorageManager> m_fileSystemStorageManager;
    std::unique_ptr<LocalStorageManager> m_localStorageManager;
    String m_customLocalStoragePath;
    String m_resolvedLocalStoragePath;
    std::unique_ptr<SessionStorageManager> m_sessionStorageManager;
    std::unique_ptr<IDBStorageManager> m_idbStorageManager;
    String m_customIDBStoragePath;
    String m_resolvedIDBStoragePath;
    std::unique_ptr<CacheStorageManager> m_cacheStorageManager;
    String m_customCacheStoragePath;
    String m_resolvedCacheStoragePath;
    UnifiedOriginStorageLevel m_level;
};

OriginStorageManager::StorageBucket::StorageBucket(const String& rootPath, const String& identifier, const String& localStoragePath, const String& idbStoragePath, const String& cacheStoragePath, UnifiedOriginStorageLevel level)
    : m_rootPath(rootPath)
    , m_identifier(identifier)
    , m_customLocalStoragePath(localStoragePath)
    , m_customIDBStoragePath(idbStoragePath)
    , m_customCacheStoragePath(cacheStoragePath)
    , m_level(level)
{
}

void OriginStorageManager::StorageBucket::connectionClosed(IPC::Connection::UniqueID connection)
{
    if (m_fileSystemStorageManager)
        m_fileSystemStorageManager->connectionClosed(connection);

    if (m_localStorageManager)
        m_localStorageManager->connectionClosed(connection);

    if (m_sessionStorageManager)
        m_sessionStorageManager->connectionClosed(connection);

    if (m_cacheStorageManager)
        m_cacheStorageManager->connectionClosed(connection);
}

std::optional<OriginStorageManager::StorageBucket::StorageType> OriginStorageManager::StorageBucket::toStorageType(WebsiteDataType websiteDataType) const
{
    switch (websiteDataType) {
    case WebsiteDataType::FileSystem:
        return StorageType::FileSystem;
    case WebsiteDataType::LocalStorage:
        return StorageType::LocalStorage;
    case WebsiteDataType::SessionStorage:
        return StorageType::SessionStorage;
    case WebsiteDataType::IndexedDBDatabases:
        return StorageType::IndexedDB;
    case WebsiteDataType::DOMCache:
        return StorageType::CacheStorage;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

String OriginStorageManager::StorageBucket::toStorageIdentifier(StorageType type) const
{
    switch (type) {
    case StorageType::FileSystem:
        return "FileSystem"_s;
    case StorageType::LocalStorage:
        return "LocalStorage"_s;
    case StorageType::SessionStorage:
        return "SessionStorage"_s;
    case StorageType::IndexedDB:
        return "IndexedDB"_s;
    case StorageType::CacheStorage:
        return "CacheStorage"_s;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return emptyString();
}

String OriginStorageManager::StorageBucket::typeStoragePath(StorageType type) const
{
    auto storageIdentifier = toStorageIdentifier(type);
    if (m_rootPath.isEmpty() || storageIdentifier.isEmpty())
        return emptyString();

    return FileSystem::pathByAppendingComponent(m_rootPath, storageIdentifier);
}

FileSystemStorageManager& OriginStorageManager::StorageBucket::fileSystemStorageManager(FileSystemStorageHandleRegistry& registry, FileSystemStorageManager::QuotaCheckFunction&& quotaCheckFunction)
{
    if (!m_fileSystemStorageManager)
        m_fileSystemStorageManager = makeUnique<FileSystemStorageManager>(typeStoragePath(StorageType::FileSystem), registry, WTFMove(quotaCheckFunction));

    return *m_fileSystemStorageManager;
}

LocalStorageManager& OriginStorageManager::StorageBucket::localStorageManager(StorageAreaRegistry& registry)
{
    if (!m_localStorageManager)
        m_localStorageManager = makeUnique<LocalStorageManager>(resolvedLocalStoragePath(), registry);

    return *m_localStorageManager;
}

SessionStorageManager& OriginStorageManager::StorageBucket::sessionStorageManager(StorageAreaRegistry& registry)
{
    if (!m_sessionStorageManager)
        m_sessionStorageManager = makeUnique<SessionStorageManager>(registry);

    return *m_sessionStorageManager;
}

IDBStorageManager& OriginStorageManager::StorageBucket::idbStorageManager(IDBStorageRegistry& registry, IDBStorageManager::QuotaCheckFunction&& quotaCheckFunction)
{
    if (!m_idbStorageManager)
        m_idbStorageManager = makeUnique<IDBStorageManager>(resolvedIDBStoragePath(), registry, WTFMove(quotaCheckFunction));

    return *m_idbStorageManager;
}

CacheStorageManager& OriginStorageManager::StorageBucket::cacheStorageManager(CacheStorageRegistry& registry, const WebCore::ClientOrigin& origin, CacheStorageManager::QuotaCheckFunction&& quotaCheckFunction, Ref<WorkQueue>&& queue)
{
    if (!m_cacheStorageManager) {
        std::optional<WebCore::ClientOrigin> optionalOrigin;
        if (m_level < UnifiedOriginStorageLevel::Standard)
            optionalOrigin = origin;
        m_cacheStorageManager = makeUnique<CacheStorageManager>(resolvedCacheStoragePath(), registry, optionalOrigin, WTFMove(quotaCheckFunction), WTFMove(queue));
    }

    return *m_cacheStorageManager;
}

bool OriginStorageManager::StorageBucket::isActive() const
{
    // We cannot remove the bucket if it has in-memory data, otherwise session
    // data may be lost.
    return (m_fileSystemStorageManager && m_fileSystemStorageManager->isActive())
    || (m_localStorageManager && (m_localStorageManager->hasDataInMemory() || m_localStorageManager->isActive()))
    || (m_sessionStorageManager && (m_sessionStorageManager->hasDataInMemory() || m_sessionStorageManager->isActive()))
    || (m_idbStorageManager && (m_idbStorageManager->hasDataInMemory() || m_idbStorageManager->isActive()))
    || (m_cacheStorageManager && (m_cacheStorageManager->hasDataInMemory() || m_cacheStorageManager->isActive()));
}

bool OriginStorageManager::StorageBucket::isEmpty()
{
    ASSERT(!RunLoop::isMain());

    auto files = FileSystem::listDirectory(m_rootPath);
    auto hasValidFile = WTF::anyOf(files, [&](auto file) {
        bool isInvalidFile = (file == originFileName);
#if PLATFORM(COCOA)
        isInvalidFile |= (file == ".DS_Store"_s);
#endif
        return !isInvalidFile;
    });
    if (hasValidFile)
        return false;

    auto idbStorageFiles = FileSystem::listDirectory(resolvedIDBStoragePath());
    return !FileSystem::fileExists(resolvedLocalStoragePath()) && idbStorageFiles.isEmpty() && !CacheStorageManager::hasCacheList(resolvedCacheStoragePath());
}

OriginStorageManager::DataTypeSizeMap OriginStorageManager::StorageBucket::fetchDataTypesInList(OptionSet<WebsiteDataType> types, bool shouldComputeSize)
{
    auto result = fetchDataTypesInListFromDisk(types, shouldComputeSize);
    auto memoryResult = fetchDataTypesInListFromMemory(types);
    for (auto type : memoryResult)
        result.add(type, 0);

    return result;
}

OptionSet<WebsiteDataType> OriginStorageManager::StorageBucket::fetchDataTypesInListFromMemory(OptionSet<WebsiteDataType> types)
{
    OptionSet<WebsiteDataType> result;
    if (types.contains(WebsiteDataType::LocalStorage)) {
        if (m_localStorageManager && m_localStorageManager->hasDataInMemory())
            result.add(WebsiteDataType::LocalStorage);
    }

    if (types.contains(WebsiteDataType::SessionStorage)) {
        if (m_sessionStorageManager && m_sessionStorageManager->hasDataInMemory())
            result.add(WebsiteDataType::SessionStorage);
    }

    if (types.contains(WebsiteDataType::IndexedDBDatabases)) {
        if (m_idbStorageManager && m_idbStorageManager->hasDataInMemory())
            result.add(WebsiteDataType::IndexedDBDatabases);
    }

    if (types.contains(WebsiteDataType::DOMCache)) {
        if (m_cacheStorageManager && m_cacheStorageManager->hasDataInMemory())
            result.add(WebsiteDataType::DOMCache);
    }

    return result;
}

OriginStorageManager::DataTypeSizeMap OriginStorageManager::StorageBucket::fetchDataTypesInListFromDisk(OptionSet<WebsiteDataType> types, bool shouldComputeSize)
{
    DataTypeSizeMap result;
    if (types.contains(WebsiteDataType::FileSystem)) {
        auto fileSystemStoragePath = typeStoragePath(StorageType::FileSystem);
        if (auto files = FileSystem::listDirectory(fileSystemStoragePath); !files.isEmpty()) {
            uint64_t size = 0;
            if (shouldComputeSize)
                size = valueOrDefault(FileSystem::directorySize(fileSystemStoragePath));
            result.add(WebsiteDataType::FileSystem, size);
        }
    }

    if (types.contains(WebsiteDataType::LocalStorage)) {
        auto localStoragePath = resolvedLocalStoragePath();
        if (FileSystem::fileExists(localStoragePath)) {
            uint64_t size = 0;
            if (shouldComputeSize)
                size = WebCore::SQLiteFileSystem::databaseFileSize(localStoragePath);
            result.add(WebsiteDataType::LocalStorage, size);
        }
    }

    if (types.contains(WebsiteDataType::IndexedDBDatabases)) {
        auto idbStoragePath = resolvedIDBStoragePath();
        if (auto databases = FileSystem::listDirectory(idbStoragePath); !databases.isEmpty()) {
            uint64_t size = 0;
            if (shouldComputeSize)
                size = valueOrDefault(FileSystem::directorySize(idbStoragePath));
            result.add(WebsiteDataType::IndexedDBDatabases, size);
        }
    }

    if (types.contains(WebsiteDataType::DOMCache)) {
        if (CacheStorageManager::hasCacheList(resolvedCacheStoragePath())) {
            uint64_t size = 0;
            if (shouldComputeSize)
                size = CacheStorageManager::cacheStorageSize(resolvedCacheStoragePath());
            result.add(WebsiteDataType::DOMCache, size);
        }
    }

    return result;
}

void OriginStorageManager::StorageBucket::deleteData(OptionSet<WebsiteDataType> types, WallTime modifiedSinceTime)
{
    if (types.contains(WebsiteDataType::FileSystem))
        deleteFileSystemStorageData(modifiedSinceTime);

    if (types.contains(WebsiteDataType::LocalStorage))
        deleteLocalStorageData(modifiedSinceTime);

    if (types.contains(WebsiteDataType::SessionStorage) && modifiedSinceTime < WallTime::now())
        deleteSessionStorageData();

    if (types.contains(WebsiteDataType::IndexedDBDatabases))
        deleteIDBStorageData(modifiedSinceTime);

    if (types.contains(WebsiteDataType::DOMCache))
        deleteCacheStorageData(modifiedSinceTime);
}

void OriginStorageManager::StorageBucket::deleteFileSystemStorageData(WallTime modifiedSinceTime)
{
    m_fileSystemStorageManager = nullptr;

    auto fileSystemStoragePath = typeStoragePath(StorageType::FileSystem);
    FileSystem::deleteAllFilesModifiedSince(fileSystemStoragePath, modifiedSinceTime);
}

void OriginStorageManager::StorageBucket::deleteLocalStorageData(WallTime time)
{
    auto currentLocalStoragePath = resolvedLocalStoragePath();
    if (FileSystem::fileModificationTime(currentLocalStoragePath) >= time) {
        if (m_localStorageManager)
            m_localStorageManager->clearDataOnDisk();
        WebCore::SQLiteFileSystem::deleteDatabaseFile(currentLocalStoragePath);
    }

    if (!m_localStorageManager)
        return;

    m_localStorageManager->clearDataInMemory();
    if (!m_localStorageManager->isActive())
        m_localStorageManager = nullptr;
}

void OriginStorageManager::StorageBucket::deleteSessionStorageData()
{
    if (!m_sessionStorageManager)
        return;

    m_sessionStorageManager->clearData();
    if (!m_sessionStorageManager->isActive())
        m_sessionStorageManager = nullptr;
}

void OriginStorageManager::StorageBucket::deleteIDBStorageData(WallTime time)
{
    if (m_idbStorageManager)
        m_idbStorageManager->closeDatabasesForDeletion();

    FileSystem::deleteAllFilesModifiedSince(resolvedIDBStoragePath(), time);
}

void OriginStorageManager::StorageBucket::deleteCacheStorageData(WallTime time)
{
    if (m_cacheStorageManager)
        m_cacheStorageManager = nullptr;

    FileSystem::deleteAllFilesModifiedSince(resolvedCacheStoragePath(), time);
}

void OriginStorageManager::StorageBucket::moveData(OptionSet<WebsiteDataType> types, const String& localStoragePath, const String& idbStoragePath)
{
    // This is only supported for IndexedDB and LocalStorage now.
    if (types.contains(WebsiteDataType::LocalStorage) && !localStoragePath.isEmpty()) {
        if (m_localStorageManager)
            m_localStorageManager->close();

        auto currentLocalStoragePath = resolvedLocalStoragePath();
        if (!currentLocalStoragePath.isEmpty()) {
            FileSystem::makeAllDirectories(FileSystem::parentPath(localStoragePath));
            WebCore::SQLiteFileSystem::moveDatabaseFile(currentLocalStoragePath, localStoragePath);
        }
    }

    if (types.contains(WebsiteDataType::IndexedDBDatabases) && !idbStoragePath.isEmpty()) {
        if (m_idbStorageManager)
            m_idbStorageManager->closeDatabasesForDeletion();
    
        auto currentIDBStoragePath = resolvedIDBStoragePath();
        if (!currentIDBStoragePath.isEmpty()) {
            FileSystem::makeAllDirectories(FileSystem::parentPath(idbStoragePath));
            FileSystem::moveFile(currentIDBStoragePath, idbStoragePath);
        }
    }
}

void OriginStorageManager::StorageBucket::deleteEmptyDirectory()
{
    switch (m_level) {
    case UnifiedOriginStorageLevel::None:
        FileSystem::deleteEmptyDirectory(typeStoragePath(StorageType::FileSystem));
        FileSystem::deleteEmptyDirectory(m_customLocalStoragePath);
        FileSystem::deleteEmptyDirectory(m_customIDBStoragePath);
        FileSystem::deleteEmptyDirectory(m_customCacheStoragePath);
        break;
    case UnifiedOriginStorageLevel::Basic:
        FileSystem::deleteEmptyDirectory(typeStoragePath(StorageType::FileSystem));
        FileSystem::deleteEmptyDirectory(typeStoragePath(StorageType::LocalStorage));
        FileSystem::deleteEmptyDirectory(typeStoragePath(StorageType::IndexedDB));
        FileSystem::deleteEmptyDirectory(m_customCacheStoragePath);
        break;
    case UnifiedOriginStorageLevel::Standard:
        FileSystem::deleteEmptyDirectory(typeStoragePath(StorageType::FileSystem));
        FileSystem::deleteEmptyDirectory(typeStoragePath(StorageType::LocalStorage));
        FileSystem::deleteEmptyDirectory(typeStoragePath(StorageType::IndexedDB));
        FileSystem::deleteEmptyDirectory(typeStoragePath(StorageType::CacheStorage));
    }
}

String OriginStorageManager::StorageBucket::resolvedLocalStoragePath()
{
    if (!m_resolvedLocalStoragePath.isNull())
        return m_resolvedLocalStoragePath;

    if (m_level == UnifiedOriginStorageLevel::None) {
        ASSERT(m_customLocalStoragePath.isEmpty() == m_rootPath.isEmpty());
        m_resolvedLocalStoragePath = m_customLocalStoragePath;
    } else if (!m_rootPath.isEmpty()) {
        auto localStorageDirectory = typeStoragePath(StorageType::LocalStorage);
        auto localStoragePath = LocalStorageManager::localStorageFilePath(localStorageDirectory);
        if (!m_customLocalStoragePath.isEmpty() && !FileSystem::fileExists(localStoragePath) && FileSystem::fileExists(m_customLocalStoragePath)) {
            RELEASE_LOG(Storage, "%p - StorageBucket::resolvedLocalStoragePath New path '%" PUBLIC_LOG_STRING "'", this, localStoragePath.utf8().data());
            FileSystem::makeAllDirectories(localStorageDirectory);
            auto moved = WebCore::SQLiteFileSystem::moveDatabaseFile(m_customLocalStoragePath, localStoragePath);
            if (!moved && !FileSystem::fileExists(localStoragePath))
                RELEASE_LOG_ERROR(Storage, "%p - StorageBucket::resolvedLocalStoragePath Fails to migrate file to new path", this);
        }

        m_resolvedLocalStoragePath = localStoragePath;
    } else
        m_resolvedLocalStoragePath = emptyString();

    return m_resolvedLocalStoragePath;
}

String OriginStorageManager::StorageBucket::resolvedIDBStoragePath()
{
    ASSERT(!RunLoop::isMain());

    if (!m_resolvedIDBStoragePath.isNull())
        return m_resolvedIDBStoragePath;

    if (m_level == UnifiedOriginStorageLevel::None) {
        ASSERT(m_customIDBStoragePath.isEmpty() == m_rootPath.isEmpty());
        m_resolvedIDBStoragePath = m_customIDBStoragePath;
    } else {
        auto idbStoragePath = typeStoragePath(StorageType::IndexedDB);
        RELEASE_LOG(Storage, "%p - StorageBucket::resolvedIDBStoragePath New path '%" PUBLIC_LOG_STRING "'", this, idbStoragePath.utf8().data());
        auto moved = IDBStorageManager::migrateOriginData(m_customIDBStoragePath, idbStoragePath);
        if (!moved && FileSystem::fileExists(idbStoragePath)) {
            auto fileNames = FileSystem::listDirectory(m_customIDBStoragePath);
            auto newFileNames = FileSystem::listDirectory(idbStoragePath);
            RELEASE_LOG_ERROR(Storage, "%p - StorageBucket::resolvedLocalStoragePath Fails to migrate all databases to new path: %zu migrated, %zu left", this, newFileNames.size(), fileNames.size());
        }
        m_resolvedIDBStoragePath = idbStoragePath;
    }
    ASSERT(!m_resolvedIDBStoragePath.isNull());
    return m_resolvedIDBStoragePath;
}

String OriginStorageManager::StorageBucket::resolvedCacheStoragePath()
{
    if (!m_resolvedCacheStoragePath.isNull())
        return m_resolvedCacheStoragePath;

    switch (m_level) {
    case UnifiedOriginStorageLevel::None:
    case UnifiedOriginStorageLevel::Basic:
        ASSERT(m_customCacheStoragePath.isEmpty() == m_rootPath.isEmpty());
        m_resolvedCacheStoragePath = m_customCacheStoragePath;
        break;
    case UnifiedOriginStorageLevel::Standard:
        auto cacheStorageDirectory = typeStoragePath(StorageType::CacheStorage);
        RELEASE_LOG(Storage, "%p - StorageBucket::resolvedCacheStoragePath New path '%" PUBLIC_LOG_STRING "'", this, cacheStorageDirectory.utf8().data());
        if (cacheStorageDirectory.isEmpty() || m_customCacheStoragePath.isEmpty() || !FileSystem::fileExists(m_customCacheStoragePath))
            m_resolvedCacheStoragePath = emptyString();
        else {
            if (!FileSystem::fileExists(cacheStorageDirectory))
                FileSystem::moveFile(m_customCacheStoragePath, cacheStorageDirectory);
            m_resolvedCacheStoragePath = cacheStorageDirectory;
        }
    }

    return m_resolvedCacheStoragePath;
}

String OriginStorageManager::StorageBucket::resolvedPath(WebsiteDataType webisteDataType)
{
    auto type = toStorageType(webisteDataType);
    if (!type)
        return { };

    switch (*type) {
    case StorageType::LocalStorage:
        return resolvedLocalStoragePath();
    case StorageType::IndexedDB:
        return resolvedIDBStoragePath();
    case StorageType::CacheStorage:
        return resolvedCacheStoragePath();
    case StorageType::SessionStorage:
    case StorageType::FileSystem:
        return typeStoragePath(*type);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void OriginStorageManager::StorageBucket::closeCacheStorageManager()
{
    m_cacheStorageManager = nullptr;
}

String OriginStorageManager::originFileIdentifier()
{
    return originFileName;
}

Ref<QuotaManager> OriginStorageManager::createQuotaManager()
{
    auto idbStoragePath = resolvedPath(WebsiteDataType::IndexedDBDatabases);
    auto cacheStoragePath = resolvedPath(WebsiteDataType::DOMCache);
    auto fileSystemStoragePath = resolvedPath(WebsiteDataType::FileSystem);
    QuotaManager::GetUsageFunction getUsageFunction = [this, weakThis = WeakPtr { *this }, idbStoragePath, cacheStoragePath, fileSystemStoragePath]() {
        uint64_t fileSystemStorageSize = valueOrDefault(FileSystem::directorySize(fileSystemStoragePath));
        if (weakThis) {
            if (auto* fileSystemStorageManager = existingFileSystemStorageManager()) {
                CheckedUint64 totalFileSystemStorageSize = fileSystemStorageSize;
                totalFileSystemStorageSize += fileSystemStorageManager->allocatedUnusedCapacity();
                if (!totalFileSystemStorageSize.hasOverflowed())
                    fileSystemStorageSize = totalFileSystemStorageSize;
            }
        }
        return IDBStorageManager::idbStorageSize(idbStoragePath) + CacheStorageManager::cacheStorageSize(cacheStoragePath) + fileSystemStorageSize;
    };
    return QuotaManager::create(m_quota, WTFMove(getUsageFunction), std::exchange(m_increaseQuotaFunction, { }));
}

OriginStorageManager::OriginStorageManager(uint64_t quota, QuotaManager::IncreaseQuotaFunction&& increaseQuotaFunction, const WebCore::ClientOrigin& origin, String&& path, String&& customLocalStoragePath, String&& customIDBStoragePath, String&& customCacheStoragePath, UnifiedOriginStorageLevel level)
    : m_origin(origin)
    , m_path(WTFMove(path))
    , m_customLocalStoragePath(WTFMove(customLocalStoragePath))
    , m_customIDBStoragePath(WTFMove(customIDBStoragePath))
    , m_customCacheStoragePath(WTFMove(customCacheStoragePath))
    , m_quota(quota)
    , m_increaseQuotaFunction(WTFMove(increaseQuotaFunction))
    , m_level(level)
{
    ASSERT(!RunLoop::isMain());
    
    m_fileSystemStorageHandleRegistry = makeUnique<FileSystemStorageHandleRegistry>();
    m_storageAreaRegistry = makeUnique<StorageAreaRegistry>();
    m_idbStorageRegistry = makeUnique<IDBStorageRegistry>();
    m_cacheStorageRegistry = makeUnique<CacheStorageRegistry>();
}

OriginStorageManager::~OriginStorageManager() = default;

void OriginStorageManager::connectionClosed(IPC::Connection::UniqueID connection)
{
    m_idbStorageRegistry->removeConnectionToClient(connection);
    if (m_defaultBucket)
        m_defaultBucket->connectionClosed(connection);
}

OriginStorageManager::StorageBucket& OriginStorageManager::defaultBucket()
{
    if (!m_defaultBucket)
        m_defaultBucket = makeUnique<StorageBucket>(m_path, "default"_s, m_customLocalStoragePath, m_customIDBStoragePath, m_customCacheStoragePath, m_level);

    return *m_defaultBucket;
}

QuotaManager& OriginStorageManager::quotaManager()
{
    if (!m_quotaManager)
        m_quotaManager = createQuotaManager();

    return *m_quotaManager;
}

FileSystemStorageManager& OriginStorageManager::fileSystemStorageManager()
{
    return defaultBucket().fileSystemStorageManager(*m_fileSystemStorageHandleRegistry, [quotaManager = ThreadSafeWeakPtr { this->quotaManager() }](uint64_t spaceRequested, CompletionHandler<void(bool)>&& completionHandler) mutable {
        auto strongReference = quotaManager.get();
        if (!strongReference)
            return completionHandler(false);

        strongReference->requestSpace(spaceRequested, [completionHandler = WTFMove(completionHandler)](auto decision) mutable {
            completionHandler(decision == QuotaManager::Decision::Grant);
        });
    });
}

FileSystemStorageManager* OriginStorageManager::existingFileSystemStorageManager()
{
    return defaultBucket().existingFileSystemStorageManager();
}

LocalStorageManager& OriginStorageManager::localStorageManager()
{
    return defaultBucket().localStorageManager(*m_storageAreaRegistry);
}

LocalStorageManager* OriginStorageManager::existingLocalStorageManager()
{
    return defaultBucket().existingLocalStorageManager();
}

SessionStorageManager& OriginStorageManager::sessionStorageManager()
{
    return defaultBucket().sessionStorageManager(*m_storageAreaRegistry);
}

SessionStorageManager* OriginStorageManager::existingSessionStorageManager()
{
    return defaultBucket().existingSessionStorageManager();
}

IDBStorageManager& OriginStorageManager::idbStorageManager()
{
    return defaultBucket().idbStorageManager(*m_idbStorageRegistry, [quotaManager = ThreadSafeWeakPtr { this->quotaManager() }](uint64_t spaceRequested, CompletionHandler<void(bool)>&& completionHandler) mutable {
        auto strongReference = quotaManager.get();
        if (!strongReference)
            return completionHandler(false);

        strongReference->requestSpace(spaceRequested, [completionHandler = WTFMove(completionHandler)](auto decision) mutable {
            completionHandler(decision == QuotaManager::Decision::Grant);
        });
    });
}

IDBStorageManager* OriginStorageManager::existingIDBStorageManager()
{
    return defaultBucket().existingIDBStorageManager();
}

CacheStorageManager* OriginStorageManager::existingCacheStorageManager()
{
    return defaultBucket().existingCacheStorageManager();
}

CacheStorageManager& OriginStorageManager::cacheStorageManager(Ref<WorkQueue>&& queue)
{
    return defaultBucket().cacheStorageManager(*m_cacheStorageRegistry, m_origin, [quotaManager = ThreadSafeWeakPtr { this->quotaManager() }](uint64_t spaceRequested, CompletionHandler<void(bool)>&& completionHandler) mutable {
        if (!quotaManager.get())
            return completionHandler(false);

        quotaManager.get()->requestSpace(spaceRequested, [completionHandler = WTFMove(completionHandler)](auto decision) mutable {
            completionHandler(decision == QuotaManager::Decision::Grant);
        });
    }, WTFMove(queue));
}

String OriginStorageManager::resolvedPath(WebsiteDataType type)
{
    return defaultBucket().resolvedPath(type);
}

bool OriginStorageManager::isActive()
{
    return defaultBucket().isActive();
}

void OriginStorageManager::setPersisted(bool value)
{
    ASSERT(!RunLoop::isMain());

    m_persisted = value;
    defaultBucket().setMode(value ? StorageBucketMode::Persistent : StorageBucketMode::BestEffort);
}

WebCore::StorageEstimate OriginStorageManager::estimate()
{
    ASSERT(!RunLoop::isMain());

    return WebCore::StorageEstimate { quotaManager().usage(), quotaManager().quota() };
}

void OriginStorageManager::didIncreaseQuota(QuotaIncreaseRequestIdentifier identifier, std::optional<uint64_t> newQuota)
{
    if (m_quotaManager)
        m_quotaManager->didIncreaseQuota(identifier, newQuota);
}

void OriginStorageManager::resetQuotaForTesting()
{
    if (m_quotaManager)
        m_quotaManager->resetQuotaForTesting();
}

void OriginStorageManager::resetQuotaUpdatedBasedOnUsageForTesting()
{
    if (m_quotaManager)
        m_quotaManager->resetQuotaUpdatedBasedOnUsageForTesting();
}

void OriginStorageManager::requestSpace(uint64_t size, CompletionHandler<void(bool)>&& completionHandler)
{
    quotaManager().requestSpace(size, [completionHandler = WTFMove(completionHandler)](auto decision) mutable {
        completionHandler(decision == QuotaManager::Decision::Grant);
    });
}

void OriginStorageManager::suspend()
{
    if (auto localStorageManager = existingLocalStorageManager())
        localStorageManager->syncLocalStorage();

    if (auto idbStorageManager = existingIDBStorageManager())
        idbStorageManager->stopDatabaseActivitiesForSuspend();
}

void OriginStorageManager::handleLowMemoryWarning()
{
    if (auto localStorageManager = existingLocalStorageManager())
        localStorageManager->handleLowMemoryWarning();

    if (auto idbStorageManager = existingIDBStorageManager())
        idbStorageManager->handleLowMemoryWarning();
}

void OriginStorageManager::clearSessionStorage(StorageNamespaceIdentifier identifier)
{
    if (auto* sessionStorageManager = existingSessionStorageManager())
        sessionStorageManager->removeNamespace(identifier);
}

void OriginStorageManager::syncLocalStorage()
{
    if (auto localStorageManager = existingLocalStorageManager())
        localStorageManager->syncLocalStorage();
}

OriginStorageManager::DataTypeSizeMap OriginStorageManager::fetchDataTypesInList(OptionSet<WebsiteDataType> types, bool shouldComputeSize)
{
    ASSERT(!RunLoop::isMain());

    return defaultBucket().fetchDataTypesInList(types, shouldComputeSize);
}

void OriginStorageManager::deleteData(OptionSet<WebsiteDataType> types, WallTime modifiedSince)
{
    ASSERT(!RunLoop::isMain());

    defaultBucket().deleteData(types, modifiedSince);
}

void OriginStorageManager::moveData(OptionSet<WebsiteDataType> types, const String& localStoragePath, const String& idbStoragePath)
{
    ASSERT(!RunLoop::isMain());

    defaultBucket().moveData(types, localStoragePath, idbStoragePath);
}

void OriginStorageManager::deleteEmptyDirectory()
{
    ASSERT(!RunLoop::isMain());

    if (m_path.isEmpty())
        return;

    defaultBucket().deleteEmptyDirectory();
}

void OriginStorageManager::closeCacheStorageManager()
{
    if (m_defaultBucket)
        m_defaultBucket->closeCacheStorageManager();
}

void OriginStorageManager::fileSystemGetDirectory(IPC::Connection& connection, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&& completionHandler)
{
    completionHandler(fileSystemStorageManager().getDirectory(connection.uniqueID()));
}

void OriginStorageManager::fileSystemCloseHandle(WebCore::FileSystemHandleIdentifier identifier)
{
    if (auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier))
        handle->close();
}

void OriginStorageManager::fileSystemIsSameEntry(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier targetIdentifier, CompletionHandler<void(bool)>&& completionHandler)
{
    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(false);
    
    completionHandler(handle->isSameEntry(targetIdentifier));
}

void OriginStorageManager::fileSystemMove(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier destinationIdentifier, const String& newName, CompletionHandler<void(std::optional<FileSystemStorageError>)>&& completionHandler)
{
    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(FileSystemStorageError::Unknown);
    
    completionHandler(handle->move(destinationIdentifier, newName));
}

void OriginStorageManager::fileSystemGetFileHandle(IPC::Connection& connection, WebCore::FileSystemHandleIdentifier identifier, String&& name, bool createIfNecessary, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&& completionHandler)
{
    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));
    
    completionHandler(handle->getFileHandle(connection.uniqueID(), WTFMove(name), createIfNecessary));
}

void OriginStorageManager::fileSystemGetDirectoryHandle(IPC::Connection& connection, WebCore::FileSystemHandleIdentifier identifier, String&& name, bool createIfNecessary, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&& completionHandler)
{
    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));
    
    completionHandler(handle->getDirectoryHandle(connection.uniqueID(), WTFMove(name), createIfNecessary));
}

void OriginStorageManager::fileSystemRemoveEntry(WebCore::FileSystemHandleIdentifier identifier, const String& name, bool deleteRecursively, CompletionHandler<void(std::optional<FileSystemStorageError>)>&& completionHandler)
{
    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(FileSystemStorageError::Unknown);
    
    completionHandler(handle->removeEntry(name, deleteRecursively));
}

void OriginStorageManager::fileSystemResolve(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemHandleIdentifier targetIdentifier, CompletionHandler<void(Expected<Vector<String>, FileSystemStorageError>)>&& completionHandler)
{
    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));
    
    completionHandler(handle->resolve(targetIdentifier));
}

void OriginStorageManager::fileSystemGetFile(WebCore::FileSystemHandleIdentifier identifier, CompletionHandler<void(Expected<String, FileSystemStorageError>)>&& completionHandler)
{
    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));
    
    completionHandler(handle->path());
}

void OriginStorageManager::fileSystemCreateSyncAccessHandle(WebCore::FileSystemHandleIdentifier identifier, CompletionHandler<void(Expected<FileSystemSyncAccessHandleInfo, FileSystemStorageError>)>&& completionHandler)
{
    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));
    
    completionHandler(handle->createSyncAccessHandle());
}

void OriginStorageManager::fileSystemCloseSyncAccessHandle(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, CompletionHandler<void()>&& completionHandler)
{
    if (auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier))
        handle->closeSyncAccessHandle(accessHandleIdentifier);
    
    completionHandler();
}

void OriginStorageManager::fileSystemRequestNewCapacityForSyncAccessHandle(WebCore::FileSystemHandleIdentifier identifier, WebCore::FileSystemSyncAccessHandleIdentifier accessHandleIdentifier, uint64_t newCapacity, CompletionHandler<void(std::optional<uint64_t>)>&& completionHandler)
{
    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(std::nullopt);
    
    handle->requestNewCapacityForSyncAccessHandle(accessHandleIdentifier, newCapacity, WTFMove(completionHandler));
}

void OriginStorageManager::fileSystemGetHandleNames(WebCore::FileSystemHandleIdentifier identifier, CompletionHandler<void(Expected<Vector<String>, FileSystemStorageError>)>&& completionHandler)
{
    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));
    
    completionHandler(handle->getHandleNames());
}

void OriginStorageManager::fileSystemGetHandle(IPC::Connection& connection, WebCore::FileSystemHandleIdentifier identifier, String&& name, CompletionHandler<void(Expected<std::pair<WebCore::FileSystemHandleIdentifier, bool>, FileSystemStorageError>)>&& completionHandler)
{
    auto handle = m_fileSystemStorageHandleRegistry->getHandle(identifier);
    if (!handle)
        return completionHandler(makeUnexpected(FileSystemStorageError::Unknown));
    
    completionHandler(handle->getHandle(connection.uniqueID(), WTFMove(name)));
}

void OriginStorageManager::webStorageConnectToStorageArea(IPC::Connection& connection, WebCore::StorageType type, StorageAreaMapIdentifier sourceIdentifier, std::optional<StorageNamespaceIdentifier> namespaceIdentifier, Ref<WorkQueue>&& taskQueue, CompletionHandler<void(StorageAreaIdentifier, HashMap<String, String>, uint64_t)>&& completionHandler)
{
    auto connectionIdentifier = connection.uniqueID();
    StorageAreaIdentifier resultIdentifier;
    switch (type) {
    case WebCore::StorageType::Local:
        resultIdentifier = localStorageManager().connectToLocalStorageArea(connectionIdentifier, sourceIdentifier, m_origin, WTFMove(taskQueue));
        break;
    case WebCore::StorageType::TransientLocal:
        resultIdentifier = localStorageManager().connectToTransientLocalStorageArea(connectionIdentifier, sourceIdentifier, m_origin);
        break;
    case WebCore::StorageType::Session:
        if (!namespaceIdentifier)
            return completionHandler(StorageAreaIdentifier { }, HashMap<String, String> { }, StorageAreaBase::nextMessageIdentifier());
        resultIdentifier = sessionStorageManager().connectToSessionStorageArea(connectionIdentifier, sourceIdentifier, m_origin, *namespaceIdentifier);
    }
    
    if (auto storageArea = m_storageAreaRegistry->getStorageArea(resultIdentifier))
        return completionHandler(resultIdentifier, storageArea->allItems(), StorageAreaBase::nextMessageIdentifier());
    
    completionHandler(resultIdentifier, HashMap<String, String> { }, StorageAreaBase::nextMessageIdentifier());
}

void OriginStorageManager::webStorageConnectToStorageAreaSync(IPC::Connection& connection, WebCore::StorageType type, StorageAreaMapIdentifier sourceIdentifier, std::optional<StorageNamespaceIdentifier> namespaceIdentifier, Ref<WorkQueue>&& taskQueue, CompletionHandler<void(StorageAreaIdentifier, HashMap<String, String>, uint64_t)>&& completionHandler)
{
    webStorageConnectToStorageArea(connection, type, sourceIdentifier, namespaceIdentifier, WTFMove(taskQueue), WTFMove(completionHandler));
}

void OriginStorageManager::webStorageCancelConnectToStorageArea(IPC::Connection& connection, WebCore::StorageType type, std::optional<StorageNamespaceIdentifier> namespaceIdentifier)
{
    auto connectionIdentifier = connection.uniqueID();
    switch (type) {
    case WebCore::StorageType::Local:
        if (auto localStorageManager = existingLocalStorageManager())
            localStorageManager->cancelConnectToLocalStorageArea(connectionIdentifier);
        break;
    case WebCore::StorageType::TransientLocal:
        if (auto localStorageManager = existingLocalStorageManager())
            localStorageManager->cancelConnectToTransientLocalStorageArea(connectionIdentifier);
        break;
    case WebCore::StorageType::Session:
        if (!namespaceIdentifier)
            return;
        if (auto sessionStorageManager = existingSessionStorageManager())
            sessionStorageManager->cancelConnectToSessionStorageArea(connectionIdentifier, *namespaceIdentifier);
    }
}

void OriginStorageManager::webStorageDisconnectFromStorageArea(IPC::Connection& connection, StorageAreaIdentifier identifier)
{
    auto storageArea = m_storageAreaRegistry->getStorageArea(identifier);
    if (!storageArea)
        return;
    
    if (storageArea->storageType() == StorageAreaBase::StorageType::Local)
        localStorageManager().disconnectFromStorageArea(connection.uniqueID(), identifier);
    else
        sessionStorageManager().disconnectFromStorageArea(connection.uniqueID(), identifier);
}

void OriginStorageManager::webStorageCloneSessionStorageNamespace(StorageNamespaceIdentifier fromIdentifier, StorageNamespaceIdentifier toIdentifier)
{
    if (auto* sessionStorageManager = existingSessionStorageManager())
        sessionStorageManager->cloneStorageArea(fromIdentifier, toIdentifier);
}

void OriginStorageManager::webStorageSetItem(IPC::Connection& connection, StorageAreaIdentifier identifier, StorageAreaImplIdentifier implIdentifier, String&& key, String&& value, String&& urlString, CompletionHandler<void(bool, HashMap<String, String>&&)>&& completionHandler)
{
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
}

void OriginStorageManager::webStorageRemoveItem(IPC::Connection& connection, StorageAreaIdentifier identifier, StorageAreaImplIdentifier implIdentifier, String&& key, String&& urlString, CompletionHandler<void()>&& completionHandler)
{
    auto storageArea = m_storageAreaRegistry->getStorageArea(identifier);
    if (!storageArea)
        return completionHandler();

    storageArea->removeItem(connection.uniqueID(), implIdentifier, WTFMove(key), WTFMove(urlString));
    completionHandler();
}

void OriginStorageManager::webStorageClear(IPC::Connection& connection, StorageAreaIdentifier identifier, StorageAreaImplIdentifier implIdentifier, String&& urlString, CompletionHandler<void()>&& completionHandler)
{
    auto storageArea = m_storageAreaRegistry->getStorageArea(identifier);
    if (!storageArea)
        return completionHandler();

    storageArea->clear(connection.uniqueID(), implIdentifier, WTFMove(urlString));
    completionHandler();
}

void OriginStorageManager::idbOpenDatabase(IPC::Connection& connection, const WebCore::IDBRequestData& requestData)
{
    auto& connectionToClient = m_idbStorageRegistry->ensureConnectionToClient(connection.uniqueID(), requestData.requestIdentifier().connectionIdentifier());
    idbStorageManager().openDatabase(connectionToClient, requestData);
}

void OriginStorageManager::idbOpenDBRequestCancelled(const WebCore::IDBRequestData& requestData)
{
    idbStorageManager().openDBRequestCancelled(requestData);
}

void OriginStorageManager::idbDeleteDatabase(IPC::Connection& connection, const WebCore::IDBRequestData& requestData)
{
    auto& connectionToClient = m_idbStorageRegistry->ensureConnectionToClient(connection.uniqueID(), requestData.requestIdentifier().connectionIdentifier());
    idbStorageManager().deleteDatabase(connectionToClient, requestData);
}

void OriginStorageManager::idbEstablishTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBTransactionInfo& transactionInfo)
{
    if (auto connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->establishTransaction(transactionInfo);
}

void OriginStorageManager::idbDatabaseConnectionPendingClose(uint64_t databaseConnectionIdentifier)
{
    if (auto connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->connectionPendingCloseFromClient();
}

void OriginStorageManager::idbDatabaseConnectionClosed(uint64_t databaseConnectionIdentifier)
{
    if (auto connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->connectionClosedFromClient();
}

void OriginStorageManager::idbAbortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const std::optional<WebCore::IDBResourceIdentifier>& transactionIdentifier)
{
    if (transactionIdentifier) {
        if (auto transaction = m_idbStorageRegistry->transaction(*transactionIdentifier))
            transaction->abortWithoutCallback();
    }

    if (auto connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->connectionClosedFromClient();
}

void OriginStorageManager::idbDidFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& requestIdentifier, const WebCore::IndexedDB::ConnectionClosedOnBehalfOfServer connectionClosed)
{
    if (auto connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->didFireVersionChangeEvent(requestIdentifier, connectionClosed);
}

void OriginStorageManager::idbAbortTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier)
{
    if (auto transaction = m_idbStorageRegistry->transaction(transactionIdentifier))
        transaction->abort();
}

void OriginStorageManager::idbCommitTransaction(const WebCore::IDBResourceIdentifier& transactionIdentifier, uint64_t pendingRequestCount)
{
    if (auto transaction = m_idbStorageRegistry->transaction(transactionIdentifier))
        transaction->commit(pendingRequestCount);
}

void OriginStorageManager::idbDidFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& transactionIdentifier)
{
    if (auto connection = m_idbStorageRegistry->connection(databaseConnectionIdentifier))
        connection->didFinishHandlingVersionChange(transactionIdentifier);
}

void OriginStorageManager::idbCreateObjectStore(const WebCore::IDBRequestData& requestData, const WebCore::IDBObjectStoreInfo& objectStoreInfo)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier())) {
        ASSERT(transaction->isVersionChange());
        transaction->createObjectStore(requestData, objectStoreInfo);
    }
}

void OriginStorageManager::idbDeleteObjectStore(const WebCore::IDBRequestData& requestData, const String& objectStoreName)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier())) {
        ASSERT(transaction->isVersionChange());
        transaction->deleteObjectStore(requestData, objectStoreName);
    }
}

void OriginStorageManager::idbRenameObjectStore(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& newName)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier())) {
        ASSERT(transaction->isVersionChange());
        transaction->renameObjectStore(requestData, objectStoreIdentifier, newName);
    }
}

void OriginStorageManager::idbClearObjectStore(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->clearObjectStore(requestData, objectStoreIdentifier);
}

void OriginStorageManager::idbCreateIndex(const WebCore::IDBRequestData& requestData, const WebCore::IDBIndexInfo& indexInfo)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->createIndex(requestData, indexInfo);
}

void OriginStorageManager::idbDeleteIndex(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier, const String& indexName)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->deleteIndex(requestData, objectStoreIdentifier, indexName);
}

void OriginStorageManager::idbRenameIndex(const WebCore::IDBRequestData& requestData, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->renameIndex(requestData, objectStoreIdentifier, indexIdentifier, newName);
}

void OriginStorageManager::idbPutOrAdd(IPC::Connection& connection, const WebCore::IDBRequestData& requestData, const WebCore::IDBKeyData& keyData, const WebCore::IDBValue& value, WebCore::IndexedDB::ObjectStoreOverwriteMode overwriteMode)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->putOrAdd(requestData, keyData, value, overwriteMode);
}

void OriginStorageManager::idbGetRecord(const WebCore::IDBRequestData& requestData, const WebCore::IDBGetRecordData& getRecordData)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->getRecord(requestData, getRecordData);
}

void OriginStorageManager::idbGetAllRecords(const WebCore::IDBRequestData& requestData, const WebCore::IDBGetAllRecordsData& getAllRecordsData)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->getAllRecords(requestData, getAllRecordsData);
}

void OriginStorageManager::idbGetCount(const WebCore::IDBRequestData& requestData, const WebCore::IDBKeyRangeData& keyRangeData)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->getCount(requestData, keyRangeData);
}

void OriginStorageManager::idbDeleteRecord(const WebCore::IDBRequestData& requestData, const WebCore::IDBKeyRangeData& keyRangeData)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->deleteRecord(requestData, keyRangeData);
}

void OriginStorageManager::idbOpenCursor(const WebCore::IDBRequestData& requestData, const WebCore::IDBCursorInfo& cursorInfo)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->openCursor(requestData, cursorInfo);
}

void OriginStorageManager::idbIterateCursor(const WebCore::IDBRequestData& requestData, const WebCore::IDBIterateCursorData& cursorData)
{
    if (auto transaction = m_idbStorageRegistry->transaction(requestData.transactionIdentifier()))
        transaction->iterateCursor(requestData, cursorData);
}

void OriginStorageManager::idbGetAllDatabaseNamesAndVersions(IPC::Connection& connection, const WebCore::IDBResourceIdentifier& requestIdentifier)
{
    auto& connectionToClient = m_idbStorageRegistry->ensureConnectionToClient(connection.uniqueID(), requestIdentifier.connectionIdentifier());
    auto result = idbStorageManager().getAllDatabaseNamesAndVersions();
    connectionToClient.didGetAllDatabaseNamesAndVersions(requestIdentifier, WTFMove(result));
}

void OriginStorageManager::cacheStorageOpenCache(Ref<WorkQueue>&& callbackQueue, const String& cacheName, WebCore::DOMCacheEngine::CacheIdentifierCallback&& completionHandler)
{
    cacheStorageManager(WTFMove(callbackQueue)).openCache(cacheName, WTFMove(completionHandler));
}

void OriginStorageManager::cacheStorageRemoveCache(WebCore::DOMCacheIdentifier cacheIdentifier, WebCore::DOMCacheEngine::RemoveCacheIdentifierCallback&& completionHandler)
{
    auto* cache = m_cacheStorageRegistry->cache(cacheIdentifier);
    if (!cache)
        return completionHandler(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    auto* cacheStorageManager = cache->manager();
    if (!cacheStorageManager)
        return completionHandler(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    cacheStorageManager->removeCache(cacheIdentifier, WTFMove(completionHandler));
}

void OriginStorageManager::cacheStorageAllCaches(Ref<WorkQueue>&& callbackQueue, uint64_t updateCounter, WebCore::DOMCacheEngine::CacheInfosCallback&& completionHandler)
{
    cacheStorageManager(WTFMove(callbackQueue)).allCaches(updateCounter, WTFMove(completionHandler));
}

void OriginStorageManager::cacheStorageReference(IPC::Connection& connection, WebCore::DOMCacheIdentifier cacheIdentifier)
{
    auto* cache = m_cacheStorageRegistry->cache(cacheIdentifier);
    if (!cache)
        return;

    auto* cacheStorageManager = cache->manager();
    if (!cacheStorageManager)
        return;

    cacheStorageManager->reference(connection.uniqueID(), cacheIdentifier);
}

void OriginStorageManager::cacheStorageDereference(IPC::Connection& connection, WebCore::DOMCacheIdentifier cacheIdentifier)
{
    auto* cache = m_cacheStorageRegistry->cache(cacheIdentifier);
    if (!cache)
        return;

    auto* cacheStorageManager = cache->manager();
    if (!cacheStorageManager)
        return;

    cacheStorageManager->dereference(connection.uniqueID(), cacheIdentifier);
}

void OriginStorageManager::cacheStorageRetrieveRecords(WebCore::DOMCacheIdentifier cacheIdentifier, WebCore::RetrieveRecordsOptions&& options, WebCore::DOMCacheEngine::RecordsCallback&& completionHandler)
{
    auto* cache = m_cacheStorageRegistry->cache(cacheIdentifier);
    if (!cache)
        return completionHandler(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    cache->retrieveRecords(WTFMove(options), WTFMove(completionHandler));
}

void OriginStorageManager::cacheStorageRemoveRecords(WebCore::DOMCacheIdentifier cacheIdentifier, WebCore::ResourceRequest&& request, WebCore::CacheQueryOptions&& options, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& completionHandler)
{
    auto* cache = m_cacheStorageRegistry->cache(cacheIdentifier);
    if (!cache)
        return completionHandler(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    cache->removeRecords(WTFMove(request), WTFMove(options), WTFMove(completionHandler));
}

void OriginStorageManager::cacheStoragePutRecords(WebCore::DOMCacheIdentifier cacheIdentifier, Vector<WebCore::DOMCacheEngine::Record>&& records, WebCore::DOMCacheEngine::RecordIdentifiersCallback&& completionHandler)
{
    auto* cache = m_cacheStorageRegistry->cache(cacheIdentifier);
    if (!cache)
        return completionHandler(makeUnexpected(WebCore::DOMCacheEngine::Error::Internal));

    cache->putRecords(WTFMove(records), WTFMove(completionHandler));
}

void OriginStorageManager::cacheStorageClearMemoryRepresentation(CompletionHandler<void(std::optional<WebCore::DOMCacheEngine::Error>&&)>&& callback)
{
    closeCacheStorageManager();
    callback(std::nullopt);
}

String OriginStorageManager::cacheStorageRepresentation(Ref<WorkQueue>&& callbackQueue)
{
    return cacheStorageManager(WTFMove(callbackQueue)).representationString();
}

} // namespace WebKit

