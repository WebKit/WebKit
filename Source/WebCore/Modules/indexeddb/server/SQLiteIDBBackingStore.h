/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef SQLiteIDBBackingStore_h
#define SQLiteIDBBackingStore_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBBackingStore.h"
#include "IDBDatabaseIdentifier.h"
#include "IDBDatabaseInfo.h"
#include "IDBResourceIdentifier.h"
#include "SQLiteIDBTransaction.h"
#include <JavaScriptCore/Strong.h>
#include <wtf/HashMap.h>

namespace WebCore {

class IndexKey;
class SQLiteDatabase;

namespace IDBServer {

class SQLiteIDBCursor;

class SQLiteIDBBackingStore : public IDBBackingStore {
public:
    SQLiteIDBBackingStore(const IDBDatabaseIdentifier&, const String& databaseRootDirectory, IDBBackingStoreTemporaryFileHandler&);
    
    ~SQLiteIDBBackingStore() final;

    IDBError getOrEstablishDatabaseInfo(IDBDatabaseInfo&) final;

    IDBError beginTransaction(const IDBTransactionInfo&) final;
    IDBError abortTransaction(const IDBResourceIdentifier& transactionIdentifier) final;
    IDBError commitTransaction(const IDBResourceIdentifier& transactionIdentifier) final;
    IDBError createObjectStore(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo&) final;
    IDBError deleteObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier) final;
    IDBError clearObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier) final;
    IDBError createIndex(const IDBResourceIdentifier& transactionIdentifier, const IDBIndexInfo&) final;
    IDBError deleteIndex(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier) final;
    IDBError keyExistsInObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyData&, bool& keyExists) final;
    IDBError deleteRange(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData&) final;
    IDBError addRecord(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo&, const IDBKeyData&, const IDBValue&) final;
    IDBError getRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData&, IDBGetResult& outValue) final;
    IDBError getIndexRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, IndexedDB::IndexRecordType, const IDBKeyRangeData&, IDBGetResult& outValue) final;
    IDBError getCount(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const IDBKeyRangeData&, uint64_t& outCount) final;
    IDBError generateKeyNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t& keyNumber) final;
    IDBError revertGeneratedKeyNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t keyNumber) final;
    IDBError maybeUpdateKeyGeneratorNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, double newKeyNumber) final;
    IDBError openCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBCursorInfo&, IDBGetResult& outResult) final;
    IDBError iterateCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBResourceIdentifier& cursorIdentifier, const IDBKeyData&, uint32_t count, IDBGetResult& outResult) final;

    IDBObjectStoreInfo* infoForObjectStore(uint64_t objectStoreIdentifier) final;
    void deleteBackingStore() final;

    bool supportsSimultaneousTransactions() final { return false; }
    bool isEphemeral() final { return false; }

    void unregisterCursor(SQLiteIDBCursor&);

    String fullDatabaseDirectory() const;

    IDBBackingStoreTemporaryFileHandler& temporaryFileHandler() const { return m_temporaryFileHandler; }

private:
    String filenameForDatabaseName() const;
    String fullDatabasePath() const;

    bool ensureValidRecordsTable();
    bool ensureValidIndexRecordsTable();
    bool ensureValidBlobTables();
    std::unique_ptr<IDBDatabaseInfo> createAndPopulateInitialDatabaseInfo();
    std::unique_ptr<IDBDatabaseInfo> extractExistingDatabaseInfo();

    IDBError deleteRecord(SQLiteIDBTransaction&, int64_t objectStoreID, const IDBKeyData&);
    IDBError uncheckedGetKeyGeneratorValue(int64_t objectStoreID, uint64_t& outValue);
    IDBError uncheckedSetKeyGeneratorValue(int64_t objectStoreID, uint64_t value);

    IDBError updateAllIndexesForAddRecord(const IDBObjectStoreInfo&, const IDBKeyData&, const ThreadSafeDataBuffer& value);
    IDBError updateOneIndexForAddRecord(const IDBIndexInfo&, const IDBKeyData&, const ThreadSafeDataBuffer& value);
    IDBError uncheckedPutIndexKey(const IDBIndexInfo&, const IDBKeyData& keyValue, const IndexKey&);
    IDBError uncheckedPutIndexRecord(int64_t objectStoreID, int64_t indexID, const IDBKeyData& keyValue, const IDBKeyData& indexKey);
    IDBError uncheckedHasIndexRecord(const IDBIndexInfo&, const IDBKeyData&, bool& hasRecord);

    IDBError deleteUnusedBlobFileRecords(SQLiteIDBTransaction&);

    JSC::VM& vm();
    JSC::JSGlobalObject& globalObject();
    void initializeVM();

    IDBDatabaseIdentifier m_identifier;
    std::unique_ptr<IDBDatabaseInfo> m_databaseInfo;
    std::unique_ptr<IDBDatabaseInfo> m_originalDatabaseInfoBeforeVersionChange;

    std::unique_ptr<SQLiteDatabase> m_sqliteDB;

    HashMap<IDBResourceIdentifier, std::unique_ptr<SQLiteIDBTransaction>> m_transactions;
    HashMap<IDBResourceIdentifier, SQLiteIDBCursor*> m_cursors;

    String m_absoluteDatabaseDirectory;

    RefPtr<JSC::VM> m_vm;
    JSC::Strong<JSC::JSGlobalObject> m_globalObject;

    IDBBackingStoreTemporaryFileHandler& m_temporaryFileHandler;
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // SQLiteIDBBackingStore_h
