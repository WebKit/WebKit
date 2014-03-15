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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WebToDatabaseProcessConnection_h
#define WebToDatabaseProcessConnection_h

#include "Connection.h"
#include "MessageSender.h"
#include <wtf/RefCounted.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebKit {

class WebIDBServerConnection;
class WebProcessIDBDatabaseBackend;

class WebToDatabaseProcessConnection : public RefCounted<WebToDatabaseProcessConnection>, public IPC::Connection::Client, public IPC::MessageSender {
public:
    static PassRefPtr<WebToDatabaseProcessConnection> create(IPC::Connection::Identifier connectionIdentifier)
    {
        return adoptRef(new WebToDatabaseProcessConnection(connectionIdentifier));
    }
    ~WebToDatabaseProcessConnection();
    
    IPC::Connection* connection() const { return m_connection.get(); }

    void registerWebIDBServerConnection(WebIDBServerConnection&);
    void removeWebIDBServerConnection(WebIDBServerConnection&);

private:
    WebToDatabaseProcessConnection(IPC::Connection::Identifier);

    // IPC::Connection::Client
    virtual void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;
    virtual void didClose(IPC::Connection*) override;
    virtual void didReceiveInvalidMessage(IPC::Connection*, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;

    // IPC::MessageSender
    virtual IPC::Connection* messageSenderConnection() override { return m_connection.get(); }
    virtual uint64_t messageSenderDestinationID() override { return 0; }

    RefPtr<IPC::Connection> m_connection;

    HashMap<uint64_t, WebIDBServerConnection*> m_webIDBServerConnections;
};

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
#endif // WebToDatabaseProcessConnection_h
