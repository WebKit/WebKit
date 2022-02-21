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

#include "CacheStorageEngine.h"
#include "FileSystemStorageHandleRegistry.h"
#include "FileSystemStorageManager.h"
#include "IDBStorageManager.h"
#include "IDBStorageRegistry.h"
#include "LocalStorageManager.h"
#include "MemoryStorageArea.h"
#include "SessionStorageManager.h"
#include "StorageAreaRegistry.h"
#include "WebsiteDataType.h"
#include <WebCore/SQLiteFileSystem.h>
#include <wtf/FileSystem.h>

namespace WebKit {

static constexpr auto originFileName = "origin"_s;
enum class OriginStorageManager::StorageBucketMode : bool { BestEffort, Persistent };

class OriginStorageManager::StorageBucket {
    WTF_MAKE_FAST_ALLOCATED;
public:
    StorageBucket(const String& rootPath, const String& identifier, const String& localStoragePath, const String& idbStoragePath, bool shouldUseCustomPaths)
        : m_rootPath(rootPath)
        , m_identifier(identifier)
        , m_customLocalStoragePath(localStoragePath)
        , m_customIDBStoragePath(idbStoragePath)
        , m_shouldUseCustomPaths(shouldUseCustomPaths)
    {
    }

    StorageBucketMode mode() const { return m_mode; }
    void setMode(StorageBucketMode mode) { m_mode = mode; }

    void connectionClosed(IPC::Connection::UniqueID connection)
    {
        if (m_fileSystemStorageManager)
            m_fileSystemStorageManager->connectionClosed(connection);

        if (m_localStorageManager)
            m_localStorageManager->connectionClosed(connection);

        if (m_sessionStorageManager)
            m_sessionStorageManager->connectionClosed(connection);
    }

    enum class StorageType : uint8_t {
        FileSystem,
        LocalStorage,
        SessionStorage,
        IndexedDB,
    };

    static std::optional<WebsiteDataType> toWebsiteDataType(const String& storageIdentifier)
    {
        if (storageIdentifier == "FileSystem"_s)
            return WebsiteDataType::FileSystem;
        if (storageIdentifier == "LocalStorage"_s)
            return WebsiteDataType::LocalStorage;
        if (storageIdentifier == "SessionStorage"_s)
            return WebsiteDataType::SessionStorage;
        if (storageIdentifier == "IndexedDB"_s)
            return WebsiteDataType::IndexedDBDatabases;

        return std::nullopt;
    }

