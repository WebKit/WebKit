/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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
#include "WebSocketImpl.h"

#include <wtf/ArrayBuffer.h>
#include "Document.h"
#include "KURL.h"
#if ENABLE(WEB_SOCKETS)
#include "WebSocketChannel.h"
#include "WebSocketChannelClient.h"
#else
namespace WebCore {
class WebSocketChannel {
};
} // namespace WebCore
#endif

#include "WebArrayBuffer.h"
#include "WebDocument.h"
#include "WebSocketClient.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"

using namespace WebCore;

namespace WebKit {

WebSocketImpl::WebSocketImpl(const WebDocument& document, WebSocketClient* client)
    : m_client(client)
    , m_binaryType(BinaryTypeBlob)
{
#if ENABLE(WEB_SOCKETS)
    m_private = WebSocketChannel::create(PassRefPtr<Document>(document).get(), this);
#else
    ASSERT_NOT_REACHED();
#endif
}

WebSocketImpl::~WebSocketImpl()
{
#if ENABLE(WEB_SOCKETS)
    m_private->disconnect();
#else
    ASSERT_NOT_REACHED();
#endif
}

WebSocket::BinaryType WebSocketImpl::binaryType() const
{
    return m_binaryType;
}

bool WebSocketImpl::setBinaryType(BinaryType binaryType)
{
    if (binaryType > BinaryTypeArrayBuffer)
        return false;
    m_binaryType = binaryType;
    return true;
}

void WebSocketImpl::connect(const WebURL& url, const WebString& protocol)
{
#if ENABLE(WEB_SOCKETS)
    m_private->connect(url, protocol);
#else
    ASSERT_NOT_REACHED();
#endif
}

WebString WebSocketImpl::subprotocol()
{
#if ENABLE(WEB_SOCKETS)
    return m_private->subprotocol();
#else
    ASSERT_NOT_REACHED();
#endif
}

WebString WebSocketImpl::extensions()
{
#if ENABLE(WEB_SOCKETS)
    return m_private->extensions();
#else
    ASSERT_NOT_REACHED();
#endif
}

bool WebSocketImpl::sendText(const WebString& message)
{
#if ENABLE(WEB_SOCKETS)
    return m_private->send(message) == ThreadableWebSocketChannel::SendSuccess;
#else
    ASSERT_NOT_REACHED();
#endif
}

bool WebSocketImpl::sendArrayBuffer(const WebArrayBuffer& webArrayBuffer)
{
#if ENABLE(WEB_SOCKETS)
    return m_private->send(*PassRefPtr<ArrayBuffer>(webArrayBuffer)) == ThreadableWebSocketChannel::SendSuccess;
#else
    ASSERT_NOT_REACHED();
#endif
}

unsigned long WebSocketImpl::bufferedAmount() const
{
#if ENABLE(WEB_SOCKETS)
    return m_private->bufferedAmount();
#else
    ASSERT_NOT_REACHED();
#endif
}

void WebSocketImpl::close(int code, const WebString& reason)
{
#if ENABLE(WEB_SOCKETS)
    m_private->close(code, reason);
#else
    ASSERT_NOT_REACHED();
#endif
}

void WebSocketImpl::fail(const WebString& reason)
{
#if ENABLE(WEB_SOCKETS)
    m_private->fail(reason);
#else
    ASSERT_NOT_REACHED();
#endif
}

void WebSocketImpl::disconnect()
{
#if ENABLE(WEB_SOCKETS)
    m_private->disconnect();
    m_client = 0;
#else
    ASSERT_NOT_REACHED();
#endif
}

void WebSocketImpl::didConnect()
{
#if ENABLE(WEB_SOCKETS)
    m_client->didConnect();
#else
    ASSERT_NOT_REACHED();
#endif
}

void WebSocketImpl::didReceiveMessage(const String& message)
{
#if ENABLE(WEB_SOCKETS)
    m_client->didReceiveMessage(WebString(message));
#else
    ASSERT_NOT_REACHED();
#endif
}

void WebSocketImpl::didReceiveBinaryData(PassOwnPtr<Vector<char> > binaryData)
{
#if ENABLE(WEB_SOCKETS)
    switch (m_binaryType) {
    case BinaryTypeBlob:
        // FIXME: Handle Blob after supporting WebBlob.
        break;
    case BinaryTypeArrayBuffer:
        m_client->didReceiveArrayBuffer(WebArrayBuffer(ArrayBuffer::create(binaryData->data(), binaryData->size())));
        break;
    }
#else
    ASSERT_NOT_REACHED();
#endif
}

void WebSocketImpl::didReceiveMessageError()
{
#if ENABLE(WEB_SOCKETS)
    m_client->didReceiveMessageError();
#else
    ASSERT_NOT_REACHED();
#endif
}

void WebSocketImpl::didUpdateBufferedAmount(unsigned long bufferedAmount)
{
#if ENABLE(WEB_SOCKETS)
    m_client->didUpdateBufferedAmount(bufferedAmount);
#else
    ASSERT_NOT_REACHED();
#endif
}

void WebSocketImpl::didStartClosingHandshake()
{
#if ENABLE(WEB_SOCKETS)
    m_client->didStartClosingHandshake();
#else
    ASSERT_NOT_REACHED();
#endif
}

void WebSocketImpl::didClose(unsigned long bufferedAmount, ClosingHandshakeCompletionStatus status, unsigned short code, const String& reason)
{
#if ENABLE(WEB_SOCKETS)
    m_client->didClose(bufferedAmount, static_cast<WebSocketClient::ClosingHandshakeCompletionStatus>(status), code, WebString(reason));
#else
    ASSERT_NOT_REACHED();
#endif
}

} // namespace WebKit
