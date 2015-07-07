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

#ifndef IDBBackingStoreTransactionLevelDB_h
#define IDBBackingStoreTransactionLevelDB_h

#include "LevelDBTransaction.h"

#if ENABLE(INDEXED_DATABASE)
#if USE(LEVELDB)

namespace WebCore {

class IDBBackingStoreLevelDB;

class IDBBackingStoreTransactionLevelDB : public RefCounted<IDBBackingStoreTransactionLevelDB> {
public:
    static PassRefPtr<IDBBackingStoreTransactionLevelDB> create(int64_t transactionID, IDBBackingStoreLevelDB* backingStore)
    {
        return adoptRef(new IDBBackingStoreTransactionLevelDB(transactionID, backingStore));
    }

    ~IDBBackingStoreTransactionLevelDB();

    void begin();
    bool commit();
    void rollback();
    void resetTransaction();

    static LevelDBTransaction* levelDBTransactionFrom(IDBBackingStoreTransactionLevelDB& transaction)
    {
        return static_cast<IDBBackingStoreTransactionLevelDB&>(transaction).m_transaction.get();
    }

    int64_t transactionID() { return m_transactionID; }

private:
    IDBBackingStoreTransactionLevelDB(int64_t transactionID, IDBBackingStoreLevelDB*);

    int64_t m_transactionID;
    IDBBackingStoreLevelDB* m_backingStore;
    RefPtr<LevelDBTransaction> m_transaction;
};

} // namespace WebCore

#endif // USE(LEVELDB)
#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBBackingStoreTransactionLevelDB_h
