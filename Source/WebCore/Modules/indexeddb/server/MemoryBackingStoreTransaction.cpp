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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MemoryBackingStoreTransaction.h"

#if ENABLE(INDEXED_DATABASE)

#include "IndexedDB.h"
#include "Logging.h"
#include "MemoryIDBBackingStore.h"

namespace WebCore {
namespace IDBServer {

std::unique_ptr<MemoryBackingStoreTransaction> MemoryBackingStoreTransaction::create(MemoryIDBBackingStore& backingStore, const IDBTransactionInfo& info)
{
    return std::make_unique<MemoryBackingStoreTransaction>(backingStore, info);
}

MemoryBackingStoreTransaction::MemoryBackingStoreTransaction(MemoryIDBBackingStore& backingStore, const IDBTransactionInfo& info)
    : m_backingStore(backingStore)
    , m_info(info)
{
    if (m_info.mode() == IndexedDB::TransactionMode::VersionChange)
        m_originalDatabaseInfo = std::make_unique<IDBDatabaseInfo>(m_backingStore.getOrEstablishDatabaseInfo());
}

MemoryBackingStoreTransaction::~MemoryBackingStoreTransaction()
{
    ASSERT(!m_inProgress);
}

void MemoryBackingStoreTransaction::abort()
{
    LOG(IndexedDB, "MemoryBackingStoreTransaction::abort()");

    if (m_originalDatabaseInfo) {
        ASSERT(m_info.mode() == IndexedDB::TransactionMode::VersionChange);
        m_backingStore.setDatabaseInfo(*m_originalDatabaseInfo);
    }

    m_inProgress = false;
}

void MemoryBackingStoreTransaction::commit()
{
    LOG(IndexedDB, "MemoryBackingStoreTransaction::commit()");

    m_inProgress = false;
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
