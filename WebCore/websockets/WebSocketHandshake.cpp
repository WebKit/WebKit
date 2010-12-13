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

#include "CharacterNames.h"
#include "Cookie.h"
#include "CookieJar.h"
#include "Document.h"
#include "HTTPHeaderMap.h"
#include "KURL.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"

#include <wtf/MD5.h>
#include <wtf/RandomNumber.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StringExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenate.h>

namespace WebCore {

static const char randomCharacterInSecWebSocketKey[] = "!\"#$%&'()*+,-./:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

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
        builder.append(':');
        builder.append(String::number(url.port()));
    }
    return builder.toString();
}

static const size_t maxConsoleMessageSize = 128;
static String trimConsoleMessage(const char* p, size_t len)
{
    String s = String(p, std::min<size_t>(len, maxConsoleMessageSize));
    if (len > maxConsoleMessageSize)
        s.append(horizontalEllipsis);
    return s;
}

static void generateSecWebSocketKey(uint32_t& number, String& key)
{
    uint32_t space = static_cast<uint32_t>(randomNumber() * 12) + 1;
    uint32_t max = 4294967295U / space;
    number = static_cast<uint32_t>(randomNumber() * max);
    uint32_t product = number * space;

    String s = String::number(product);
    int n = static_cast<int>(randomNumber() * 12) + 1;
    DEFINE_STATIC_LOCAL(String, randomChars, (randomCharacterInSecWebSocketKey));
    for (int i = 0; i < n; i++) {
        int pos = static_cast<int>(randomNumber() * (s.length() + 1));
        int chpos = static_cast<int>(randomNumber() * randomChars.length());
        s.insert(randomChars.substring(chpos, 1), pos);
    }
    DEFINE_STATIC_LOCAL(String, spaceChar, (" "));
    for (uint32_t i = 0; i < space; i++) {
        int pos = static_cast<int>(randomNumber() * (s.length() - 1)) + 1;
        s.insert(spaceChar, pos);
    }
    ASSERT(s[0] != ' ');
    ASSERT(s[s.length() - 1] != ' ');
    key = s;
}

static void generateKey3(unsigned char key3[8])
{
    for (int i = 0; i < 8; i++)
        key3[i] = randomNumber() * 256;
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
    Vector<uint8_t, 16> digest;
    md5.checksum(digest);
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
    WebSocketHandshakeRequest request("GET", m_url);
    request.addHeaderField("Upgrade", "WebSocket");
    request.addHeaderField("Connection", "Upgrade");
    request.addHeaderField("Host", hostName(m_url, m_secure));
    request.addHeaderField("Origin", clientOrigin());
    if (!m_clientProtocol.isEmpty())
        request.addHeaderField("Sec-WebSocket-Protocol:", m_clientProtocol);

    KURL url = httpURLForAuthenticationAndCookies();
    if (m_context->isDocument()) {
        Document* document = static_cast<Document*>(m_context);
        String cookie = cookieRequestHeaderFieldValue(document, url);
        if (!cookie.isEmpty())
            request.addHeaderField("Cookie", cookie);
        // Set "Cookie2: <cookie>" if cookies 2 exists for url?
    }

    request.addHeaderField("Sec-WebSocket-Key1", m_secWebSocketKey1);
    request.addHeaderField("Sec-WebSocket-Key2", m_secWebSocketKey2);
    request.setKey3(m_key3);

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
    int statusCode;
    String statusText;
    int lineLength = readStatusLine(header, len, statusCode, statusText);
    if (lineLength == -1)
        return -1;
    if (statusCode == -1) {
        m_mode = Failed;
        return len;
    }
    LOG(Network, "response code: %d", statusCode);
    m_response.setStatusCode(statusCode);
    m_response.setStatusText(statusText);
    if (statusCode != 101) {
        m_mode = Failed;
        m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, makeString("Unexpected response code: ", String::number(statusCode)), 0, clientOrigin());
        return len;
    }
    m_mode = Normal;
    if (!strnstr(header, "\r\n\r\n", len)) {
        // Just hasn't been received fully yet.
        m_mode = Incomplete;
        return -1;
    }
    const char* p = readHTTPHeaders(header + lineLength, header + len);
    if (!p) {
        LOG(Network, "readHTTPHeaders failed");
        m_mode = Failed;
        return len;
    }
    processHeaders();
    if (!checkResponseHeaders()) {
        LOG(Network, "header process failed");
        m_mode = Failed;
        return p - header;
    }
    if (len < static_cast<size_t>(p - header + sizeof(m_expectedChallengeResponse))) {
        // Just hasn't been received /expected/ yet.
        m_mode = Incomplete;
        return -1;
    }
    m_response.setChallengeResponse(static_cast<const unsigned char*>(static_cast<const void*>(p)));
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

