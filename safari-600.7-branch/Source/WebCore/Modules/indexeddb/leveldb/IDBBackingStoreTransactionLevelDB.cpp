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

#include "config.h"
#include "IDBBackingStoreTransactionLevelDB.h"

#if ENABLE(INDEXED_DATABASE) && USE(LEVELDB)

#include "IDBBackingStoreLevelDB.h"
#include "Logging.h"
#include <wtf/text/CString.h>

namespace WebCore {

IDBBackingStoreTransactionLevelDB::IDBBackingStoreTransactionLevelDB(int64_t transactionID, IDBBackingStoreLevelDB* backingStore)
    : m_transactionID(transactionID)
    , m_backingStore(backingStore)
{
}

IDBBackingStoreTransactionLevelDB::~IDBBackingStoreTransactionLevelDB()
{
    if (m_backingStore)
        m_backingStore->removeBackingStoreTransaction(this);
}

void IDBBackingStoreTransactionLevelDB::begin()
{
    LOG(StorageAPI, "IDBBackingStoreTransactionLevelDB::begin");
    ASSERT(!m_transaction);
    m_transaction = LevelDBTransaction::create(reinterpret_cast<IDBBackingStoreLevelDB*>(m_backingStore)->levelDBDatabase());
}

bool IDBBackingStoreTransactionLevelDB::commit()
{
    LOG(StorageAPI, "IDBBackingStoreTransactionLevelDB::commit");
    ASSERT(m_transaction);
    bool result = m_transaction->commit();
    m_transaction.clear();
    if (!result)
        LOG_ERROR("IndexedDB TransactionCommit Error");
    return result;
}

void IDBBackingStoreTransactionLevelDB::rollback()
{
    LOG(StorageAPI, "IDBBackingStoreTransactionLevelDB::rollback");
    ASSERT(m_transaction);
    m_transaction->rollback();
    m_transaction.clear();
}

void IDBBackingStoreTransactionLevelDB::resetTransaction()
{
    ASSERT(m_backingStore);
    m_backingStore->removeBackingStoreTransaction(this);
    m_backingStore = 0;
    m_transaction = 0;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE) && USE(LEVELDB)
