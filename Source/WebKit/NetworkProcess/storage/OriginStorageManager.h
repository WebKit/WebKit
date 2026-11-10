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
#include <wtf/TZoneMalloc.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
class OriginStorageManager;
}

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

class OriginStorageManager final : public CanMakeWeakPtr<OriginStorageManager>, public CanMakeThreadSafeCheckedPtr<OriginStorageManager> {
    WTF_MAKE_TZONE_ALLOCATED(OriginStorageManager);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(OriginStorageManager);
public:
    static RefPtr<StringImpl> originFileIdentifier();

    OriginStorageManager(OriginQuotaManager::Parameters&&, RefPtr<StringImpl>&& path, RefPtr<StringImpl>&& cusotmLocalStoragePath, RefPtr<StringImpl>&& customIDBStoragePath, RefPtr<StringImpl>&& customCacheStoragePath, UnifiedOriginStorageLevel);
    ~OriginStorageManager();

    void connectionClosed(IPC::Connection::UniqueID);
    WebCore::StorageEstimate estimate();
    const RefPtr<StringImpl>& path() const LIFETIME_BOUND { return m_path; }
    NODELETE OriginQuotaManager& quotaManager();
    FileSystemStorageManager& fileSystemStorageManager(FileSystemStorageHandleRegistry&, const WebCore::ClientOrigin&);
    FileSystemStorageManager* existingFileSystemStorageManager();
    LocalStorageManager& localStorageManager(StorageAreaRegistry&) LIFETIME_BOUND;
    LocalStorageManager* existingLocalStorageManager() LIFETIME_BOUND;
    SessionStorageManager& sessionStorageManager(StorageAreaRegistry&) LIFETIME_BOUND;
    SessionStorageManager* existingSessionStorageManager() LIFETIME_BOUND;
    IDBStorageManager& idbStorageManager(IDBStorageRegistry&, bool useSQLiteMemoryBackingStore);
    IDBStorageManager* existingIDBStorageManager();
    CacheStorageManager& cacheStorageManager(CacheStorageRegistry&, const WebCore::ClientOrigin&, Ref<WorkQueue>&&);
    CacheStorageManager* existingCacheStorageManager();
    BackgroundFetchStoreManager& backgroundFetchManager(Ref<WTF::WorkQueue>&&);
    ServiceWorkerStorageManager& serviceWorkerStorageManager() LIFETIME_BOUND;
    uint64_t cacheStorageSize();
    void closeCacheStorageManager();
    RefPtr<StringImpl> resolvedPath(WebsiteDataType);
    bool isActive();
    bool hasDataInMemory();
    bool isEmpty();
    using DataTypeSizeMap = HashMap<WebsiteDataType, uint64_t, IntHash<WebsiteDataType>, WTF::StrongEnumHashTraits<WebsiteDataType>>;
    DataTypeSizeMap fetchDataTypesInList(OptionSet<WebsiteDataType>, bool shouldComputeSize);
    void deleteData(OptionSet<WebsiteDataType>, WallTime);
    void moveData(OptionSet<WebsiteDataType>, StringImpl* localStoragePath, StringImpl* idbStoragePath);
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
    StorageBucket& defaultBucket() LIFETIME_BOUND;

    const std::unique_ptr<StorageBucket> m_defaultBucket;
    RefPtr<StringImpl> m_path;
    RefPtr<StringImpl> m_customLocalStoragePath;
    RefPtr<StringImpl> m_customIDBStoragePath;
    RefPtr<StringImpl> m_customCacheStoragePath;
    const Ref<OriginQuotaManager> m_quotaManager;
    UnifiedOriginStorageLevel m_level;
    Markable<WallTime> m_originFileCreationTimestamp;
#if PLATFORM(IOS_FAMILY)
    bool m_includedInBackup { false };
#endif
};

} // namespace WebKit

