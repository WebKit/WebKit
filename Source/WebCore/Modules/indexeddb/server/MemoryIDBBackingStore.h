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

#pragma once

#if ENABLE(INDEXED_DATABASE)

#include "IDBBackingStore.h"
#include "IDBDatabaseIdentifier.h"
#include "IDBResourceIdentifier.h"
#include "MemoryBackingStoreTransaction.h"
#include <wtf/HashMap.h>

namespace WebCore {
namespace IDBServer {

class MemoryObjectStore;

class MemoryIDBBackingStore : public IDBBackingStore {
public:
    static std::unique_ptr<MemoryIDBBackingStore> create(const IDBDatabaseIdentifier&);
    
    MemoryIDBBackingStore(const IDBDatabaseIdentifier&);
    ~MemoryIDBBackingStore() final;

    IDBError getOrEstablishDatabaseInfo(IDBDatabaseInfo&) final;
    void setDatabaseInfo(const IDBDatabaseInfo&);

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
    IDBError addRecord(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo&, const IDBKeyData&, const IDBValue&) final;
    IDBError getRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData&, IDBGetRecordDataType, IDBGetResult& outValue) final;
    IDBError getAllRecords(const IDBResourceIdentifier& transactionIdentifier, const IDBGetAllRecordsData&, IDBGetAllResult& outValue) final;
    IDBError getIndexRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, IndexedDB::IndexRecordType, const IDBKeyRangeData&, IDBGetResult& outValue) final;
    IDBError getCount(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const IDBKeyRangeData&, uint64_t& outCount) final;
    IDBError generateKeyNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t& keyNumber) final;
    IDBError revertGeneratedKeyNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t keyNumber) final;
    IDBError maybeUpdateKeyGeneratorNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, double newKeyNumber) final;
    IDBError openCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBCursorInfo&, IDBGetResult& outResult) final;
    IDBError iterateCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBResourceIdentifier& cursorIdentifier, const IDBIterateCursorData&, IDBGetResult& outResult) final;
    bool prefetchCursor(const IDBResourceIdentifier&, const IDBResourceIdentifier&) final { return false; }

    IDBObjectStoreInfo* infoForObjectStore(uint64_t objectStoreIdentifier) final;
    void deleteBackingStore() final;

    bool supportsSimultaneousTransactions() final { return true; }
    bool isEphemeral() final { return true; }

    void setQuota(uint64_t quota) final { UNUSED_PARAM(quota); };

    void removeObjectStoreForVersionChangeAbort(MemoryObjectStore&);
    void restoreObjectStoreForVersionChangeAbort(Ref<MemoryObjectStore>&&);

private:
    RefPtr<MemoryObjectStore> takeObjectStoreByIdentifier(uint64_t identifier);

    IDBDatabaseIdentifier m_identifier;
    std::unique_ptr<IDBDatabaseInfo> m_databaseInfo;

    HashMap<IDBResourceIdentifier, std::unique_ptr<MemoryBackingStoreTransaction>> m_transactions;

    void registerObjectStore(Ref<MemoryObjectStore>&&);
    void unregisterObjectStore(MemoryObjectStore&);
    HashMap<uint64_t, RefPtr<MemoryObjectStore>> m_objectStoresByIdentifier;
    HashMap<String, MemoryObjectStore*> m_objectStoresByName;
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
