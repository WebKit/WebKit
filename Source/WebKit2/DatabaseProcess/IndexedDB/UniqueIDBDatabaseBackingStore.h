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

struct IDBDatabaseMetadata;
struct IDBObjectStoreMetadata;
}

namespace WebKit {

class IDBTransactionIdentifier;

class UniqueIDBDatabaseBackingStore : public RefCounted<UniqueIDBDatabaseBackingStore> {
public:
    virtual ~UniqueIDBDatabaseBackingStore() { }

    virtual std::unique_ptr<WebCore::IDBDatabaseMetadata> getOrEstablishMetadata() = 0;

    virtual bool establishTransaction(const IDBTransactionIdentifier&, const Vector<int64_t>& objectStoreIDs, WebCore::IndexedDB::TransactionMode) = 0;
    virtual bool beginTransaction(const IDBTransactionIdentifier&) = 0;
    virtual bool commitTransaction(const IDBTransactionIdentifier&) = 0;
    virtual bool resetTransaction(const IDBTransactionIdentifier&) = 0;
    virtual bool rollbackTransaction(const IDBTransactionIdentifier&) = 0;

    virtual bool changeDatabaseVersion(const IDBTransactionIdentifier&, uint64_t newVersion) = 0;
    virtual bool createObjectStore(const IDBTransactionIdentifier&, const WebCore::IDBObjectStoreMetadata&) = 0;
    virtual bool deleteObjectStore(const IDBTransactionIdentifier&, int64_t objectStoreID) = 0;

    virtual PassRefPtr<WebCore::IDBKey> generateKey(const IDBTransactionIdentifier&, int64_t objectStoreID) = 0;
    virtual bool keyExistsInObjectStore(const IDBTransactionIdentifier&, int64_t objectStoreID, const WebCore::IDBKey&, bool& keyExists) = 0;
    virtual bool putRecord(const IDBTransactionIdentifier&, int64_t objectStoreID, const WebCore::IDBKey&, const uint8_t* valueBuffer, size_t valueSize) = 0;
    virtual bool updateKeyGenerator(const IDBTransactionIdentifier&, int64_t objectStoreId, const WebCore::IDBKey&, bool checkCurrent) = 0;


};

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
#endif // UniqueIDBDatabaseBackingStore_h
