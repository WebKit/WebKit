/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionToClient.h"
#include "IDBDatabaseIdentifier.h"
#include "StorageQuotaManager.h"
#include "UniqueIDBDatabase.h"
#include "UniqueIDBDatabaseConnection.h"
#include <pal/HysteresisActivity.h>
#include <pal/SessionID.h>
#include <wtf/CrossThreadTaskHandler.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class IDBCursorInfo;
class IDBRequestData;
class IDBValue;
class StorageQuotaManager;

struct IDBGetRecordData;

namespace IDBServer {

class IDBServer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using StorageQuotaManagerSpaceRequester = Function<StorageQuotaManager::Decision(const ClientOrigin&, uint64_t spaceRequested)>;
    WEBCORE_EXPORT IDBServer(PAL::SessionID, const String& databaseDirectoryPath, StorageQuotaManagerSpaceRequester&&);
    WEBCORE_EXPORT ~IDBServer();

    WEBCORE_EXPORT void registerConnection(IDBConnectionToClient&);
    WEBCORE_EXPORT void unregisterConnection(IDBConnectionToClient&);

    // Operations requested by the client.
    WEBCORE_EXPORT void openDatabase(const IDBRequestData&);
    WEBCORE_EXPORT void deleteDatabase(const IDBRequestData&);
    WEBCORE_EXPORT void abortTransaction(const IDBResourceIdentifier&);
    WEBCORE_EXPORT void commitTransaction(const IDBResourceIdentifier&);
    WEBCORE_EXPORT void didFinishHandlingVersionChangeTransaction(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier&);
    WEBCORE_EXPORT void createObjectStore(const IDBRequestData&, const IDBObjectStoreInfo&);
    WEBCORE_EXPORT void renameObjectStore(const IDBRequestData&, uint64_t objectStoreIdentifier, const String& newName);
    WEBCORE_EXPORT void deleteObjectStore(const IDBRequestData&, const String& objectStoreName);
    WEBCORE_EXPORT void clearObjectStore(const IDBRequestData&, uint64_t objectStoreIdentifier);
    WEBCORE_EXPORT void createIndex(const IDBRequestData&, const IDBIndexInfo&);
    WEBCORE_EXPORT void deleteIndex(const IDBRequestData&, uint64_t objectStoreIdentifier, const String& indexName);
    WEBCORE_EXPORT void renameIndex(const IDBRequestData&, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName);
    WEBCORE_EXPORT void putOrAdd(const IDBRequestData&, const IDBKeyData&, const IDBValue&, IndexedDB::ObjectStoreOverwriteMode);
    WEBCORE_EXPORT void getRecord(const IDBRequestData&, const IDBGetRecordData&);
    WEBCORE_EXPORT void getAllRecords(const IDBRequestData&, const IDBGetAllRecordsData&);
    WEBCORE_EXPORT void getCount(const IDBRequestData&, const IDBKeyRangeData&);
    WEBCORE_EXPORT void deleteRecord(const IDBRequestData&, const IDBKeyRangeData&);
    WEBCORE_EXPORT void openCursor(const IDBRequestData&, const IDBCursorInfo&);
    WEBCORE_EXPORT void iterateCursor(const IDBRequestData&, const IDBIterateCursorData&);

    WEBCORE_EXPORT void establishTransaction(uint64_t databaseConnectionIdentifier, const IDBTransactionInfo&);
    WEBCORE_EXPORT void databaseConnectionPendingClose(uint64_t databaseConnectionIdentifier);
    WEBCORE_EXPORT void databaseConnectionClosed(uint64_t databaseConnectionIdentifier);
    WEBCORE_EXPORT void abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& transactionIdentifier);
    WEBCORE_EXPORT void didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier, IndexedDB::ConnectionClosedOnBehalfOfServer);
    WEBCORE_EXPORT void openDBRequestCancelled(const IDBRequestData&);

    WEBCORE_EXPORT void getAllDatabaseNamesAndVersions(IDBConnectionIdentifier, const IDBResourceIdentifier&, const ClientOrigin&);


    void registerDatabaseConnection(UniqueIDBDatabaseConnection&);
    void unregisterDatabaseConnection(UniqueIDBDatabaseConnection&);
    void registerTransaction(UniqueIDBDatabaseTransaction&);
    void unregisterTransaction(UniqueIDBDatabaseTransaction&);

    std::unique_ptr<UniqueIDBDatabase> closeAndTakeUniqueIDBDatabase(UniqueIDBDatabase&);

    std::unique_ptr<IDBBackingStore> createBackingStore(const IDBDatabaseIdentifier&);

    WEBCORE_EXPORT void closeAndDeleteDatabasesModifiedSince(WallTime);
    WEBCORE_EXPORT void closeAndDeleteDatabasesForOrigins(const Vector<SecurityOriginData>&);
    void closeDatabasesForOrigins(const Vector<SecurityOriginData>&, Function<bool(const SecurityOriginData&, const ClientOrigin&)>&&);
    WEBCORE_EXPORT void renameOrigin(const WebCore::SecurityOriginData&, const WebCore::SecurityOriginData&);

    StorageQuotaManager::Decision requestSpace(const ClientOrigin&, uint64_t taskSize);
    WEBCORE_EXPORT static uint64_t diskUsage(const String& rootDirectory, const ClientOrigin&);

    WEBCORE_EXPORT void stopDatabaseActivitiesOnMainThread();

    Lock& lock() { return m_lock; };

private:
    UniqueIDBDatabase& getOrCreateUniqueIDBDatabase(const IDBDatabaseIdentifier&);
    
    String databaseDirectoryPathIsolatedCopy() const { return m_databaseDirectoryPath.isolatedCopy(); }

    void upgradeFilesIfNecessary();
    void removeDatabasesModifiedSinceForVersion(WallTime, const String&);
    void removeDatabasesWithOriginsForVersion(const Vector<SecurityOriginData>&, const String&);

    PAL::SessionID m_sessionID;
    HashMap<IDBConnectionIdentifier, RefPtr<IDBConnectionToClient>> m_connectionMap;
    HashMap<IDBDatabaseIdentifier, std::unique_ptr<UniqueIDBDatabase>> m_uniqueIDBDatabaseMap;

    HashMap<uint64_t, UniqueIDBDatabaseConnection*> m_databaseConnections;
    HashMap<IDBResourceIdentifier, UniqueIDBDatabaseTransaction*> m_transactions;

    HashMap<uint64_t, Function<void ()>> m_deleteDatabaseCompletionHandlers;

    String m_databaseDirectoryPath;

    StorageQuotaManagerSpaceRequester m_spaceRequester;

    Lock m_lock;
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
