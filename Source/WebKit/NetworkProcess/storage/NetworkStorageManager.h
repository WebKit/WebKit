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
#include "FileSystemStorageError.h"
#include "FileSystemSyncAccessHandleInfo.h"
#include "OriginStorageManager.h"
#include "StorageAreaIdentifier.h"
#include "StorageAreaImplIdentifier.h"
#include "StorageAreaMapIdentifier.h"
#include "StorageNamespaceIdentifier.h"
#include "WebPageProxyIdentifier.h"
#include "WebsiteData.h"
#include "WorkQueueMessageReceiver.h"
#include <WebCore/ClientOrigin.h>
#include <WebCore/DOMCacheEngine.h>
#include <WebCore/FileSystemHandleIdentifier.h>
#include <WebCore/FileSystemSyncAccessHandleIdentifier.h>
#include <WebCore/IDBResourceIdentifier.h>
#include <WebCore/IndexedDB.h>
#include <WebCore/ServiceWorkerTypes.h>
#include <pal/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/ThreadSafeWeakHashSet.h>

namespace IPC {
class SharedFileHandle;
}

namespace WebCore {
class IDBCursorInfo;
class IDBKeyData;
class IDBIndexInfo;
class IDBObjectStoreInfo;
class IDBRequestData;
class IDBTransactionInfo;
class IDBValue;
class ServiceWorkerRegistrationKey;
struct ClientOrigin;
struct IDBGetAllRecordsData;
struct IDBGetRecordData;
struct IDBGetAllRecordsData;
struct IDBIterateCursorData;
struct IDBKeyRangeData;
struct RetrieveRecordsOptions;
struct ServiceWorkerContextData;
enum class StorageType : uint8_t;
}

namespace WebKit {

enum class BackgroundFetchChange : uint8_t;
enum class UnifiedOriginStorageLevel : uint8_t;
class FileSystemStorageHandleRegistry;
class IDBStorageRegistry;
class NetworkProcess;
class ServiceWorkerStorageManager;
class StorageAreaBase;
class StorageAreaRegistry;

class NetworkStorageManager final : public IPC::WorkQueueMessageReceiver {
public:
    static Ref<NetworkStorageManager> create(NetworkProcess&, PAL::SessionID, Markable<WTF::UUID>, IPC::Connection::UniqueID, const String& path, const String& customLocalStoragePath, const String& customIDBStoragePath, const String& customCacheStoragePath, const String& customServiceWorkerStoragePath, uint64_t defaultOriginQuota, std::optional<double> originQuotaRatio, std::optional<double> totalQuotaRatio, std::optional<uint64_t> standardVolumeCapacity, std::optional<uint64_t> volumeCapacityOverride, UnifiedOriginStorageLevel);
    static bool canHandleTypes(OptionSet<WebsiteDataType>);
    static OptionSet<WebsiteDataType> allManagedTypes();

    void startReceivingMessageFromConnection(IPC::Connection&);
    void stopReceivingMessageFromConnection(IPC::Connection&);

