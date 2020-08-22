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

#pragma once

#if ENABLE(INDEXED_DATABASE)

#include "IDBBackingStore.h"
#include "IDBDatabaseIdentifier.h"
#include "IDBDatabaseInfo.h"
#include "IDBDatabaseNameAndVersion.h"
#include "IDBResourceIdentifier.h"
#include "SQLiteIDBTransaction.h"
#include "SQLiteStatementAutoResetScope.h"
#include <JavaScriptCore/Strong.h>
#include <pal/SessionID.h>
#include <wtf/HashMap.h>

namespace WebCore {

class IndexKey;
class SQLiteDatabase;
class SQLiteStatement;

namespace IDBServer {

enum class IsSchemaUpgraded : bool { No, Yes };

class IDBSerializationContext;
class SQLiteIDBCursor;

class SQLiteIDBBackingStore final : public IDBBackingStore {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SQLiteIDBBackingStore(PAL::SessionID, const IDBDatabaseIdentifier&, const String& databaseRootDirectory);
    
    ~SQLiteIDBBackingStore() final;

    IDBError getOrEstablishDatabaseInfo(IDBDatabaseInfo&) final;

    IDBError beginTransaction(const IDBTransactionInfo&) final;
    IDBError abortTransaction(const IDBResourceIdentifier& transactionIdentifier) final;
    IDBError commitTransaction(const IDBResourceIdentifier& transactionIdentifier) final;
    IDBError createObjectStore(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo&) final;
    IDBError deleteObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier) final;
    IDBError renameObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const String& newName) final;
    IDBError clearObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier) final;
    IDBError createIndex(const IDBResourceIdentifier& transactionIdentifier, const IDBIndexInfo&) final;
    IDBError deleteIndex(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier) final;
    IDBError renameIndex(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName) final;
    IDBError keyExistsInObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyData&, bool& keyExists) final;
    IDBError deleteRange(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData&) final;
    IDBError addRecord(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo&, const IDBKeyData&, const IndexIDToIndexKeyMap&, const IDBValue&) final;
    IDBError getRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData&, IDBGetRecordDataType, IDBGetResult& outValue) final;
    IDBError getAllRecords(const IDBResourceIdentifier& transactionIdentifier, const IDBGetAllRecordsData&, IDBGetAllResult& outValue) final;
    IDBError getIndexRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, IndexedDB::IndexRecordType, const IDBKeyRangeData&, IDBGetResult& outValue) final;
    IDBError getCount(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const IDBKeyRangeData&, uint64_t& outCount) final;
    IDBError generateKeyNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t& keyNumber) final;
    IDBError revertGeneratedKeyNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t keyNumber) final;
    IDBError maybeUpdateKeyGeneratorNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, double newKeyNumber) final;
    IDBError openCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBCursorInfo&, IDBGetResult& outResult) final;
    IDBError iterateCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBResourceIdentifier& cursorIdentifier, const IDBIterateCursorData&, IDBGetResult& outResult) final;

    IDBSerializationContext& serializationContext() final;

    IDBObjectStoreInfo* infoForObjectStore(uint64_t objectStoreIdentifier) final;
    void deleteBackingStore() final;

    bool supportsSimultaneousTransactions() final { return false; }
    bool isEphemeral() final { return false; }

    bool hasTransaction(const IDBResourceIdentifier&) const final;

    void unregisterCursor(SQLiteIDBCursor&);

    IDBError getBlobRecordsForObjectStoreRecord(int64_t objectStoreRecord, Vector<String>& blobURLs, Vector<String>& blobFilePaths);

    static String databaseNameFromEncodedFilename(const String&);
    static uint64_t databasesSizeForDirectory(const String& directory);

    String databaseDirectory() const { return m_databaseDirectory; };
    static String fullDatabasePathForDirectory(const String&);
    static Optional<IDBDatabaseNameAndVersion> databaseNameAndVersionFromFile(const String&);

    PAL::SessionID sessionID() const { return m_sessionID; }

