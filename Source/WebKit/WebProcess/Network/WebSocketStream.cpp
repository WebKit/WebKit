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
#include "WebSocketStream.h"

#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkSocketStreamMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/CookieRequestHeaderFieldProxy.h>
#include <WebCore/SocketStreamError.h>
#include <WebCore/SocketStreamHandleClient.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {
using namespace WebCore;

static Lock globalWebSocketStreamMapLock;
static HashMap<WebSocketIdentifier, WebSocketStream*>& globalWebSocketStreamMap() WTF_REQUIRES_LOCK(globalWebSocketStreamMapLock)
{
    static NeverDestroyed<HashMap<WebSocketIdentifier, WebSocketStream*>> globalMap;
    return globalMap;
}

WebSocketStream* WebSocketStream::streamWithIdentifier(WebSocketIdentifier identifier)
{
    Locker stateLocker { globalWebSocketStreamMapLock };
    return globalWebSocketStreamMap().get(identifier);
}

void WebSocketStream::networkProcessCrashed()
{
    Vector<RefPtr<WebSocketStream>> sockets;
    {
        Locker stateLocker { globalWebSocketStreamMapLock };
        sockets.reserveInitialCapacity(globalWebSocketStreamMap().size());
        for (auto& stream : globalWebSocketStreamMap().values())
            sockets.uncheckedAppend(stream);
    }

    for (auto& stream : sockets) {
        for (auto& callback : stream->m_sendDataCallbacks.values())
            callback(false);
        for (auto& callback : stream->m_sendHandshakeCallbacks.values())
            callback(false, false);
        stream->m_client.didFailSocketStream(*stream, SocketStreamError(0, { }, "Network process crashed."));
        stream = nullptr;
    }

    Locker stateLocker { globalWebSocketStreamMapLock };
    globalWebSocketStreamMap().clear();
}

Ref<WebSocketStream> WebSocketStream::create(const URL& url, SocketStreamHandleClient& client, WebSocketIdentifier identifier, const String& credentialPartition)
{
    return adoptRef(*new WebSocketStream(url, client, identifier, credentialPartition));
}

WebSocketStream::WebSocketStream(const URL& url, WebCore::SocketStreamHandleClient& client, WebSocketIdentifier identifier, const String& cachePartition)
    : SocketStreamHandle(url, client)
    ,  m_identifier(identifier)
    , m_client(client)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::CreateSocketStream(url, cachePartition, m_identifier), 0);

    Locker stateLocker { globalWebSocketStreamMapLock };
    ASSERT(!globalWebSocketStreamMap().contains(m_identifier));
    globalWebSocketStreamMap().set(m_identifier, this);
}

WebSocketStream::~WebSocketStream()
{
    Locker stateLocker { globalWebSocketStreamMapLock };
    ASSERT(globalWebSocketStreamMap().contains(m_identifier));
    globalWebSocketStreamMap().remove(m_identifier);
}

IPC::Connection* WebSocketStream::messageSenderConnection() const
{
    return &WebProcess::singleton().ensureNetworkProcessConnection().connection();
}

uint64_t WebSocketStream::messageSenderDestinationID() const
{
    return m_identifier.toUInt64();
}

void WebSocketStream::platformSend(const uint8_t* data, size_t length, Function<void(bool)>&& completionHandler)
{
    static uint64_t nextDataIdentifier = 1;
    uint64_t dataIdentifier = nextDataIdentifier++;
    send(Messages::NetworkSocketStream::SendData(IPC::DataReference(data, length), dataIdentifier));
    ASSERT(!m_sendDataCallbacks.contains(dataIdentifier));
    m_sendDataCallbacks.add(dataIdentifier, WTFMove(completionHandler));
}

void WebSocketStream::platformSendHandshake(const uint8_t* data, size_t length, const std::optional<CookieRequestHeaderFieldProxy>& headerFieldProxy, Function<void(bool, bool)>&& completionHandler)
{
    static uint64_t nextDataIdentifier = 1;
    uint64_t dataIdentifier = nextDataIdentifier++;
    send(Messages::NetworkSocketStream::SendHandshake(IPC::DataReference(data, length), headerFieldProxy, dataIdentifier));
    ASSERT(!m_sendHandshakeCallbacks.contains(dataIdentifier));
    m_sendHandshakeCallbacks.add(dataIdentifier, WTFMove(completionHandler));
}

void WebSocketStream::didSendData(uint64_t identifier, bool success)
{
    ASSERT(m_sendDataCallbacks.contains(identifier));
    m_sendDataCallbacks.take(identifier)(success);
}

void WebSocketStream::didSendHandshake(uint64_t identifier, bool success, bool didAccessSecureCookies)
{
    ASSERT(m_sendHandshakeCallbacks.contains(identifier));
    m_sendHandshakeCallbacks.take(identifier)(success, didAccessSecureCookies);
}

void WebSocketStream::platformClose()
{
    send(Messages::NetworkSocketStream::Close());
}

size_t WebSocketStream::bufferedAmount()
{
    return m_bufferedAmount;
}

void WebSocketStream::didOpenSocketStream()
{
    m_state = Open;
    m_client.didOpenSocketStream(*this);
}

void WebSocketStream::didCloseSocketStream()
{
    m_state = Closed;
    m_client.didCloseSocketStream(*this);
}

void WebSocketStream::didReceiveSocketStreamData(const IPC::DataReference& data)
{
    m_client.didReceiveSocketStreamData(*this, data.data(), data.size());
}

void WebSocketStream::didFailToReceiveSocketStreamData()
{
    m_client.didFailToReceiveSocketStreamData(*this);
}

void WebSocketStream::didUpdateBufferedAmount(uint64_t newAmount)
{
    m_bufferedAmount = newAmount;
    m_client.didUpdateBufferedAmount(*this, newAmount);
}

void WebSocketStream::didFailSocketStream(WebCore::SocketStreamError&& error)
{
    m_client.didFailSocketStream(*this, WTFMove(error));
}

} // namespace WebKit
