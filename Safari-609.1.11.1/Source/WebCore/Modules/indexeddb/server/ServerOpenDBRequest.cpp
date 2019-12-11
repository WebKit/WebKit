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
#include "ServerOpenDBRequest.h"

#if ENABLE(INDEXED_DATABASE)

#include "IDBResultData.h"

namespace WebCore {
namespace IDBServer {

Ref<ServerOpenDBRequest> ServerOpenDBRequest::create(IDBConnectionToClient& connection, const IDBRequestData& requestData)
{
    return adoptRef(*new ServerOpenDBRequest(connection, requestData));
}

ServerOpenDBRequest::ServerOpenDBRequest(IDBConnectionToClient& connection, const IDBRequestData& requestData)
    : m_connection(connection)
    , m_requestData(requestData)
{
}

bool ServerOpenDBRequest::isOpenRequest() const
{
    return m_requestData.isOpenRequest();
}

bool ServerOpenDBRequest::isDeleteRequest() const
{
    return m_requestData.isDeleteRequest();
}

void ServerOpenDBRequest::maybeNotifyRequestBlocked(uint64_t currentVersion)
{
    if (m_notifiedBlocked)
        return;

    uint64_t requestedVersion = isOpenRequest() ?  m_requestData.requestedVersion() : 0;
    m_connection->notifyOpenDBRequestBlocked(m_requestData.requestIdentifier(), currentVersion, requestedVersion);

    m_notifiedBlocked = true;
}

void ServerOpenDBRequest::notifyDidDeleteDatabase(const IDBDatabaseInfo& info)
{
    ASSERT(isDeleteRequest());

    m_connection->didDeleteDatabase(IDBResultData::deleteDatabaseSuccess(m_requestData.requestIdentifier(), info));
}

void ServerOpenDBRequest::notifiedConnectionsOfVersionChange(HashSet<uint64_t>&& connectionIdentifiers)
{
    ASSERT(!m_notifiedConnectionsOfVersionChange);

    m_notifiedConnectionsOfVersionChange = true;
    m_connectionsPendingVersionChangeEvent = WTFMove(connectionIdentifiers);
}

void ServerOpenDBRequest::connectionClosedOrFiredVersionChangeEvent(uint64_t connectionIdentifier)
{
    m_connectionsPendingVersionChangeEvent.remove(connectionIdentifier);
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