const WebSocketHandshakeResponse& WebSocketHandshake::serverHandshakeResponse() const
{
    return m_response;
}

KURL WebSocketHandshake::httpURLForAuthenticationAndCookies() const
{
    KURL url = m_url.copy();
    bool couldSetProtocol = url.setProtocol(m_secure ? "https" : "http");
    ASSERT_UNUSED(couldSetProtocol, couldSetProtocol);
    return url;
}

// Returns the header length (including "\r\n"), or -1 if we have not received enough data yet.
// If the line is malformed or the status code is not a 3-digit number,
// statusCode and statusText will be set to -1 and a null string, respectively.
int WebSocketHandshake::readStatusLine(const char* header, size_t headerLength, int& statusCode, String& statusText)
{
    statusCode = -1;
    statusText = String();

    const char* space1 = 0;
    const char* space2 = 0;
    const char* p;
    size_t consumedLength;

    for (p = header, consumedLength = 0; consumedLength < headerLength; p++, consumedLength++) {
        if (*p == ' ') {
            if (!space1)
                space1 = p;
            else if (!space2)
                space2 = p;
        } else if (*p == '\n')
            break;
    }
    if (consumedLength == headerLength)
        return -1; // We have not received '\n' yet.

    const char* end = p + 1;
    if (end - header > INT_MAX) {
        m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Status line is too long: " + trimConsoleMessage(header, maxConsoleMessageSize + 1), 0, clientOrigin());
        return INT_MAX;
    }
    int lineLength = end - header;

    if (!space1 || !space2) {
        m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "No response code found: " + trimConsoleMessage(header, lineLength - 1), 0, clientOrigin());
        return lineLength;
    }

    // The line must end with "\r\n".
    if (*(end - 2) != '\r') {
        m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Status line does not end with CRLF", 0, clientOrigin());
        return lineLength;
    }

    String statusCodeString(space1 + 1, space2 - space1 - 1);
    if (statusCodeString.length() != 3) // Status code must consist of three digits.
        return lineLength;
    for (int i = 0; i < 3; ++i)
        if (statusCodeString[i] < '0' || statusCodeString[i] > '9') {
            m_context->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Invalid status code: " + statusCodeString, 0, clientOrigin());
            return lineLength;
        }

    bool ok = false;
    statusCode = statusCodeString.toInt(&ok);
    ASSERT(ok);

    statusText = String(space2 + 1, end - space2 - 3); // Exclude "\r\n".
    return lineLength;
}

const char* WebSocketHandshake::readHTTPHeaders(const char* start, const char* end)
{
    m_response.clearHeaderFields();

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
        m_response.addHeaderField(nameStr, valueStr);
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void WebSocketHandshake::processHeaders()
{
    ASSERT(m_mode == Normal);
    const HTTPHeaderMap& headers = m_response.headerFields();
    m_wsOrigin = headers.get("sec-websocket-origin");
    m_wsLocation = headers.get("sec-websocket-location");
    m_wsProtocol = headers.get("sec-websocket-protocol");
    m_setCookie = headers.get("set-cookie");
    m_setCookie2 = headers.get("set-cookie2");
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