    PAL::SessionID sessionID() const { return m_sessionID; }
    void close(CompletionHandler<void()>&&);
    void resetStoragePersistedState(CompletionHandler<void()>&&);
    void clearStorageForWebPage(WebPageProxyIdentifier);
    void cloneSessionStorageForWebPage(WebPageProxyIdentifier, WebPageProxyIdentifier);
    void didIncreaseQuota(WebCore::ClientOrigin&&, QuotaIncreaseRequestIdentifier, std::optional<uint64_t> newQuota);
    enum class ShouldComputeSize : bool { No, Yes };
    void fetchData(OptionSet<WebsiteDataType>, ShouldComputeSize, CompletionHandler<void(Vector<WebsiteData::Entry>&&)>&&);
    void deleteData(OptionSet<WebsiteDataType>, const Vector<WebCore::SecurityOriginData>&, CompletionHandler<void()>&&);
    void deleteData(OptionSet<WebsiteDataType>, const WebCore::ClientOrigin&, CompletionHandler<void()>&&);
    void deleteDataModifiedSince(OptionSet<WebsiteDataType>, WallTime, CompletionHandler<void()>&&);
    void deleteDataForRegistrableDomains(OptionSet<WebsiteDataType>, const Vector<WebCore::RegistrableDomain>&, CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&&);
    void moveData(OptionSet<WebsiteDataType>, WebCore::SecurityOriginData&& source, WebCore::SecurityOriginData&& target, CompletionHandler<void()>&&);
    void getOriginDirectory(WebCore::ClientOrigin&&, WebsiteDataType, CompletionHandler<void(const String&)>&&);
    void suspend(CompletionHandler<void()>&&);
    void resume();
    void handleLowMemoryWarning();
    void syncLocalStorage(CompletionHandler<void()>&&);
    void registerTemporaryBlobFilePaths(IPC::Connection&, const Vector<String>&);
    void requestSpace(const WebCore::ClientOrigin&, uint64_t size, CompletionHandler<void(bool)>&&);
    void resetQuotaForTesting(CompletionHandler<void()>&&);
    void resetQuotaUpdatedBasedOnUsageForTesting(WebCore::ClientOrigin&&);
#if PLATFORM(IOS_FAMILY)
    void setBackupExclusionPeriodForTesting(Seconds, CompletionHandler<void()>&&);
#endif

#if ENABLE(SERVICE_WORKER)
    void dispatchTaskToBackgroundFetchManager(const WebCore::ClientOrigin&, Function<void(BackgroundFetchStoreManager*)>&&);
    void notifyBackgroundFetchChange(const String&, BackgroundFetchChange);
    void closeServiceWorkerRegistrationFiles(CompletionHandler<void()>&&);
    void clearServiceWorkerRegistrations(CompletionHandler<void()>&&);
    void importServiceWorkerRegistrations(CompletionHandler<void(std::optional<Vector<WebCore::ServiceWorkerContextData>>)>&&);
    void updateServiceWorkerRegistrations(Vector<WebCore::ServiceWorkerContextData>&&, Vector<WebCore::ServiceWorkerRegistrationKey>&&, CompletionHandler<void(std::optional<Vector<WebCore::ServiceWorkerScripts>>)>&&);
#endif // ENABLE(SERVICE_WORKER)

private:
    NetworkStorageManager(NetworkProcess&, PAL::SessionID, Markable<WTF::UUID>, IPC::Connection::UniqueID, const String& path, const String& customLocalStoragePath, const String& customIDBStoragePath, const String& customCacheStoragePath, const String& customServiceWorkerStoragePath, uint64_t defaultOriginQuota, std::optional<double> originQuotaRatio, std::optional<double> totalQuotaRatio, std::optional<uint64_t> standardVolumeCapacity, std::optional<uint64_t> volumeCapacityOverride, UnifiedOriginStorageLevel);
    ~NetworkStorageManager();
    void writeOriginToFileIfNecessary(const WebCore::ClientOrigin&, StorageAreaBase* = nullptr);
    enum class ShouldWriteOriginFile : bool { No, Yes };
    OriginStorageManager& originStorageManager(const WebCore::ClientOrigin&, ShouldWriteOriginFile = ShouldWriteOriginFile::Yes);
    bool removeOriginStorageManagerIfPossible(const WebCore::ClientOrigin&);

    void forEachOriginDirectory(const Function<void(const String&)>&);
    HashSet<WebCore::ClientOrigin> getAllOrigins();
    Vector<WebsiteData::Entry> fetchDataFromDisk(OptionSet<WebsiteDataType>, ShouldComputeSize);
    HashSet<WebCore::ClientOrigin> deleteDataOnDisk(OptionSet<WebsiteDataType>, WallTime, const Function<bool(const WebCore::ClientOrigin&)>&);
#if PLATFORM(IOS_FAMILY)
    void includeOriginInBackupIfNecessary(OriginStorageManager&);
#endif

    // IPC::MessageReceiver (implemented by generated code)
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>& replyEncoder);