    static String toStorageIdentifier(StorageType type)
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
        default:
            break;
        }
        ASSERT_NOT_REACHED();
        return ""_s;
    }

    String typeStoragePath(StorageType type) const
    {
        auto storageIdentifier = toStorageIdentifier(type);
        if (m_rootPath.isEmpty() || storageIdentifier.isEmpty())
            return emptyString();

        return FileSystem::pathByAppendingComponent(m_rootPath, storageIdentifier);
    }

    FileSystemStorageManager& fileSystemStorageManager(FileSystemStorageHandleRegistry& registry)
    {
        if (!m_fileSystemStorageManager)
            m_fileSystemStorageManager = makeUnique<FileSystemStorageManager>(typeStoragePath(StorageType::FileSystem), registry);

        return *m_fileSystemStorageManager;
    }

    LocalStorageManager& localStorageManager(StorageAreaRegistry& registry)
    {
        if (!m_localStorageManager)
            m_localStorageManager = makeUnique<LocalStorageManager>(resolvedLocalStoragePath(), registry);

        return *m_localStorageManager;
    }

    LocalStorageManager* existingLocalStorageManager()
    {
        return m_localStorageManager.get();
    }

    SessionStorageManager& sessionStorageManager(StorageAreaRegistry& registry)
    {
        if (!m_sessionStorageManager)
            m_sessionStorageManager = makeUnique<SessionStorageManager>(registry);

        return *m_sessionStorageManager;
    }

    SessionStorageManager* existingSessionStorageManager()
    {
        return m_sessionStorageManager.get();
    }

    IDBStorageManager& idbStorageManager(IDBStorageRegistry& registry, IDBStorageManager::QuotaCheckFunction&& quotaCheckFunction)
    {
        if (!m_idbStorageManager)
            m_idbStorageManager = makeUnique<IDBStorageManager>(resolvedIDBStoragePath(), registry, WTFMove(quotaCheckFunction));

        return *m_idbStorageManager;
    }

    IDBStorageManager* existingIDBStorageManager()
    {
        return m_idbStorageManager.get();
    }

    bool isActive() const
    {
        // We cannot remove the bucket if it has in-memory data, otherwise session
        // data may be lost.
        return (m_fileSystemStorageManager && m_fileSystemStorageManager->isActive())
            || (m_localStorageManager && (m_localStorageManager->hasDataInMemory() || m_localStorageManager->isActive()))
            || (m_sessionStorageManager && (m_sessionStorageManager->hasDataInMemory() || m_sessionStorageManager->isActive()))
            || (m_idbStorageManager && (m_idbStorageManager->hasDataInMemory() || m_idbStorageManager->isActive()));
    }

    bool isEmpty()
    {
        ASSERT(!RunLoop::isMain());

        auto files = FileSystem::listDirectory(m_rootPath);
        auto hasValidFile = WTF::anyOf(files, [&](auto file) {
            bool isInvalidFile = (file == originFileName);
#if PLATFORM(COCOA)
            isInvalidFile |= (file == ".DS_Store");
#endif
            return !isInvalidFile;
        });
        if (hasValidFile)
            return false;

        auto idbStorageFiles = FileSystem::listDirectory(resolvedIDBStoragePath());
        return !FileSystem::fileExists(resolvedLocalStoragePath()) && idbStorageFiles.isEmpty();
    }

    OptionSet<WebsiteDataType> fetchDataTypesInList(OptionSet<WebsiteDataType> types)
    {
        auto result = fetchDataTypesInListFromMemory(types);
        result.add(fetchDataTypesInListFromDisk(types));

        return result;
    }

    void deleteData(OptionSet<WebsiteDataType> types, WallTime modifiedSinceTime)
    {
        if (types.contains(WebsiteDataType::FileSystem))
            deleteFileSystemStorageData(modifiedSinceTime);

        if (types.contains(WebsiteDataType::LocalStorage))
            deleteLocalStorageData(modifiedSinceTime);

        if (types.contains(WebsiteDataType::SessionStorage) && modifiedSinceTime < WallTime::now())
            deleteSessionStorageData();

        if (types.contains(WebsiteDataType::IndexedDBDatabases))
            deleteIDBStorageData(modifiedSinceTime);
    }

    void moveData(OptionSet<WebsiteDataType> types, const String& localStoragePath, const String& idbStoragePath)
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

    String resolvedLocalStoragePath()
    {
        if (!m_resolvedLocalStoragePath.isNull())
            return m_resolvedLocalStoragePath;

        if (m_shouldUseCustomPaths) {
            ASSERT(m_customLocalStoragePath.isEmpty() == m_rootPath.isEmpty());
            m_resolvedLocalStoragePath = m_customLocalStoragePath;
        } else if (!m_rootPath.isEmpty()) {
            auto localStorageDirectory = typeStoragePath(StorageType::LocalStorage);
            FileSystem::makeAllDirectories(localStorageDirectory);
            FileSystem::excludeFromBackup(localStorageDirectory);

            auto localStoragePath = LocalStorageManager::localStorageFilePath(localStorageDirectory);
            if (!m_customLocalStoragePath.isEmpty() && !FileSystem::fileExists(localStoragePath) && FileSystem::fileExists(m_customLocalStoragePath))
                WebCore::SQLiteFileSystem::moveDatabaseFile(m_customLocalStoragePath, localStoragePath);

            m_resolvedLocalStoragePath = localStoragePath;
        } else
            m_resolvedLocalStoragePath = emptyString();

        return m_resolvedLocalStoragePath;
    }

    String resolvedIDBStoragePath()
    {
        ASSERT(!RunLoop::isMain());

        if (!m_resolvedIDBStoragePath.isNull())
            return m_resolvedIDBStoragePath;

        if (m_shouldUseCustomPaths) {
            ASSERT(m_customIDBStoragePath.isEmpty() == m_rootPath.isEmpty());
            m_resolvedIDBStoragePath = m_customIDBStoragePath;
        } else {
            auto idbStoragePath = typeStoragePath(StorageType::IndexedDB);
            if (!m_customIDBStoragePath.isEmpty() && !FileSystem::fileExists(idbStoragePath) && FileSystem::fileExists(m_customIDBStoragePath)) {
                FileSystem::makeAllDirectories(idbStoragePath);
                FileSystem::moveFile(m_customIDBStoragePath, idbStoragePath);
            }

            m_resolvedIDBStoragePath = idbStoragePath;
        }
        
        ASSERT(!m_resolvedIDBStoragePath.isNull());
        return m_resolvedIDBStoragePath;
    }

