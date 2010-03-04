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

#ifndef WebSocketHandshakeRequest_h
#define WebSocketHandshakeRequest_h

#if ENABLE(WEB_SOCKETS)

#include "KURL.h"
#include "PlatformString.h"
#include <wtf/Vector.h>

namespace WebCore {

class AtomicString;

class WebSocketHandshakeRequest {
public:
    WebSocketHandshakeRequest(const KURL&, const String& origin, const String& webSocketProtocol);
    ~WebSocketHandshakeRequest();

    // According to current Web Socket protocol specification, four mandatory headers (Upgrade, Connection, Host, and Origin) and
    // one optional header (WebSocket-Protocol) should be sent in this order, at the beginning of the handshake request.
    // The remaining headers can be set by using the following function.
    void addExtraHeaderField(const AtomicString& name, const String& value);
    void addExtraHeaderField(const char* name, const String& value);

    // Returns the list of header fields including five special ones.
    typedef std::pair<AtomicString, String> HeaderField;
    Vector<HeaderField> headerFields() const;

private:
    String host() const;

    KURL m_url;
    bool m_secure;
    String m_origin;
    String m_webSocketProtocol;
    Vector<HeaderField> m_extraHeaderFields;
};

} // namespace WebCore

#endif // ENABLE(WEB_SOCKETS)

#endif // WebSocketHandshakeRequest_h
