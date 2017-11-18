/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "MessageSender.h"

#include <WebCore/SWServer.h>
#include <pal/SessionID.h>
#include <wtf/HashMap.h>

namespace WebKit {

class WebIDBConnectionToClient;
class WebSWServerConnection;

class StorageToWebProcessConnection : public RefCounted<StorageToWebProcessConnection>, private IPC::Connection::Client, private IPC::MessageSender {
public:
    static Ref<StorageToWebProcessConnection> create(IPC::Connection::Identifier);
    ~StorageToWebProcessConnection();

    IPC::Connection& connection() { return m_connection.get(); }

#if ENABLE(SERVICE_WORKER)
    void workerContextProcessConnectionCreated();
#endif

private:
    StorageToWebProcessConnection(IPC::Connection::Identifier);

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;
    void didClose(IPC::Connection&) override;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;
    void didReceiveStorageToWebProcessConnectionMessage(IPC::Connection&, IPC::Decoder&);
    void didReceiveSyncStorageToWebProcessConnectionMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() override { return m_connection.ptr(); }
    uint64_t messageSenderDestinationID() override { return 0; }

#if ENABLE(INDEXED_DATABASE)
    // Messages handlers (Modern IDB)
    void establishIDBConnectionToServer(PAL::SessionID, uint64_t& serverConnectionIdentifier);
    void removeIDBConnectionToServer(uint64_t serverConnectionIdentifier);

    HashMap<uint64_t, RefPtr<WebIDBConnectionToClient>> m_webIDBConnections;
#endif // ENABLE(INDEXED_DATABASE)

#if ENABLE(SERVICE_WORKER)
    void establishSWServerConnection(PAL::SessionID, WebCore::SWServerConnectionIdentifier&);
    void removeSWServerConnection(WebCore::SWServerConnectionIdentifier);
    HashMap<WebCore::SWServerConnectionIdentifier, std::unique_ptr<WebSWServerConnection>> m_swConnections;
#endif

    Ref<IPC::Connection> m_connection;
};

} // namespace WebKit
