/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IDBBackingStore_h
#define IDBBackingStore_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBCursor.h"
#include "IDBKeyPath.h"
#include "IDBMetadata.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class IDBKey;
class IDBKeyRange;
class SecurityOrigin;
struct IDBDatabaseMetadata;

class IDBBackingStore : public RefCounted<IDBBackingStore> {
public:
    class Transaction;

    virtual ~IDBBackingStore() {};

    virtual Vector<String> getDatabaseNames() = 0;
    virtual bool getIDBDatabaseMetaData(const String& name, IDBDatabaseMetadata* foundMetadata) = 0;
    virtual bool createIDBDatabaseMetaData(const String& name, const String& stringVersion, int64_t intVersion, int64_t& rowId) = 0;
    virtual bool updateIDBDatabaseIntVersion(Transaction*, int64_t rowId, int64_t intVersion) = 0;
    virtual bool updateIDBDatabaseMetaData(Transaction*, int64_t rowId, const String& version) = 0;
    virtual bool deleteDatabase(const String& name) = 0;

    virtual Vector<IDBObjectStoreMetadata> getObjectStores(int64_t databaseId) = 0;
    virtual bool createObjectStore(Transaction*, int64_t databaseId, int64_t objectStoreId, const String& name, const IDBKeyPath&, bool autoIncrement) = 0;
    virtual void deleteObjectStore(Transaction*, int64_t databaseId, int64_t objectStoreId) = 0;

    class RecordIdentifier : public RefCounted<RecordIdentifier> {
    public:
        virtual bool isValid() const = 0;
        virtual ~RecordIdentifier() { }
    };
    virtual PassRefPtr<RecordIdentifier> createInvalidRecordIdentifier() = 0;

    virtual String getRecord(Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBKey&) = 0;
    virtual bool putRecord(Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBKey&, const String& value, RecordIdentifier*) = 0;
    virtual void clearObjectStore(Transaction*, int64_t databaseId, int64_t objectStoreId) = 0;
    virtual void deleteRecord(Transaction*, int64_t databaseId, int64_t objectStoreId, const RecordIdentifier*) = 0;
    virtual int64_t getKeyGeneratorCurrentNumber(Transaction*, int64_t databaseId, int64_t objectStoreId) = 0;
    virtual bool maybeUpdateKeyGeneratorCurrentNumber(Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t newNumber, bool checkCurrent) = 0;
    virtual bool keyExistsInObjectStore(Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBKey&, RecordIdentifier* foundRecordIdentifier) = 0;

    virtual Vector<IDBIndexMetadata> getIndexes(int64_t databaseId, int64_t objectStoreId) = 0;
    virtual bool createIndex(Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const String& name, const IDBKeyPath&, bool isUnique, bool isMultiEntry) = 0;
    virtual void deleteIndex(Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId) = 0;
    virtual bool putIndexDataForRecord(Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&, const RecordIdentifier*) = 0;
    virtual bool deleteIndexDataForRecord(Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const RecordIdentifier*) = 0;
    virtual PassRefPtr<IDBKey> getPrimaryKeyViaIndex(Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&) = 0;
    virtual bool keyExistsInIndex(Transaction*, int64_t databaseid, int64_t objectStoreId, int64_t indexId, const IDBKey& indexKey, RefPtr<IDBKey>& foundPrimaryKey) = 0;

    class Cursor : public RefCounted<Cursor> {
    public:
        enum IteratorState {
            Ready = 0,
            Seek
        };

        virtual PassRefPtr<Cursor> clone() = 0;
        virtual bool advance(unsigned long) = 0;
        virtual bool continueFunction(const IDBKey* = 0, IteratorState = Seek) = 0;
        virtual PassRefPtr<IDBKey> key() = 0;
        virtual PassRefPtr<IDBKey> primaryKey() = 0;
        virtual String value() = 0;
        virtual PassRefPtr<RecordIdentifier> objectStoreRecordIdentifier() = 0;
        virtual ~Cursor() { };
    };

    virtual PassRefPtr<Cursor> openObjectStoreCursor(Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBKeyRange*, IDBCursor::Direction) = 0;
    virtual PassRefPtr<Cursor> openObjectStoreKeyCursor(Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBKeyRange*, IDBCursor::Direction) = 0;
    virtual PassRefPtr<Cursor> openIndexKeyCursor(Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange*, IDBCursor::Direction) = 0;
    virtual PassRefPtr<Cursor> openIndexCursor(Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange*, IDBCursor::Direction) = 0;

    class Transaction : public RefCounted<Transaction> {
    public:
        virtual ~Transaction() { }
        virtual void begin() = 0;
        virtual bool commit() = 0;
        virtual void rollback() = 0;
    };
    virtual PassRefPtr<Transaction> createTransaction() = 0;
};

} // namespace WebCore

#endif

#endif // IDBBackingStore_h
