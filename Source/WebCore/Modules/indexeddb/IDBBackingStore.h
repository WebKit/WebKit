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
#include "IDBMetadata.h"
#include "LevelDBTransaction.h"
#include <wtf/OwnPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class LevelDBComparator;
class LevelDBDatabase;
class LevelDBTransaction;
class IDBFactoryBackendImpl;
class IDBKey;
class IDBKeyRange;

class IDBBackingStore : public RefCounted<IDBBackingStore> {
public:
    class Transaction;

    virtual ~IDBBackingStore();
    static PassRefPtr<IDBBackingStore> open(SecurityOrigin*, const String& pathBase, const String& fileIdentifier, IDBFactoryBackendImpl*);

    virtual Vector<String> getDatabaseNames();
    virtual bool getIDBDatabaseMetaData(const String& name, IDBDatabaseMetadata*);
    virtual bool createIDBDatabaseMetaData(const String& name, const String& version, int64_t intVersion, int64_t& rowId);
    virtual bool updateIDBDatabaseMetaData(IDBBackingStore::Transaction*, int64_t rowId, const String& version);
    virtual bool updateIDBDatabaseIntVersion(IDBBackingStore::Transaction*, int64_t rowId, int64_t intVersion);
    virtual bool deleteDatabase(const String& name);

    virtual Vector<IDBObjectStoreMetadata> getObjectStores(int64_t databaseId);
    virtual bool createObjectStore(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, const String& name, const IDBKeyPath&, bool autoIncrement);
    virtual void deleteObjectStore(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId);

    class RecordIdentifier : public RefCounted<RecordIdentifier> {
    public:
        static PassRefPtr<RecordIdentifier> create(const Vector<char>& primaryKey, int64_t version) { return adoptRef(new RecordIdentifier(primaryKey, version)); }
        static PassRefPtr<RecordIdentifier> create() { return adoptRef(new RecordIdentifier()); }
        virtual ~RecordIdentifier() { }

        virtual bool isValid() const { return m_primaryKey.isEmpty(); }
        Vector<char> primaryKey() const { return m_primaryKey; }
        void setPrimaryKey(const Vector<char>& primaryKey) { m_primaryKey = primaryKey; }
        int64_t version() const { return m_version; }
        void setVersion(int64_t version) { m_version = version; }

    private:
        RecordIdentifier(const Vector<char>& primaryKey, int64_t version) : m_primaryKey(primaryKey), m_version(version) { ASSERT(!primaryKey.isEmpty()); }
        RecordIdentifier() : m_primaryKey(), m_version(-1) { }

        Vector<char> m_primaryKey; // FIXME: Make it more clear that this is the *encoded* version of the key.
        int64_t m_version;
    };

    virtual String getRecord(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBKey&);
    virtual bool putRecord(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBKey&, const String& value, RecordIdentifier*);
    virtual void clearObjectStore(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId);
    virtual void deleteRecord(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, const RecordIdentifier*);
    virtual int64_t getKeyGeneratorCurrentNumber(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId);
    virtual bool maybeUpdateKeyGeneratorCurrentNumber(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t newState, bool checkCurrent);
    virtual bool keyExistsInObjectStore(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBKey&, RecordIdentifier* foundRecordIdentifier);

    virtual Vector<IDBIndexMetadata> getIndexes(int64_t databaseId, int64_t objectStoreId);
    virtual bool createIndex(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const String& name, const IDBKeyPath&, bool isUnique, bool isMultiEntry);
    virtual void deleteIndex(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId);
    virtual bool putIndexDataForRecord(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&, const RecordIdentifier*);
    virtual bool deleteIndexDataForRecord(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const RecordIdentifier*);
    virtual PassRefPtr<IDBKey> getPrimaryKeyViaIndex(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&);
    virtual bool keyExistsInIndex(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey& indexKey, RefPtr<IDBKey>& foundPrimaryKey);

    class Cursor : public RefCounted<Cursor> {
    public:
        enum IteratorState {
            Ready = 0,
            Seek
        };

        struct CursorOptions {
            Vector<char> lowKey;
            bool lowOpen;
            Vector<char> highKey;
            bool highOpen;
            bool forward;
            bool unique;
        };

        Cursor(LevelDBTransaction* transaction, const CursorOptions& cursorOptions)
            : m_transaction(transaction)
            , m_cursorOptions(cursorOptions)
        {
        }
        Cursor(const IDBBackingStore::Cursor* other);

        PassRefPtr<IDBKey> key() const { return m_currentKey; }
        bool continueFunction(const IDBKey* = 0, IteratorState = Seek);
        bool advance(unsigned long);
        bool firstSeek();

        virtual PassRefPtr<Cursor> clone() = 0;
        virtual PassRefPtr<IDBKey> primaryKey() const { return m_currentKey; }
        virtual String value() const = 0;
        virtual PassRefPtr<RecordIdentifier> recordIdentifier() = 0;
        virtual ~Cursor() { }
        virtual bool loadCurrentRow() = 0;

    protected:
        bool isPastBounds() const;
        bool haveEnteredRange() const;

        LevelDBTransaction* m_transaction;
        const CursorOptions m_cursorOptions;
        OwnPtr<LevelDBIterator> m_iterator;
        RefPtr<IDBKey> m_currentKey;
    };

    virtual PassRefPtr<Cursor> openObjectStoreKeyCursor(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBKeyRange*, IDBCursor::Direction);
    virtual PassRefPtr<Cursor> openObjectStoreCursor(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBKeyRange*, IDBCursor::Direction);
    virtual PassRefPtr<Cursor> openIndexKeyCursor(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange*, IDBCursor::Direction);
    virtual PassRefPtr<Cursor> openIndexCursor(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange*, IDBCursor::Direction);

    class Transaction {
    public:
        Transaction(IDBBackingStore*);
        void begin();
        bool commit();
        void rollback();

        static LevelDBTransaction* levelDBTransactionFrom(Transaction* transaction)
        {
            return static_cast<Transaction*>(transaction)->m_transaction.get();
        }

    private:
        IDBBackingStore* m_backingStore;
        RefPtr<LevelDBTransaction> m_transaction;
    };

protected:
    IDBBackingStore(const String& identifier, IDBFactoryBackendImpl*, PassOwnPtr<LevelDBDatabase>);

    // Should only used for mocking.
    IDBBackingStore();

private:
    bool findKeyInIndex(IDBBackingStore::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&, Vector<char>& foundEncodedPrimaryKey);

    String m_identifier;
    RefPtr<IDBFactoryBackendImpl> m_factory;
    OwnPtr<LevelDBDatabase> m_db;
    OwnPtr<LevelDBComparator> m_comparator;

};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBBackingStore_h
