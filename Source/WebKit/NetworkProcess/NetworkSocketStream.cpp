/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "NetworkSocketStream.h"

#include "DataReference.h"
#include "NetworkStorageSessionProvider.h"
#include "WebSocketStreamMessages.h"
#include <WebCore/CookieRequestHeaderFieldProxy.h>
#include <WebCore/SocketStreamError.h>
#include <wtf/CryptographicallyRandomNumber.h>

namespace WebKit {
using namespace WebCore;

Ref<NetworkSocketStream> NetworkSocketStream::create(NetworkProcess& networkProcess, URL&& url, PAL::SessionID sessionID, const String& credentialPartition, WebSocketIdentifier identifier, IPC::Connection& connection, SourceApplicationAuditToken&& auditData)
{
    return adoptRef(*new NetworkSocketStream(networkProcess, WTFMove(url), sessionID, credentialPartition, identifier, connection, WTFMove(auditData)));
}

NetworkSocketStream::NetworkSocketStream(NetworkProcess& networkProcess, URL&& url, PAL::SessionID sessionID, const String& credentialPartition, WebSocketIdentifier identifier, IPC::Connection& connection, SourceApplicationAuditToken&& auditData)
    : m_identifier(identifier)
    , m_connection(connection)
    , m_impl(SocketStreamHandleImpl::create(url, *this, sessionID, credentialPartition, WTFMove(auditData), NetworkStorageSessionProvider::create(networkProcess, sessionID).ptr()))
    , m_delayFailTimer(*this, &NetworkSocketStream::sendDelayedFailMessage)
{
}

void NetworkSocketStream::sendData(const IPC::DataReference& data, uint64_t identifier)
{
    m_impl->platformSend(data.data(), data.size(), [this, protectedThis = makeRef(*this), identifier] (bool success) {
        send(Messages::WebSocketStream::DidSendData(identifier, success));
    });
}

void NetworkSocketStream::sendHandshake(const IPC::DataReference& data, const Optional<CookieRequestHeaderFieldProxy>& headerFieldProxy, uint64_t identifier)
{
    m_impl->platformSendHandshake(data.data(), data.size(), headerFieldProxy, [this, protectedThis = makeRef(*this), identifier] (bool success, bool didAccessSecureCookies) {
        send(Messages::WebSocketStream::DidSendHandshake(identifier, success, didAccessSecureCookies));
    });
}

void NetworkSocketStream::close()
{
    m_impl->platformClose();
}

NetworkSocketStream::~NetworkSocketStream()
{
    close();
}

void NetworkSocketStream::didOpenSocketStream(SocketStreamHandle& handle)
{
    ASSERT_UNUSED(handle, &handle == m_impl.ptr());
    send(Messages::WebSocketStream::DidOpenSocketStream());
}

void NetworkSocketStream::didCloseSocketStream(SocketStreamHandle& handle)
{
    ASSERT_UNUSED(handle, &handle == m_impl.ptr());
    send(Messages::WebSocketStream::DidCloseSocketStream());
}

void NetworkSocketStream::didReceiveSocketStreamData(SocketStreamHandle& handle, const char* data, size_t length)
{
    ASSERT_UNUSED(handle, &handle == m_impl.ptr());
    send(Messages::WebSocketStream::DidReceiveSocketStreamData(IPC::DataReference(reinterpret_cast<const uint8_t*>(data), length)));
}

void NetworkSocketStream::didFailToReceiveSocketStreamData(WebCore::SocketStreamHandle& handle)
{
    ASSERT_UNUSED(handle, &handle == m_impl.ptr());
    send(Messages::WebSocketStream::DidFailToReceiveSocketStreamData());
}

void NetworkSocketStream::didUpdateBufferedAmount(SocketStreamHandle& handle, size_t amount)
{
    ASSERT_UNUSED(handle, &handle == m_impl.ptr());
    send(Messages::WebSocketStream::DidUpdateBufferedAmount(amount));
}

static constexpr auto delayMax = 100_ms;
static constexpr auto delayMin = 10_ms;
static constexpr auto closedPortErrorCode = 61;

static Seconds randomDelay()
{
    return delayMin + Seconds::fromMilliseconds(static_cast<double>(cryptographicallyRandomNumber())) % delayMax;
}

void NetworkSocketStream::sendDelayedFailMessage()
{
    send(Messages::WebSocketStream::DidFailSocketStream(m_closedPortError));
}

void NetworkSocketStream::didFailSocketStream(SocketStreamHandle& handle, const SocketStreamError& error)
{
    ASSERT_UNUSED(handle, &handle == m_impl.ptr());
    
    if (error.errorCode() == closedPortErrorCode) {
        m_closedPortError = error;
        m_delayFailTimer.startOneShot(randomDelay());
    } else
        send(Messages::WebSocketStream::DidFailSocketStream(error));
}

IPC::Connection* NetworkSocketStream::messageSenderConnection() const
{
    return &m_connection;
}

uint64_t NetworkSocketStream::messageSenderDestinationID() const
{
    return m_identifier.toUInt64();
}

} // namespace WebKit
