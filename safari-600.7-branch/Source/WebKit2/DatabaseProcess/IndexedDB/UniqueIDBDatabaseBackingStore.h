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

#ifndef UniqueIDBDatabaseBackingStore_h
#define UniqueIDBDatabaseBackingStore_h

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include <WebCore/IndexedDB.h>
#include <wtf/RefCounted.h>

namespace WebCore {
class IDBKey;
class IDBKeyRange;
class SharedBuffer;

struct IDBDatabaseMetadata;
struct IDBGetResult;
struct IDBKeyData;
struct IDBObjectStoreMetadata;
}

namespace WebKit {

class IDBIdentifier;

class UniqueIDBDatabaseBackingStore : public RefCounted<UniqueIDBDatabaseBackingStore> {
public:
    virtual ~UniqueIDBDatabaseBackingStore() { }

    virtual std::unique_ptr<WebCore::IDBDatabaseMetadata> getOrEstablishMetadata() = 0;

    virtual bool establishTransaction(const IDBIdentifier& transactionIdentifier, const Vector<int64_t>& objectStoreIDs, WebCore::IndexedDB::TransactionMode) = 0;
    virtual bool beginTransaction(const IDBIdentifier&) = 0;
    virtual bool commitTransaction(const IDBIdentifier&) = 0;
    virtual bool resetTransaction(const IDBIdentifier&) = 0;
    virtual bool rollbackTransaction(const IDBIdentifier&) = 0;

    virtual bool changeDatabaseVersion(const IDBIdentifier& transactionIdentifier, uint64_t newVersion) = 0;
    virtual bool createObjectStore(const IDBIdentifier& transactionIdentifier, const WebCore::IDBObjectStoreMetadata&) = 0;
    virtual bool deleteObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID) = 0;
    virtual bool clearObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID) = 0;
    virtual bool createIndex(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBIndexMetadata&) = 0;
    virtual bool deleteIndex(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID) = 0;

    virtual bool generateKeyNumber(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t& generatedKey) = 0;
    virtual bool updateKeyGeneratorNumber(const IDBIdentifier& transactionIdentifier, int64_t objectStoreId, int64_t keyNumber, bool checkCurrent) = 0;

    virtual bool keyExistsInObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBKeyData&, bool& keyExists) = 0;
    virtual bool putRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBKeyData&, const uint8_t* valueBuffer, size_t valueSize) = 0;
    virtual bool putIndexRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const WebCore::IDBKeyData&, const WebCore::IDBKeyData& indexKey) = 0;
    virtual bool getIndexRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const WebCore::IDBKeyRangeData&, WebCore::IndexedDB::CursorType, WebCore::IDBGetResult&) = 0;
    virtual bool deleteRange(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBKeyRangeData&) = 0;
    virtual bool deleteRecord(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBKeyData&) = 0;

    virtual bool getKeyRecordFromObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBKey&, RefPtr<WebCore::SharedBuffer>& result) = 0;
    virtual bool getKeyRangeRecordFromObjectStore(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, const WebCore::IDBKeyRange&, RefPtr<WebCore::SharedBuffer>& result, RefPtr<WebCore::IDBKey>& resultKey) = 0;
    virtual bool count(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, const WebCore::IDBKeyRangeData&, int64_t& count) = 0;

    virtual bool openCursor(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID, int64_t indexID, WebCore::IndexedDB::CursorDirection, WebCore::IndexedDB::CursorType, WebCore::IDBDatabaseBackend::TaskType, const WebCore::IDBKeyRangeData&, int64_t& cursorID, WebCore::IDBKeyData&, WebCore::IDBKeyData&, Vector<uint8_t>&) = 0;
    virtual bool advanceCursor(const IDBIdentifier& cursorIdentifier, uint64_t count, WebCore::IDBKeyData&, WebCore::IDBKeyData&, Vector<uint8_t>&) = 0;
    virtual bool iterateCursor(const IDBIdentifier& cursorIdentifier, const WebCore::IDBKeyData&, WebCore::IDBKeyData&, WebCore::IDBKeyData&, Vector<uint8_t>&) = 0;
    virtual void notifyCursorsOfChanges(const IDBIdentifier& transactionIdentifier, int64_t objectStoreID) = 0;
};

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
#endif // UniqueIDBDatabaseBackingStore_h