private:
    String filenameForDatabaseName() const;
    String fullDatabasePath() const;
    String fullDatabaseDirectoryWithUpgrade();
    
    String databaseRootDirectoryIsolatedCopy() const { return m_databaseRootDirectory.isolatedCopy(); }

    bool ensureValidRecordsTable();
    bool ensureValidIndexRecordsTable();
    bool ensureValidIndexRecordsIndex();
    bool ensureValidIndexRecordsRecordIndex();
    bool ensureValidBlobTables();
    Optional<IsSchemaUpgraded> ensureValidObjectStoreInfoTable();
    std::unique_ptr<IDBDatabaseInfo> createAndPopulateInitialDatabaseInfo();
    std::unique_ptr<IDBDatabaseInfo> extractExistingDatabaseInfo();

    IDBError deleteRecord(SQLiteIDBTransaction&, int64_t objectStoreID, const IDBKeyData&);
    IDBError uncheckedGetKeyGeneratorValue(int64_t objectStoreID, uint64_t& outValue);
    IDBError uncheckedSetKeyGeneratorValue(int64_t objectStoreID, uint64_t value);

    IDBError updateAllIndexesForAddRecord(const IDBObjectStoreInfo&, const IDBKeyData&, const IndexIDToIndexKeyMap&, int64_t recordID);
    IDBError updateOneIndexForAddRecord(const IDBIndexInfo&, const IDBKeyData&, const ThreadSafeDataBuffer& value, int64_t recordID);
    IDBError uncheckedPutIndexKey(const IDBIndexInfo&, const IDBKeyData& keyValue, const IndexKey&, int64_t recordID);
    IDBError uncheckedPutIndexRecord(int64_t objectStoreID, int64_t indexID, const IDBKeyData& keyValue, const IDBKeyData& indexKey, int64_t recordID);
    IDBError uncheckedHasIndexRecord(const IDBIndexInfo&, const IDBKeyData&, bool& hasRecord);
    IDBError uncheckedGetIndexRecordForOneKey(int64_t indexeID, int64_t objectStoreID, IndexedDB::IndexRecordType, const IDBKeyData&, IDBGetResult&);

    IDBError deleteUnusedBlobFileRecords(SQLiteIDBTransaction&);

    IDBError getAllObjectStoreRecords(const IDBResourceIdentifier& transactionIdentifier, const IDBGetAllRecordsData&, IDBGetAllResult& outValue);
    IDBError getAllIndexRecords(const IDBResourceIdentifier& transactionIdentifier, const IDBGetAllRecordsData&, IDBGetAllResult& outValue);

    void closeSQLiteDB();
    void close() final;

    enum class SQL : size_t {
        CreateObjectStoreInfo,
        CreateObjectStoreKeyGenerator,
        DeleteObjectStoreInfo,
        DeleteObjectStoreKeyGenerator,
        DeleteObjectStoreRecords,
        DeleteObjectStoreIndexInfo,
        DeleteObjectStoreIndexRecords,
        DeleteObjectStoreBlobRecords,
        RenameObjectStore,
        ClearObjectStoreRecords,
        ClearObjectStoreIndexRecords,
        CreateIndexInfo,
        DeleteIndexInfo,
        HasIndexRecord,
        PutIndexRecord,
        GetIndexRecordForOneKey,
        DeleteIndexRecords,
        RenameIndex,
        KeyExistsInObjectStore,
        GetUnusedBlobFilenames,
        DeleteUnusedBlobs,
        GetObjectStoreRecord,
        DeleteBlobRecord,
        DeleteObjectStoreRecord,
        DeleteObjectStoreIndexRecord,
        AddObjectStoreRecord,
        AddBlobRecord,
        BlobFilenameForBlobURL,
        AddBlobFilename,
        GetBlobURL,
        GetKeyGeneratorValue,
        SetKeyGeneratorValue,
        GetAllKeyRecordsLowerOpenUpperOpen,
        GetAllKeyRecordsLowerOpenUpperClosed,
        GetAllKeyRecordsLowerClosedUpperOpen,
        GetAllKeyRecordsLowerClosedUpperClosed,
        GetValueRecordsLowerOpenUpperOpen,
        GetValueRecordsLowerOpenUpperClosed,
        GetValueRecordsLowerClosedUpperOpen,
        GetValueRecordsLowerClosedUpperClosed,
        GetKeyRecordsLowerOpenUpperOpen,
        GetKeyRecordsLowerOpenUpperClosed,
        GetKeyRecordsLowerClosedUpperOpen,
        GetKeyRecordsLowerClosedUpperClosed,
        CountRecordsLowerOpenUpperOpen,
        CountRecordsLowerOpenUpperClosed,
        CountRecordsLowerClosedUpperOpen,
        CountRecordsLowerClosedUpperClosed,
        CountIndexRecordsLowerOpenUpperOpen,
        CountIndexRecordsLowerOpenUpperClosed,
        CountIndexRecordsLowerClosedUpperOpen,
        CountIndexRecordsLowerClosedUpperClosed,
        Invalid,
    };

    SQLiteStatementAutoResetScope cachedStatement(SQL, const char*);
    SQLiteStatementAutoResetScope cachedStatementForGetAllObjectStoreRecords(const IDBGetAllRecordsData&);

    std::unique_ptr<SQLiteStatement> m_cachedStatements[static_cast<int>(SQL::Invalid)];

    PAL::SessionID m_sessionID;
    IDBDatabaseIdentifier m_identifier;
    std::unique_ptr<IDBDatabaseInfo> m_databaseInfo;
    std::unique_ptr<IDBDatabaseInfo> m_originalDatabaseInfoBeforeVersionChange;

    std::unique_ptr<SQLiteDatabase> m_sqliteDB;

    HashMap<IDBResourceIdentifier, std::unique_ptr<SQLiteIDBTransaction>> m_transactions;
    HashMap<IDBResourceIdentifier, SQLiteIDBCursor*> m_cursors;

    String m_databaseRootDirectory;
    String m_databaseDirectory;

    Ref<IDBSerializationContext> m_serializationContext;
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
