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
#include "IDBConnectionToServer.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBOpenDBRequestImpl.h"
#include "IDBRequestData.h"
#include "IDBResultData.h"
#include "Logging.h"

namespace WebCore {
namespace IDBClient {

Ref<IDBConnectionToServer> IDBConnectionToServer::create(IDBConnectionToServerDelegate& delegate)
{
    return adoptRef(*new IDBConnectionToServer(delegate));
}

IDBConnectionToServer::IDBConnectionToServer(IDBConnectionToServerDelegate& delegate)
    : m_delegate(delegate)
{
}

uint64_t IDBConnectionToServer::identifier() const
{
    return m_delegate->identifier();
}

void IDBConnectionToServer::deleteDatabase(IDBOpenDBRequest& request)
{
    LOG(IndexedDB, "IDBConnectionToServer::deleteDatabase - %s", request.databaseIdentifier().debugString().utf8().data());

    ASSERT(!m_openDBRequestMap.contains(request.resourceIdentifier()));
    m_openDBRequestMap.set(request.resourceIdentifier(), &request);
    
    IDBRequestData requestData(*this, request);
    m_delegate->deleteDatabase(requestData);
}

void IDBConnectionToServer::didDeleteDatabase(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didDeleteDatabase");

    auto request = m_openDBRequestMap.take(resultData.requestIdentifier());
    ASSERT(request);

    request->requestCompleted(resultData);
}

void IDBConnectionToServer::openDatabase(IDBOpenDBRequest& request)
{
    LOG(IndexedDB, "IDBConnectionToServer::openDatabase - %s", request.databaseIdentifier().debugString().utf8().data());

    ASSERT(!m_openDBRequestMap.contains(request.resourceIdentifier()));
    m_openDBRequestMap.set(request.resourceIdentifier(), &request);
    
    IDBRequestData requestData(*this, request);
    m_delegate->openDatabase(requestData);
}

void IDBConnectionToServer::didOpenDatabase(const IDBResultData& resultData)
{
    LOG(IndexedDB, "IDBConnectionToServer::didOpenDatabase");

    auto request = m_openDBRequestMap.take(resultData.requestIdentifier());
    ASSERT(request);

    request->requestCompleted(resultData);
}

void IDBConnectionToServer::commitTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBConnectionToServer::commitTransaction");
    ASSERT(!m_committingTransactions.contains(transaction.info().identifier()));
    m_committingTransactions.set(transaction.info().identifier(), &transaction);

    auto identifier = transaction.info().identifier();
    m_delegate->commitTransaction(identifier);
}

void IDBConnectionToServer::didCommitTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    LOG(IndexedDB, "IDBConnectionToServer::didCommitTransaction");

    auto transaction = m_committingTransactions.take(transactionIdentifier);
    ASSERT(transaction);

    transaction->didCommit(error);
}

void IDBConnectionToServer::abortTransaction(IDBTransaction& transaction)
{
    LOG(IndexedDB, "IDBConnectionToServer::abortTransaction");
    ASSERT(!m_abortingTransactions.contains(transaction.info().identifier()));
    m_abortingTransactions.set(transaction.info().identifier(), &transaction);

    auto identifier = transaction.info().identifier();
    m_delegate->abortTransaction(identifier);
}

void IDBConnectionToServer::didAbortTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    LOG(IndexedDB, "IDBConnectionToServer::didAbortTransaction");

    auto transaction = m_abortingTransactions.take(transactionIdentifier);
    ASSERT(transaction);

    transaction->didAbort(error);
}

void IDBConnectionToServer::fireVersionChangeEvent(uint64_t databaseConnectionIdentifier, uint64_t requestedVersion)
{
    LOG(IndexedDB, "IDBConnectionToServer::fireVersionChangeEvent");

    auto connection = m_databaseConnectionMap.get(databaseConnectionIdentifier);
    if (!connection)
        return;

    connection->fireVersionChangeEvent(requestedVersion);
}

void IDBConnectionToServer::databaseConnectionClosed(IDBDatabase& database)
{
    LOG(IndexedDB, "IDBConnectionToServer::databaseConnectionClosed");

    m_delegate->databaseConnectionClosed(database.databaseConnectionIdentifier());
}

void IDBConnectionToServer::registerDatabaseConnection(IDBDatabase& database)
{
    ASSERT(!m_databaseConnectionMap.contains(database.databaseConnectionIdentifier()));
    m_databaseConnectionMap.set(database.databaseConnectionIdentifier(), &database);
}

void IDBConnectionToServer::unregisterDatabaseConnection(IDBDatabase& database)
{
    ASSERT(m_databaseConnectionMap.contains(database.databaseConnectionIdentifier()));
    ASSERT(m_databaseConnectionMap.get(database.databaseConnectionIdentifier()) == &database);
    m_databaseConnectionMap.remove(database.databaseConnectionIdentifier());
}

} // namespace IDBClient
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
