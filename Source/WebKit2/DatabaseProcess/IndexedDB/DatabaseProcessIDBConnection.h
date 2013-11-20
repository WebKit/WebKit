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

#ifndef DatabaseProcessIDBConnection_h
#define DatabaseProcessIDBConnection_h

#include "MessageSender.h"

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

namespace WebKit {

class DatabaseToWebProcessConnection;

struct SecurityOriginData;

class DatabaseProcessIDBConnection : public RefCounted<DatabaseProcessIDBConnection>, public CoreIPC::MessageSender {
public:
    static RefPtr<DatabaseProcessIDBConnection> create(uint64_t backendIdentifier)
    {
        return adoptRef(new DatabaseProcessIDBConnection(backendIdentifier));
    }

    virtual ~DatabaseProcessIDBConnection();

    // Message handlers.
    void didReceiveDatabaseProcessIDBConnectionMessage(CoreIPC::Connection*, CoreIPC::MessageDecoder&);

private:
    DatabaseProcessIDBConnection(uint64_t backendIdentifier);

    // CoreIPC::MessageSender
    virtual CoreIPC::Connection* messageSenderConnection() OVERRIDE;
    virtual uint64_t messageSenderDestinationID() OVERRIDE { return m_backendIdentifier; }

    // Message handlers.
    void establishConnection(const String& databaseName, const SecurityOriginData& openingOrigin, const SecurityOriginData& mainFrameOrigin);
    void getOrEstablishIDBDatabaseMetadata();

    RefPtr<DatabaseToWebProcessConnection> m_connection;
    uint64_t m_backendIdentifier;
};

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
#endif // DatabaseProcessIDBConnection_h
