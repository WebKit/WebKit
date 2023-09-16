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
#include "IDBConnectionToClient.h"

#include "IDBDatabaseNameAndVersion.h"
#include "UniqueIDBDatabaseConnection.h"

namespace WebCore {
namespace IDBServer {

Ref<IDBConnectionToClient> IDBConnectionToClient::create(IDBConnectionToClientDelegate& delegate)
{
    return adoptRef(*new IDBConnectionToClient(delegate));
}

IDBConnectionToClient::IDBConnectionToClient(IDBConnectionToClientDelegate& delegate)
    : m_delegate(&delegate)
{
}

IDBConnectionIdentifier IDBConnectionToClient::identifier() const
{
    ASSERT(m_delegate);
    return m_delegate->identifier();
}

void IDBConnectionToClient::didDeleteDatabase(const IDBResultData& result)
{
    if (m_delegate)
        m_delegate->didDeleteDatabase(result);
}

void IDBConnectionToClient::didOpenDatabase(const IDBResultData& result)
{
    if (m_delegate)
        m_delegate->didOpenDatabase(result);
}

void IDBConnectionToClient::didAbortTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    if (m_delegate)
        m_delegate->didAbortTransaction(transactionIdentifier, error);
}

void IDBConnectionToClient::didCreateObjectStore(const IDBResultData& result)
{
    if (m_delegate)
        m_delegate->didCreateObjectStore(result);
}

void IDBConnectionToClient::didDeleteObjectStore(const IDBResultData& result)
{
    if (m_delegate)
        m_delegate->didDeleteObjectStore(result);
}

void IDBConnectionToClient::didRenameObjectStore(const IDBResultData& result)
{
    if (m_delegate)
        m_delegate->didRenameObjectStore(result);
}

void IDBConnectionToClient::didClearObjectStore(const IDBResultData& result)
{
    if (m_delegate)
        m_delegate->didClearObjectStore(result);
}

void IDBConnectionToClient::didCreateIndex(const IDBResultData& result)
{
    if (m_delegate)
        m_delegate->didCreateIndex(result);
}

void IDBConnectionToClient::didDeleteIndex(const IDBResultData& result)
{
    if (m_delegate)
        m_delegate->didDeleteIndex(result);
}

void IDBConnectionToClient::didRenameIndex(const IDBResultData& result)
{
    if (m_delegate)
        m_delegate->didRenameIndex(result);
}

void IDBConnectionToClient::didPutOrAdd(const IDBResultData& result)
{
    if (m_delegate)
        m_delegate->didPutOrAdd(result);
}

void IDBConnectionToClient::didGetRecord(const IDBResultData& result)
{
    if (m_delegate)
        m_delegate->didGetRecord(result);
}

void IDBConnectionToClient::didGetAllRecords(const IDBResultData& result)
{
    if (m_delegate)
        m_delegate->didGetAllRecords(result);
}

void IDBConnectionToClient::didGetCount(const IDBResultData& result)
{
    if (m_delegate)
        m_delegate->didGetCount(result);
}

void IDBConnectionToClient::didDeleteRecord(const IDBResultData& result)
{
    if (m_delegate)
        m_delegate->didDeleteRecord(result);
}

void IDBConnectionToClient::didOpenCursor(const IDBResultData& result)
{
    if (m_delegate)
        m_delegate->didOpenCursor(result);
}

void IDBConnectionToClient::didIterateCursor(const IDBResultData& result)
{
    if (m_delegate)
        m_delegate->didIterateCursor(result);
}

void IDBConnectionToClient::didCommitTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    if (m_delegate)
        m_delegate->didCommitTransaction(transactionIdentifier, error);
}

void IDBConnectionToClient::fireVersionChangeEvent(UniqueIDBDatabaseConnection& connection, const IDBResourceIdentifier& requestIdentifier, uint64_t requestedVersion)
{
    if (m_delegate)
        m_delegate->fireVersionChangeEvent(connection, requestIdentifier, requestedVersion);
}

void IDBConnectionToClient::didStartTransaction(const IDBResourceIdentifier& transactionIdentifier, const IDBError& error)
{
    if (m_delegate)
        m_delegate->didStartTransaction(transactionIdentifier, error);
}

void IDBConnectionToClient::didCloseFromServer(UniqueIDBDatabaseConnection& connection, const IDBError& error)
{
    if (m_delegate)
        m_delegate->didCloseFromServer(connection, error);
}

void IDBConnectionToClient::notifyOpenDBRequestBlocked(const IDBResourceIdentifier& requestIdentifier, uint64_t oldVersion, uint64_t newVersion)
{
    if (m_delegate)
        m_delegate->notifyOpenDBRequestBlocked(requestIdentifier, oldVersion, newVersion);
}

void IDBConnectionToClient::didGetAllDatabaseNamesAndVersions(const IDBResourceIdentifier& requestIdentifier, Vector<IDBDatabaseNameAndVersion>&& databases)
{
    if (m_delegate)
        m_delegate->didGetAllDatabaseNamesAndVersions(requestIdentifier, WTFMove(databases));
}

void IDBConnectionToClient::registerDatabaseConnection(UniqueIDBDatabaseConnection& connection)
{
    ASSERT(!m_databaseConnections.contains(&connection));
    m_databaseConnections.add(&connection);
}

void IDBConnectionToClient::unregisterDatabaseConnection(UniqueIDBDatabaseConnection& connection)
{
    m_databaseConnections.remove(&connection);
}

void IDBConnectionToClient::connectionToClientClosed()
{
    m_isClosed = true;
    auto databaseConnections = m_databaseConnections;

    for (RefPtr connection : databaseConnections) {
        ASSERT(m_databaseConnections.contains(connection.get()));
        connection->connectionClosedFromClient();
    }

    ASSERT(m_databaseConnections.isEmpty());
}

} // namespace IDBServer
} // namespace WebCore
