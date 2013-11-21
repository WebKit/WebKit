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

#include "config.h"
#include "DatabaseProcessIDBConnection.h"

#if ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)

#include "DatabaseToWebProcessConnection.h"
#include "WebCoreArgumentCoders.h"
#include "WebIDBServerConnectionMessages.h"
#include <WebCore/IDBDatabaseMetadata.h>

using namespace WebCore;

namespace WebKit {

DatabaseProcessIDBConnection::DatabaseProcessIDBConnection(DatabaseToWebProcessConnection& connection, uint64_t serverConnectionIdentifier)
    : m_connection(connection)
    , m_serverConnectionIdentifier(serverConnectionIdentifier)
{
}

DatabaseProcessIDBConnection::~DatabaseProcessIDBConnection()
{
}

void DatabaseProcessIDBConnection::disconnectedFromWebProcess()
{
    // Do any necessary cleanup work here.
}

void DatabaseProcessIDBConnection::establishConnection(const String& databaseName, const SecurityOriginData& openingOrigin, const SecurityOriginData& mainFrameOrigin)
{
    // This method should only be called once, so the stored database name should still be null.
    // Also, it is invalid to set the stored database name to the null string.
    ASSERT(m_databaseName.isNull());
    ASSERT(!databaseName.isNull());

    m_databaseName = databaseName;
    m_openingOrigin = openingOrigin;
    m_mainFrameOrigin = mainFrameOrigin;
}

void DatabaseProcessIDBConnection::getOrEstablishIDBDatabaseMetadata(uint64_t requestID)
{
    // FIXME: This method is successfully called by messaging from the WebProcess, and calls back with dummy data.
    // Needs real implementation.

    IDBDatabaseMetadata data;
    send(Messages::WebIDBServerConnection::DidGetOrEstablishIDBDatabaseMetadata(requestID, false, data));
}

CoreIPC::Connection* DatabaseProcessIDBConnection::messageSenderConnection()
{
    return m_connection->connection();
}

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE) && ENABLE(DATABASE_PROCESS)
