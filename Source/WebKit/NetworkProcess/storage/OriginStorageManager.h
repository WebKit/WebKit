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
#include "QuotaManager.h"
#include "StorageAreaIdentifier.h"
#include "StorageAreaImplIdentifier.h"
#include "StorageAreaMapIdentifier.h"
#include "StorageNamespaceIdentifier.h"
#include "WebsiteDataType.h"
#include <WebCore/ClientOrigin.h>
#include <WebCore/DOMCacheEngine.h>
#include <WebCore/FileSystemHandleIdentifier.h>
#include <WebCore/FileSystemSyncAccessHandleIdentifier.h>
#include <WebCore/IDBResourceIdentifier.h>
#include <WebCore/IndexedDB.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class IDBCursorInfo;
class IDBKeyData;
class IDBIndexInfo;
class IDBObjectStoreInfo;
class IDBRequestData;
class IDBTransactionInfo;
class IDBValue;
struct IDBGetAllRecordsData;
struct IDBGetRecordData;
struct IDBGetAllRecordsData;
struct IDBIterateCursorData;
struct IDBKeyRangeData;
struct RetrieveRecordsOptions;
struct StorageEstimate;
enum class StorageType : uint8_t;
}

namespace WebKit {

class CacheStorageManager;
class CacheStorageRegistry;
class FileSystemStorageHandleRegistry;
class FileSystemStorageManager;
class IDBStorageManager;
class IDBStorageRegistry;
class LocalStorageManager;
class SessionStorageManager;
class StorageAreaRegistry;
enum class UnifiedOriginStorageLevel : uint8_t;
enum class WebsiteDataType : uint32_t;

class OriginStorageManager : public CanMakeWeakPtr<OriginStorageManager> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static String originFileIdentifier();

    OriginStorageManager(uint64_t quota, QuotaManager::IncreaseQuotaFunction&&, const WebCore::ClientOrigin&, String&& path, String&& cusotmLocalStoragePath, String&& customIDBStoragePath, String&& customCacheStoragePath, UnifiedOriginStorageLevel);
    ~OriginStorageManager();

    String resolvedPath(WebsiteDataType);
    bool isActive();

    void connectionClosed(IPC::Connection::UniqueID);
    bool persisted() const { return m_persisted; }
    void setPersisted(bool value);
    WebCore::StorageEstimate estimate();
    void didIncreaseQuota(QuotaIncreaseRequestIdentifier, std::optional<uint64_t> newQuota);
    void resetQuotaForTesting();
    void resetQuotaUpdatedBasedOnUsageForTesting();
    void requestSpace(uint64_t size, CompletionHandler<void(bool)>&&);
    void suspend();
    void handleLowMemoryWarning();
    void clearSessionStorage(StorageNamespaceIdentifier);
    void syncLocalStorage();
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

    // FileSystem
    void fileSystemGetDirectory(IPC::Connection&, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&&);
    void fileSystemCloseHandle(WebCore::FileSystemHandleIdentifier);
    void fileSystemIsSameEntry(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemHandleIdentifier, CompletionHandler<void(bool)>&&);
    void fileSystemMove(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemHandleIdentifier, const String& newName, CompletionHandler<void(std::optional<FileSystemStorageError>)>&&);
    void fileSystemGetFileHandle(IPC::Connection&, WebCore::FileSystemHandleIdentifier, String&& name, bool createIfNecessary, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&&);
    void fileSystemGetDirectoryHandle(IPC::Connection&, WebCore::FileSystemHandleIdentifier, String&& name, bool createIfNecessary, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&&);
    void fileSystemRemoveEntry(WebCore::FileSystemHandleIdentifier, const String& name, bool deleteRecursively, CompletionHandler<void(std::optional<FileSystemStorageError>)>&&);
    void fileSystemResolve(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemHandleIdentifier, CompletionHandler<void(Expected<Vector<String>, FileSystemStorageError>)>&&);
    void fileSystemGetFile(WebCore::FileSystemHandleIdentifier, CompletionHandler<void(Expected<String, FileSystemStorageError>)>&&);
    void fileSystemCreateSyncAccessHandle(WebCore::FileSystemHandleIdentifier, CompletionHandler<void(Expected<FileSystemSyncAccessHandleInfo, FileSystemStorageError>)>&&);
    void fileSystemCloseSyncAccessHandle(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemSyncAccessHandleIdentifier, CompletionHandler<void()>&&);
    void fileSystemRequestNewCapacityForSyncAccessHandle(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemSyncAccessHandleIdentifier, uint64_t newCapacity, CompletionHandler<void(std::optional<uint64_t>)>&&);
    void fileSystemGetHandleNames(WebCore::FileSystemHandleIdentifier, CompletionHandler<void(Expected<Vector<String>, FileSystemStorageError>)>&&);
    void fileSystemGetHandle(IPC::Connection&, WebCore::FileSystemHandleIdentifier, String&& name, CompletionHandler<void(Expected<std::pair<WebCore::FileSystemHandleIdentifier, bool>, FileSystemStorageError>)>&&);

