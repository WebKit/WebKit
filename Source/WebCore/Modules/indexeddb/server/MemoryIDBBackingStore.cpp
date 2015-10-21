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

#include "config.h"
#include "MemoryIDBBackingStore.h"

#if ENABLE(INDEXED_DATABASE)

#include "Logging.h"

namespace WebCore {
namespace IDBServer {

std::unique_ptr<MemoryIDBBackingStore> MemoryIDBBackingStore::create(const IDBDatabaseIdentifier& identifier)
{
    return std::make_unique<MemoryIDBBackingStore>(identifier);
}

MemoryIDBBackingStore::MemoryIDBBackingStore(const IDBDatabaseIdentifier& identifier)
    : m_identifier(identifier)
{
}

MemoryIDBBackingStore::~MemoryIDBBackingStore()
{
}

const IDBDatabaseInfo& MemoryIDBBackingStore::getOrEstablishDatabaseInfo()
{
    if (!m_databaseInfo)
        m_databaseInfo = std::make_unique<IDBDatabaseInfo>(m_identifier.databaseName(), 0);

    return *m_databaseInfo;
}

void MemoryIDBBackingStore::setDatabaseInfo(const IDBDatabaseInfo& info)
{
    // It is not valid to directly set database info on a backing store that hasn't already set its own database info.
    ASSERT(m_databaseInfo);

    m_databaseInfo = std::make_unique<IDBDatabaseInfo>(info);
}

IDBError MemoryIDBBackingStore::beginTransaction(const IDBTransactionInfo& info)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::beginTransaction");

    if (m_transactions.contains(info.identifier()))
        return IDBError(IDBExceptionCode::InvalidStateError, "Backing store asked to create transaction it already has a record of");

    auto transaction = MemoryBackingStoreTransaction::create(*this, info);
    m_transactions.set(info.identifier(), WTF::move(transaction));

    return IDBError();
}

IDBError MemoryIDBBackingStore::abortTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::abortTransaction");

    auto transaction = m_transactions.take(transactionIdentifier);
    if (!transaction)
        return IDBError(IDBExceptionCode::InvalidStateError, "Backing store asked to abort transaction it didn't have record of");

    transaction->abort();

    return IDBError();
}

IDBError MemoryIDBBackingStore::commitTransaction(const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "MemoryIDBBackingStore::commitTransaction");

    auto transaction = m_transactions.take(transactionIdentifier);
    if (!transaction)
        return IDBError(IDBExceptionCode::InvalidStateError, "Backing store asked to commit transaction it didn't have record of");

    transaction->commit();

    return IDBError();
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
