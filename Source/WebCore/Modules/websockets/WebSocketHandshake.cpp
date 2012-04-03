/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
#include "WebSocket.h"

#include "Base64.h"
#include "Cookie.h"
#include "CookieJar.h"
#include "Document.h"
#include "HTTPHeaderMap.h"
#include "HTTPParsers.h"
#include "KURL.h"
#include "Logging.h"
#include "ScriptCallStack.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/MD5.h>
#include <wtf/SHA1.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StringExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/CharacterNames.h>

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

static const size_t maxInputSampleSize = 128;
static String trimInputSample(const char* p, size_t len)
{
    String s = String(p, std::min<size_t>(len, maxInputSampleSize));
    if (len > maxInputSampleSize)
        s.append(horizontalEllipsis);
    return s;
}

static uint32_t randomNumberLessThan(uint32_t n)
{
    if (!n)
        return 0;
    if (n == std::numeric_limits<uint32_t>::max())
        return cryptographicallyRandomNumber();
    uint32_t max = std::numeric_limits<uint32_t>::max() - (std::numeric_limits<uint32_t>::max() % n);
    ASSERT(!(max % n));
    uint32_t v;
    do {
        v = cryptographicallyRandomNumber();
    } while (v >= max);
    return v % n;
}

static void generateHixie76SecWebSocketKey(uint32_t& number, String& key)
{
    uint32_t space = randomNumberLessThan(12) + 1;
    uint32_t max = 4294967295U / space;
    number = randomNumberLessThan(max);
    uint32_t product = number * space;

    String s = String::number(product);
    int n = randomNumberLessThan(12) + 1;
    DEFINE_STATIC_LOCAL(String, randomChars, (randomCharacterInSecWebSocketKey));
    for (int i = 0; i < n; i++) {
        int pos = randomNumberLessThan(s.length() + 1);
        int chpos = randomNumberLessThan(randomChars.length());
        s.insert(randomChars.substring(chpos, 1), pos);
    }
    DEFINE_STATIC_LOCAL(String, spaceChar, (" "));
    for (uint32_t i = 0; i < space; i++) {
        int pos = randomNumberLessThan(s.length() - 1) + 1;
        s.insert(spaceChar, pos);
    }
    ASSERT(s[0] != ' ');
    ASSERT(s[s.length() - 1] != ' ');
    key = s;
}

