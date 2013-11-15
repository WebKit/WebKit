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

#include "IDBBackingStoreCursorInterface.h"
#include "IDBBackingStoreTransactionInterface.h"
#include "IDBDatabaseBackend.h"
#include "IDBMetadata.h"
#include "IndexedDB.h"

#include <functional>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBIndexWriterLevelDB;
class IDBKey;
class IDBKeyPath;
class IDBKeyRange;
class IDBRecordIdentifier;
class IDBTransactionBackend;
class SharedBuffer;

class IDBBackingStoreInterface : public RefCounted<IDBBackingStoreInterface> {
public:
    virtual ~IDBBackingStoreInterface() { }

    virtual void establishBackingStoreTransaction(int64_t transactionID) = 0;

    // New-style asynchronous callbacks
    typedef std::function<void (const IDBDatabaseMetadata&, bool success)> GetIDBDatabaseMetadataFunction;
    virtual void getOrEstablishIDBDatabaseMetadata(const String& name, GetIDBDatabaseMetadataFunction) = 0;

    typedef std::function<void (bool success)> BoolCallbackFunction;
    virtual void deleteDatabase(const String& name, BoolCallbackFunction) = 0;

    // Old-style synchronous callbacks
    virtual bool keyExistsInObjectStore(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, const IDBKey&, RefPtr<IDBRecordIdentifier>& foundIDBRecordIdentifier) = 0;

    virtual bool putIndexDataForRecord(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&, const IDBRecordIdentifier*) = 0;
    virtual bool keyExistsInIndex(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&, RefPtr<IDBKey>& foundPrimaryKey, bool& exists) = 0;

    virtual bool updateIDBDatabaseVersion(IDBBackingStoreTransactionInterface&, int64_t rowId, uint64_t version) = 0;

    virtual bool getRecord(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, const IDBKey&, Vector<char>& record) = 0;
    virtual bool putRecord(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, const IDBKey&, PassRefPtr<SharedBuffer> value, IDBRecordIdentifier*) = 0;
    virtual bool deleteRecord(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, const IDBRecordIdentifier&) = 0;

    virtual bool createObjectStore(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, const String& name, const IDBKeyPath&, bool autoIncrement) = 0;
    virtual bool deleteObjectStore(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId) = 0;
    virtual bool clearObjectStore(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId) = 0;

    virtual bool createIndex(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const String& name, const IDBKeyPath&, bool isUnique, bool isMultiEntry) = 0;
    virtual bool deleteIndex(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t indexId) = 0;

    virtual PassRefPtr<IDBBackingStoreCursorInterface> openObjectStoreKeyCursor(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, const IDBKeyRange*, IndexedDB::CursorDirection) = 0;
    virtual PassRefPtr<IDBBackingStoreCursorInterface> openObjectStoreCursor(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, const IDBKeyRange*, IndexedDB::CursorDirection) = 0;
    virtual PassRefPtr<IDBBackingStoreCursorInterface> openIndexKeyCursor(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange*, IndexedDB::CursorDirection) = 0;
    virtual PassRefPtr<IDBBackingStoreCursorInterface> openIndexCursor(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKeyRange*, IndexedDB::CursorDirection) = 0;

    virtual bool getPrimaryKeyViaIndex(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t indexId, const IDBKey&, RefPtr<IDBKey>& primaryKey) = 0;

    virtual bool getKeyGeneratorCurrentNumber(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t& keyGeneratorCurrentNumber) = 0;
    virtual bool maybeUpdateKeyGeneratorCurrentNumber(IDBBackingStoreTransactionInterface&, int64_t databaseId, int64_t objectStoreId, int64_t newState, bool checkCurrent) = 0;

    virtual bool makeIndexWriters(int64_t transactionID, int64_t databaseId, const IDBObjectStoreMetadata&, IDBKey& primaryKey, bool keyWasGenerated, const Vector<int64_t>& indexIds, const Vector<Vector<RefPtr<IDBKey>>>&, Vector<RefPtr<IDBIndexWriterLevelDB>>& indexWriters, String* errorMessage, bool& completed) = 0;

    virtual PassRefPtr<IDBKey> generateKey(IDBTransactionBackend&, int64_t databaseId, int64_t objectStoreId) = 0;
    virtual bool updateKeyGenerator(IDBTransactionBackend&, int64_t databaseId, int64_t objectStoreId, const IDBKey&, bool checkCurrent) = 0;

};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBBackingStoreInterface_h