    // Message handlers for FileSystem.
    void persisted(const WebCore::ClientOrigin&, CompletionHandler<void(bool)>&&);
    void persist(const WebCore::ClientOrigin&, CompletionHandler<void(bool)>&&);
    void estimate(const WebCore::ClientOrigin&, CompletionHandler<void(std::optional<WebCore::StorageEstimate>)>&&);
    void fileSystemGetDirectory(IPC::Connection&, WebCore::ClientOrigin&&, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&&);
    void closeHandle(WebCore::FileSystemHandleIdentifier);
    void isSameEntry(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemHandleIdentifier, CompletionHandler<void(bool)>&&);
    void move(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemHandleIdentifier, const String& newName, CompletionHandler<void(std::optional<FileSystemStorageError>)>&&);
    void getFileHandle(IPC::Connection&, WebCore::FileSystemHandleIdentifier, String&& name, bool createIfNecessary, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&&);
    void getDirectoryHandle(IPC::Connection&, WebCore::FileSystemHandleIdentifier, String&& name, bool createIfNecessary, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&&);
    void removeEntry(WebCore::FileSystemHandleIdentifier, const String& name, bool deleteRecursively, CompletionHandler<void(std::optional<FileSystemStorageError>)>&&);
    void resolve(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemHandleIdentifier, CompletionHandler<void(Expected<Vector<String>, FileSystemStorageError>)>&&);
    void getFile(WebCore::FileSystemHandleIdentifier, CompletionHandler<void(Expected<String, FileSystemStorageError>)>&&);
    void createSyncAccessHandle(WebCore::FileSystemHandleIdentifier, CompletionHandler<void(Expected<FileSystemSyncAccessHandleInfo, FileSystemStorageError>)>&&);
    void closeSyncAccessHandle(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemSyncAccessHandleIdentifier, CompletionHandler<void()>&&);
    void requestNewCapacityForSyncAccessHandle(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemSyncAccessHandleIdentifier, uint64_t newCapacity, CompletionHandler<void(std::optional<uint64_t>)>&&);
    void getHandleNames(WebCore::FileSystemHandleIdentifier, CompletionHandler<void(Expected<Vector<String>, FileSystemStorageError>)>&&);
    void getHandle(IPC::Connection&, WebCore::FileSystemHandleIdentifier, String&& name, CompletionHandler<void(Expected<std::pair<WebCore::FileSystemHandleIdentifier, bool>, FileSystemStorageError>)>&&);
    
    // Message handlers for WebStorage.
    void connectToStorageArea(IPC::Connection&, WebCore::StorageType, StorageAreaMapIdentifier, std::optional<StorageNamespaceIdentifier>, const WebCore::ClientOrigin&, CompletionHandler<void(StorageAreaIdentifier, HashMap<String, String>, uint64_t)>&&);
    void connectToStorageAreaSync(IPC::Connection&, WebCore::StorageType, StorageAreaMapIdentifier, std::optional<StorageNamespaceIdentifier>, const WebCore::ClientOrigin&, CompletionHandler<void(StorageAreaIdentifier, HashMap<String, String>, uint64_t)>&&);
    void cancelConnectToStorageArea(IPC::Connection&, WebCore::StorageType, std::optional<StorageNamespaceIdentifier>, const WebCore::ClientOrigin&);
    void disconnectFromStorageArea(IPC::Connection&, StorageAreaIdentifier);
    void cloneSessionStorageNamespace(StorageNamespaceIdentifier, StorageNamespaceIdentifier);
    void setItem(IPC::Connection&, StorageAreaIdentifier, StorageAreaImplIdentifier, String&& key, String&& value, String&& urlString, CompletionHandler<void(bool, HashMap<String, String>&&)>&&);
    void removeItem(IPC::Connection&, StorageAreaIdentifier, StorageAreaImplIdentifier, String&& key, String&& urlString, CompletionHandler<void(bool, HashMap<String, String>&&)>&&);
    void clear(IPC::Connection&, StorageAreaIdentifier, StorageAreaImplIdentifier, String&& urlString, CompletionHandler<void()>&&);