static void generateHixie76Key3(unsigned char key3[8])
{
    cryptographicallyRandomValues(key3, 8);
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

static void generateHixie76ExpectedChallengeResponse(uint32_t number1, uint32_t number2, unsigned char key3[8], unsigned char expectedChallenge[16])
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

static String generateSecWebSocketKey()
{
    static const size_t nonceSize = 16;
    unsigned char key[nonceSize];
    cryptographicallyRandomValues(key, nonceSize);
    return base64Encode(reinterpret_cast<char*>(key), nonceSize);
}

String WebSocketHandshake::getExpectedWebSocketAccept(const String& secWebSocketKey)
{
    static const char* const webSocketKeyGUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    static const size_t sha1HashSize = 20; // FIXME: This should be defined in SHA1.h.
    SHA1 sha1;
    CString keyData = secWebSocketKey.ascii();
    sha1.addBytes(reinterpret_cast<const uint8_t*>(keyData.data()), keyData.length());
    sha1.addBytes(reinterpret_cast<const uint8_t*>(webSocketKeyGUID), strlen(webSocketKeyGUID));
    Vector<uint8_t, sha1HashSize> hash;
    sha1.computeHash(hash);
    return base64Encode(reinterpret_cast<const char*>(hash.data()), sha1HashSize);
}

WebSocketHandshake::WebSocketHandshake(const KURL& url, const String& protocol, ScriptExecutionContext* context, bool useHixie76Protocol)
    : m_url(url)
    , m_clientProtocol(protocol)
    , m_secure(m_url.protocolIs("wss"))
    , m_context(context)
    , m_useHixie76Protocol(useHixie76Protocol)
    , m_mode(Incomplete)
{
    if (m_useHixie76Protocol) {
        uint32_t number1;
        uint32_t number2;
        generateHixie76SecWebSocketKey(number1, m_hixie76SecWebSocketKey1);
        generateHixie76SecWebSocketKey(number2, m_hixie76SecWebSocketKey2);
        generateHixie76Key3(m_hixie76Key3);
        generateHixie76ExpectedChallengeResponse(number1, number2, m_hixie76Key3, m_hixie76ExpectedChallengeResponse);
    } else {
        m_secWebSocketKey = generateSecWebSocketKey();
        m_expectedAccept = getExpectedWebSocketAccept(m_secWebSocketKey);
    }
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
    if (m_useHixie76Protocol)
        fields.append("Upgrade: WebSocket");
    else
        fields.append("Upgrade: websocket");
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

    if (m_useHixie76Protocol) {
        fields.append("Sec-WebSocket-Key1: " + m_hixie76SecWebSocketKey1);
        fields.append("Sec-WebSocket-Key2: " + m_hixie76SecWebSocketKey2);
    } else {
        fields.append("Sec-WebSocket-Key: " + m_secWebSocketKey);
        fields.append("Sec-WebSocket-Version: 13");
        const String extensionValue = m_extensionDispatcher.createHeaderValue();
        if (extensionValue.length())
            fields.append("Sec-WebSocket-Extensions: " + extensionValue);
    }

    // Fields in the handshake are sent by the client in a random order; the
    // order is not meaningful.  Thus, it's ok to send the order we constructed
    // the fields.

    for (size_t i = 0; i < fields.size(); i++) {
        builder.append(fields[i]);
        builder.append("\r\n");
    }

    builder.append("\r\n");

    CString handshakeHeader = builder.toString().utf8();
    // Hybi-10 handshake is complete at this point.
    if (!m_useHixie76Protocol)
        return handshakeHeader;
    // Hixie-76 protocol requires sending eight-byte data (so-called "key3") after the request header fields.
    char* characterBuffer = 0;
    CString msg = CString::newUninitialized(handshakeHeader.length() + sizeof(m_hixie76Key3), characterBuffer);
    memcpy(characterBuffer, handshakeHeader.data(), handshakeHeader.length());
    memcpy(characterBuffer + handshakeHeader.length(), m_hixie76Key3, sizeof(m_hixie76Key3));
    return msg;
}

PassRefPtr<WebSocketHandshakeRequest> WebSocketHandshake::clientHandshakeRequest() const
{
    // Keep the following consistent with clientHandshakeMessage().
    // FIXME: do we need to store m_secWebSocketKey1, m_secWebSocketKey2 and
    // m_key3 in WebSocketHandshakeRequest?
    RefPtr<WebSocketHandshakeRequest> request = WebSocketHandshakeRequest::create("GET", m_url);
    if (m_useHixie76Protocol)
        request->addHeaderField("Upgrade", "WebSocket");
    else
        request->addHeaderField("Upgrade", "websocket");
    request->addHeaderField("Connection", "Upgrade");
    request->addHeaderField("Host", hostName(m_url, m_secure));
    request->addHeaderField("Origin", clientOrigin());
    if (!m_clientProtocol.isEmpty())
        request->addHeaderField("Sec-WebSocket-Protocol", m_clientProtocol);

    KURL url = httpURLForAuthenticationAndCookies();
    if (m_context->isDocument()) {
        Document* document = static_cast<Document*>(m_context);
        String cookie = cookieRequestHeaderFieldValue(document, url);
        if (!cookie.isEmpty())
            request->addHeaderField("Cookie", cookie);
        // Set "Cookie2: <cookie>" if cookies 2 exists for url?
    }

    if (m_useHixie76Protocol) {
        request->addHeaderField("Sec-WebSocket-Key1", m_hixie76SecWebSocketKey1);
        request->addHeaderField("Sec-WebSocket-Key2", m_hixie76SecWebSocketKey2);
        request->setKey3(m_hixie76Key3);
    } else {
        request->addHeaderField("Sec-WebSocket-Key", m_secWebSocketKey);
        request->addHeaderField("Sec-WebSocket-Version", "13");
        const String extensionValue = m_extensionDispatcher.createHeaderValue();
        if (extensionValue.length())
            request->addHeaderField("Sec-WebSocket-Extensions", extensionValue);
    }

    return request.release();
}

void WebSocketHandshake::reset()
{
    m_mode = Incomplete;
    m_extensionDispatcher.reset();
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
        m_mode = Failed; // m_failureReason is set inside readStatusLine().
        return len;
    }
    LOG(Network, "response code: %d", statusCode);
    m_response.setStatusCode(statusCode);
    m_response.setStatusText(statusText);
    if (statusCode != 101) {
        m_mode = Failed;
        m_failureReason = "Unexpected response code: " + String::number(statusCode);
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
        m_mode = Failed; // m_failureReason is set inside readHTTPHeaders().
        return len;
    }
    if (!checkResponseHeaders()) {
        LOG(Network, "header process failed");
        m_mode = Failed;
        return p - header;
    }

    if (!m_useHixie76Protocol) { // Hybi-10 handshake is complete at this point.
        m_mode = Connected;
        return p - header;
    }

    // In hixie-76 protocol, server's handshake contains sixteen-byte data (called "challenge response")
    // after the header fields.
    if (len < static_cast<size_t>(p - header + sizeof(m_hixie76ExpectedChallengeResponse))) {
        // Just hasn't been received /expected/ yet.
        m_mode = Incomplete;
        return -1;
    }

    m_response.setChallengeResponse(static_cast<const unsigned char*>(static_cast<const void*>(p)));
    if (memcmp(p, m_hixie76ExpectedChallengeResponse, sizeof(m_hixie76ExpectedChallengeResponse))) {
        m_mode = Failed;
        return (p - header) + sizeof(m_hixie76ExpectedChallengeResponse);
    }
    m_mode = Connected;
    return (p - header) + sizeof(m_hixie76ExpectedChallengeResponse);
}

