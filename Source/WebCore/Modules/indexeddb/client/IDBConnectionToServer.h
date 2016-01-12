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

#ifndef IDBConnectionToServer_h
#define IDBConnectionToServer_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionToServerDelegate.h"
#include "IDBResourceIdentifier.h"
#include "TransactionOperation.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class IDBCursorInfo;
class IDBError;
class IDBObjectStoreInfo;
class IDBResultData;

namespace IDBClient {

class IDBDatabase;
class IDBOpenDBRequest;
class IDBTransaction;
class TransactionOperation;

class IDBConnectionToServer : public RefCounted<IDBConnectionToServer> {
public:
    static Ref<IDBConnectionToServer> create(IDBConnectionToServerDelegate&);

    uint64_t identifier() const;

    void deleteDatabase(IDBOpenDBRequest&);
    void didDeleteDatabase(const IDBResultData&);

    void openDatabase(IDBOpenDBRequest&);
    void didOpenDatabase(const IDBResultData&);

    void createObjectStore(TransactionOperation&, const IDBObjectStoreInfo&);
    void didCreateObjectStore(const IDBResultData&);

    void deleteObjectStore(TransactionOperation&, const String& objectStoreName);
    void didDeleteObjectStore(const IDBResultData&);

    void clearObjectStore(TransactionOperation&, uint64_t objectStoreIdentifier);
    void didClearObjectStore(const IDBResultData&);

    void createIndex(TransactionOperation&, const IDBIndexInfo&);
    void didCreateIndex(const IDBResultData&);

    void deleteIndex(TransactionOperation&, uint64_t objectStoreIdentifier, const String& indexName);
    void didDeleteIndex(const IDBResultData&);

    void putOrAdd(TransactionOperation&, RefPtr<IDBKey>&, RefPtr<SerializedScriptValue>&, const IndexedDB::ObjectStoreOverwriteMode);
    void didPutOrAdd(const IDBResultData&);

    void getRecord(TransactionOperation&, const IDBKeyRangeData&);
    void didGetRecord(const IDBResultData&);

    void getCount(TransactionOperation&, const IDBKeyRangeData&);
    void didGetCount(const IDBResultData&);

    void deleteRecord(TransactionOperation&, const IDBKeyRangeData&);
    void didDeleteRecord(const IDBResultData&);

    void openCursor(TransactionOperation&, const IDBCursorInfo&);
    void didOpenCursor(const IDBResultData&);

    void iterateCursor(TransactionOperation&, const IDBKeyData&, unsigned long count);
    void didIterateCursor(const IDBResultData&);

    void commitTransaction(IDBTransaction&);
    void didCommitTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&);

    void didFinishHandlingVersionChangeTransaction(IDBTransaction&);

    void abortTransaction(IDBTransaction&);
    void didAbortTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&);

    void fireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion);
    void didFireVersionChangeEvent(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& requestIdentifier);

    void didStartTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError&);
    void notifyOpenDBRequestBlocked(const IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion);

    void establishTransaction(IDBTransaction&);

    void databaseConnectionClosed(IDBDatabase&);

    // To be used when an IDBOpenDBRequest gets a new database connection, optionally with a
    // versionchange transaction, but the page is already torn down.
    void abortOpenAndUpgradeNeeded(uint64_t databaseConnectionIdentifier, const IDBResourceIdentifier& transactionIdentifier);
    
    void registerDatabaseConnection(IDBDatabase&);
    void unregisterDatabaseConnection(IDBDatabase&);

private:
    IDBConnectionToServer(IDBConnectionToServerDelegate&);

    void saveOperation(TransactionOperation&);
    void completeOperation(const IDBResultData&);

    bool hasRecordOfTransaction(const IDBTransaction&) const;

    Ref<IDBConnectionToServerDelegate> m_delegate;

    HashMap<IDBResourceIdentifier, RefPtr<IDBClient::IDBOpenDBRequest>> m_openDBRequestMap;
    HashMap<uint64_t, IDBDatabase*> m_databaseConnectionMap;
    HashMap<IDBResourceIdentifier, RefPtr<IDBTransaction>> m_pendingTransactions;
    HashMap<IDBResourceIdentifier, RefPtr<IDBTransaction>> m_committingTransactions;
    HashMap<IDBResourceIdentifier, RefPtr<IDBTransaction>> m_abortingTransactions;
    HashMap<IDBResourceIdentifier, RefPtr<TransactionOperation>> m_activeOperations;
};

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBConnectionToServer_h
