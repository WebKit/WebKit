/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
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

#include "WebSocketHandshakeRequest.h"

#include "AtomicString.h"
#include "StringBuilder.h"
#include <utility>
#include <wtf/Assertions.h>

using namespace std;

namespace WebCore {

WebSocketHandshakeRequest::WebSocketHandshakeRequest(const KURL& url, const String& origin, const String& webSocketProtocol)
    : m_url(url)
    , m_secure(m_url.protocolIs("wss"))
    , m_origin(origin)
    , m_webSocketProtocol(webSocketProtocol)
{
    ASSERT(!origin.isNull());
}

WebSocketHandshakeRequest::~WebSocketHandshakeRequest()
{
}

void WebSocketHandshakeRequest::addExtraHeaderField(const AtomicString& name, const String& value)
{
    m_extraHeaderFields.append(HeaderField(name, value));
}

void WebSocketHandshakeRequest::addExtraHeaderField(const char* name, const String& value)
{
    m_extraHeaderFields.append(HeaderField(name, value));
}

Vector<WebSocketHandshakeRequest::HeaderField> WebSocketHandshakeRequest::headerFields() const
{
    Vector<HeaderField> fields;
    fields.append(HeaderField("Upgrade", "WebSocket"));
    fields.append(HeaderField("Connection", "Upgrade"));
    fields.append(HeaderField("Host", host()));
    fields.append(HeaderField("Origin", m_origin));
    if (!m_webSocketProtocol.isEmpty())
        fields.append(HeaderField("WebSocket-Protocol", m_webSocketProtocol));
    fields.append(m_extraHeaderFields);
    return fields;
}

String WebSocketHandshakeRequest::host() const
{
    StringBuilder builder;
    builder.append(m_url.host().lower());
    if ((!m_secure && m_url.port() != 80) || (m_secure && m_url.port() != 443)) {
        builder.append(":");
        builder.append(String::number(m_url.port()));
    }
    return builder.toString();
}

} // namespace WebCore

#endif // ENABLE(WEB_SOCKETS)
