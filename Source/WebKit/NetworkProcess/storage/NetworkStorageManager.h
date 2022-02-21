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
#include "OriginStorageManager.h"
#include "StorageAreaIdentifier.h"
#include "StorageAreaImplIdentifier.h"
#include "StorageAreaMapIdentifier.h"
#include "StorageNamespaceIdentifier.h"
#include "WebsiteData.h"
#include <WebCore/ClientOrigin.h>
#include <WebCore/FileSystemHandleIdentifier.h>
#include <WebCore/FileSystemSyncAccessHandleIdentifier.h>
#include <WebCore/IDBResourceIdentifier.h>
#include <WebCore/IndexedDB.h>
#include <pal/SessionID.h>
#include <wtf/Forward.h>

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
struct ClientOrigin;
struct IDBGetAllRecordsData;
struct IDBGetRecordData;
struct IDBGetAllRecordsData;
struct IDBIterateCursorData;
struct IDBKeyRangeData;
enum class StorageType : uint8_t;
}

namespace WebKit {

class FileSystemStorageHandleRegistry;
class IDBStorageRegistry;
class StorageAreaRegistry;

class NetworkStorageManager final : public IPC::Connection::WorkQueueMessageReceiver {
public:
    static Ref<NetworkStorageManager> create(PAL::SessionID, IPC::Connection::UniqueID, const String& path, const String& customLocalStoragePath, const String& customIDBStoragePath, const String& customCacheStoragePath, uint64_t defaultOriginQuota, uint64_t defaultThirdPartyOriginQuota, bool shouldUseCustomPaths);
    static bool canHandleTypes(OptionSet<WebsiteDataType>);

    void startReceivingMessageFromConnection(IPC::Connection&);
    void stopReceivingMessageFromConnection(IPC::Connection&);

    PAL::SessionID sessionID() const { return m_sessionID; }
    void close();
    void clearStorageForTesting(CompletionHandler<void()>&&);
    void didIncreaseQuota(const WebCore::ClientOrigin&, QuotaIncreaseRequestIdentifier, std::optional<uint64_t> newQuota);
    void fetchData(OptionSet<WebsiteDataType>, CompletionHandler<void(Vector<WebsiteData::Entry>&&)>&&);
    void deleteData(OptionSet<WebsiteDataType>, const Vector<WebCore::SecurityOriginData>&, CompletionHandler<void()>&&);
    void deleteDataModifiedSince(OptionSet<WebsiteDataType>, WallTime, CompletionHandler<void()>&&);
    void deleteDataForRegistrableDomains(OptionSet<WebsiteDataType>, const Vector<WebCore::RegistrableDomain>&, CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&&);
    void moveData(OptionSet<WebsiteDataType>, const WebCore::SecurityOriginData& source, const WebCore::SecurityOriginData& target, CompletionHandler<void()>&&);
    void suspend(CompletionHandler<void()>&&);
    void resume();
    void handleLowMemoryWarning();
    void syncLocalStorage(CompletionHandler<void()>&&);
    void registerTemporaryBlobFilePaths(IPC::Connection&, const Vector<String>&);
    void requestSpace(const WebCore::ClientOrigin&, uint64_t size, CompletionHandler<void(bool)>&&);
    void resetQuotaForTesting(CompletionHandler<void()>&&);
    void resetQuotaUpdatedBasedOnUsageForTesting(const WebCore::ClientOrigin&);

private:
    NetworkStorageManager(PAL::SessionID, IPC::Connection::UniqueID, const String& path, const String& customLocalStoragePath, const String& customIDBStoragePath, const String& customCacheStoragePath, uint64_t defaultOriginQuota, uint64_t defaultThirdPartyOriginQuota, bool shouldUseCustomPaths);
    ~NetworkStorageManager();
    enum class ShouldWriteOriginFile : bool { No, Yes };
    OriginStorageManager& localOriginStorageManager(const WebCore::ClientOrigin&, ShouldWriteOriginFile = ShouldWriteOriginFile::Yes);
    bool removeOriginStorageManagerIfPossible(const WebCore::ClientOrigin&);
    void deleteOriginDirectoryIfPossible(const WebCore::ClientOrigin&);

    void forEachOriginDirectory(const Function<void(const String&)>&);
    HashSet<WebCore::ClientOrigin> getAllOrigins();
    Vector<WebsiteData::Entry> fetchDataFromDisk(OptionSet<WebsiteDataType>);
    HashSet<WebCore::ClientOrigin> deleteDataOnDisk(OptionSet<WebsiteDataType>, WallTime, const Function<bool(const WebCore::ClientOrigin&)>&);

    // IPC::MessageReceiver (implemented by generated code)
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>& replyEncoder);