    // Message handlers for IndexedDB.
    void openDatabase(IPC::Connection&, const WebCore::IDBRequestData&);
    void openDBRequestCancelled(const WebCore::IDBRequestData&);
    void deleteDatabase(IPC::Connection&, const WebCore::IDBRequestData&);
    void establishTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBTransactionInfo&);
    void databaseConnectionPendingClose(uint64_t databaseConnectionIdentifier);
    void databaseConnectionClosed(uint64_t databaseConnectionIdentifier);
    void abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const std::optional<WebCore::IDBResourceIdentifier>& transactionIdentifier);
    void didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& requestIdentifier, const WebCore::IndexedDB::ConnectionClosedOnBehalfOfServer);
    void abortTransaction(const WebCore::IDBResourceIdentifier&);
    void commitTransaction(const WebCore::IDBResourceIdentifier&, uint64_t pendingRequestCount);
    void didFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier&);
    void createObjectStore(const WebCore::IDBRequestData&, const WebCore::IDBObjectStoreInfo&);
    void deleteObjectStore(const WebCore::IDBRequestData&, const String& objectStoreName);
    void renameObjectStore(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier, const String& newName);
    void clearObjectStore(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier);
    void createIndex(const WebCore::IDBRequestData&, const WebCore::IDBIndexInfo&);
    void deleteIndex(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier, const String& indexName);
    void renameIndex(const WebCore::IDBRequestData&, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName);
    void putOrAdd(IPC::Connection&, const WebCore::IDBRequestData&, const WebCore::IDBKeyData&, const WebCore::IDBValue&, WebCore::IndexedDB::ObjectStoreOverwriteMode);
    void getRecord(const WebCore::IDBRequestData&, const WebCore::IDBGetRecordData&);
    void getAllRecords(const WebCore::IDBRequestData&, const WebCore::IDBGetAllRecordsData&);
    void getCount(const WebCore::IDBRequestData&, const WebCore::IDBKeyRangeData&);
    void deleteRecord(const WebCore::IDBRequestData&, const WebCore::IDBKeyRangeData&);
    void openCursor(const WebCore::IDBRequestData&, const WebCore::IDBCursorInfo&);
    void iterateCursor(const WebCore::IDBRequestData&, const WebCore::IDBIterateCursorData&);
    void getAllDatabaseNamesAndVersions(IPC::Connection&, const WebCore::IDBResourceIdentifier&, const WebCore::ClientOrigin&);

    // Message handlers for CacheStorage.
    void cacheStorageOpenCache(const WebCore::ClientOrigin&, const String& cacheName, WebCore::DOMCacheEngine::CacheIdentifierCallback&&);
    void cacheStorageRemoveCache(WebCore::DOMCacheIdentifier, WebCore::DOMCacheEngine::RemoveCacheIdentifierCallback&&);
    void cacheStorageAllCaches(const WebCore::ClientOrigin&, uint64_t updateCounter, WebCore::DOMCacheEngine::CacheInfosCallback&&);
    void cacheStorageReference(IPC::Connection&, WebCore::DOMCacheIdentifier);
    void cacheStorageDereference(IPC::Connection&, WebCore::DOMCacheIdentifier);
    void cacheStorageRetrieveRecords(WebCore::DOMCacheIdentifier, WebCore::RetrieveRecordsOptions&&, WebCore::DOMCacheEngine::CrossThreadRecordsCallback&&);
    void cacheStorageRemoveRecords(WebCore::DOMCacheIdentifier, WebCore::ResourceRequest&&, WebCore::CacheQueryOptions&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);
    void cacheStoragePutRecords(WebCore::DOMCacheIdentifier, Vector<WebCore::DOMCacheEngine::CrossThreadRecord>&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);
    void cacheStorageClearMemoryRepresentation(const WebCore::ClientOrigin&, CompletionHandler<void(std::optional<WebCore::DOMCacheEngine::Error>&&)>&&);
    void cacheStorageRepresentation(CompletionHandler<void(String&&)>&&);

#if ENABLE(SERVICE_WORKER)
    bool shouldManageServiceWorkerRegistrationsByOrigin();
    void migrateServiceWorkerRegistrationsToOrigins();
    Vector<WebCore::ServiceWorkerScripts> updateServiceWorkerRegistrationsByOrigin(Vector<WebCore::ServiceWorkerContextData>&&, Vector<WebCore::ServiceWorkerRegistrationKey>&&);
