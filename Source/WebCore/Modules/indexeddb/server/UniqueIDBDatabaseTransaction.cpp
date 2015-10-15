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
#include "UniqueIDBDatabaseTransaction.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBServer.h"
#include "Logging.h"
#include "UniqueIDBDatabase.h"

namespace WebCore {
namespace IDBServer {

Ref<UniqueIDBDatabaseTransaction> UniqueIDBDatabaseTransaction::create(UniqueIDBDatabaseConnection& connection, IDBTransactionInfo& info)
{
    return adoptRef(*new UniqueIDBDatabaseTransaction(connection, info));
}

UniqueIDBDatabaseTransaction::UniqueIDBDatabaseTransaction(UniqueIDBDatabaseConnection& connection, IDBTransactionInfo& info)
    : m_databaseConnection(connection)
    , m_transactionInfo(info)
{
    m_databaseConnection->database().server().registerTransaction(*this);
}

UniqueIDBDatabaseTransaction::~UniqueIDBDatabaseTransaction()
{
    m_databaseConnection->database().transactionDestroyed(*this);
    m_databaseConnection->database().server().unregisterTransaction(*this);
}

void UniqueIDBDatabaseTransaction::commit()
{
    LOG(IndexedDB, "UniqueIDBDatabaseTransaction::commit");

    RefPtr<UniqueIDBDatabaseTransaction> self(this);
    m_databaseConnection->database().commitTransaction(*this, [this, self](const IDBError& error) {
        LOG(IndexedDB, "UniqueIDBDatabaseTransaction::commit (callback)");
        m_databaseConnection->didCommitTransaction(*this, error);
    });
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