private:
    OptionSet<WebsiteDataType> fetchDataTypesInListFromMemory(OptionSet<WebsiteDataType> types)
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

        return result;
    }

    OptionSet<WebsiteDataType> fetchDataTypesInListFromDisk(OptionSet<WebsiteDataType> types)
    {
        OptionSet<WebsiteDataType> result;
        if (types.contains(WebsiteDataType::FileSystem)) {
            auto fileSystemStoragePath = typeStoragePath(StorageType::FileSystem);
            if (auto files = FileSystem::listDirectory(fileSystemStoragePath); !files.isEmpty())
                result.add(WebsiteDataType::FileSystem);
        }

        if (types.contains(WebsiteDataType::LocalStorage)) {
            if (FileSystem::fileExists(resolvedLocalStoragePath()))
                result.add(WebsiteDataType::LocalStorage);
        }

        if (types.contains(WebsiteDataType::IndexedDBDatabases)) {
            if (auto databases = FileSystem::listDirectory(resolvedIDBStoragePath()); !databases.isEmpty())
                result.add(WebsiteDataType::IndexedDBDatabases);
        }

        return result;
    }

    void deleteFileSystemStorageData(WallTime modifiedSinceTime)
    {
        m_fileSystemStorageManager = nullptr;

        auto fileSystemStoragePath = typeStoragePath(StorageType::FileSystem);
        FileSystem::deleteAllFilesModifiedSince(fileSystemStoragePath, modifiedSinceTime);
    }

    void deleteLocalStorageData(WallTime time)
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

    void deleteSessionStorageData()
    {
        if (!m_sessionStorageManager)
            return;

        m_sessionStorageManager->clearData();
        if (!m_sessionStorageManager->isActive())
            m_sessionStorageManager = nullptr;
    }
    
    void deleteIDBStorageData(WallTime time)
    {
        if (m_idbStorageManager)
            m_idbStorageManager->closeDatabasesForDeletion();

        FileSystem::deleteAllFilesModifiedSince(resolvedIDBStoragePath(), time);
    }

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
    bool m_shouldUseCustomPaths;
};

String OriginStorageManager::originFileIdentifier()
{
    return originFileName;
}

static Ref<QuotaManager> createQuotaManager(uint64_t quota, const String& idbStoragePath, const String& cacheStoragePath, QuotaManager::IncreaseQuotaFunction&& increaseQuotaFunction)
{
    QuotaManager::GetUsageFunction getUsageFunction = [idbStoragePath, cacheStoragePath]() {
        return IDBStorageManager::idbStorageSize(idbStoragePath) + CacheStorage::Engine::diskUsage(cacheStoragePath);
    };
    return QuotaManager::create(quota, WTFMove(getUsageFunction), WTFMove(increaseQuotaFunction));
}

