/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#include "Logging.h"
#include <WebCore/UniqueIDBDatabaseConnection.h>
#include <WebCore/UniqueIDBDatabaseTransaction.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK_WITH_RETURN_VALUE(assertion, connection, returnValue) MESSAGE_CHECK_WITH_RETURN_VALUE_BASE(assertion, connection, returnValue)

namespace WebKit {

IDBStorageRegistry::IDBStorageRegistry() = default;

IDBStorageRegistry::~IDBStorageRegistry() = default;

WTF_MAKE_TZONE_ALLOCATED_IMPL(IDBStorageRegistry);

WebCore::IDBServer::IDBConnectionToClient* IDBStorageRegistry::ensureConnectionToClient(IPC::Connection& ipcConnection, const WebCore::IDBResourceIdentifier& requestIdentifier, NetworkStorageManager& networkStorageManager)
{
    MESSAGE_CHECK_WITH_RETURN_VALUE(requestIdentifier.connectionIdentifier(), ipcConnection, nullptr);
    auto identifier = *requestIdentifier.connectionIdentifier();
    auto addResult = m_connectionsToClient.add(identifier, nullptr);
    if (addResult.isNewEntry)
        addResult.iterator->value = makeUnique<IDBStorageConnectionToClient>(ipcConnection.uniqueID(), identifier, networkStorageManager);

    MESSAGE_CHECK_WITH_RETURN_VALUE(addResult.iterator->value->ipcConnection() == ipcConnection.uniqueID(), ipcConnection, nullptr);
    return &addResult.iterator->value->connectionToClient();
}

WebCore::IDBServer::IDBConnectionToClient* IDBStorageRegistry::existingConnectionToClient(WebCore::IDBConnectionIdentifier identifier)
{
    auto* connectionToClient = m_connectionsToClient.get(identifier);
    return connectionToClient ? &connectionToClient->connectionToClient() : nullptr;
}

void IDBStorageRegistry::removeConnectionToClient(IPC::Connection::UniqueID connection)
{
    auto allConnectionsToClient = std::exchange(m_connectionsToClient, { });
    for (auto& [identifier, connectionToClient] : allConnectionsToClient) {
        if (connectionToClient->ipcConnection() != connection) {
            m_connectionsToClient.add(identifier, WTF::move(connectionToClient));
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

bool IDBStorageRegistry::isValidConnectionForIPC(WebCore::IDBServer::UniqueIDBDatabaseConnection& databaseConnection, IPC::Connection& ipcConnection)
{
    auto connectionIdentifier = databaseConnection.connectionToClient().identifier();
    auto it = m_connectionsToClient.find(connectionIdentifier);
    if (it == m_connectionsToClient.end())
        return true;
    return it->value->ipcConnection() == ipcConnection.uniqueID();
}

SUPPRESS_NODELETE RefPtr<WebCore::IDBServer::UniqueIDBDatabaseConnection> IDBStorageRegistry::connection(WebCore::IDBDatabaseConnectionIdentifier identifier, IPC::Connection& ipcConnection)
{
    RefPtr databaseConnection = m_connections.get(identifier);
    if (!databaseConnection)
        return nullptr;

    MESSAGE_CHECK_WITH_RETURN_VALUE(isValidConnectionForIPC(*databaseConnection, ipcConnection), ipcConnection, nullptr);

    return databaseConnection;
}

SUPPRESS_NODELETE RefPtr<WebCore::IDBServer::UniqueIDBDatabaseTransaction> IDBStorageRegistry::transaction(WebCore::IDBResourceIdentifier identifier, IPC::Connection& ipcConnection)
{
    MESSAGE_CHECK_WITH_RETURN_VALUE(identifier.connectionIdentifier(), ipcConnection, nullptr);
    if (identifier.isEmpty())
        return nullptr;
    RefPtr transaction = m_transactions.get(identifier);
    if (!transaction)
        return nullptr;

    if (RefPtr databaseConnection = transaction->databaseConnection())
        MESSAGE_CHECK_WITH_RETURN_VALUE(isValidConnectionForIPC(*databaseConnection, ipcConnection), ipcConnection, nullptr);

    return transaction;
}

} // namespace WebKit

#undef MESSAGE_CHECK_WITH_RETURN_VALUE