    // Message handlers for FileSystem.
    void persisted(const WebCore::ClientOrigin&, CompletionHandler<void(bool)>&&);
    void persist(const WebCore::ClientOrigin&, CompletionHandler<void(bool)>&&);
    void fileSystemGetDirectory(IPC::Connection&, const WebCore::ClientOrigin&, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&&);
    void closeHandle(WebCore::FileSystemHandleIdentifier);
    void isSameEntry(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemHandleIdentifier, CompletionHandler<void(bool)>&&);
    void move(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemHandleIdentifier, const String& newName, CompletionHandler<void(std::optional<FileSystemStorageError>)>&&);
    void getFileHandle(IPC::Connection&, WebCore::FileSystemHandleIdentifier, String&& name, bool createIfNecessary, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&&);
    void getDirectoryHandle(IPC::Connection&, WebCore::FileSystemHandleIdentifier, String&& name, bool createIfNecessary, CompletionHandler<void(Expected<WebCore::FileSystemHandleIdentifier, FileSystemStorageError>)>&&);
    void removeEntry(WebCore::FileSystemHandleIdentifier, const String& name, bool deleteRecursively, CompletionHandler<void(std::optional<FileSystemStorageError>)>&&);
    void resolve(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemHandleIdentifier, CompletionHandler<void(Expected<Vector<String>, FileSystemStorageError>)>&&);
    void getFile(WebCore::FileSystemHandleIdentifier, CompletionHandler<void(Expected<String, FileSystemStorageError>)>&&);
    using AccessHandleInfo = std::pair<WebCore::FileSystemSyncAccessHandleIdentifier, IPC::SharedFileHandle>;
    void createSyncAccessHandle(WebCore::FileSystemHandleIdentifier, CompletionHandler<void(Expected<AccessHandleInfo, FileSystemStorageError>)>&&);
    void closeSyncAccessHandle(WebCore::FileSystemHandleIdentifier, WebCore::FileSystemSyncAccessHandleIdentifier, CompletionHandler<void(std::optional<FileSystemStorageError>)>&&);
    void getHandleNames(WebCore::FileSystemHandleIdentifier, CompletionHandler<void(Expected<Vector<String>, FileSystemStorageError>)>&&);
    void getHandle(IPC::Connection&, WebCore::FileSystemHandleIdentifier, String&& name, CompletionHandler<void(Expected<std::pair<WebCore::FileSystemHandleIdentifier, bool>, FileSystemStorageError>)>&&);
    
    // Message handlers for WebStorage.
    void connectToStorageArea(IPC::Connection&, WebCore::StorageType, StorageAreaMapIdentifier, StorageNamespaceIdentifier, const WebCore::ClientOrigin&, CompletionHandler<void(StorageAreaIdentifier, HashMap<String, String>, uint64_t)>&&);
    void connectToStorageAreaSync(IPC::Connection&, WebCore::StorageType, StorageAreaMapIdentifier, StorageNamespaceIdentifier, const WebCore::ClientOrigin&, CompletionHandler<void(StorageAreaIdentifier, HashMap<String, String>, uint64_t)>&&);
    void disconnectFromStorageArea(IPC::Connection&, StorageAreaIdentifier);
    void cloneSessionStorageNamespace(IPC::Connection&, StorageNamespaceIdentifier, StorageNamespaceIdentifier);
    void setItem(IPC::Connection&, StorageAreaIdentifier, StorageAreaImplIdentifier, String&& key, String&& value, String&& urlString, CompletionHandler<void(bool)>&&);
    void removeItem(IPC::Connection&, StorageAreaIdentifier, StorageAreaImplIdentifier, String&& key, String&& urlString, CompletionHandler<void()>&&);
    void clear(IPC::Connection&, StorageAreaIdentifier, StorageAreaImplIdentifier, String&& urlString, CompletionHandler<void()>&&);

    // Message handlers for IndexedDB.
    void openDatabase(IPC::Connection&, const WebCore::IDBRequestData&);
    void openDBRequestCancelled(const WebCore::IDBRequestData&);
    void deleteDatabase(IPC::Connection&, const WebCore::IDBRequestData&);
    void establishTransaction(uint64_t databaseConnectionIdentifier, const WebCore::IDBTransactionInfo&);
    void databaseConnectionPendingClose(uint64_t databaseConnectionIdentifier);
    void databaseConnectionClosed(uint64_t databaseConnectionIdentifier);
    void abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const WebCore::IDBResourceIdentifier& transactionIdentifier);
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

    PAL::SessionID m_sessionID;
    Ref<SuspendableWorkQueue> m_queue;
    String m_path;
    FileSystem::Salt m_salt;
    bool m_closed { false };
    HashMap<WebCore::ClientOrigin, std::unique_ptr<OriginStorageManager>> m_localOriginStorageManagers;
    WeakHashSet<IPC::Connection> m_connections; // Main thread only.
    std::unique_ptr<FileSystemStorageHandleRegistry> m_fileSystemStorageHandleRegistry;
    std::unique_ptr<StorageAreaRegistry> m_storageAreaRegistry;
    std::unique_ptr<IDBStorageRegistry> m_idbStorageRegistry;
    String m_customLocalStoragePath;
    String m_customIDBStoragePath;
    String m_customCacheStoragePath;
    uint64_t m_defaultOriginQuota;
    uint64_t m_defaultThirdPartyOriginQuota;
    bool m_shouldUseCustomPaths;
    IPC::Connection::UniqueID m_parentConnection;
    HashMap<IPC::Connection::UniqueID, HashSet<String>> m_temporaryBlobPathsByConnection;
};

} // namespace WebKit