OriginStorageManager::OriginStorageManager(uint64_t quota, QuotaManager::IncreaseQuotaFunction&& increaseQuotaFunction, String&& path, String&& customLocalStoragePath, String&& customIDBStoragePath, String&& cacheStoragePath, bool shouldUseCustomPaths)
    : m_path(WTFMove(path))
    , m_customLocalStoragePath(WTFMove(customLocalStoragePath))
    , m_customIDBStoragePath(WTFMove(customIDBStoragePath))
    , m_cacheStoragePath(WTFMove(cacheStoragePath))
    , m_quota(quota)
    , m_increaseQuotaFunction(WTFMove(increaseQuotaFunction))
    , m_shouldUseCustomPaths(shouldUseCustomPaths)
{
    ASSERT(!RunLoop::isMain());
}

OriginStorageManager::~OriginStorageManager() = default;

void OriginStorageManager::connectionClosed(IPC::Connection::UniqueID connection)
{
    if (m_defaultBucket)
        m_defaultBucket->connectionClosed(connection);
}

OriginStorageManager::StorageBucket& OriginStorageManager::defaultBucket()
{
    if (!m_defaultBucket)
        m_defaultBucket = makeUnique<StorageBucket>(m_path, "default"_s, m_customLocalStoragePath, m_customIDBStoragePath, m_shouldUseCustomPaths);

    return *m_defaultBucket;
}

QuotaManager& OriginStorageManager::quotaManager()
{
    if (!m_quotaManager) {
        auto idbStoragePath = defaultBucket().resolvedIDBStoragePath();
        m_quotaManager = createQuotaManager(m_quota, idbStoragePath, m_cacheStoragePath, std::exchange(m_increaseQuotaFunction, { }));
    }

    return *m_quotaManager;
}

FileSystemStorageManager& OriginStorageManager::fileSystemStorageManager(FileSystemStorageHandleRegistry& registry)
{
    return defaultBucket().fileSystemStorageManager(registry);
}

LocalStorageManager& OriginStorageManager::localStorageManager(StorageAreaRegistry& registry)
{
    return defaultBucket().localStorageManager(registry);
}

LocalStorageManager* OriginStorageManager::existingLocalStorageManager()
{
    return defaultBucket().existingLocalStorageManager();
}

SessionStorageManager& OriginStorageManager::sessionStorageManager(StorageAreaRegistry& registry)
{
    return defaultBucket().sessionStorageManager(registry);
}

SessionStorageManager* OriginStorageManager::existingSessionStorageManager()
{
    return defaultBucket().existingSessionStorageManager();
}

IDBStorageManager& OriginStorageManager::idbStorageManager(IDBStorageRegistry& registry)
{
    return defaultBucket().idbStorageManager(registry, [quotaManager = WeakPtr { this->quotaManager() }](uint64_t spaceRequested, CompletionHandler<void(bool)>&& completionHandler) mutable {
        if (!quotaManager)
            return completionHandler(false);

        quotaManager->requestSpace(spaceRequested, [completionHandler = WTFMove(completionHandler)](auto decision) mutable {
            completionHandler(decision == QuotaManager::Decision::Grant);
        });
    });
}

IDBStorageManager* OriginStorageManager::existingIDBStorageManager()
{
    return defaultBucket().existingIDBStorageManager();
}

String OriginStorageManager::resolvedLocalStoragePath()
{
    return defaultBucket().resolvedLocalStoragePath();
}

String OriginStorageManager::resolvedIDBStoragePath()
{
    return defaultBucket().resolvedIDBStoragePath();
}

bool OriginStorageManager::isActive()
{
    return defaultBucket().isActive();
}

bool OriginStorageManager::isEmpty()
{
    return defaultBucket().isEmpty();
}

void OriginStorageManager::setPersisted(bool value)
{
    ASSERT(!RunLoop::isMain());

    m_persisted = value;
    defaultBucket().setMode(value ? StorageBucketMode::Persistent : StorageBucketMode::BestEffort);
}

OptionSet<WebsiteDataType> OriginStorageManager::fetchDataTypesInList(OptionSet<WebsiteDataType> types)
{
    ASSERT(!RunLoop::isMain());

    return defaultBucket().fetchDataTypesInList(types);
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

} // namespace WebKit

