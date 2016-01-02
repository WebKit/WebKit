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

#ifndef IDBBackingStore_h
#define IDBBackingStore_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBDatabaseInfo.h"
#include "IDBError.h"

namespace WebCore {

class IDBCursorInfo;
class IDBGetResult;
class IDBIndexInfo;
class IDBKeyData;
class IDBObjectStoreInfo;
class IDBResourceIdentifier;
class IDBTransactionInfo;
class ThreadSafeDataBuffer;

struct IDBKeyRangeData;

namespace IndexedDB {
enum class IndexRecordType;
}

namespace IDBServer {

class IDBBackingStore {
public:
    virtual ~IDBBackingStore() { }

    virtual const IDBDatabaseInfo& getOrEstablishDatabaseInfo() = 0;

    virtual IDBError beginTransaction(const IDBTransactionInfo&) = 0;
    virtual IDBError abortTransaction(const IDBResourceIdentifier& transactionIdentifier) = 0;
    virtual IDBError commitTransaction(const IDBResourceIdentifier& transactionIdentifier) = 0;

    virtual IDBError createObjectStore(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo&) = 0;
    virtual IDBError deleteObjectStore(const IDBResourceIdentifier& transactionIdentifier, const String& objectStoreName) = 0;
    virtual IDBError clearObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier) = 0;
    virtual IDBError createIndex(const IDBResourceIdentifier& transactionIdentifier, const IDBIndexInfo&) = 0;
    virtual IDBError deleteIndex(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const String& indexName) = 0;
    virtual IDBError keyExistsInObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyData&, bool& keyExists) = 0;
    virtual IDBError deleteRange(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData&) = 0;
    virtual IDBError addRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyData&, const ThreadSafeDataBuffer& value) = 0;
    virtual IDBError getRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData&, ThreadSafeDataBuffer& outValue) = 0;
    virtual IDBError getIndexRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, IndexedDB::IndexRecordType, const IDBKeyRangeData&, IDBGetResult& outValue) = 0;
    virtual IDBError getCount(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const IDBKeyRangeData&, uint64_t& outCount) = 0;
    virtual IDBError generateKeyNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t& keyNumber) = 0;
    virtual IDBError maybeUpdateKeyGeneratorNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, double newKeyNumber) = 0;
    virtual IDBError openCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBCursorInfo&, IDBGetResult& outResult) = 0;
    virtual IDBError iterateCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBResourceIdentifier& cursorIdentifier, const IDBKeyData&, uint32_t count, IDBGetResult& outResult) = 0;

    virtual void deleteBackingStore() = 0;
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBBackingStore_h
