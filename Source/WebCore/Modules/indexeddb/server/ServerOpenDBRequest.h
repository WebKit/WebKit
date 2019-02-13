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

#pragma once

#if ENABLE(INDEXED_DATABASE)

#include "IDBConnectionToClient.h"
#include "IDBRequestData.h"
#include <wtf/HashSet.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class IDBDatabaseInfo;

namespace IDBServer {

class ServerOpenDBRequest : public RefCounted<ServerOpenDBRequest> {
public:
    static Ref<ServerOpenDBRequest> create(IDBConnectionToClient&, const IDBRequestData&);

    IDBConnectionToClient& connection() { return m_connection; }
    const IDBRequestData& requestData() const { return m_requestData; }

    bool isOpenRequest() const;
    bool isDeleteRequest() const;

    void maybeNotifyRequestBlocked(uint64_t currentVersion);
    void notifyDidDeleteDatabase(const IDBDatabaseInfo&);

    uint64_t versionChangeID() const;

    void notifiedConnectionsOfVersionChange(HashSet<uint64_t>&& connectionIdentifiers);
    void connectionClosedOrFiredVersionChangeEvent(uint64_t connectionIdentifier);
    bool hasConnectionsPendingVersionChangeEvent() const { return !m_connectionsPendingVersionChangeEvent.isEmpty(); }
    bool hasNotifiedConnectionsOfVersionChange() const { return m_notifiedConnectionsOfVersionChange; }


private:
    ServerOpenDBRequest(IDBConnectionToClient&, const IDBRequestData&);

    Ref<IDBConnectionToClient> m_connection;
    IDBRequestData m_requestData;

    bool m_notifiedBlocked { false };

    bool m_notifiedConnectionsOfVersionChange { false };
    HashSet<uint64_t> m_connectionsPendingVersionChangeEvent;
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
