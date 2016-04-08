/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef IDBServer_h
#define IDBServer_h

#if ENABLE(INDEXED_DATABASE)

#include "CrossThreadTask.h"
#include "IDBConnectionToClient.h"
#include "IDBDatabaseIdentifier.h"
#include "UniqueIDBDatabase.h"
#include "UniqueIDBDatabaseConnection.h"
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/MessageQueue.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CrossThreadTask;
class IDBCursorInfo;
class IDBRequestData;
class IDBValue;

namespace IDBServer {

class IDBBackingStoreTemporaryFileHandler;

class IDBServer : public RefCounted<IDBServer> {
public:
    static Ref<IDBServer> create(IDBBackingStoreTemporaryFileHandler&);
    WEBCORE_EXPORT static Ref<IDBServer> create(const String& databaseDirectoryPath, IDBBackingStoreTemporaryFileHandler&);

    WEBCORE_EXPORT void registerConnection(IDBConnectionToClient&);
    WEBCORE_EXPORT void unregisterConnection(IDBConnectionToClient&);

    // Operations requested by the client.
    WEBCORE_EXPORT void openDatabase(const IDBRequestData&);
    WEBCORE_EXPORT void deleteDatabase(const IDBRequestData&);
    WEBCORE_EXPORT void abortTransaction(const IDBResourceIdentifier&);
    WEBCORE_EXPORT void commitTransaction(const IDBResourceIdentifier&);
    WEBCORE_EXPORT void didFinishHandlingVersionChangeTransaction(const IDBResourceIdentifier&);
    WEBCORE_EXPORT void createObjectStore(const IDBRequestData&, const IDBObjectStoreInfo&);
    WEBCORE_EXPORT void deleteObjectStore(const IDBRequestData&, const String& objectStoreName);
    WEBCORE_EXPORT void clearObjectStore(const IDBRequestData&, uint64_t objectStoreIdentifier);
    WEBCORE_EXPORT void createIndex(const IDBRequestData&, const IDBIndexInfo&);
    WEBCORE_EXPORT void deleteIndex(const IDBRequestData&, uint64_t objectStoreIdentifier, const String& indexName);
    WEBCORE_EXPORT void putOrAdd(const IDBRequestData&, const IDBKeyData&, const IDBValue&, IndexedDB::ObjectStoreOverwriteMode);
    WEBCORE_EXPORT void getRecord(const IDBRequestData&, const IDBKeyRangeData&);
    WEBCORE_EXPORT void getCount(const IDBRequestData&, const IDBKeyRangeData&);
    WEBCORE_EXPORT void deleteRecord(const IDBRequestData&, const IDBKeyRangeData&);
    WEBCORE_EXPORT void openCursor(const IDBRequestData&, const IDBCursorInfo&);
    WEBCORE_EXPORT void iterateCursor(const IDBRequestData&, const IDBKeyData&, unsigned long count);

    WEBCORE_EXPORT void establishTransaction(uint64_t databaseConnectionIdentifier, const IDBTransactionInfo&);
    WEBCORE_EXPORT void databaseConnectionClosed(uint64_t databaseConnectionIdentifier);
    WEBCORE_EXPORT void abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& transactionIdentifier);
    WEBCORE_EXPORT void didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier);

    void postDatabaseTask(std::unique_ptr<CrossThreadTask>&&);
    void postDatabaseTaskReply(std::unique_ptr<CrossThreadTask>&&);

    void registerDatabaseConnection(UniqueIDBDatabaseConnection&);
    void unregisterDatabaseConnection(UniqueIDBDatabaseConnection&);
    void registerTransaction(UniqueIDBDatabaseTransaction&);
    void unregisterTransaction(UniqueIDBDatabaseTransaction&);

    void closeUniqueIDBDatabase(UniqueIDBDatabase&);

    std::unique_ptr<IDBBackingStore> createBackingStore(const IDBDatabaseIdentifier&);

private:
    IDBServer(IDBBackingStoreTemporaryFileHandler&);
    IDBServer(const String& databaseDirectoryPath, IDBBackingStoreTemporaryFileHandler&);

    UniqueIDBDatabase& getOrCreateUniqueIDBDatabase(const IDBDatabaseIdentifier&);

    static void databaseThreadEntry(void*);
    void databaseRunLoop();
    void handleTaskRepliesOnMainThread();

    HashMap<uint64_t, RefPtr<IDBConnectionToClient>> m_connectionMap;
    HashMap<IDBDatabaseIdentifier, RefPtr<UniqueIDBDatabase>> m_uniqueIDBDatabaseMap;

    ThreadIdentifier m_threadID { 0 };
    Lock m_databaseThreadCreationLock;
    Lock m_mainThreadReplyLock;
    bool m_mainThreadReplyScheduled { false };

    MessageQueue<CrossThreadTask> m_databaseQueue;
    MessageQueue<CrossThreadTask> m_databaseReplyQueue;

    HashMap<uint64_t, UniqueIDBDatabaseConnection*> m_databaseConnections;
    HashMap<IDBResourceIdentifier, UniqueIDBDatabaseTransaction*> m_transactions;

    String m_databaseDirectoryPath;
    IDBBackingStoreTemporaryFileHandler& m_backingStoreTemporaryFileHandler;
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBServer_h