    // WebStorage
    void webStorageConnectToStorageArea(IPC::Connection&, WebCore::StorageType, StorageAreaMapIdentifier, std::optional<StorageNamespaceIdentifier>, Ref<WorkQueue>&&, CompletionHandler<void(StorageAreaIdentifier, HashMap<String, String>, uint64_t)>&&);
    void webStorageConnectToStorageAreaSync(IPC::Connection&, WebCore::StorageType, StorageAreaMapIdentifier, std::optional<StorageNamespaceIdentifier>, Ref<WorkQueue>&&, CompletionHandler<void(StorageAreaIdentifier, HashMap<String, String>, uint64_t)>&&);
    void webStorageCancelConnectToStorageArea(IPC::Connection&, WebCore::StorageType, std::optional<StorageNamespaceIdentifier>);
    void webStorageDisconnectFromStorageArea(IPC::Connection&, StorageAreaIdentifier);
    void webStorageCloneSessionStorageNamespace(StorageNamespaceIdentifier, StorageNamespaceIdentifier);
    void webStorageSetItem(IPC::Connection&, StorageAreaIdentifier, StorageAreaImplIdentifier, String&& key, String&& value, String&& urlString, CompletionHandler<void(bool, HashMap<String, String>&&)>&&);
    void webStorageRemoveItem(IPC::Connection&, StorageAreaIdentifier, StorageAreaImplIdentifier, String&& key, String&& urlString, CompletionHandler<void()>&&);
    void webStorageClear(IPC::Connection&, StorageAreaIdentifier, StorageAreaImplIdentifier, String&& urlString, CompletionHandler<void()>&&);

