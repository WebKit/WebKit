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

#include "FileSystemStorageHandleRegistry.h"
#include "FileSystemStorageManager.h"

namespace WebKit {

enum class OriginStorageManager::StorageBucketMode : bool { BestEffort, Persistent };

class OriginStorageManager::StorageBucket {
    WTF_MAKE_FAST_ALLOCATED;
public:
    StorageBucket(const String& rootPath, const String& identifier)
        : m_rootPath(rootPath)
        , m_identifier(identifier)
    {
    }

    StorageBucketMode mode() const { return m_mode; }
    void setMode(StorageBucketMode mode) { m_mode = mode; }

    void connectionClosed(IPC::Connection::UniqueID connection)
    {
        if (m_fileSystemStorageManager)
            m_fileSystemStorageManager->connectionClosed(connection);
    }

    enum class StorageType : uint8_t {
        FileSystem,
    };

    static String toStorageIdentifier(StorageType type)
    {
        switch (type) {
        case StorageType::FileSystem:
            return "FileSystem"_s;
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

    bool isActive()
    {
        return !!m_fileSystemStorageManager;
    }

    void deleteData(OptionSet<WebsiteDataType> types, WallTime modifiedSinceTime)
    {
        if (types.contains(WebsiteDataType::FileSystem))
            deleteFileSystemStorageData(modifiedSinceTime);
    }

private:
    void deleteFileSystemStorageData(WallTime modifiedSinceTime)
    {
        m_fileSystemStorageManager = nullptr;

        auto fileSystemStoragePath = typeStoragePath(StorageType::FileSystem);
        FileSystem::deleteAllFilesModifiedSince(fileSystemStoragePath, modifiedSinceTime);
    }

    String m_rootPath;
    String m_identifier;
    StorageBucketMode m_mode { StorageBucketMode::BestEffort };
    std::unique_ptr<FileSystemStorageManager> m_fileSystemStorageManager;
};

OriginStorageManager::OriginStorageManager(String&& path)
    : m_path(WTFMove(path))
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
        m_defaultBucket = makeUnique<StorageBucket>(m_path, "default"_s);

    return *m_defaultBucket;
}

FileSystemStorageManager& OriginStorageManager::fileSystemStorageManager(FileSystemStorageHandleRegistry& registry)
{
    return defaultBucket().fileSystemStorageManager(registry);
}

bool OriginStorageManager::isActive()
{
    return defaultBucket().isActive();
}

void OriginStorageManager::deleteData(OptionSet<WebsiteDataType> types, WallTime modifiedSince)
{
    ASSERT(!RunLoop::isMain());
    defaultBucket().deleteData(types, modifiedSince);
}

void OriginStorageManager::setPersisted(bool value)
{
    ASSERT(!RunLoop::isMain());

    m_persisted = value;
    defaultBucket().setMode(value ? StorageBucketMode::Persistent : StorageBucketMode::BestEffort);
}

} // namespace WebKit

