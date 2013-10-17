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
#include "MessageSender.h"

#include <wtf/HashMap.h>

#if ENABLE(DATABASE_PROCESS)

namespace WebKit {

class DatabaseProcessIDBDatabaseBackend;

class DatabaseToWebProcessConnection : public RefCounted<DatabaseToWebProcessConnection>, public CoreIPC::Connection::Client, public CoreIPC::MessageSender {
public:
    static PassRefPtr<DatabaseToWebProcessConnection> create(CoreIPC::Connection::Identifier);
    ~DatabaseToWebProcessConnection();

    CoreIPC::Connection* connection() const { return m_connection.get(); }

private:
    DatabaseToWebProcessConnection(CoreIPC::Connection::Identifier);

    // CoreIPC::Connection::Client
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageDecoder&) OVERRIDE;
    virtual void didClose(CoreIPC::Connection*) OVERRIDE;
    virtual void didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::StringReference messageReceiverName, CoreIPC::StringReference messageName) OVERRIDE;
    void didReceiveDatabaseToWebProcessConnectionMessage(CoreIPC::Connection*, CoreIPC::MessageDecoder&);

    // CoreIPC::MessageSender
    virtual CoreIPC::Connection* messageSenderConnection() OVERRIDE { return m_connection.get(); }
    virtual uint64_t messageSenderDestinationID() OVERRIDE { return 0; }

#if ENABLE(INDEXED_DATABASE)
    // Messages handlers
    void establishIDBDatabaseBackend(uint64_t backendIdentifier);

    typedef HashMap<uint64_t, RefPtr<DatabaseProcessIDBDatabaseBackend>> IDBDatabaseBackendMap;
    IDBDatabaseBackendMap m_idbDatabaseBackends;
#endif // ENABLE(INDEXED_DATABASE)

    RefPtr<CoreIPC::Connection> m_connection;
};

} // namespace WebKit

#endif // ENABLE(DATABASE_PROCESS)
#endif // DatabaseToWebProcessConnection_h
