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

#include <wtf/MD5.h>
#include <wtf/RandomNumber.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StringExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

namespace WebCore {

static const char randomCharacterInSecWebSocketKey[] = "!\"#$%&'()*+,-./:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

static String extractResponseCode(const char* header, int len, size_t& lineLength)
{
    const char* space1 = 0;
    const char* space2 = 0;
    const char* p;
    lineLength = 0;
    for (p = header; p - header < len; p++, lineLength++) {
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

static String hostName(const KURL& url, bool secure)
{
    ASSERT(url.protocolIs("wss") == secure);
    StringBuilder builder;
    builder.append(url.host().lower());
    if (url.port() && ((!secure && url.port() != 80) || (secure && url.port() != 443))) {
        builder.append(":");
        builder.append(String::number(url.port()));
    }
    return builder.toString();
}

static String trimConsoleMessage(const char* p, size_t len)
{
    String s = String(p, std::min<size_t>(len, 128));
    if (len > 128)
        s += "...";
    return s;
}

static void generateSecWebSocketKey(uint32_t& number, String& key)
{
    uint32_t space = static_cast<uint32_t>(WTF::randomNumber() * 12) + 1;
    uint32_t max = 4294967295U / space;
    number = static_cast<uint32_t>(WTF::randomNumber() * max);
    uint32_t product = number * space;

    String s = String::number(product);
    int n = static_cast<int>(WTF::randomNumber() * 12) + 1;
    DEFINE_STATIC_LOCAL(String, randomChars, (randomCharacterInSecWebSocketKey));
    for (int i = 0; i < n; i++) {
        int pos = static_cast<int>(WTF::randomNumber() * (s.length() + 1));
        int chpos = static_cast<int>(WTF::randomNumber() * randomChars.length());
        s.insert(randomChars.substring(chpos, 1), pos);
    }
    DEFINE_STATIC_LOCAL(String, spaceChar, (" "));
    for (uint32_t i = 0; i < space; i++) {
        int pos = static_cast<int>(WTF::randomNumber() * s.length() - 1) + 1;
        s.insert(spaceChar, pos);
    }
    key = s;
}

static void generateKey3(unsigned char key3[8])
{
    for (int i = 0; i < 8; i++)
        key3[i] = WTF::randomNumber() * 256;
}

static void setChallengeNumber(unsigned char* buf, uint32_t number)
{
    unsigned char* p = buf + 3;
    for (int i = 0; i < 4; i++) {
        *p = number & 0xFF;
        --p;
        number >>= 8;
    }
}

static void generateExpectedChallengeResponse(uint32_t number1, uint32_t number2, unsigned char key3[8], unsigned char expectedChallenge[16])
{
    unsigned char challenge[16];
    setChallengeNumber(&challenge[0], number1);
    setChallengeNumber(&challenge[4], number2);
    memcpy(&challenge[8], key3, 8);
    MD5 md5;
    md5.addBytes(challenge, sizeof(challenge));
    Vector<uint8_t, 16> digest = md5.checksum();
    memcpy(expectedChallenge, digest.data(), 16);
}

WebSocketHandshake::WebSocketHandshake(const KURL& url, const String& protocol, ScriptExecutionContext* context)
    : m_url(url)
    , m_clientProtocol(protocol)
    , m_secure(m_url.protocolIs("wss"))
    , m_context(context)
    , m_mode(Incomplete)
{
    uint32_t number1;
    uint32_t number2;
    generateSecWebSocketKey(number1, m_secWebSocketKey1);
    generateSecWebSocketKey(number2, m_secWebSocketKey2);
    generateKey3(m_key3);
    generateExpectedChallengeResponse(number1, number2, m_key3, m_expectedChallengeResponse);
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
    builder.append(hostName(m_url, m_secure));
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

    Vector<String> fields;
    fields.append("Upgrade: WebSocket");
    fields.append("Connection: Upgrade");
    fields.append("Host: " + hostName(m_url, m_secure));
    fields.append("Origin: " + clientOrigin());
    if (!m_clientProtocol.isEmpty())
        fields.append("Sec-WebSocket-Protocol: " + m_clientProtocol);

    KURL url = httpURLForAuthenticationAndCookies();
    if (m_context->isDocument()) {
        Document* document = static_cast<Document*>(m_context);
        String cookie = cookieRequestHeaderFieldValue(document, url);
        if (!cookie.isEmpty())
            fields.append("Cookie: " + cookie);
        // Set "Cookie2: <cookie>" if cookies 2 exists for url?
    }

    fields.append("Sec-WebSocket-Key1: " + m_secWebSocketKey1);
    fields.append("Sec-WebSocket-Key2: " + m_secWebSocketKey2);

    // Fields in the handshake are sent by the client in a random order; the
    // order is not meaningful.  Thus, it's ok to send the order we constructed
    // the fields.

    for (size_t i = 0; i < fields.size(); i++) {
        builder.append(fields[i]);
        builder.append("\r\n");
    }

    builder.append("\r\n");

    CString handshakeHeader = builder.toString().utf8();
    char* characterBuffer = 0;
    CString msg = CString::newUninitialized(handshakeHeader.length() + sizeof(m_key3), characterBuffer);
    memcpy(characterBuffer, handshakeHeader.data(), handshakeHeader.length());
    memcpy(characterBuffer + handshakeHeader.length(), m_key3, sizeof(m_key3));
    return msg;
}

WebSocketHandshakeRequest WebSocketHandshake::clientHandshakeRequest() const
{
    // Keep the following consistent with clientHandshakeMessage().
    // FIXME: do we need to store m_secWebSocketKey1, m_secWebSocketKey2 and
    // m_key3 in WebSocketHandshakeRequest?
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
    size_t lineLength;
    const String& code = extractResponseCode(header, len, lineLength);
    if (code.isNull()) {
        // Just hasn't been received yet.
        return -1;
    }
    if (code.isEmpty()) {
        m_mode = Failed;
        m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "No response code found: " + trimConsoleMessage(header, lineLength), 0, clientOrigin());
        return len;
    }
    LOG(Network, "response code: %s", code.utf8().data());
    if (code != "101") {
        m_mode = Failed;
        m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Unexpected response code:" + code, 0, clientOrigin());
        return len;
    }
    m_mode = Normal;
    if (!strnstr(header, "\r\n\r\n", len)) {
        // Just hasn't been received fully yet.
        m_mode = Incomplete;
        return -1;
    }
    HTTPHeaderMap headers;
    const char* headerFields = strnstr(header, "\r\n", len); // skip status line
    ASSERT(headerFields);
    headerFields += 2; // skip "\r\n".
    const char* p = readHTTPHeaders(headerFields, header + len, &headers);
    if (!p) {
        LOG(Network, "readHTTPHeaders failed");
        m_mode = Failed;
        return len;
    }
    if (!processHeaders(headers) || !checkResponseHeaders()) {
        LOG(Network, "header process failed");
        m_mode = Failed;
        return p - header;
    }
    if (len < static_cast<size_t>(p - header + sizeof(m_expectedChallengeResponse))) {
        // Just hasn't been received /expected/ yet.
        m_mode = Incomplete;
        return -1;
    }
    if (memcmp(p, m_expectedChallengeResponse, sizeof(m_expectedChallengeResponse))) {
        m_mode = Failed;
        return (p - header) + sizeof(m_expectedChallengeResponse);
    }
    m_mode = Connected;
    return (p - header) + sizeof(m_expectedChallengeResponse);
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
                m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Unexpected CR in name at " + trimConsoleMessage(name.data(), name.size()), 0, clientOrigin());
                return 0;
            case '\n':
                m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Unexpected LF in name at " + trimConsoleMessage(name.data(), name.size()), 0, clientOrigin());
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
                m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Unexpected LF in value at " + trimConsoleMessage(value.data(), value.size()), 0, clientOrigin());
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
            if (it->first == "sec-websocket-origin")
                m_wsOrigin = it->second;
            else if (it->first == "sec-websocket-location")
                m_wsLocation = it->second;
            else if (it->first == "sec-websocket-protocol")
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

bool WebSocketHandshake::checkResponseHeaders()
{
    if (m_wsOrigin.isNull()) {
        m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Error during WebSocket handshake: 'sec-websocket-origin' header is missing", 0, clientOrigin());
        return false;
    }
    if (m_wsLocation.isNull()) {
        m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Error during WebSocket handshake: 'sec-websocket-location' header is missing", 0, clientOrigin());
        return false;
    }

    if (clientOrigin() != m_wsOrigin) {
        m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Error during WebSocket handshake: origin mismatch: " + clientOrigin() + " != " + m_wsOrigin, 0, clientOrigin());
        return false;
    }
    if (clientLocation() != m_wsLocation) {
        m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Error during WebSocket handshake: location mismatch: " + clientLocation() + " != " + m_wsLocation, 0, clientOrigin());
        return false;
    }
    if (!m_clientProtocol.isEmpty() && m_clientProtocol != m_wsProtocol) {
        m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Error during WebSocket handshake: protocol mismatch: " + m_clientProtocol + " != " + m_wsProtocol, 0, clientOrigin());
        return false;
    }
    return true;
}

} // namespace WebCore

#endif // ENABLE(WEB_SOCKETS)
