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

#include "WebSocketHandshake.h"

#include "AtomicString.h"
#include "Cookie.h"
#include "CookieJar.h"
#include "Document.h"
#include "HTTPHeaderMap.h"
#include "KURL.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "StringBuilder.h"
#include <wtf/text/CString.h>
#include <wtf/StringExtras.h>
#include <wtf/Vector.h>

namespace WebCore {

const char webSocketServerHandshakeHeader[] = "HTTP/1.1 101 Web Socket Protocol Handshake\r\n";
const char webSocketUpgradeHeader[] = "Upgrade: WebSocket\r\n";
const char webSocketConnectionHeader[] = "Connection: Upgrade\r\n";

static String extractResponseCode(const char* header, int len)
{
    const char* space1 = 0;
    const char* space2 = 0;
    const char* p;
    for (p = header; p - header < len; p++) {
        if (*p == ' ') {
            if (!space1)
                space1 = p;
            else if (!space2)
                space2 = p;
        } else if (*p == '\n')
            break;
    }
    if (p - header == len)
        return String();
    if (!space1 || !space2)
        return "";
    return String(space1 + 1, space2 - space1 - 1);
}

static String resourceName(const KURL& url)
{
    String name = url.path();
    if (name.isEmpty())
        name = "/";
    if (!url.query().isNull())
        name += "?" + url.query();
    ASSERT(!name.isEmpty());
    ASSERT(!name.contains(' '));
    return name;
}

static String trimConsoleMessage(const char* p, size_t len)
{
    String s = String(p, std::min<size_t>(len, 128));
    if (len > 128)
        s += "...";
    return s;
}

WebSocketHandshake::WebSocketHandshake(const KURL& url, const String& protocol, ScriptExecutionContext* context)
    : m_url(url)
    , m_clientProtocol(protocol)
    , m_secure(m_url.protocolIs("wss"))
    , m_context(context)
    , m_mode(Incomplete)
{
}

WebSocketHandshake::~WebSocketHandshake()
{
}

const KURL& WebSocketHandshake::url() const
{
    return m_url;
}

void WebSocketHandshake::setURL(const KURL& url)
{
    m_url = url.copy();
}

const String WebSocketHandshake::host() const
{
    return m_url.host().lower();
}

const String& WebSocketHandshake::clientProtocol() const
{
    return m_clientProtocol;
}

void WebSocketHandshake::setClientProtocol(const String& protocol)
{
    m_clientProtocol = protocol;
}

bool WebSocketHandshake::secure() const
{
    return m_secure;
}

String WebSocketHandshake::clientOrigin() const
{
    return m_context->securityOrigin()->toString();
}

String WebSocketHandshake::clientLocation() const
{
    StringBuilder builder;
    builder.append(m_secure ? "wss" : "ws");
    builder.append("://");
    builder.append(m_url.host().lower());
    if (m_url.port()) {
        if ((!m_secure && m_url.port() != 80) || (m_secure && m_url.port() != 443)) {
            builder.append(":");
            builder.append(String::number(m_url.port()));
        }
    }
    builder.append(resourceName(m_url));
    return builder.toString();
}

CString WebSocketHandshake::clientHandshakeMessage() const
{
    // Keep the following consistent with clientHandshakeRequest().
    StringBuilder builder;

    builder.append("GET ");
    builder.append(resourceName(m_url));
    builder.append(" HTTP/1.1\r\n");
    builder.append("Upgrade: WebSocket\r\n");
    builder.append("Connection: Upgrade\r\n");
    builder.append("Host: ");
    builder.append(m_url.host().lower());
    if (m_url.port() && ((!m_secure && m_url.port() != 80) || (m_secure && m_url.port() != 443))) {
        builder.append(":");
        builder.append(String::number(m_url.port()));
    }
    builder.append("\r\n");
    builder.append("Origin: ");
    builder.append(clientOrigin());
    builder.append("\r\n");
    if (!m_clientProtocol.isEmpty()) {
        builder.append("WebSocket-Protocol: ");
        builder.append(m_clientProtocol);
        builder.append("\r\n");
    }

    KURL url = httpURLForAuthenticationAndCookies();
    if (m_context->isDocument()) {
        Document* document = static_cast<Document*>(m_context);
        String cookie = cookieRequestHeaderFieldValue(document, url);
        if (!cookie.isEmpty()) {
            builder.append("Cookie: ");
            builder.append(cookie);
            builder.append("\r\n");
        }
        // Set "Cookie2: <cookie>" if cookies 2 exists for url?
    }

    builder.append("\r\n");
    return builder.toString().utf8();
}

WebSocketHandshakeRequest WebSocketHandshake::clientHandshakeRequest() const
{
    // Keep the following consistent with clientHandshakeMessage().
    WebSocketHandshakeRequest request(m_url, clientOrigin(), m_clientProtocol);

    KURL url = httpURLForAuthenticationAndCookies();
    if (m_context->isDocument()) {
        Document* document = static_cast<Document*>(m_context);
        String cookie = cookieRequestHeaderFieldValue(document, url);
        if (!cookie.isEmpty())
            request.addExtraHeaderField("Cookie", cookie);
        // Set "Cookie2: <cookie>" if cookies 2 exists for url?
    }

    return request;
}

void WebSocketHandshake::reset()
{
    m_mode = Incomplete;

    m_wsOrigin = String();
    m_wsLocation = String();
    m_wsProtocol = String();
    m_setCookie = String();
    m_setCookie2 = String();
}

void WebSocketHandshake::clearScriptExecutionContext()
{
    m_context = 0;
}

int WebSocketHandshake::readServerHandshake(const char* header, size_t len)
{
    m_mode = Incomplete;
    if (len < sizeof(webSocketServerHandshakeHeader) - 1) {
        // Just hasn't been received fully yet.
        return -1;
    }
    if (!memcmp(header, webSocketServerHandshakeHeader, sizeof(webSocketServerHandshakeHeader) - 1))
        m_mode = Normal;
    else {
        const String& code = extractResponseCode(header, len);
        if (code.isNull()) {
            m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Short server handshake: " + trimConsoleMessage(header, len), 0, clientOrigin());
            return -1;
        }
        if (code.isEmpty()) {
            m_mode = Failed;
            m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "No response code found: " + trimConsoleMessage(header, len), 0, clientOrigin());
            return len;
        }
        LOG(Network, "response code: %s", code.utf8().data());
        if (code == "401") {
            m_mode = Failed;
            m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Authentication required, but not implemented yet.", 0, clientOrigin());
            return len;
        } else {
            m_mode = Failed;
            m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Unexpected response code:" + code, 0, clientOrigin());
            return len;
        }
    }
    const char* p = header + sizeof(webSocketServerHandshakeHeader) - 1;
    const char* end = header + len;

    if (m_mode == Normal) {
        size_t headerSize = end - p;
        if (headerSize < sizeof(webSocketUpgradeHeader) - 1) {
            m_mode = Incomplete;
            return 0;
        }
        if (memcmp(p, webSocketUpgradeHeader, sizeof(webSocketUpgradeHeader) - 1)) {
            m_mode = Failed;
            m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Bad Upgrade header: " + trimConsoleMessage(p, end - p), 0, clientOrigin());
            return p - header + sizeof(webSocketUpgradeHeader) - 1;
        }
        p += sizeof(webSocketUpgradeHeader) - 1;

        headerSize = end - p;
        if (headerSize < sizeof(webSocketConnectionHeader) - 1) {
            m_mode = Incomplete;
            return -1;
        }
        if (memcmp(p, webSocketConnectionHeader, sizeof(webSocketConnectionHeader) - 1)) {
            m_mode = Failed;
            m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Bad Connection header: " + trimConsoleMessage(p, end - p), 0, clientOrigin());
            return p - header + sizeof(webSocketConnectionHeader) - 1;
        }
        p += sizeof(webSocketConnectionHeader) - 1;
    }

    if (!strnstr(p, "\r\n\r\n", end - p)) {
        // Just hasn't been received fully yet.
        m_mode = Incomplete;
        return -1;
    }
    HTTPHeaderMap headers;
    p = readHTTPHeaders(p, end, &headers);
    if (!p) {
        LOG(Network, "readHTTPHeaders failed");
        m_mode = Failed;
        return len;
    }
    if (!processHeaders(headers)) {
        LOG(Network, "header process failed");
        m_mode = Failed;
        return p - header;
    }
    switch (m_mode) {
    case Normal:
        checkResponseHeaders();
        break;
    default:
        m_mode = Failed;
        break;
    }
    return p - header;
}

WebSocketHandshake::Mode WebSocketHandshake::mode() const
{
    return m_mode;
}

const String& WebSocketHandshake::serverWebSocketOrigin() const
{
    return m_wsOrigin;
}

void WebSocketHandshake::setServerWebSocketOrigin(const String& webSocketOrigin)
{
    m_wsOrigin = webSocketOrigin;
}

const String& WebSocketHandshake::serverWebSocketLocation() const
{
    return m_wsLocation;
}

void WebSocketHandshake::setServerWebSocketLocation(const String& webSocketLocation)
{
    m_wsLocation = webSocketLocation;
}

const String& WebSocketHandshake::serverWebSocketProtocol() const
{
    return m_wsProtocol;
}

void WebSocketHandshake::setServerWebSocketProtocol(const String& webSocketProtocol)
{
    m_wsProtocol = webSocketProtocol;
}

const String& WebSocketHandshake::serverSetCookie() const
{
    return m_setCookie;
}

void WebSocketHandshake::setServerSetCookie(const String& setCookie)
{
    m_setCookie = setCookie;
}

