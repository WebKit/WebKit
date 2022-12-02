/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "NetworkSocketChannel.h"

#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkSession.h"
#include "WebCoreArgumentCoders.h"
#include "WebSocketChannelMessages.h"
#include "WebSocketTask.h"

namespace WebKit {
using namespace WebCore;

std::unique_ptr<NetworkSocketChannel> NetworkSocketChannel::create(NetworkConnectionToWebProcess& connection, PAL::SessionID sessionID, const ResourceRequest& request, const String& protocol, WebSocketIdentifier identifier, WebPageProxyIdentifier webPageProxyID, const WebCore::ClientOrigin& clientOrigin, bool hadMainFrameMainResourcePrivateRelayed, bool allowPrivacyProxy, OptionSet<NetworkConnectionIntegrity> networkConnectionIntegrityPolicy)
{
    auto result = makeUnique<NetworkSocketChannel>(connection, connection.networkProcess().networkSession(sessionID), request, protocol, identifier, webPageProxyID, clientOrigin, hadMainFrameMainResourcePrivateRelayed, allowPrivacyProxy, networkConnectionIntegrityPolicy);
    if (!result->m_socket) {
        result->didClose(0, "Cannot create a web socket task"_s);
        return nullptr;
    }
    return result;
}

NetworkSocketChannel::NetworkSocketChannel(NetworkConnectionToWebProcess& connection, NetworkSession* session, const ResourceRequest& request, const String& protocol, WebSocketIdentifier identifier, WebPageProxyIdentifier webPageProxyID, const WebCore::ClientOrigin& clientOrigin, bool hadMainFrameMainResourcePrivateRelayed, bool allowPrivacyProxy, OptionSet<NetworkConnectionIntegrity> networkConnectionIntegrityPolicy)
    : m_connectionToWebProcess(connection)
    , m_identifier(identifier)
    , m_session(session)
    , m_errorTimer(*this, &NetworkSocketChannel::sendDelayedError)
    , m_webPageProxyID(webPageProxyID)
{
    if (!m_session)
        return;

    m_socket = m_session->createWebSocketTask(webPageProxyID, *this, request, protocol, clientOrigin, hadMainFrameMainResourcePrivateRelayed, allowPrivacyProxy, networkConnectionIntegrityPolicy);
    if (m_socket) {
        m_session->addWebSocketTask(webPageProxyID, *m_socket);
        m_socket->resume();
    }
}

NetworkSocketChannel::~NetworkSocketChannel()
{
    if (m_socket) {
        if (m_session && m_socket->sessionSet())
            m_session->removeWebSocketTask(*m_socket->sessionSet(), *m_socket);
        m_socket->cancel();
    }
}

void NetworkSocketChannel::sendString(const IPC::DataReference& message, CompletionHandler<void()>&& callback)
{
    m_socket->sendString(message, WTFMove(callback));
}

void NetworkSocketChannel::sendData(const IPC::DataReference& data, CompletionHandler<void()>&& callback)
{
    m_socket->sendData(data, WTFMove(callback));
}

void NetworkSocketChannel::finishClosingIfPossible()
{
    if (m_state == State::Open) {
        m_state = State::Closing;
        return;
    }
    ASSERT(m_state == State::Closing);
    m_state = State::Closed;
    m_connectionToWebProcess.removeSocketChannel(m_identifier);
}

void NetworkSocketChannel::close(int32_t code, const String& reason)
{
    m_socket->close(code, reason);
    finishClosingIfPossible();
}

void NetworkSocketChannel::didConnect(const String& subprotocol, const String& extensions)
{
    send(Messages::WebSocketChannel::DidConnect { subprotocol, extensions });
}

void NetworkSocketChannel::didReceiveText(const String& text)
{
    send(Messages::WebSocketChannel::DidReceiveText { text });
}

void NetworkSocketChannel::didReceiveBinaryData(const uint8_t* data, size_t length)
{
    send(Messages::WebSocketChannel::DidReceiveBinaryData { { data, length } });
}

void NetworkSocketChannel::didClose(unsigned short code, const String& reason)
{
    if (m_errorTimer.isActive()) {
        m_closeInfo = std::make_pair(code, reason);
        return;
    }
    send(Messages::WebSocketChannel::DidClose { code, reason });
    finishClosingIfPossible();
}

void NetworkSocketChannel::didReceiveMessageError(String&& errorMessage)
{
    m_errorMessage = WTFMove(errorMessage);
    m_errorTimer.startOneShot(NetworkProcess::randomClosedPortDelay());
}

void NetworkSocketChannel::sendDelayedError()
{
    send(Messages::WebSocketChannel::DidReceiveMessageError { m_errorMessage });
    if (m_closeInfo) {
        send(Messages::WebSocketChannel::DidClose { m_closeInfo->first, m_closeInfo->second });
        finishClosingIfPossible();
    }
}

void NetworkSocketChannel::didSendHandshakeRequest(ResourceRequest&& request)
{
    send(Messages::WebSocketChannel::DidSendHandshakeRequest { request });
}

void NetworkSocketChannel::didReceiveHandshakeResponse(ResourceResponse&& response)
{
    response.sanitizeHTTPHeaderFields(ResourceResponse::SanitizationType::CrossOriginSafe);
    send(Messages::WebSocketChannel::DidReceiveHandshakeResponse { response });
}

IPC::Connection* NetworkSocketChannel::messageSenderConnection() const
{
    return &m_connectionToWebProcess.connection();
}

NetworkSession* NetworkSocketChannel::session()
{
    return m_session.get();
}

} // namespace WebKit
