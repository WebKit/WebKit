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

#include "config.h"
#include "WebToStorageProcessConnection.h"

#include "StorageToWebProcessConnectionMessages.h"
#include "WebIDBConnectionToServerMessages.h"
#include "WebProcess.h"

using namespace WebCore;

namespace WebKit {

WebToStorageProcessConnection::WebToStorageProcessConnection(IPC::Connection::Identifier connectionIdentifier)
    : m_connection(IPC::Connection::createClientConnection(connectionIdentifier, *this))
{
    m_connection->open();
}

WebToStorageProcessConnection::~WebToStorageProcessConnection()
{
    m_connection->invalidate();
}

void WebToStorageProcessConnection::didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
#if ENABLE(INDEXED_DATABASE)
    if (decoder.messageReceiverName() == Messages::WebIDBConnectionToServer::messageReceiverName()) {
        auto idbConnection = m_webIDBConnectionsByIdentifier.get(decoder.destinationID());
        if (idbConnection)
            idbConnection->didReceiveMessage(connection, decoder);
        return;
    }
#endif
    
    ASSERT_NOT_REACHED();
}

void WebToStorageProcessConnection::didClose(IPC::Connection& connection)
{
#if ENABLE(INDEXED_DATABASE)
    for (auto& connection : m_webIDBConnectionsByIdentifier.values())
        connection->connectionToServerLost();

    m_webIDBConnectionsByIdentifier.clear();
    m_webIDBConnectionsBySession.clear();
#endif

    WebProcess::singleton().webToStorageProcessConnectionClosed(this);
}

void WebToStorageProcessConnection::didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName)
{
}

#if ENABLE(INDEXED_DATABASE)
WebIDBConnectionToServer& WebToStorageProcessConnection::idbConnectionToServerForSession(const SessionID& sessionID)
{
    auto result = m_webIDBConnectionsBySession.add(sessionID, nullptr);
    if (result.isNewEntry) {
        result.iterator->value = WebIDBConnectionToServer::create(sessionID);
        ASSERT(!m_webIDBConnectionsByIdentifier.contains(result.iterator->value->identifier()));
        m_webIDBConnectionsByIdentifier.set(result.iterator->value->identifier(), result.iterator->value);
    }

    return *result.iterator->value;
}
#endif

} // namespace WebKit
