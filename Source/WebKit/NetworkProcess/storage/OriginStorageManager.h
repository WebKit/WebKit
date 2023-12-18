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

#pragma once

#include "Connection.h"
#include "OriginQuotaManager.h"
#include "WebsiteDataType.h"
#include <wtf/text/WTFString.h>

namespace WebCore {
struct ClientOrigin;
struct StorageEstimate;
}

namespace WebKit {

class BackgroundFetchStoreManager;
class CacheStorageManager;
class CacheStorageRegistry;
class FileSystemStorageHandleRegistry;
class FileSystemStorageManager;
class IDBStorageManager;
class IDBStorageRegistry;
class LocalStorageManager;
class ServiceWorkerStorageManager;
class SessionStorageManager;
class StorageAreaRegistry;

enum class UnifiedOriginStorageLevel : uint8_t;
enum class WebsiteDataType : uint32_t;

class OriginStorageManager : public CanMakeWeakPtr<OriginStorageManager> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static String originFileIdentifier();

    OriginStorageManager(OriginQuotaManager::Parameters&&, String&& path, String&& cusotmLocalStoragePath, String&& customIDBStoragePath, String&& customCacheStoragePath, UnifiedOriginStorageLevel);
    ~OriginStorageManager();

    void connectionClosed(IPC::Connection::UniqueID);
    WebCore::StorageEstimate estimate();
    const String& path() const { return m_path; }
    OriginQuotaManager& quotaManager();
    FileSystemStorageManager& fileSystemStorageManager(FileSystemStorageHandleRegistry&);
    FileSystemStorageManager* existingFileSystemStorageManager();
    LocalStorageManager& localStorageManager(StorageAreaRegistry&);
    LocalStorageManager* existingLocalStorageManager();
    SessionStorageManager& sessionStorageManager(StorageAreaRegistry&);
    SessionStorageManager* existingSessionStorageManager();
    IDBStorageManager& idbStorageManager(IDBStorageRegistry&);
    IDBStorageManager* existingIDBStorageManager();
    CacheStorageManager& cacheStorageManager(CacheStorageRegistry&, const WebCore::ClientOrigin&, Ref<WorkQueue>&&);
    CacheStorageManager* existingCacheStorageManager();
    BackgroundFetchStoreManager& backgroundFetchManager(Ref<WTF::WorkQueue>&&);
    ServiceWorkerStorageManager& serviceWorkerStorageManager();
    uint64_t cacheStorageSize();
    void closeCacheStorageManager();
    String resolvedPath(WebsiteDataType);
    bool isActive();
    bool hasDataInMemory();
    bool isEmpty();
    using DataTypeSizeMap = HashMap<WebsiteDataType, uint64_t, IntHash<WebsiteDataType>, WTF::StrongEnumHashTraits<WebsiteDataType>>;
    DataTypeSizeMap fetchDataTypesInList(OptionSet<WebsiteDataType>, bool shouldComputeSize);
    void deleteData(OptionSet<WebsiteDataType>, WallTime);
    void moveData(OptionSet<WebsiteDataType>, const String& localStoragePath, const String& idbStoragePath);
    void deleteEmptyDirectory();
    std::optional<WallTime> originFileCreationTimestamp() const { return m_originFileCreationTimestamp; }
    void setOriginFileCreationTimestamp(std::optional<WallTime> timestamp) { m_originFileCreationTimestamp = timestamp; }
#if PLATFORM(IOS_FAMILY)
    bool includedInBackup() const { return m_includedInBackup; }
    void markIncludedInBackup() { m_includedInBackup = true; }
#endif

private:
    Ref<OriginQuotaManager> createQuotaManager(OriginQuotaManager::Parameters&&);
    enum class StorageBucketMode : bool;
    class StorageBucket;
    StorageBucket& defaultBucket();

    std::unique_ptr<StorageBucket> m_defaultBucket;
    String m_path;
    String m_customLocalStoragePath;
    String m_customIDBStoragePath;
    String m_customCacheStoragePath;
    Ref<OriginQuotaManager> m_quotaManager;
    UnifiedOriginStorageLevel m_level;
    Markable<WallTime> m_originFileCreationTimestamp;
#if PLATFORM(IOS_FAMILY)
    bool m_includedInBackup { false };
#endif
};

} // namespace WebKit