#endif

    void spaceGrantedForOrigin(const WebCore::ClientOrigin&, uint64_t amount);
    void prepareForEviction();
    void donePrepareForEviction(const std::optional<HashMap<WebCore::RegistrableDomain, WallTime>>&);
    WallTime lastModificationTimeForOrigin(const WebCore::ClientOrigin&, OriginStorageManager&) const;
    void updateLastModificationTimeForOrigin(const WebCore::ClientOrigin&);
    void schedulePerformEviction();
    bool persistedInternal(const WebCore::ClientOrigin&);
    String persistedFilePath(const WebCore::ClientOrigin&);
    void fetchRegistrableDomainsForPersist();
    void didFetchRegistrableDomainsForPersist(HashSet<WebCore::RegistrableDomain>&&);
    bool persistOrigin(const WebCore::ClientOrigin&);
    struct AccessRecord {
        bool isActive { false };
        std::optional<bool> isPersisted;
        uint64_t usage { 0 };
        WallTime lastAccessTime;
        Vector<WebCore::SecurityOriginData> clientOrigins;
    };
    void performEviction(HashMap<WebCore::SecurityOriginData, AccessRecord>&&);
    SuspendableWorkQueue& workQueue() WTF_RETURNS_CAPABILITY(m_queue.get()) { return m_queue; }

    WeakPtr<NetworkProcess> m_process;
    PAL::SessionID m_sessionID;
    String m_queueName;
    Ref<SuspendableWorkQueue> m_queue;
    String m_path;
    FileSystem::Salt m_salt;
    bool m_closed { false };
    HashMap<WebCore::ClientOrigin, std::unique_ptr<OriginStorageManager>> m_originStorageManagers WTF_GUARDED_BY_CAPABILITY(workQueue());
    ThreadSafeWeakHashSet<IPC::Connection> m_connections;
    std::unique_ptr<FileSystemStorageHandleRegistry> m_fileSystemStorageHandleRegistry;
    std::unique_ptr<StorageAreaRegistry> m_storageAreaRegistry;
    std::unique_ptr<IDBStorageRegistry> m_idbStorageRegistry;
    std::unique_ptr<CacheStorageRegistry> m_cacheStorageRegistry;
    String m_customLocalStoragePath;
    String m_customIDBStoragePath;
    String m_customCacheStoragePath;
    String m_customServiceWorkerStoragePath;
    uint64_t m_defaultOriginQuota;
    std::optional<double> m_originQuotaRatio;
    std::optional<double> m_totalQuotaRatio;
    std::optional<uint64_t> m_standardVolumeCapacity;
    std::optional<uint64_t> m_volumeCapacityOverride;
    std::optional<uint64_t> m_totalUsage;
    uint64_t m_totalQuota;
    bool m_isEvictionScheduled { false };
    UnifiedOriginStorageLevel m_unifiedOriginStorageLevel;
    IPC::Connection::UniqueID m_parentConnection;
    HashMap<IPC::Connection::UniqueID, HashSet<String>> m_temporaryBlobPathsByConnection WTF_GUARDED_BY_CAPABILITY(workQueue());
    using OriginPersistCompletionHandler = std::pair<WebCore::ClientOrigin, CompletionHandler<void(bool)>>;
    Vector<OriginPersistCompletionHandler> m_persistCompletionHandlers WTF_GUARDED_BY_CAPABILITY(workQueue());
    std::optional<HashSet<WebCore::RegistrableDomain>> m_domainsExemptFromEviction WTF_GUARDED_BY_CAPABILITY(workQueue());
#if PLATFORM(IOS_FAMILY)
    Seconds m_backupExclusionPeriod;
#endif
#if ENABLE(SERVICE_WORKER)
    std::unique_ptr<ServiceWorkerStorageManager> m_sharedServiceWorkerStorageManager WTF_GUARDED_BY_CAPABILITY(workQueue());
#endif
};

} // namespace WebKit
