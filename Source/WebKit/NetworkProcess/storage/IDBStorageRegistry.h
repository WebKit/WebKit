/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include "Connection.h"
#include <WebCore/IDBResourceIdentifier.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
namespace IDBServer {
class UniqueIDBDatabaseConnection;
class UniqueIDBDatabaseTransaction;
}
}

namespace WebKit {

class IDBStorageConnectionToClient;

class IDBStorageRegistry {
    WTF_MAKE_FAST_ALLOCATED;
public:
    IDBStorageRegistry();
    WebCore::IDBServer::IDBConnectionToClient& ensureConnectionToClient(IPC::Connection::UniqueID, WebCore::IDBConnectionIdentifier);
    void removeConnectionToClient(IPC::Connection::UniqueID);
    void registerConnection(WebCore::IDBServer::UniqueIDBDatabaseConnection&);
    void unregisterConnection(WebCore::IDBServer::UniqueIDBDatabaseConnection&);
    WebCore::IDBServer::UniqueIDBDatabaseConnection* connection(uint64_t identifier);
    void registerTransaction(WebCore::IDBServer::UniqueIDBDatabaseTransaction&);
    void unregisterTransaction(WebCore::IDBServer::UniqueIDBDatabaseTransaction&);
    WebCore::IDBServer::UniqueIDBDatabaseTransaction* transaction(WebCore::IDBResourceIdentifier);

private:
    HashMap<WebCore::IDBConnectionIdentifier, std::unique_ptr<IDBStorageConnectionToClient>> m_connectionsToClient;
    HashMap<uint64_t, WeakPtr<WebCore::IDBServer::UniqueIDBDatabaseConnection>> m_connections;
    HashMap<WebCore::IDBResourceIdentifier, WeakPtr<WebCore::IDBServer::UniqueIDBDatabaseTransaction>> m_transactions;
};

} // namespace WebKit
