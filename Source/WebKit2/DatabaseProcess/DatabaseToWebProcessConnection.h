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

#ifndef DatabaseToWebProcessConnection_h
#define DatabaseToWebProcessConnection_h

#include "Connection.h"
#include "DatabaseProcessIDBConnection.h"
#include "MessageSender.h"

#include <wtf/HashMap.h>

#if ENABLE(DATABASE_PROCESS)

namespace WebKit {

class DatabaseToWebProcessConnection : public RefCounted<DatabaseToWebProcessConnection>, public IPC::Connection::Client, public IPC::MessageSender {
public:
    static PassRefPtr<DatabaseToWebProcessConnection> create(IPC::Connection::Identifier);
    ~DatabaseToWebProcessConnection();

    IPC::Connection* connection() const { return m_connection.get(); }

private:
    DatabaseToWebProcessConnection(IPC::Connection::Identifier);

    // IPC::Connection::Client
    virtual void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;
    virtual void didClose(IPC::Connection*) override;
    virtual void didReceiveInvalidMessage(IPC::Connection*, IPC::StringReference messageReceiverName, IPC::StringReference messageName) override;
    void didReceiveDatabaseToWebProcessConnectionMessage(IPC::Connection*, IPC::MessageDecoder&);

    // IPC::MessageSender
    virtual IPC::Connection* messageSenderConnection() override { return m_connection.get(); }
    virtual uint64_t messageSenderDestinationID() override { return 0; }

#if ENABLE(INDEXED_DATABASE)
    // Messages handlers
    void establishIDBConnection(uint64_t serverConnectionIdentifier);
    void removeDatabaseProcessIDBConnection(uint64_t serverConnectionIdentifier);

    typedef HashMap<uint64_t, RefPtr<DatabaseProcessIDBConnection>> IDBConnectionMap;
    IDBConnectionMap m_idbConnections;
#endif // ENABLE(INDEXED_DATABASE)

    RefPtr<IPC::Connection> m_connection;
};

} // namespace WebKit

#endif // ENABLE(DATABASE_PROCESS)
#endif // DatabaseToWebProcessConnection_h
