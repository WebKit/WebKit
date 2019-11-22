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
#include <pal/SessionID.h>
#include <wtf/HashMap.h>

namespace WebCore {
namespace IDBServer {

class MemoryObjectStore;

class MemoryIDBBackingStore : public IDBBackingStore {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<MemoryIDBBackingStore> create(PAL::SessionID, const IDBDatabaseIdentifier&);
    
    MemoryIDBBackingStore(PAL::SessionID, const IDBDatabaseIdentifier&);
    ~MemoryIDBBackingStore() final;

    IDBError getOrEstablishDatabaseInfo(IDBDatabaseInfo&, const LockHolder&) final;
    IDBError getOrEstablishDatabaseInfo(IDBDatabaseInfo&);
    void setDatabaseInfo(const IDBDatabaseInfo&);

    IDBError beginTransaction(const IDBTransactionInfo&, const LockHolder&) final;
    IDBError abortTransaction(const IDBResourceIdentifier& transactionIdentifier, const LockHolder&) final;
    IDBError commitTransaction(const IDBResourceIdentifier& transactionIdentifier, const LockHolder&) final;
    IDBError createObjectStore(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo&, const LockHolder&) final;
    IDBError deleteObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const LockHolder&) final;
    IDBError renameObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const String& newName, const LockHolder&) final;
    IDBError clearObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const LockHolder&) final;
    IDBError createIndex(const IDBResourceIdentifier& transactionIdentifier, const IDBIndexInfo&, const LockHolder&) final;
    IDBError deleteIndex(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const LockHolder&) final;
    IDBError renameIndex(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const String& newName, const LockHolder&) final;
    IDBError keyExistsInObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyData&, bool& keyExists, const LockHolder&) final;
    IDBError deleteRange(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData&, const LockHolder&) final;
    IDBError addRecord(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo&, const IDBKeyData&, const IDBValue&, const LockHolder&) final;
    IDBError getRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyRangeData&, IDBGetRecordDataType, IDBGetResult& outValue, const LockHolder&) final;
    IDBError getAllRecords(const IDBResourceIdentifier& transactionIdentifier, const IDBGetAllRecordsData&, IDBGetAllResult& outValue, const LockHolder&) final;
    IDBError getIndexRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, IndexedDB::IndexRecordType, const IDBKeyRangeData&, IDBGetResult& outValue, const LockHolder&) final;
    IDBError getCount(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t indexIdentifier, const IDBKeyRangeData&, uint64_t& outCount, const LockHolder&) final;
    IDBError generateKeyNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t& keyNumber, const LockHolder&) final;
    IDBError revertGeneratedKeyNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, uint64_t keyNumber, const LockHolder&) final;
    IDBError maybeUpdateKeyGeneratorNumber(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, double newKeyNumber, const LockHolder&) final;
    IDBError openCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBCursorInfo&, IDBGetResult& outResult, const LockHolder&) final;
    IDBError iterateCursor(const IDBResourceIdentifier& transactionIdentifier, const IDBResourceIdentifier& cursorIdentifier, const IDBIterateCursorData&, IDBGetResult& outResult, const LockHolder&) final;
    bool prefetchCursor(const IDBResourceIdentifier&, const IDBResourceIdentifier&, const LockHolder&) final { return false; }

    IDBObjectStoreInfo* infoForObjectStore(uint64_t objectStoreIdentifier, const LockHolder&) final;
    void deleteBackingStore(const LockHolder&) final;

    bool supportsSimultaneousTransactions(const LockHolder&) final { return true; }
    bool isEphemeral(const LockHolder&) final { return true; }

    void removeObjectStoreForVersionChangeAbort(MemoryObjectStore&);
    void restoreObjectStoreForVersionChangeAbort(Ref<MemoryObjectStore>&&);

    bool hasTransaction(const IDBResourceIdentifier& identifier, const LockHolder&) const final { return m_transactions.contains(identifier); }

private:
    RefPtr<MemoryObjectStore> takeObjectStoreByIdentifier(uint64_t identifier);

    void close(const LockHolder&) final;

    IDBDatabaseIdentifier m_identifier;
    PAL::SessionID m_sessionID;
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
