/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebSharedWorkerServerToContextConnection.h"

#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "WebCoreArgumentCoders.h"
#include "WebSharedWorker.h"
#include "WebSharedWorkerContextManagerConnectionMessages.h"
#include "WebSharedWorkerServer.h"

namespace WebKit {

#define CONTEXT_CONNECTION_RELEASE_LOG(fmt, ...) RELEASE_LOG(SharedWorker, "%p - [webProcessIdentifier=%" PRIu64 "] WebSharedWorkerServerToContextConnection::" fmt, this, webProcessIdentifier().toUInt64(), ##__VA_ARGS__)

WebSharedWorkerServerToContextConnection::WebSharedWorkerServerToContextConnection(NetworkConnectionToWebProcess& connection, const WebCore::RegistrableDomain& registrableDomain, WebSharedWorkerServer& server)
    : m_connection(connection)
    , m_server(server)
    , m_registrableDomain(registrableDomain)
{
    CONTEXT_CONNECTION_RELEASE_LOG("WebSharedWorkerServerToContextConnection:");
    server.addContextConnection(*this);
}

WebSharedWorkerServerToContextConnection::~WebSharedWorkerServerToContextConnection()
{
    CONTEXT_CONNECTION_RELEASE_LOG("~WebSharedWorkerServerToContextConnection:");
    if (m_server && m_server->contextConnectionForRegistrableDomain(registrableDomain()) == this)
        m_server->removeContextConnection(*this);
}

WebCore::ProcessIdentifier WebSharedWorkerServerToContextConnection::webProcessIdentifier() const
{
    return m_connection.webProcessIdentifier();
}

IPC::Connection& WebSharedWorkerServerToContextConnection::ipcConnection() const
{
    return m_connection.connection();
}

IPC::Connection* WebSharedWorkerServerToContextConnection::messageSenderConnection() const
{
    return &ipcConnection();
}

uint64_t WebSharedWorkerServerToContextConnection::messageSenderDestinationID() const
{
    return 0;
}

void WebSharedWorkerServerToContextConnection::connectionIsNoLongerNeeded()
{
    CONTEXT_CONNECTION_RELEASE_LOG("connectionIsNoLongerNeeded:");
    m_connection.sharedWorkerServerToContextConnectionIsNoLongerNeeded();
}

void WebSharedWorkerServerToContextConnection::postExceptionToWorkerObject(WebCore::SharedWorkerIdentifier sharedWorkerIdentifier, const String& errorMessage, int lineNumber, int columnNumber, const String& sourceURL)
{
    CONTEXT_CONNECTION_RELEASE_LOG("postExceptionToWorkerObject: sharedWorkerIdentifier=%" PRIu64, sharedWorkerIdentifier.toUInt64());
    if (m_server)
        m_server->postExceptionToWorkerObject(sharedWorkerIdentifier, errorMessage, lineNumber, columnNumber, sourceURL);
}

void WebSharedWorkerServerToContextConnection::launchSharedWorker(WebSharedWorker& sharedWorker)
{
    CONTEXT_CONNECTION_RELEASE_LOG("launchSharedWorker: sharedWorkerIdentifier=%" PRIu64, sharedWorker.identifier().toUInt64());
    sharedWorker.markAsRunning();
    send(Messages::WebSharedWorkerContextManagerConnection::LaunchSharedWorker { sharedWorker.origin(), sharedWorker.identifier(), sharedWorker.url(), sharedWorker.workerOptions(), sharedWorker.fetchResult() });
    for (auto& port : sharedWorker.sharedWorkerObjects().values())
        postConnectEvent(sharedWorker, port);
}

void WebSharedWorkerServerToContextConnection::postConnectEvent(const WebSharedWorker& sharedWorker, const WebCore::TransferredMessagePort& port)
{
    CONTEXT_CONNECTION_RELEASE_LOG("postConnectEvent: sharedWorkerIdentifier=%" PRIu64, sharedWorker.identifier().toUInt64());
    send(Messages::WebSharedWorkerContextManagerConnection::PostConnectEvent { sharedWorker.identifier(), port, sharedWorker.origin().clientOrigin.toString() });
}

void WebSharedWorkerServerToContextConnection::terminateSharedWorker(const WebSharedWorker& sharedWorker)
{
    CONTEXT_CONNECTION_RELEASE_LOG("terminateSharedWorker: sharedWorkerIdentifier=%" PRIu64, sharedWorker.identifier().toUInt64());
    send(Messages::WebSharedWorkerContextManagerConnection::TerminateSharedWorker { sharedWorker.identifier() });
}

#undef CONTEXT_CONNECTION_RELEASE_LOG

} // namespace WebKit
