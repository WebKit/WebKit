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
#include "StorageQuotaUser.h"
#include "UniqueIDBDatabase.h"
#include "UniqueIDBDatabaseConnection.h"
#include <pal/HysteresisActivity.h>
#include <pal/SessionID.h>
#include <wtf/CrossThreadTaskHandler.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class IDBCursorInfo;
class IDBRequestData;
class IDBValue;
class StorageQuotaManager;

struct IDBGetRecordData;

namespace IDBServer {

class IDBBackingStoreTemporaryFileHandler;

enum class ShouldForceStop : bool { No, Yes };

class IDBServer : public RefCounted<IDBServer>, public CrossThreadTaskHandler, public CanMakeWeakPtr<IDBServer> {
public:
    using QuotaManagerGetter = WTF::Function<StorageQuotaManager*(PAL::SessionID, const ClientOrigin&)>;
    static Ref<IDBServer> create(PAL::SessionID, IDBBackingStoreTemporaryFileHandler&, QuotaManagerGetter&&);
    WEBCORE_EXPORT static Ref<IDBServer> create(PAL::SessionID, const String& databaseDirectoryPath, IDBBackingStoreTemporaryFileHandler&, QuotaManagerGetter&&);

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
    WEBCORE_EXPORT void didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier);
    WEBCORE_EXPORT void openDBRequestCancelled(const IDBRequestData&);
    WEBCORE_EXPORT void confirmDidCloseFromServer(uint64_t databaseConnectionIdentifier);

    WEBCORE_EXPORT void getAllDatabaseNames(uint64_t serverConnectionIdentifier, const SecurityOriginData& mainFrameOrigin, const SecurityOriginData& openingOrigin, uint64_t callbackID);

    void postDatabaseTask(CrossThreadTask&&);
    void postDatabaseTaskReply(CrossThreadTask&&);

    void registerDatabaseConnection(UniqueIDBDatabaseConnection&);
    void unregisterDatabaseConnection(UniqueIDBDatabaseConnection&);
    void registerTransaction(UniqueIDBDatabaseTransaction&);
    void unregisterTransaction(UniqueIDBDatabaseTransaction&);

    std::unique_ptr<UniqueIDBDatabase> closeAndTakeUniqueIDBDatabase(UniqueIDBDatabase&);

    std::unique_ptr<IDBBackingStore> createBackingStore(const IDBDatabaseIdentifier&);

    WEBCORE_EXPORT void closeAndDeleteDatabasesModifiedSince(WallTime, Function<void ()>&& completionHandler);
    WEBCORE_EXPORT void closeAndDeleteDatabasesForOrigins(const Vector<SecurityOriginData>&, Function<void ()>&& completionHandler);

    void requestSpace(const ClientOrigin&, uint64_t taskSize, CompletionHandler<void(StorageQuotaManager::Decision)>&&);
    void increasePotentialSpaceUsed(const ClientOrigin&, uint64_t taskSize);
    void decreasePotentialSpaceUsed(const ClientOrigin&, uint64_t taskSize);
    void increaseSpaceUsed(const ClientOrigin&, uint64_t size);
    void decreaseSpaceUsed(const ClientOrigin&, uint64_t size);
    void resetSpaceUsed(const ClientOrigin&);

    void initializeQuotaUser(const ClientOrigin& origin) { ensureQuotaUser(origin); }

    WEBCORE_EXPORT void tryStop(ShouldForceStop);
    WEBCORE_EXPORT void resume();

private:
    IDBServer(PAL::SessionID, IDBBackingStoreTemporaryFileHandler&, QuotaManagerGetter&&);
    IDBServer(PAL::SessionID, const String& databaseDirectoryPath, IDBBackingStoreTemporaryFileHandler&, QuotaManagerGetter&&);

    UniqueIDBDatabase& getOrCreateUniqueIDBDatabase(const IDBDatabaseIdentifier&);
    
    String databaseDirectoryPathIsolatedCopy() const { return m_databaseDirectoryPath.isolatedCopy(); }

    void performGetAllDatabaseNames(uint64_t serverConnectionIdentifier, const SecurityOriginData& mainFrameOrigin, const SecurityOriginData& openingOrigin, uint64_t callbackID);
    void didGetAllDatabaseNames(uint64_t serverConnectionIdentifier, uint64_t callbackID, const Vector<String>& databaseNames);

    void performCloseAndDeleteDatabasesModifiedSince(WallTime, uint64_t callbackID);
    void performCloseAndDeleteDatabasesForOrigins(const Vector<SecurityOriginData>&, uint64_t callbackID);
    void didPerformCloseAndDeleteDatabases(uint64_t callbackID);

    void upgradeFilesIfNecessary();
    void removeDatabasesModifiedSinceForVersion(WallTime, const String&);
    void removeDatabasesWithOriginsForVersion(const Vector<SecurityOriginData>&, const String&);

    class QuotaUser final : public StorageQuotaUser {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        QuotaUser(IDBServer&, StorageQuotaManager*, ClientOrigin&&);
        ~QuotaUser();

        StorageQuotaManager* manager() { return m_manager.get(); }

        void setSpaceUsed(uint64_t spaceUsed) { m_spaceUsed = spaceUsed; }
        void resetSpaceUsed();

        void increasePotentialSpaceUsed(uint64_t increase) { m_estimatedSpaceIncrease += increase; }
        void decreasePotentialSpaceUsed(uint64_t decrease)
        {
            ASSERT(m_estimatedSpaceIncrease >= decrease);
            m_estimatedSpaceIncrease -= decrease;
        }
        void increaseSpaceUsed(uint64_t size);
        void decreaseSpaceUsed(uint64_t size);

        void initializeSpaceUsed(uint64_t spaceUsed);

    private:
        uint64_t spaceUsed() const final { return m_spaceUsed + m_estimatedSpaceIncrease; }
        void whenInitialized(CompletionHandler<void()>&&) final;

        IDBServer& m_server;
        WeakPtr<StorageQuotaManager> m_manager;
        ClientOrigin m_origin;
        bool m_isInitialized { false };
        uint64_t m_spaceUsed { 0 };
        uint64_t m_estimatedSpaceIncrease { 0 };
        CompletionHandler<void()> m_initializationCallback;
    };

    WEBCORE_EXPORT QuotaUser& ensureQuotaUser(const ClientOrigin&);
    void startComputingSpaceUsedForOrigin(const ClientOrigin&);
    void computeSpaceUsedForOrigin(const ClientOrigin&);
    void finishComputingSpaceUsedForOrigin(const ClientOrigin&, uint64_t spaceUsed);

    PAL::SessionID m_sessionID;
    HashMap<uint64_t, RefPtr<IDBConnectionToClient>> m_connectionMap;
    HashMap<IDBDatabaseIdentifier, std::unique_ptr<UniqueIDBDatabase>> m_uniqueIDBDatabaseMap;

    HashMap<uint64_t, UniqueIDBDatabaseConnection*> m_databaseConnections;
    HashMap<IDBResourceIdentifier, UniqueIDBDatabaseTransaction*> m_transactions;

    HashMap<uint64_t, Function<void ()>> m_deleteDatabaseCompletionHandlers;

    String m_databaseDirectoryPath;
    IDBBackingStoreTemporaryFileHandler& m_backingStoreTemporaryFileHandler;

    HashMap<ClientOrigin, std::unique_ptr<QuotaUser>> m_quotaUsers;
    QuotaManagerGetter m_quotaManagerGetter;
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
