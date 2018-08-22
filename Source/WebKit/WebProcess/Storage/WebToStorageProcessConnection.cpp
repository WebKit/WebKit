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

#include "ServiceWorkerClientFetchMessages.h"
#include "StorageToWebProcessConnectionMessages.h"
#include "WebIDBConnectionToServerMessages.h"
#include "WebProcess.h"
#include "WebSWClientConnection.h"
#include "WebSWClientConnectionMessages.h"
#include "WebSWContextManagerConnection.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebServiceWorkerProvider.h"
#include <WebCore/SWContextManager.h>

namespace WebKit {
using namespace PAL;
using namespace WebCore;

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

#if ENABLE(SERVICE_WORKER)
    if (decoder.messageReceiverName() == Messages::WebSWClientConnection::messageReceiverName()) {
        auto serviceWorkerConnection = m_swConnectionsByIdentifier.get(makeObjectIdentifier<SWServerConnectionIdentifierType>(decoder.destinationID()));
        if (serviceWorkerConnection)
            serviceWorkerConnection->didReceiveMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::ServiceWorkerClientFetch::messageReceiverName()) {
        WebServiceWorkerProvider::singleton().didReceiveServiceWorkerClientFetchMessage(connection, decoder);
        return;
    }
    if (decoder.messageReceiverName() == Messages::WebSWContextManagerConnection::messageReceiverName()) {
        ASSERT(SWContextManager::singleton().connection());
        if (auto* contextManagerConnection = SWContextManager::singleton().connection())
            static_cast<WebSWContextManagerConnection&>(*contextManagerConnection).didReceiveMessage(connection, decoder);
        return;
    }
#endif
    ASSERT_NOT_REACHED();
}

void WebToStorageProcessConnection::didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, std::unique_ptr<IPC::Encoder>& replyEncoder)
{
#if ENABLE(SERVICE_WORKER)
    if (decoder.messageReceiverName() == Messages::WebSWContextManagerConnection::messageReceiverName()) {
        ASSERT(SWContextManager::singleton().connection());
        if (auto* contextManagerConnection = SWContextManager::singleton().connection())
            static_cast<WebSWContextManagerConnection&>(*contextManagerConnection).didReceiveSyncMessage(connection, decoder, replyEncoder);
        return;
    }
#endif
    ASSERT_NOT_REACHED();
}

void WebToStorageProcessConnection::didClose(IPC::Connection& connection)
{
    auto protectedThis = makeRef(*this);

#if ENABLE(INDEXED_DATABASE)
    for (auto& connection : m_webIDBConnectionsByIdentifier.values())
        connection->connectionToServerLost();
#endif
#if ENABLE(SERVICE_WORKER)
    for (auto& connection : m_swConnectionsBySession.values())
        connection->connectionToServerLost();

    m_swConnectionsByIdentifier.clear();
    m_swConnectionsBySession.clear();
#endif

    WebProcess::singleton().webToStorageProcessConnectionClosed(this);

#if ENABLE(INDEXED_DATABASE)
    m_webIDBConnectionsByIdentifier.clear();
    m_webIDBConnectionsBySession.clear();
#endif
}

void WebToStorageProcessConnection::didReceiveInvalidMessage(IPC::Connection&, IPC::StringReference messageReceiverName, IPC::StringReference messageName)
{
}

#if ENABLE(INDEXED_DATABASE)
WebIDBConnectionToServer& WebToStorageProcessConnection::idbConnectionToServerForSession(SessionID sessionID)
{
    return *m_webIDBConnectionsBySession.ensure(sessionID, [&] {
        auto connection = WebIDBConnectionToServer::create(sessionID);

        auto result = m_webIDBConnectionsByIdentifier.add(connection->identifier(), connection.copyRef());
        ASSERT_UNUSED(result, result.isNewEntry);

        return connection;
    }).iterator->value;
}
#endif

#if ENABLE(SERVICE_WORKER)
WebSWClientConnection& WebToStorageProcessConnection::serviceWorkerConnectionForSession(SessionID sessionID)
{
    ASSERT(sessionID.isValid());
    return *m_swConnectionsBySession.ensure(sessionID, [&] {
        auto connection = WebSWClientConnection::create(m_connection, sessionID);

        auto result = m_swConnectionsByIdentifier.add(connection->serverConnectionIdentifier(), connection.ptr());
        ASSERT_UNUSED(result, result.isNewEntry);

        return connection;
    }).iterator->value;
}
#endif

} // namespace WebKit
