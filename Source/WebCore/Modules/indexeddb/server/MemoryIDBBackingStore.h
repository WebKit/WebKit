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

#ifndef MemoryIDBBackingStore_h
#define MemoryIDBBackingStore_h

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
    friend std::unique_ptr<MemoryIDBBackingStore> std::make_unique<MemoryIDBBackingStore>(const WebCore::IDBDatabaseIdentifier&);
public:
    static std::unique_ptr<MemoryIDBBackingStore> create(const IDBDatabaseIdentifier&);
    
    ~MemoryIDBBackingStore() override final;

    IDBError getOrEstablishDatabaseInfo(IDBDatabaseInfo&) override final;
    void setDatabaseInfo(const IDBDatabaseInfo&);

    IDBError beginTransaction(const IDBTransactionInfo&) override final;
    IDBError abortTransaction(const IDBResourceIdentifier& transactionIdentifier) override final;
    IDBError commitTransaction(const IDBResourceIdentifier& transactionIdentifier) override final;
    IDBError createObjectStore(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo&) override final;
    IDBError deleteObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier) override final;
    IDBError clearObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier) override final;
    IDBError createIndex(const IDBResourceIdentifier& transactionIdentifier, const IDBIndexInfo&) override final;
    IDBError deleteIndex(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier) override final;
    IDBError keyExistsInObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyData&, bool& keyExists) override final;
    IDBError deleteRange(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData&) override final;
    IDBError addRecord(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo&, const IDBKeyData&, const ThreadSafeDataBuffer& value) override final;
    IDBError getRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData&, ThreadSafeDataBuffer& outValue) override final;
    IDBError getIndexRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, IndexedDB::IndexRecordType, const IDBKeyRangeData&, IDBGetResult& outValue) override final;
    IDBError getCount(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const IDBKeyRangeData&, uint64_t& outCount) override final;
    IDBError generateKeyNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t& keyNumber) override final;
    IDBError revertGeneratedKeyNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t keyNumber) override final;
    IDBError maybeUpdateKeyGeneratorNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, double newKeyNumber) override final;
    IDBError openCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBCursorInfo&, IDBGetResult& outResult) override final;
    IDBError iterateCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBResourceIdentifier& cursorIdentifier, const IDBKeyData&, uint32_t count, IDBGetResult& outResult) override final;

    IDBObjectStoreInfo* infoForObjectStore(uint64_t objectStoreIdentifier) override final;
    void deleteBackingStore() override final;

    bool supportsSimultaneousTransactions() override final { return true; }
    bool isEphemeral() override final { return true; }

    void removeObjectStoreForVersionChangeAbort(MemoryObjectStore&);
    void restoreObjectStoreForVersionChangeAbort(Ref<MemoryObjectStore>&&);

private:
    MemoryIDBBackingStore(const IDBDatabaseIdentifier&);

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
#endif // MemoryIDBBackingStore_h
