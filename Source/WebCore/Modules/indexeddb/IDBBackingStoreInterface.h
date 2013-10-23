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

#ifndef IDBBackingStoreInterface_h
#define IDBBackingStoreInterface_h

#include "IDBDatabaseBackendInterface.h"
#include "IDBMetadata.h"
#include "IndexedDB.h"

#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBIndexWriter;
class IDBKey;
class IDBKeyPath;
class IDBKeyRange;
class IDBRecordIdentifier;
class IDBTransactionBackendInterface;
class SharedBuffer;

class IDBBackingStoreInterface : public RefCounted<IDBBackingStoreInterface> {
public:
    virtual ~IDBBackingStoreInterface() { }

    class Transaction {
    public:
        virtual ~Transaction() { }
    };

    class Cursor : public RefCounted<Cursor> {
    public:
        enum IteratorState {
            Ready = 0,
            Seek
        };

        virtual ~Cursor() { }

        virtual PassRefPtr<IDBKey> key() const = 0;
        virtual bool continueFunction(const IDBKey* = 0, IteratorState = Seek) = 0;
        virtual bool advance(unsigned long) = 0;

        virtual PassRefPtr<Cursor> clone() = 0;
        virtual PassRefPtr<IDBKey> primaryKey() const = 0;
        virtual PassRefPtr<SharedBuffer> value() const = 0;
        virtual const IDBRecordIdentifier& recordIdentifier() const = 0;
    };

    virtual bool getIDBDatabaseMetaData(const String& name, IDBDatabaseMetadata*, bool& found) = 0;
    virtual bool getObjectStores(int64_t databaseId, IDBDatabaseMetadata::ObjectStoreMap* objectStores) = 0;
    virtual bool createIDBDatabaseMetaData(const String& name, const String& version, int64_t intVersion, int64_t& rowId) = 0;
    virtual bool keyExistsInObjectStore(IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBKey&, RefPtr<IDBRecordIdentifier>& foundIDBRecordIdentifier) = 0;

    virtual bool putIndexDataForRecord(IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&, const IDBRecordIdentifier*) = 0;
    virtual bool keyExistsInIndex(IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&, RefPtr<IDBKey>& foundPrimaryKey, bool& exists) = 0;

    virtual bool deleteDatabase(const String& name) = 0;
    virtual bool updateIDBDatabaseIntVersion(IDBBackingStoreInterface::Transaction*, int64_t rowId, int64_t intVersion) = 0;

    virtual bool getRecord(IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBKey&, Vector<char>& record) = 0;
    virtual bool putRecord(IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBKey&, PassRefPtr<SharedBuffer> value, IDBRecordIdentifier*) = 0;
    virtual bool deleteRecord(IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBRecordIdentifier&) = 0;

    virtual bool createObjectStore(IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId, const String& name, const IDBKeyPath&, bool autoIncrement) = 0;
    virtual bool deleteObjectStore(IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId) = 0;
    virtual bool clearObjectStore(IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId) = 0;

    virtual bool createIndex(IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const String& name, const IDBKeyPath&, bool isUnique, bool isMultiEntry) = 0;
    virtual bool deleteIndex(IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId) = 0;

    virtual PassRefPtr<IDBBackingStoreInterface::Cursor> openObjectStoreKeyCursor(IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBKeyRange*, IndexedDB::CursorDirection) = 0;
    virtual PassRefPtr<IDBBackingStoreInterface::Cursor> openObjectStoreCursor(IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId, const IDBKeyRange*, IndexedDB::CursorDirection) = 0;
    virtual PassRefPtr<IDBBackingStoreInterface::Cursor> openIndexKeyCursor(IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange*, IndexedDB::CursorDirection) = 0;
    virtual PassRefPtr<IDBBackingStoreInterface::Cursor> openIndexCursor(IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange*, IndexedDB::CursorDirection) = 0;

    virtual bool getPrimaryKeyViaIndex(IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&, RefPtr<IDBKey>& primaryKey) = 0;

    virtual bool getKeyGeneratorCurrentNumber(IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t& keyGeneratorCurrentNumber) = 0;
    virtual bool maybeUpdateKeyGeneratorCurrentNumber(IDBBackingStoreInterface::Transaction*, int64_t databaseId, int64_t objectStoreId, int64_t newState, bool checkCurrent) = 0;

    virtual bool makeIndexWriters(IDBTransactionBackendInterface&, int64_t databaseId, const IDBObjectStoreMetadata&, IDBKey& primaryKey, bool keyWasGenerated, const Vector<int64_t>& indexIds, const Vector<IndexKeys>&, Vector<RefPtr<IDBIndexWriter>>& indexWriters, String* errorMessage, bool& completed) = 0;

    virtual PassRefPtr<IDBKey> generateKey(IDBTransactionBackendInterface&, int64_t databaseId, int64_t objectStoreId) = 0;
    virtual bool updateKeyGenerator(IDBTransactionBackendInterface&, int64_t databaseId, int64_t objectStoreId, const IDBKey&, bool checkCurrent) = 0;

};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBBackingStoreTransactionInterface_h
