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

#ifndef WebSocketHandshake_h
#define WebSocketHandshake_h

#if ENABLE(WEB_SOCKETS)

#include "KURL.h"
#include "PlatformString.h"
#include "WebSocketHandshakeRequest.h"
#include "WebSocketHandshakeResponse.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

    class ScriptExecutionContext;

    class WebSocketHandshake : public Noncopyable {
    public:
        enum Mode {
            Incomplete, Normal, Failed, Connected
        };
        WebSocketHandshake(const KURL&, const String& protocol, ScriptExecutionContext*);
        ~WebSocketHandshake();

        const KURL& url() const;
        void setURL(const KURL&);
        const String host() const;

        const String& clientProtocol() const;
        void setClientProtocol(const String& protocol);

        bool secure() const;

        String clientOrigin() const;
        String clientLocation() const;

        CString clientHandshakeMessage() const;
        WebSocketHandshakeRequest clientHandshakeRequest() const;

        void reset();
        void clearScriptExecutionContext();

        int readServerHandshake(const char* header, size_t len);
        Mode mode() const;

        const String& serverWebSocketOrigin() const;
        void setServerWebSocketOrigin(const String& webSocketOrigin);

        const String& serverWebSocketLocation() const;
        void setServerWebSocketLocation(const String& webSocketLocation);

        const String& serverWebSocketProtocol() const;
        void setServerWebSocketProtocol(const String& webSocketProtocol);

        const String& serverSetCookie() const;
        void setServerSetCookie(const String& setCookie);
        const String& serverSetCookie2() const;
        void setServerSetCookie2(const String& setCookie2);

        const WebSocketHandshakeResponse& serverHandshakeResponse() const;

    private:
        KURL httpURLForAuthenticationAndCookies() const;

        int readStatusLine(const char* header, size_t headerLength, int& statusCode, String& statusText);

        // Reads all headers except for the two predefined ones.
        const char* readHTTPHeaders(const char* start, const char* end);
        void processHeaders();
        bool checkResponseHeaders();

        KURL m_url;
        String m_clientProtocol;
        bool m_secure;
        ScriptExecutionContext* m_context;

        Mode m_mode;

        String m_wsOrigin;
        String m_wsLocation;
        String m_wsProtocol;
        String m_setCookie;
        String m_setCookie2;

        String m_secWebSocketKey1;
        String m_secWebSocketKey2;
        unsigned char m_key3[8];
        unsigned char m_expectedChallengeResponse[16];

        WebSocketHandshakeResponse m_response;
    };

} // namespace WebCore

#endif // ENABLE(WEB_SOCKETS)

#endif // WebSocketHandshake_h