    // IndexedDB
    void idbOpenDatabase(IPC::Connection&, const WebCore::IDBRequestData&);
    void idbOpenDBRequestCancelled(const WebCore::IDBRequestData&);
    void idbDeleteDatabase(IPC::Connection&, const WebCore::IDBRequestData&);
    void idbEstablishTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBTransactionInfo&);
    void idbDatabaseConnectionPendingClose(uint64_t databaseConnectionIdentifier);
    void idbDatabaseConnectionClosed(uint64_t databaseConnectionIdentifier);
    void idbAbortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const std::optional<WebCore::IDBResourceIdentifier>& transactionIdentifier);
    void idbDidFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& requestIdentifier, const WebCore::IndexedDB::ConnectionClosedOnBehalfOfServer);
    void idbAbortTransaction(const WebCore::IDBResourceIdentifier&);
    void idbCommitTransaction(const WebCore::IDBResourceIdentifier&, uint64_t pendingRequestCount);
    void idbDidFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier&);
    void idbCreateObjectStore(const WebCore::IDBRequestData&, const WebCore::IDBObjectStoreInfo&);
    void idbDeleteObjectStore(const WebCore::IDBRequestData&, const String& objectStoreName);
    void idbRenameObjectStore(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier, const String& newName);
    void idbClearObjectStore(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier);
    void idbCreateIndex(const WebCore::IDBRequestData&, const WebCore::IDBIndexInfo&);
    void idbDeleteIndex(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier, const String& indexName);
    void idbRenameIndex(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName);
    void idbPutOrAdd(IPC::Connection&, const WebCore::IDBRequestData&, const WebCore::IDBKeyData&, const WebCore::IDBValue&, WebCore::IndexedDB::ObjectStoreOverwriteMode);
    void idbGetRecord(const WebCore::IDBRequestData&, const WebCore::IDBGetRecordData&);
    void idbGetAllRecords(const WebCore::IDBRequestData&, const WebCore::IDBGetAllRecordsData&);
    void idbGetCount(const WebCore::IDBRequestData&, const WebCore::IDBKeyRangeData&);
    void idbDeleteRecord(const WebCore::IDBRequestData&, const WebCore::IDBKeyRangeData&);
    void idbOpenCursor(const WebCore::IDBRequestData&, const WebCore::IDBCursorInfo&);
    void idbIterateCursor(const WebCore::IDBRequestData&, const WebCore::IDBIterateCursorData&);
    void idbGetAllDatabaseNamesAndVersions(IPC::Connection&, const WebCore::IDBResourceIdentifier&);

    // CacheStorage
    void cacheStorageOpenCache(Ref<WorkQueue>&&, const String& cacheName, WebCore::DOMCacheEngine::CacheIdentifierCallback&&);
    void cacheStorageRemoveCache(WebCore::DOMCacheIdentifier, WebCore::DOMCacheEngine::RemoveCacheIdentifierCallback&&);
    void cacheStorageAllCaches(Ref<WorkQueue>&&, uint64_t updateCounter, WebCore::DOMCacheEngine::CacheInfosCallback&&);
    void cacheStorageReference(IPC::Connection&, WebCore::DOMCacheIdentifier);
    void cacheStorageDereference(IPC::Connection&, WebCore::DOMCacheIdentifier);
    void cacheStorageRetrieveRecords(WebCore::DOMCacheIdentifier, WebCore::RetrieveRecordsOptions&&, WebCore::DOMCacheEngine::RecordsCallback&&);
    void cacheStorageRemoveRecords(WebCore::DOMCacheIdentifier, WebCore::ResourceRequest&&, WebCore::CacheQueryOptions&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);
    void cacheStoragePutRecords(WebCore::DOMCacheIdentifier, Vector<WebCore::DOMCacheEngine::Record>&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);
    void cacheStorageClearMemoryRepresentation(CompletionHandler<void(std::optional<WebCore::DOMCacheEngine::Error>&&)>&& callback);
    String cacheStorageRepresentation(Ref<WorkQueue>&&);

private:
    Ref<QuotaManager> createQuotaManager();
    enum class StorageBucketMode : bool;
    class StorageBucket;
    StorageBucket& defaultBucket();
    QuotaManager& quotaManager();
    FileSystemStorageManager& fileSystemStorageManager();
    FileSystemStorageManager* existingFileSystemStorageManager();
    LocalStorageManager& localStorageManager();
    LocalStorageManager* existingLocalStorageManager();
    SessionStorageManager& sessionStorageManager();
    SessionStorageManager* existingSessionStorageManager();
    IDBStorageManager& idbStorageManager();
    IDBStorageManager* existingIDBStorageManager();
    CacheStorageManager& cacheStorageManager(Ref<WorkQueue>&&);
    CacheStorageManager* existingCacheStorageManager();
    void closeCacheStorageManager();

    std::unique_ptr<StorageBucket> m_defaultBucket;
    std::unique_ptr<FileSystemStorageHandleRegistry> m_fileSystemStorageHandleRegistry;
    std::unique_ptr<StorageAreaRegistry> m_storageAreaRegistry;
    std::unique_ptr<IDBStorageRegistry> m_idbStorageRegistry;
    std::unique_ptr<CacheStorageRegistry> m_cacheStorageRegistry;
    WebCore::ClientOrigin m_origin;
    String m_path;
    String m_customLocalStoragePath;
    String m_customIDBStoragePath;
    String m_customCacheStoragePath;
    uint64_t m_quota;
    QuotaManager::IncreaseQuotaFunction m_increaseQuotaFunction;
    RefPtr<QuotaManager> m_quotaManager;
    bool m_persisted { false };
    UnifiedOriginStorageLevel m_level;
    Markable<WallTime> m_originFileCreationTimestamp;
#if PLATFORM(IOS_FAMILY)
    bool m_includedInBackup { false };
#endif
};

} // namespace WebKit

