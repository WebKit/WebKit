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

#ifndef DatabaseProcessIDBDatabaseBackend_h
#define DatabaseProcessIDBDatabaseBackend_h

#include "MessageSender.h"
#include <WebCore/IDBDatabaseBackendInterface.h>

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

namespace WebKit {

class DatabaseToWebProcessConnection;

// FIXME: When adding actual IDB logic, this class will inherit from WebCore::IDBDatabaseBackendInterface
// instead of inheriting from RefCounted directly.
// Doing so will also add a large number of interface methods to implement which is why I'm holding off for now.

class DatabaseProcessIDBDatabaseBackend : public RefCounted<DatabaseProcessIDBDatabaseBackend>, public CoreIPC::MessageSender {
public:
    static RefPtr<DatabaseProcessIDBDatabaseBackend> create(uint64_t backendIdentifier)
    {
        return adoptRef(new DatabaseProcessIDBDatabaseBackend(backendIdentifier));
    }

    virtual ~DatabaseProcessIDBDatabaseBackend();

    // Message handlers.
    void didReceiveDatabaseProcessIDBDatabaseBackendMessage(CoreIPC::Connection*, CoreIPC::MessageDecoder&);

private:
    DatabaseProcessIDBDatabaseBackend(uint64_t backendIdentifier);

    // CoreIPC::MessageSender
    virtual CoreIPC::Connection* messageSenderConnection() OVERRIDE;
    virtual uint64_t messageSenderDestinationID() OVERRIDE { return m_backendIdentifier; }

    // Message handlers.
    void openConnection();

    RefPtr<DatabaseToWebProcessConnection> m_connection;
    uint64_t m_backendIdentifier;
};

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
#endif // DatabaseProcessIDBDatabaseBackend_h
