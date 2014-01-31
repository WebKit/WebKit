/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef UniqueIDBDatabase_h
#define UniqueIDBDatabase_h

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include "IDBIdentifier.h"
#include "UniqueIDBDatabaseIdentifier.h"
#include <WebCore/IDBDatabaseBackend.h>
#include <WebCore/IndexedDB.h>
#include <functional>
#include <wtf/Deque.h>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class SharedBuffer;

struct IDBDatabaseMetadata;
struct IDBKeyData;
}

namespace WebKit {

class AsyncRequest;
class AsyncTask;
class DatabaseProcessIDBConnection;
class UniqueIDBDatabaseBackingStore;

struct SecurityOriginData;

class UniqueIDBDatabase : public ThreadSafeRefCounted<UniqueIDBDatabase> {
public:
    static PassRefPtr<UniqueIDBDatabase> create(const UniqueIDBDatabaseIdentifier& identifier)
    {
        return adoptRef(new UniqueIDBDatabase(identifier));
    }

    ~UniqueIDBDatabase();

    const UniqueIDBDatabaseIdentifier& identifier() const { return m_identifier; }

    void registerConnection(DatabaseProcessIDBConnection&);
    void unregisterConnection(DatabaseProcessIDBConnection&);

    void getOrEstablishIDBDatabaseMetadata(std::function<void(bool, const WebCore::IDBDatabaseMetadata&)> completionCallback);

    void openTransaction(const IDBIdentifier& transactionIdentifier, const Vector<int64_t>& objectStoreIDs, WebCore::IndexedDB::TransactionMode, std::function<void(bool)> successCallback);
    void beginTransaction(const IDBIdentifier& transactionIdentifier, std::function<void(bool)> successCallback);
    void commitTransaction(const IDBIdentifier& transactionIdentifier, std::function<void(bool)> successCallback);
    void resetTransaction(const IDBIdentifier& transactionIdentifier, std::function<void(bool)> successCallback);
    void rollbackTransaction(const IDBIdentifier& transactionIdentifier, std::function<void(bool)> successCallback);

    void changeDatabaseVersion(const IDBIdentifier& transactionIdentifier, uint64_t newVersion, std::function<void(bool)> successCallback);
    void createObjectStore(const IDBIdentifier& transactionIdentifier, const WebCore::IDBObjectStoreMetadata&, std::function<void(bool)> successCallback);
    void deleteObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, std::function<void(bool)> successCallback);
    void clearObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, std::function<void(bool)> successCallback);
    void createIndex(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBIndexMetadata&, std::function<void(bool)> successCallback);
    void deleteIndex(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, std::function<void(bool)> successCallback);

    void putRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBKeyData&, const IPC::DataReference& value, int64_t putMode, const Vector<int64_t>& indexIDs, const Vector<Vector<WebCore::IDBKeyData>>& indexKeys, std::function<void(const WebCore::IDBKeyData&, uint32_t, const String&)> callback);
    void getRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const WebCore::IDBKeyRangeData&, WebCore::IndexedDB::CursorType, std::function<void(const WebCore::IDBGetResult&, uint32_t, const String&)> callback);

    void openCursor(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, WebCore::IndexedDB::CursorDirection, WebCore::IndexedDB::CursorType, WebCore::IDBDatabaseBackend::TaskType, const WebCore::IDBKeyRangeData&, std::function<void(int64_t, const WebCore::IDBKeyData&, const WebCore::IDBKeyData&, PassRefPtr<WebCore::SharedBuffer>, uint32_t, const String&)> callback);
    void cursorAdvance(const IDBIdentifier& cursorIdentifier, uint64_t count, std::function<void(const WebCore::IDBKeyData&, const WebCore::IDBKeyData&, PassRefPtr<WebCore::SharedBuffer>, uint32_t, const String&)> callback);
    void cursorIterate(const IDBIdentifier& cursorIdentifier, const WebCore::IDBKeyData&, std::function<void(const WebCore::IDBKeyData&, const WebCore::IDBKeyData&, PassRefPtr<WebCore::SharedBuffer>, uint32_t, const String&)> callback);

    void count(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const WebCore::IDBKeyRangeData&, std::function<void(int64_t, uint32_t, const String&)> callback);
    void deleteRange(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBKeyRangeData&, std::function<void(uint32_t, const String&)> callback);