WebSocketHandshake::Mode WebSocketHandshake::mode() const
{
    return m_mode;
}

String WebSocketHandshake::failureReason() const
{
    return m_failureReason;
}

String WebSocketHandshake::serverWebSocketOrigin() const
{
    return m_response.headerFields().get("sec-websocket-origin");
}

String WebSocketHandshake::serverWebSocketLocation() const
{
    return m_response.headerFields().get("sec-websocket-location");
}

String WebSocketHandshake::serverWebSocketProtocol() const
{
    return m_response.headerFields().get("sec-websocket-protocol");
}

String WebSocketHandshake::serverSetCookie() const
{
    return m_response.headerFields().get("set-cookie");
}

String WebSocketHandshake::serverSetCookie2() const
{
    return m_response.headerFields().get("set-cookie2");
}

String WebSocketHandshake::serverUpgrade() const
{
    return m_response.headerFields().get("upgrade");
}

String WebSocketHandshake::serverConnection() const
{
    return m_response.headerFields().get("connection");
}

String WebSocketHandshake::serverWebSocketAccept() const
{
    return m_response.headerFields().get("sec-websocket-accept");
}

String WebSocketHandshake::acceptedExtensions() const
{
    return m_extensionDispatcher.acceptedExtensions();
}

const WebSocketHandshakeResponse& WebSocketHandshake::serverHandshakeResponse() const
{
    return m_response;
}

void WebSocketHandshake::addExtensionProcessor(PassOwnPtr<WebSocketExtensionProcessor> processor)
{
    m_extensionDispatcher.addProcessor(processor);
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
    // Arbitrary size limit to prevent the server from sending an unbounded
    // amount of data with no newlines and forcing us to buffer it all.
    static const int maximumLength = 1024;

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
        } else if (*p == '\0') {
            // The caller isn't prepared to deal with null bytes in status
            // line. WebSockets specification doesn't prohibit this, but HTTP
            // does, so we'll just treat this as an error.
            m_failureReason = "Status line contains embedded null";
            return p + 1 - header;
        } else if (*p == '\n')
            break;
    }
    if (consumedLength == headerLength)
        return -1; // We have not received '\n' yet.

    const char* end = p + 1;
    int lineLength = end - header;
    if (lineLength > maximumLength) {
        m_failureReason = "Status line is too long";
        return maximumLength;
    }

    // The line must end with "\r\n".
    if (lineLength < 2 || *(end - 2) != '\r') {
        m_failureReason = "Status line does not end with CRLF";
        return lineLength;
    }

    if (!space1 || !space2) {
        m_failureReason = "No response code found: " + trimInputSample(header, lineLength - 2);
        return lineLength;
    }

    String statusCodeString(space1 + 1, space2 - space1 - 1);
    if (statusCodeString.length() != 3) // Status code must consist of three digits.
        return lineLength;
    for (int i = 0; i < 3; ++i)
        if (statusCodeString[i] < '0' || statusCodeString[i] > '9') {
            m_failureReason = "Invalid status code: " + statusCodeString;
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

    AtomicString name;
    String value;
    bool sawSecWebSocketAcceptHeaderField = false;
    bool sawSecWebSocketProtocolHeaderField = false;
    const char* p = start;
    for (; p < end; p++) {
        size_t consumedLength = parseHTTPHeader(p, end - p, m_failureReason, name, value);
        if (!consumedLength)
            return 0;
        p += consumedLength;

        // Stop once we consumed an empty line.
        if (name.isEmpty())
            break;

        // Sec-WebSocket-Extensions may be split. We parse and check the
        // header value every time the header appears.
        if (equalIgnoringCase("sec-websocket-extensions", name)) {
            if (!m_extensionDispatcher.processHeaderValue(value)) {
                m_failureReason = m_extensionDispatcher.failureReason();
                return 0;
            }
        } else if (equalIgnoringCase("Sec-WebSocket-Accept", name)) {
            if (sawSecWebSocketAcceptHeaderField) {
                m_failureReason = "The Sec-WebSocket-Accept header MUST NOT appear more than once in an HTTP response";
                return 0;
            }
            m_response.addHeaderField(name, value);
            sawSecWebSocketAcceptHeaderField = true;
        } else if (equalIgnoringCase("Sec-WebSocket-Protocol", name)) {
            if (sawSecWebSocketProtocolHeaderField) {
                m_failureReason = "The Sec-WebSocket-Protocol header MUST NOT appear more than once in an HTTP response";
                return 0;
            }
            m_response.addHeaderField(name, value);
            sawSecWebSocketProtocolHeaderField = true;
        } else
            m_response.addHeaderField(name, value);
    }
    return p;
}

