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
    
    virtual ~MemoryIDBBackingStore() override final;

    virtual const IDBDatabaseInfo& getOrEstablishDatabaseInfo() override final;
    void setDatabaseInfo(const IDBDatabaseInfo&);

    virtual IDBError beginTransaction(const IDBTransactionInfo&) override final;
    virtual IDBError abortTransaction(const IDBResourceIdentifier& transactionIdentifier) override final;
    virtual IDBError commitTransaction(const IDBResourceIdentifier& transactionIdentifier) override final;
    virtual IDBError createObjectStore(const IDBResourceIdentifier& transactionIdentifier, const IDBObjectStoreInfo&) override final;
    virtual IDBError keyExistsInObjectStore(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyData&, bool& keyExists) override final;
    virtual IDBError deleteRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyData&) override final;
    virtual IDBError putRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyData&, const ThreadSafeDataBuffer& value) override final;
    virtual IDBError getRecord(const IDBResourceIdentifier& transactionIdentifier, uint64_t objectStoreIdentifier, const IDBKeyData&, ThreadSafeDataBuffer& outValue) override final;

    void removeObjectStoreForVersionChangeAbort(MemoryObjectStore&);

private:
    MemoryIDBBackingStore(const IDBDatabaseIdentifier&);

    IDBDatabaseIdentifier m_identifier;
    std::unique_ptr<IDBDatabaseInfo> m_databaseInfo;

    HashMap<IDBResourceIdentifier, std::unique_ptr<MemoryBackingStoreTransaction>> m_transactions;
    HashMap<uint64_t, std::unique_ptr<MemoryObjectStore>> m_objectStores;
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // MemoryIDBBackingStore_h
