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

#include "WebSocket.h"

#include "Event.h"
#include "EventException.h"
#include "EventListener.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

WebSocket::WebSocket(ScriptExecutionContext* context)
    : ActiveDOMObject(context, this)
    , m_state(CONNECTING)
{
}

WebSocket::~WebSocket()
{
    close();
}

void WebSocket::connect(const KURL& url, ExceptionCode& ec)
{
    connect(url, String(), ec);
}

void WebSocket::connect(const KURL& url, const String& protocol, ExceptionCode& ec)
{
    m_url = url;
    m_protocol = protocol;

    if (!m_url.protocolIs("ws") && !m_url.protocolIs("wss")) {
        m_state = CLOSED;
        ec = SYNTAX_ERR;
        return;
    }
    if (!m_protocol.isNull() && m_protocol.isEmpty()) {
        m_state = CLOSED;
        ec = SYNTAX_ERR;
        return;
    }
    // FIXME: Check protocol is valid form.
    // FIXME: Connect WebSocketChannel.
}

bool WebSocket::send(const String&, ExceptionCode& ec)
{
    if (m_state != OPEN) {
        ec = INVALID_STATE_ERR;
        return false;
    }
    // FIXME: send message on WebSocketChannel.
    return false;
}

void WebSocket::close()
{
    if (m_state == CLOSED)
        return;
    m_state = CLOSED;
    // FIXME: close WebSocketChannel.
}

const KURL& WebSocket::url() const
{
    return m_url;
}

WebSocket::State WebSocket::readyState() const
{
    return m_state;
}

unsigned long WebSocket::bufferedAmount() const
{
    // FIXME: ask platform code to get buffered amount to be sent.
    return 0;
}

ScriptExecutionContext* WebSocket::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

void WebSocket::addEventListener(const AtomicString&, PassRefPtr<EventListener>, bool)
{
    // FIXME: implement this.
}

void WebSocket::removeEventListener(const AtomicString&, EventListener*, bool)
{
    // FIXME: implement this.
}

bool WebSocket::dispatchEvent(PassRefPtr<Event>, ExceptionCode&)
{
    // FIXME: implement this.
    return false;
}

void WebSocket::didConnect()
{
    if (m_state != CONNECTING) {
        didClose();
        return;
    }
    m_state = OPEN;
    dispatchOpenEvent();
}

void WebSocket::didReceiveMessage(const String& msg)
{
    if (m_state != OPEN)
        return;
    dispatchMessageEvent(msg);
}

void WebSocket::didClose()
{
    m_state = CLOSED;
    dispatchCloseEvent();
}

void WebSocket::dispatchOpenEvent()
{
    // FIXME: implement this.
}

void WebSocket::dispatchMessageEvent(const String&)
{
    // FIXME: implement this.
}

void WebSocket::dispatchCloseEvent()
{
    // FIXME: implement this.
}

}  // namespace WebCore

#endif