const String& WebSocketHandshake::serverSetCookie2() const
{
    return m_setCookie2;
}

void WebSocketHandshake::setServerSetCookie2(const String& setCookie2)
{
    m_setCookie2 = setCookie2;
}

KURL WebSocketHandshake::httpURLForAuthenticationAndCookies() const
{
    KURL url = m_url.copy();
    bool couldSetProtocol = url.setProtocol(m_secure ? "https" : "http");
    ASSERT_UNUSED(couldSetProtocol, couldSetProtocol);
    return url;
}

const char* WebSocketHandshake::readHTTPHeaders(const char* start, const char* end, HTTPHeaderMap* headers)
{
    Vector<char> name;
    Vector<char> value;
    for (const char* p = start; p < end; p++) {
        name.clear();
        value.clear();

        for (; p < end; p++) {
            switch (*p) {
            case '\r':
                if (name.isEmpty()) {
                    if (p + 1 < end && *(p + 1) == '\n')
                        return p + 2;
                    m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "CR doesn't follow LF at " + trimConsoleMessage(p, end - p), 0, clientOrigin());
                    return 0;
                }
                m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Unexpected CR in name at " + trimConsoleMessage(p, end - p), 0, clientOrigin());
                return 0;
            case '\n':
                m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Unexpected LF in name at " + trimConsoleMessage(p, end - p), 0, clientOrigin());
                return 0;
            case ':':
                break;
            default:
                if (*p >= 0x41 && *p <= 0x5a)
                    name.append(*p + 0x20);
                else
                    name.append(*p);
                continue;
            }
            if (*p == ':') {
                ++p;
                break;
            }
        }

        for (; p < end && *p == 0x20; p++) { }

        for (; p < end; p++) {
            switch (*p) {
            case '\r':
                break;
            case '\n':
                m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Unexpected LF in value at " + trimConsoleMessage(p, end - p), 0, clientOrigin());
                return 0;
            default:
                value.append(*p);
            }
            if (*p == '\r') {
                ++p;
                break;
            }
        }
        if (p >= end || *p != '\n') {
            m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "CR doesn't follow LF after value at " + trimConsoleMessage(p, end - p), 0, clientOrigin());
            return 0;
        }
        AtomicString nameStr(String::fromUTF8(name.data(), name.size()));
        String valueStr = String::fromUTF8(value.data(), value.size());
        if (nameStr.isNull()) {
            m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "invalid UTF-8 sequence in header name", 0, clientOrigin());
            return 0;
        }
        if (valueStr.isNull()) {
            m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "invalid UTF-8 sequence in header value", 0, clientOrigin());
            return 0;
        }
        LOG(Network, "name=%s value=%s", nameStr.string().utf8().data(), valueStr.utf8().data());
        headers->add(nameStr, valueStr);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

bool WebSocketHandshake::processHeaders(const HTTPHeaderMap& headers)
{
    for (HTTPHeaderMap::const_iterator it = headers.begin(); it != headers.end(); ++it) {
        switch (m_mode) {
        case Normal:
            if (it->first == "websocket-origin")
                m_wsOrigin = it->second;
            else if (it->first == "websocket-location")
                m_wsLocation = it->second;
            else if (it->first == "websocket-protocol")
                m_wsProtocol = it->second;
            else if (it->first == "set-cookie")
                m_setCookie = it->second;
            else if (it->first == "set-cookie2")
                m_setCookie2 = it->second;
            continue;
        case Incomplete:
        case Failed:
        case Connected:
            ASSERT_NOT_REACHED();
        }
        ASSERT_NOT_REACHED();
    }
    return true;
}

void WebSocketHandshake::checkResponseHeaders()
{
    ASSERT(m_mode == Normal);
    m_mode = Failed;
    if (m_wsOrigin.isNull()) {
        m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Error during WebSocket handshake: 'websocket-origin' header is missing", 0, clientOrigin());
        return;
    }
    if (m_wsLocation.isNull()) {
        m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Error during WebSocket handshake: 'websocket-location' header is missing", 0, clientOrigin());
        return;
    }

    if (clientOrigin() != m_wsOrigin) {
        m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Error during WebSocket handshake: origin mismatch: " + clientOrigin() + " != " + m_wsOrigin, 0, clientOrigin());
        return;
    }
    if (clientLocation() != m_wsLocation) {
        m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Error during WebSocket handshake: location mismatch: " + clientLocation() + " != " + m_wsLocation, 0, clientOrigin());
        return;
    }
    if (!m_clientProtocol.isEmpty() && m_clientProtocol != m_wsProtocol) {
        m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Error during WebSocket handshake: protocol mismatch: " + m_clientProtocol + " != " + m_wsProtocol, 0, clientOrigin());
        return;
    }
    m_mode = Connected;
    return;
}

}  // namespace WebCore

#endif  // ENABLE(WEB_SOCKETS)
