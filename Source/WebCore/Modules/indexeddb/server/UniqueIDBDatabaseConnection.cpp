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
#include "UniqueIDBDatabaseConnection.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionToClient.h"
#include "IDBTransactionInfo.h"
#include "Logging.h"

namespace WebCore {
namespace IDBServer {

static uint64_t nextDatabaseConnectionIdentifier()
{
    static uint64_t nextIdentifier = 0;
    return ++nextIdentifier;
}

Ref<UniqueIDBDatabaseConnection> UniqueIDBDatabaseConnection::create(UniqueIDBDatabase& database, IDBConnectionToClient& connection)
{
    return adoptRef(*new UniqueIDBDatabaseConnection(database, connection));
}

UniqueIDBDatabaseConnection::UniqueIDBDatabaseConnection(UniqueIDBDatabase& database, IDBConnectionToClient& connection)
    : m_identifier(nextDatabaseConnectionIdentifier())
    , m_database(database)
    , m_connectionToClient(connection)
{
}

void UniqueIDBDatabaseConnection::fireVersionChangeEvent(uint64_t requestedVersion)
{
    ASSERT(!m_closePending);
    m_connectionToClient.fireVersionChangeEvent(*this, requestedVersion);
}

Ref<UniqueIDBDatabaseTransaction> UniqueIDBDatabaseConnection::createVersionChangeTransaction(uint64_t newVersion)
{
    LOG(IndexedDB, "UniqueIDBDatabaseConnection::createVersionChangeTransaction");
    ASSERT(!m_closePending);

    IDBTransactionInfo info = IDBTransactionInfo::versionChange(m_connectionToClient, newVersion);

    Ref<UniqueIDBDatabaseTransaction> transaction = UniqueIDBDatabaseTransaction::create(*this, info);
    m_transactionMap.set(transaction->info().identifier(), &transaction.get());

    return WTF::move(transaction);
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
