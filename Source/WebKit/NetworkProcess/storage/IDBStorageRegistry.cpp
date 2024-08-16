/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "IDBStorageRegistry.h"

#include "IDBStorageConnectionToClient.h"
#include <WebCore/UniqueIDBDatabaseConnection.h>
#include <WebCore/UniqueIDBDatabaseTransaction.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

IDBStorageRegistry::IDBStorageRegistry() = default;

WTF_MAKE_TZONE_ALLOCATED_IMPL(IDBStorageRegistry);

WebCore::IDBServer::IDBConnectionToClient& IDBStorageRegistry::ensureConnectionToClient(IPC::Connection::UniqueID connection, WebCore::IDBConnectionIdentifier identifier)
{
    auto addResult = m_connectionsToClient.add(identifier, nullptr);
    if (addResult.isNewEntry)
        addResult.iterator->value = makeUnique<IDBStorageConnectionToClient>(connection, identifier);

    ASSERT(addResult.iterator->value->ipcConnection() == connection);
    return addResult.iterator->value->connectionToClient();
}

void IDBStorageRegistry::removeConnectionToClient(IPC::Connection::UniqueID connection)
{
    auto allConnectionsToClient = std::exchange(m_connectionsToClient, { });
    for (auto& [identifier, connectionToClient] : allConnectionsToClient) {
        if (connectionToClient->ipcConnection() != connection) {
            m_connectionsToClient.add(identifier, WTFMove(connectionToClient));
            continue;
        }
        connectionToClient->connectionToClient().connectionToClientClosed();
    }
}

void IDBStorageRegistry::registerConnection(WebCore::IDBServer::UniqueIDBDatabaseConnection& connection)
{
    auto identifier = connection.identifier();
    ASSERT(!m_connections.contains(identifier));

    m_connections.add(identifier, connection);
}

void IDBStorageRegistry::unregisterConnection(WebCore::IDBServer::UniqueIDBDatabaseConnection& connection)
{
    auto identifier = connection.identifier();
    ASSERT(m_connections.contains(identifier));

    m_connections.remove(identifier);
}

void IDBStorageRegistry::registerTransaction(WebCore::IDBServer::UniqueIDBDatabaseTransaction& transaction)
{
    auto identifier = transaction.info().identifier();
    ASSERT(!m_transactions.contains(identifier));

    m_transactions.add(identifier, transaction);
}

void IDBStorageRegistry::unregisterTransaction(WebCore::IDBServer::UniqueIDBDatabaseTransaction& transaction)
{
    auto identifier = transaction.info().identifier();
    ASSERT(m_transactions.contains(identifier));

    m_transactions.remove(identifier);
}

WebCore::IDBServer::UniqueIDBDatabaseConnection* IDBStorageRegistry::connection(WebCore::IDBDatabaseConnectionIdentifier identifier)
{
    return m_connections.get(identifier).get();
}

WebCore::IDBServer::UniqueIDBDatabaseTransaction* IDBStorageRegistry::transaction(WebCore::IDBResourceIdentifier identifier)
{
    return m_transactions.get(identifier).get();
}

} // namespace WebKit
