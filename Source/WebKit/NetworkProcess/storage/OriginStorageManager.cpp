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
    StorageBucket(const String& rootPath, const String& identifier, const String& localStoragePath, const String& idbStoragePath)
        : m_rootPath(rootPath)
        , m_identifier(identifier)
        , m_localStoragePath(localStoragePath)
        , m_idbStoragePath(idbStoragePath)
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
            m_localStorageManager = makeUnique<LocalStorageManager>(m_localStoragePath, registry);

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
            m_idbStorageManager = makeUnique<IDBStorageManager>(m_idbStoragePath, registry, WTFMove(quotaCheckFunction));

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

    bool isEmpty() const
    {
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

        FileSystem::deleteEmptyDirectory(m_idbStoragePath);
        return !FileSystem::fileExists(m_localStoragePath) && !FileSystem::fileExists(m_idbStoragePath);
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

    void moveData(const String& path, const String& localStoragePath, const String& idbStoragePath)
    {
        m_fileSystemStorageManager = nullptr;
        if (m_localStorageManager)
            m_localStorageManager->close();

        if (m_idbStorageManager)
            m_idbStorageManager->closeDatabasesForDeletion();

        FileSystem::makeAllDirectories(FileSystem::parentPath(path));
        FileSystem::moveFile(m_rootPath, path);

        if (!m_localStoragePath.isEmpty() && !localStoragePath.isEmpty()) {
            FileSystem::makeAllDirectories(FileSystem::parentPath(localStoragePath));
            WebCore::SQLiteFileSystem::moveDatabaseFile(m_localStoragePath, localStoragePath);
        }

        if (!m_idbStoragePath.isEmpty() && !idbStoragePath.isEmpty()) {
            FileSystem::makeAllDirectories(FileSystem::parentPath(idbStoragePath));
            WebCore::SQLiteFileSystem::moveDatabaseFile(m_idbStoragePath, idbStoragePath);
        }
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
        for (auto& storageType : FileSystem::listDirectory(m_rootPath)) {
            if (auto type = toWebsiteDataType(storageType); type && types.contains(*type))
                result.add(*type);
        }

        if (types.contains(WebsiteDataType::LocalStorage) && !result.contains(WebsiteDataType::LocalStorage)) {
            if (FileSystem::fileExists(m_localStoragePath))
                result.add(WebsiteDataType::LocalStorage);
        }

        if (types.contains(WebsiteDataType::IndexedDBDatabases) && !result.contains(WebsiteDataType::IndexedDBDatabases)) {
            if (auto databases = FileSystem::listDirectory(m_idbStoragePath); !databases.isEmpty())
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
        if (FileSystem::fileModificationTime(m_localStoragePath) >= time) {
            if (m_localStorageManager)
                m_localStorageManager->clearDataOnDisk();
            WebCore::SQLiteFileSystem::deleteDatabaseFile(m_localStoragePath);
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

        FileSystem::deleteAllFilesModifiedSince(m_idbStoragePath, time);
    }
    
    String m_rootPath;
    String m_identifier;
    StorageBucketMode m_mode { StorageBucketMode::BestEffort };
    std::unique_ptr<FileSystemStorageManager> m_fileSystemStorageManager;
    std::unique_ptr<LocalStorageManager> m_localStorageManager;
    String m_localStoragePath;
    std::unique_ptr<SessionStorageManager> m_sessionStorageManager;
    std::unique_ptr<IDBStorageManager> m_idbStorageManager;
    String m_idbStoragePath;
};

static Ref<QuotaManager> createQuotaManager(uint64_t quota, const String& idbStoragePath, const String& cacheStoragePath, QuotaManager::IncreaseQuotaFunction&& increaseQuotaFunction)
{
    QuotaManager::GetUsageFunction getUsageFunction = [idbStoragePath, cacheStoragePath]() {
        return IDBStorageManager::idbStorageSize(idbStoragePath) + CacheStorage::Engine::diskUsage(cacheStoragePath);
    };
    return QuotaManager::create(quota, WTFMove(getUsageFunction), WTFMove(increaseQuotaFunction));
}

OriginStorageManager::OriginStorageManager(uint64_t quota, QuotaManager::IncreaseQuotaFunction&& increaseQuotaFunction, String&& path, String&& localStoragePath, String&& idbStoragePath, String&& cacheStoragePath)
    : m_path(WTFMove(path))
    , m_localStoragePath(WTFMove(localStoragePath))
    , m_idbStoragePath(WTFMove(idbStoragePath))
    , m_cacheStoragePath(WTFMove(cacheStoragePath))
    , m_quotaManager(createQuotaManager(quota, m_idbStoragePath, m_cacheStoragePath, WTFMove(increaseQuotaFunction)))
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
        m_defaultBucket = makeUnique<StorageBucket>(m_path, "default"_s, m_localStoragePath, m_idbStoragePath);

    return *m_defaultBucket;
}

QuotaManager& OriginStorageManager::quotaManager()
{
    return m_quotaManager.get();
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

void OriginStorageManager::moveData(const String& newPath, const String& localStoragePath, const String& idbStoragePath)
{
    ASSERT(!RunLoop::isMain());

    defaultBucket().moveData(newPath, localStoragePath, idbStoragePath);
}

} // namespace WebKit