private:
    UniqueIDBDatabase(const UniqueIDBDatabaseIdentifier&);

    UniqueIDBDatabaseIdentifier m_identifier;

    bool m_inMemory;
    String m_databaseRelativeDirectory;

    HashSet<RefPtr<DatabaseProcessIDBConnection>> m_connections;
    HashMap<uint64_t, RefPtr<AsyncRequest>> m_databaseRequests;

    String absoluteDatabaseDirectory() const;

    void postDatabaseTask(std::unique_ptr<AsyncTask>);
    void shutdown();

    // Method that attempts to make legal filenames from all legal database names
    String filenameForDatabaseName() const;

    // Returns a string that is appropriate for use as a unique filename
    String databaseFilenameIdentifier(const SecurityOriginData&) const;

    // Returns true if this origin can use the same databases as the given origin.
    bool canShareDatabases(const SecurityOriginData&, const SecurityOriginData&) const;

    void postTransactionOperation(const IDBIdentifier& transactionIdentifier, std::unique_ptr<AsyncTask>, std::function<void(bool)> successCallback);
    
    // To be called from the database workqueue thread only
    void performNextDatabaseTask();
    void postMainThreadTask(std::unique_ptr<AsyncTask>);
    void openBackingStoreAndReadMetadata(const UniqueIDBDatabaseIdentifier&, const String& databaseDirectory);
    void openBackingStoreTransaction(const IDBIdentifier& transactionIdentifier, const Vector<int64_t>& objectStoreIDs, WebCore::IndexedDB::TransactionMode);
    void beginBackingStoreTransaction(const IDBIdentifier&);
    void commitBackingStoreTransaction(const IDBIdentifier&);
    void resetBackingStoreTransaction(const IDBIdentifier&);
    void rollbackBackingStoreTransaction(const IDBIdentifier&);

    void changeDatabaseVersionInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, uint64_t newVersion);
    void createObjectStoreInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, const WebCore::IDBObjectStoreMetadata&);
    void deleteObjectStoreInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID);
    void clearObjectStoreInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID);
    void createIndexInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBIndexMetadata&);
    void deleteIndexInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID);
    void putRecordInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, const WebCore::IDBObjectStoreMetadata&, const WebCore::IDBKeyData&, const Vector<uint8_t>& value, int64_t putMode, const Vector<int64_t>& indexIDs, const Vector<Vector<WebCore::IDBKeyData>>& indexKeys);
    void getRecordFromBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, const WebCore::IDBObjectStoreMetadata&, int64_t indexID, const WebCore::IDBKeyRangeData&, WebCore::IndexedDB::CursorType);
    void openCursorInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, WebCore::IndexedDB::CursorDirection, WebCore::IndexedDB::CursorType, WebCore::IDBDatabaseBackend::TaskType, const WebCore::IDBKeyRangeData&);
    void advanceCursorInBackingStore(uint64_t requestID, const IDBIdentifier& cursorIdentifier, uint64_t count);
    void iterateCursorInBackingStore(uint64_t requestID, const IDBIdentifier& cursorIdentifier, const WebCore::IDBKeyData&);
    void countInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const WebCore::IDBKeyRangeData&);
    void deleteRangeInBackingStore(uint64_t requestID, const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBKeyRangeData&);

    void shutdownBackingStore();

    // Callbacks from the database workqueue thread, to be performed on the main thread only
    void performNextMainThreadTask();
    void didOpenBackingStoreAndReadMetadata(const WebCore::IDBDatabaseMetadata&, bool success);
    void didCompleteTransactionOperation(const IDBIdentifier& transactionIdentifier, bool success);
    void didChangeDatabaseVersion(uint64_t requestID, bool success);
    void didCreateObjectStore(uint64_t requestID, bool success);
    void didDeleteObjectStore(uint64_t requestID, bool success);
    void didClearObjectStore(uint64_t requestID, bool success);
    void didCreateIndex(uint64_t requestID, bool success);
    void didDeleteIndex(uint64_t requestID, bool success);
    void didPutRecordInBackingStore(uint64_t requestID, const WebCore::IDBKeyData&, uint32_t errorCode, const String& errorMessage);
    void didGetRecordFromBackingStore(uint64_t requestID, const WebCore::IDBGetResult&, uint32_t errorCode, const String& errorMessage);
    void didOpenCursorInBackingStore(uint64_t requestID, int64_t cursorID, const WebCore::IDBKeyData&, const WebCore::IDBKeyData&, const Vector<char>&, uint32_t errorCode, const String& errorMessage);
    void didAdvanceCursorInBackingStore(uint64_t requestID, const WebCore::IDBKeyData&, const WebCore::IDBKeyData&, const Vector<char>&, uint32_t errorCode, const String& errorMessage);
    void didIterateCursorInBackingStore(uint64_t requestID, const WebCore::IDBKeyData&, const WebCore::IDBKeyData&, const Vector<char>&, uint32_t errorCode, const String& errorMessage);
    void didCountInBackingStore(uint64_t requestID, int64_t count, uint32_t errorCode, const String& errorMessage);
    void didDeleteRangeInBackingStore(uint64_t requestID, uint32_t errorCode, const String& errorMessage);

    void didShutdownBackingStore();
    void didCompleteBoolRequest(uint64_t requestID, bool success);

    bool m_acceptingNewRequests;

    Deque<RefPtr<AsyncRequest>> m_pendingMetadataRequests;
    HashMap<IDBIdentifier, RefPtr<AsyncRequest>> m_pendingTransactionRequests;
    HashMap<uint64_t, RefPtr<AsyncRequest>> m_pendingDatabaseTasks;

    std::unique_ptr<WebCore::IDBDatabaseMetadata> m_metadata;
    bool m_didGetMetadataFromBackingStore;

    RefPtr<UniqueIDBDatabaseBackingStore> m_backingStore;

    Deque<std::unique_ptr<AsyncTask>> m_databaseTasks;
    Mutex m_databaseTaskMutex;

    Deque<std::unique_ptr<AsyncTask>> m_mainThreadTasks;
    Mutex m_mainThreadTaskMutex;
};

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
#endif // UniqueIDBDatabase_h