bool WebSocketHandshake::checkResponseHeaders()
{
    const String& serverWebSocketLocation = this->serverWebSocketLocation();
    const String& serverWebSocketOrigin = this->serverWebSocketOrigin();
    const String& serverWebSocketProtocol = this->serverWebSocketProtocol();
    const String& serverUpgrade = this->serverUpgrade();
    const String& serverConnection = this->serverConnection();
    const String& serverWebSocketAccept = this->serverWebSocketAccept();

    if (serverUpgrade.isNull()) {
        m_failureReason = "Error during WebSocket handshake: 'Upgrade' header is missing";
        return false;
    }
    if (serverConnection.isNull()) {
        m_failureReason = "Error during WebSocket handshake: 'Connection' header is missing";
        return false;
    }
    if (m_useHixie76Protocol) {
        if (serverWebSocketOrigin.isNull()) {
            m_failureReason = "Error during WebSocket handshake: 'Sec-WebSocket-Origin' header is missing";
            return false;
        }
        if (serverWebSocketLocation.isNull()) {
            m_failureReason = "Error during WebSocket handshake: 'Sec-WebSocket-Location' header is missing";
            return false;
        }
    } else {
        if (serverWebSocketAccept.isNull()) {
            m_failureReason = "Error during WebSocket handshake: 'Sec-WebSocket-Accept' header is missing";
            return false;
        }
    }

    if (!equalIgnoringCase(serverUpgrade, "websocket")) {
        m_failureReason = "Error during WebSocket handshake: 'Upgrade' header value is not 'WebSocket'";
        return false;
    }
    if (!equalIgnoringCase(serverConnection, "upgrade")) {
        m_failureReason = "Error during WebSocket handshake: 'Connection' header value is not 'Upgrade'";
        return false;
    }

    if (m_useHixie76Protocol) {
        if (clientOrigin() != serverWebSocketOrigin) {
            m_failureReason = "Error during WebSocket handshake: origin mismatch: " + clientOrigin() + " != " + serverWebSocketOrigin;
            return false;
        }
        if (clientLocation() != serverWebSocketLocation) {
            m_failureReason = "Error during WebSocket handshake: location mismatch: " + clientLocation() + " != " + serverWebSocketLocation;
            return false;
        }
        if (!m_clientProtocol.isEmpty() && m_clientProtocol != serverWebSocketProtocol) {
            m_failureReason = "Error during WebSocket handshake: protocol mismatch: " + m_clientProtocol + " != " + serverWebSocketProtocol;
            return false;
        }
    } else {
        if (serverWebSocketAccept != m_expectedAccept) {
            m_failureReason = "Error during WebSocket handshake: Sec-WebSocket-Accept mismatch";
            return false;
        }
        if (!serverWebSocketProtocol.isNull()) {
            if (m_clientProtocol.isEmpty()) {
                m_failureReason = "Error during WebSocket handshake: Sec-WebSocket-Protocol mismatch";
                return false;
            }
            Vector<String> result;
            m_clientProtocol.split(String(WebSocket::subProtocolSeperator()), result);
            if (!result.contains(serverWebSocketProtocol)) {
                m_failureReason = "Error during WebSocket handshake: Sec-WebSocket-Protocol mismatch";
                return false;
            }
        }
    }
    return true;
}

} // namespace WebCore

#endif // ENABLE(WEB_SOCKETS)
