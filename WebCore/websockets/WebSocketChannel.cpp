/*
 * Copyright (C) 2009 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_SOCKETS)

#include "WebSocketChannel.h"

#include "CString.h"
#include "CookieJar.h"
#include "Document.h"
#include "Logging.h"
#include "PlatformString.h"
#include "ScriptExecutionContext.h"
#include "SocketStreamError.h"
#include "SocketStreamHandle.h"
#include "StringHash.h"
#include "WebSocketChannelClient.h"

#include <wtf/Deque.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashMap.h>

namespace WebCore {

WebSocketChannel::WebSocketChannel(ScriptExecutionContext* context, WebSocketChannelClient* client, const KURL& url, const String& protocol)
    : m_context(context)
    , m_client(client)
    , m_handshake(url, protocol, context)
    , m_buffer(0)
    , m_bufferSize(0)
    , m_unhandledBufferSize(0)
{
}

WebSocketChannel::~WebSocketChannel()
{
    fastFree(m_buffer);
}

void WebSocketChannel::connect()
{
    LOG(Network, "WebSocketChannel %p connect", this);
    ASSERT(!m_handle.get());
    m_handshake.reset();
    ref();
    m_handle = SocketStreamHandle::create(m_handshake.url(), this);
}

bool WebSocketChannel::send(const String& msg)
{
    LOG(Network, "WebSocketChannel %p send %s", this, msg.utf8().data());
    Vector<char> buf;
    buf.append('\0');  // frame type
    buf.append(msg.utf8().data(), msg.utf8().length());
    buf.append('\xff');  // frame end
    if (!m_handle.get()) {
        m_unhandledBufferSize += buf.size();
        return false;
    }
    return m_handle->send(buf.data(), buf.size());
}

unsigned long WebSocketChannel::bufferedAmount() const
{
    LOG(Network, "WebSocketChannel %p bufferedAmount", this);
    if (!m_handle.get())
        return m_unhandledBufferSize;
    return m_handle->bufferedAmount();
}

void WebSocketChannel::close()
{
    LOG(Network, "WebSocketChannel %p close", this);
    if (m_handle.get())
        m_handle->close();  // will call didClose()
}

void WebSocketChannel::disconnect()
{
    LOG(Network, "WebSocketChannel %p disconnect", this);
    m_client = 0;
    if (m_handle.get())
        m_handle->close();
}

void WebSocketChannel::willOpenStream(SocketStreamHandle*, const KURL&)
{
}

void WebSocketChannel::willSendData(SocketStreamHandle*, const char*, int)
{
}

void WebSocketChannel::didOpen(SocketStreamHandle* handle)
{
    LOG(Network, "WebSocketChannel %p didOpen", this);
    ASSERT(handle == m_handle.get());
    const CString& handshakeMessage = m_handshake.clientHandshakeMessage();
    if (!handle->send(handshakeMessage.data(), handshakeMessage.length())) {
        LOG(Network, "Error in sending handshake message.");
        handle->close();
    }
}

void WebSocketChannel::didClose(SocketStreamHandle* handle)
{
    LOG(Network, "WebSocketChannel %p didClose", this);
    ASSERT(handle == m_handle.get() || !m_handle.get());
    if (m_handle.get()) {
        m_unhandledBufferSize = handle->bufferedAmount();
        WebSocketChannelClient* client = m_client;
        m_client = 0;
        m_handle = 0;
        if (client)
            client->didClose();
    }
    deref();
}

void WebSocketChannel::didReceiveData(SocketStreamHandle* handle, const char* data, int len)
{
    LOG(Network, "WebSocketChannel %p didReceiveData %d", this, len);
    ASSERT(handle == m_handle.get());
    if (!appendToBuffer(data, len)) {
        handle->close();
        return;
    }
    if (!m_client) {
        handle->close();
        return;
    }
    if (m_handshake.mode() != WebSocketHandshake::Connected) {
        int headerLength = m_handshake.readServerHandshake(m_buffer, m_bufferSize);
        if (headerLength <= 0)
            return;
        switch (m_handshake.mode()) {
        case WebSocketHandshake::Connected:
            if (!m_handshake.serverSetCookie().isEmpty()) {
                if (m_context->isDocument()) {
                    Document* document = static_cast<Document*>(m_context);
                    if (cookiesEnabled(document))
                        document->setCookie(m_handshake.serverSetCookie());
                }
            }
            // FIXME: handle set-cookie2.
            LOG(Network, "WebSocketChannel %p connected", this);
            m_client->didConnect();
            break;
        default:
            LOG(Network, "WebSocketChannel %p connection failed", this);
            handle->close();
            return;
        }
        skipBuffer(headerLength);
        if (!m_buffer)
            return;
        LOG(Network, "remaining in read buf %ul", m_bufferSize);
    }

    const char* nextFrame = m_buffer;
    const char* p = m_buffer;
    const char* end = p + m_bufferSize;
    while (p < end) {
        unsigned char frameByte = static_cast<unsigned char>(*p++);
        if ((frameByte & 0x80) == 0x80) {
            int length = 0;
            while (p < end && (*p & 0x80) == 0x80) {
                if (length > std::numeric_limits<int>::max() / 128) {
                    LOG(Network, "frame length overflow %d", length);
                    handle->close();
                    return;
                }
                length = length * 128 + (*p & 0x7f);
                ++p;
            }
            if (p + length < end) {
                p += length;
                nextFrame = p;
            }
        } else {
            const char* msgStart = p;
            while (p < end && *p != '\xff')
                ++p;
            if (p < end && *p == '\xff') {
                if (frameByte == 0x00)
                    m_client->didReceiveMessage(String::fromUTF8(msgStart, p - msgStart));
                ++p;
                nextFrame = p;
            }
        }
    }
    skipBuffer(nextFrame - m_buffer);
}

void WebSocketChannel::didFail(SocketStreamHandle* handle, const SocketStreamError&)
{
    LOG(Network, "WebSocketChannel %p didFail", this);
    ASSERT(handle == m_handle.get() || !m_handle.get());
    handle->close();
}

bool WebSocketChannel::appendToBuffer(const char* data, int len)
{
    char* newBuffer = 0;
    if (tryFastMalloc(m_bufferSize + len).getValue(newBuffer)) {
        if (m_buffer)
            memcpy(newBuffer, m_buffer, m_bufferSize);
        memcpy(newBuffer + m_bufferSize, data, len);
        fastFree(m_buffer);
        m_buffer = newBuffer;
        m_bufferSize += len;
        return true;
    }
    LOG(Network, "Too long WebSocket frame %d", m_bufferSize + len);
    return false;
}

void WebSocketChannel::skipBuffer(int len)
{
    ASSERT(len <= m_bufferSize);
    m_bufferSize -= len;
    if (!m_bufferSize) {
        fastFree(m_buffer);
        m_buffer = 0;
        return;
    }
    memmove(m_buffer, m_buffer + len, m_bufferSize);
}

}  // namespace WebCore

#endif  // ENABLE(WEB_SOCKETS)
