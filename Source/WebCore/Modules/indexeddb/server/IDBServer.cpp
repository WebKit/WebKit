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
#include "IDBServer.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBRequestData.h"
#include "IDBResultData.h"
#include "Logging.h"

namespace WebCore {
namespace IDBServer {

Ref<IDBServer> IDBServer::create()
{
    return adoptRef(*new IDBServer());
}

IDBServer::IDBServer()
{
}

void IDBServer::registerConnection(IDBConnectionToClient& connection)
{
    ASSERT(!m_connectionMap.contains(connection.identifier()));
    m_connectionMap.set(connection.identifier(), &connection);
}

void IDBServer::unregisterConnection(IDBConnectionToClient& connection)
{
    ASSERT(m_connectionMap.contains(connection.identifier()));
    ASSERT(m_connectionMap.get(connection.identifier()) == &connection);

    m_connectionMap.remove(connection.identifier());
}

void IDBServer::deleteDatabase(const IDBRequestData& data)
{
    LOG(IndexedDB, "IDBServer::deleteDatabase - %s", data.databaseIdentifier().debugString().utf8().data());
    
    auto connection = m_connectionMap.get(data.requestIdentifier().connectionIdentifier());
    if (!connection) {
        // If we don't have record of this connection (e.g. it has dropped due to a process crashing)
        // then we can't report back status of the operation.
        // Therefore we shouldn't bother performing it.
        return;
    }
    
    // FIXME: During bringup of modern IDB, the database deletion is a no-op, and is
    // immediately reported back to the WebProcess as failure.
    IDBResultData result(data.requestIdentifier(), IDBError(IDBExceptionCode::Unknown));
    connection->didDeleteDatabase(result);
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
