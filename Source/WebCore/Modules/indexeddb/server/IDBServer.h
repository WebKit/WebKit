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

namespace IDBServer {

class IDBServer : public RefCounted<IDBServer> {
public:
    static Ref<IDBServer> create();

    void registerConnection(IDBConnectionToClient&);
    void unregisterConnection(IDBConnectionToClient&);

    // Operations requested by the client.
    void openDatabase(const IDBRequestData&);
    void deleteDatabase(const IDBRequestData&);
    void abortTransaction(const IDBResourceIdentifier&);
    void commitTransaction(const IDBResourceIdentifier&);
    void didFinishHandlingVersionChangeTransaction(const IDBResourceIdentifier&);
    void createObjectStore(const IDBRequestData&, const IDBObjectStoreInfo&);
    void deleteObjectStore(const IDBRequestData&, const String& objectStoreName);
    void clearObjectStore(const IDBRequestData&, uint64_t objectStoreIdentifier);
    void createIndex(const IDBRequestData&, const IDBIndexInfo&);
    void deleteIndex(const IDBRequestData&, uint64_t objectStoreIdentifier, const String& indexName);
    void putOrAdd(const IDBRequestData&, const IDBKeyData&, const ThreadSafeDataBuffer& valueData, IndexedDB::ObjectStoreOverwriteMode);
    void getRecord(const IDBRequestData&, const IDBKeyRangeData&);
    void getCount(const IDBRequestData&, const IDBKeyRangeData&);
    void deleteRecord(const IDBRequestData&, const IDBKeyRangeData&);
    void openCursor(const IDBRequestData&, const IDBCursorInfo&);
    void iterateCursor(const IDBRequestData&, const IDBKeyData&, unsigned long count);

    void establishTransaction(uint64_t databaseConnectionIdentifier, const IDBTransactionInfo&);
    void databaseConnectionClosed(uint64_t databaseConnectionIdentifier);
    void didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier);

    void postDatabaseTask(std::unique_ptr<CrossThreadTask>&&);
    void postDatabaseTaskReply(std::unique_ptr<CrossThreadTask>&&);

    void registerDatabaseConnection(UniqueIDBDatabaseConnection&);
    void unregisterDatabaseConnection(UniqueIDBDatabaseConnection&);
    void registerTransaction(UniqueIDBDatabaseTransaction&);
    void unregisterTransaction(UniqueIDBDatabaseTransaction&);

    void deleteUniqueIDBDatabase(UniqueIDBDatabase&);

    std::unique_ptr<IDBBackingStore> createBackingStore(const IDBDatabaseIdentifier&);

private:
    IDBServer();

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
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBServer_h
