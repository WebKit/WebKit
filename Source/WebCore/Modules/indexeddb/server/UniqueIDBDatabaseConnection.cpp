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
#include "ServerOpenDBRequest.h"
#include "UniqueIDBDatabase.h"

namespace WebCore {
namespace IDBServer {

Ref<UniqueIDBDatabaseConnection> UniqueIDBDatabaseConnection::create(UniqueIDBDatabase& database, ServerOpenDBRequest& request)
{
    return adoptRef(*new UniqueIDBDatabaseConnection(database, request));
}

UniqueIDBDatabaseConnection::UniqueIDBDatabaseConnection(UniqueIDBDatabase& database, ServerOpenDBRequest& request)
    : m_database(&database)
    , m_connectionToClient(request.connection())
    , m_openRequestIdentifier(request.requestData().requestIdentifier())
{
    server()->registerDatabaseConnection(*this);
    m_connectionToClient->registerDatabaseConnection(*this);
}

UniqueIDBDatabaseConnection::~UniqueIDBDatabaseConnection()
{
    ASSERT(m_transactionMap.isEmpty());

    server()->unregisterDatabaseConnection(*this);
    m_connectionToClient->unregisterDatabaseConnection(*this);
}

bool UniqueIDBDatabaseConnection::hasNonFinishedTransactions() const
{
    return !m_transactionMap.isEmpty();
}

void UniqueIDBDatabaseConnection::abortTransactionWithoutCallback(UniqueIDBDatabaseTransaction& transaction)
{
    ASSERT(m_transactionMap.contains(transaction.info().identifier()));

    const auto& transactionIdentifier = transaction.info().identifier();
    RefPtr<UniqueIDBDatabaseConnection> protectedThis(this);

    m_database->abortTransaction(transaction, [this, protectedThis, transactionIdentifier](const IDBError&) {
        ASSERT(m_transactionMap.contains(transactionIdentifier));
        m_transactionMap.remove(transactionIdentifier);
    });
}

void UniqueIDBDatabaseConnection::connectionPendingCloseFromClient()
{
    LOG(IndexedDB, "UniqueIDBDatabaseConnection::connectionPendingCloseFromClient - %s - %" PRIu64, m_openRequestIdentifier.loggingString().utf8().data(), identifier());

    m_closePending = true;
}

void UniqueIDBDatabaseConnection::connectionClosedFromClient()
{
    LOG(IndexedDB, "UniqueIDBDatabaseConnection::connectionClosedFromClient - %s - %" PRIu64, m_openRequestIdentifier.loggingString().utf8().data(), identifier());

    m_database->connectionClosedFromClient(*this);
}

void UniqueIDBDatabaseConnection::didFireVersionChangeEvent(const IDBResourceIdentifier& requestIdentifier, IndexedDB::ConnectionClosedOnBehalfOfServer connectionClosed)
{
    LOG(IndexedDB, "UniqueIDBDatabaseConnection::didFireVersionChangeEvent - %s - %" PRIu64, m_openRequestIdentifier.loggingString().utf8().data(), identifier());

    m_database->didFireVersionChangeEvent(*this, requestIdentifier, connectionClosed);
}

void UniqueIDBDatabaseConnection::didFinishHandlingVersionChange(const IDBResourceIdentifier& transactionIdentifier)
{
    LOG(IndexedDB, "UniqueIDBDatabaseConnection::didFinishHandlingVersionChange - %s - %" PRIu64, transactionIdentifier.loggingString().utf8().data(), identifier());

    m_database->didFinishHandlingVersionChange(*this, transactionIdentifier);
}

void UniqueIDBDatabaseConnection::fireVersionChangeEvent(const IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion)
{
    ASSERT(!m_closePending);
    m_connectionToClient->fireVersionChangeEvent(*this, requestIdentifier, requestedVersion);
}

UniqueIDBDatabaseTransaction& UniqueIDBDatabaseConnection::createVersionChangeTransaction(uint64_t newVersion)
{
    LOG(IndexedDB, "UniqueIDBDatabaseConnection::createVersionChangeTransaction - %s - %" PRIu64, m_openRequestIdentifier.loggingString().utf8().data(), identifier());
    ASSERT(!m_closePending);

    IDBTransactionInfo info = IDBTransactionInfo::versionChange(m_connectionToClient, m_database->info(), newVersion);

    Ref<UniqueIDBDatabaseTransaction> transaction = UniqueIDBDatabaseTransaction::create(*this, info);
    m_transactionMap.set(transaction->info().identifier(), &transaction.get());

    return transaction.get();
}

void UniqueIDBDatabaseConnection::establishTransaction(const IDBTransactionInfo& info)
{
    LOG(IndexedDB, "UniqueIDBDatabaseConnection::establishTransaction - %s - %" PRIu64, m_openRequestIdentifier.loggingString().utf8().data(), identifier());

    ASSERT(info.mode() != IDBTransactionMode::Versionchange);

    // No transactions should ever come from the client after the client has already told us
    // the connection is closing.
    ASSERT(!m_closePending);

    Ref<UniqueIDBDatabaseTransaction> transaction = UniqueIDBDatabaseTransaction::create(*this, info);
    m_transactionMap.set(transaction->info().identifier(), &transaction.get());

    m_database->enqueueTransaction(WTFMove(transaction));
}

void UniqueIDBDatabaseConnection::didAbortTransaction(UniqueIDBDatabaseTransaction& transaction, const IDBError& error)
{
    LOG(IndexedDB, "UniqueIDBDatabaseConnection::didAbortTransaction - %s - %" PRIu64, m_openRequestIdentifier.loggingString().utf8().data(), identifier());

    auto transactionIdentifier = transaction.info().identifier();
    auto takenTransaction = m_transactionMap.take(transactionIdentifier);
    ASSERT(takenTransaction);

    m_connectionToClient->didAbortTransaction(transactionIdentifier, error);
}

void UniqueIDBDatabaseConnection::didCommitTransaction(UniqueIDBDatabaseTransaction& transaction, const IDBError& error)
{
    LOG(IndexedDB, "UniqueIDBDatabaseConnection::didCommitTransaction - %s - %" PRIu64, m_openRequestIdentifier.loggingString().utf8().data(), identifier());

    auto transactionIdentifier = transaction.info().identifier();

    ASSERT(m_transactionMap.contains(transactionIdentifier) || !error.isNull());
    m_transactionMap.remove(transactionIdentifier);

    m_connectionToClient->didCommitTransaction(transactionIdentifier, error);
}

void UniqueIDBDatabaseConnection::didCreateObjectStore(const IDBResultData& resultData)
{
    LOG(IndexedDB, "UniqueIDBDatabaseConnection::didCreateObjectStore");

    m_connectionToClient->didCreateObjectStore(resultData);
}

void UniqueIDBDatabaseConnection::didDeleteObjectStore(const IDBResultData& resultData)
{
    LOG(IndexedDB, "UniqueIDBDatabaseConnection::didDeleteObjectStore");

    m_connectionToClient->didDeleteObjectStore(resultData);
}

void UniqueIDBDatabaseConnection::didRenameObjectStore(const IDBResultData& resultData)
{
    LOG(IndexedDB, "UniqueIDBDatabaseConnection::didRenameObjectStore");

    m_connectionToClient->didRenameObjectStore(resultData);
}

void UniqueIDBDatabaseConnection::didClearObjectStore(const IDBResultData& resultData)
{
    LOG(IndexedDB, "UniqueIDBDatabaseConnection::didClearObjectStore");

    m_connectionToClient->didClearObjectStore(resultData);
}

void UniqueIDBDatabaseConnection::didCreateIndex(const IDBResultData& resultData)
{
    LOG(IndexedDB, "UniqueIDBDatabaseConnection::didCreateIndex");

    m_connectionToClient->didCreateIndex(resultData);
}

void UniqueIDBDatabaseConnection::didDeleteIndex(const IDBResultData& resultData)
{
    LOG(IndexedDB, "UniqueIDBDatabaseConnection::didDeleteIndex");

    m_connectionToClient->didDeleteIndex(resultData);
}

void UniqueIDBDatabaseConnection::didRenameIndex(const IDBResultData& resultData)
{
    LOG(IndexedDB, "UniqueIDBDatabaseConnection::didRenameIndex");

    m_connectionToClient->didRenameIndex(resultData);
}

bool UniqueIDBDatabaseConnection::connectionIsClosing() const
{
    return m_closePending;
}

void UniqueIDBDatabaseConnection::deleteTransaction(UniqueIDBDatabaseTransaction& transaction)
{
    LOG(IndexedDB, "UniqueIDBDatabaseConnection::deleteTransaction - %s", transaction.info().loggingString().utf8().data());
    
    auto transactionIdentifier = transaction.info().identifier();
    
    ASSERT(m_transactionMap.contains(transactionIdentifier));
    m_transactionMap.remove(transactionIdentifier);
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
