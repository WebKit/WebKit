/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "WebSocketHandshake.h"

#include "Cookie.h"
#include "CookieJar.h"
#include "HTTPHeaderMap.h"
#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "ResourceRequest.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include <wtf/URL.h>
#include "WebSocket.h"
#include <wtf/ASCIICType.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/MD5.h>
#include <wtf/SHA1.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StringExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/Base64.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

static String resourceName(const URL& url)
{
    StringBuilder name;
    name.append(url.path());
    if (name.isEmpty())
        name.append('/');
    if (!url.query().isNull()) {
        name.append('?');
        name.append(url.query());
    }
    String result = name.toString();
    ASSERT(!result.isEmpty());
    ASSERT(!result.contains(' '));
    return result;
}

static String hostName(const URL& url, bool secure)
{
    ASSERT(url.protocolIs("wss") == secure);
    StringBuilder builder;
    builder.append(url.host().convertToASCIILowercase());
    if (url.port() && ((!secure && url.port().value() != 80) || (secure && url.port().value() != 443))) {
        builder.append(':');
        builder.appendNumber(url.port().value());
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

static String generateSecWebSocketKey()
{
    static const size_t nonceSize = 16;
    unsigned char key[nonceSize];
    cryptographicallyRandomValues(key, nonceSize);
    return base64Encode(key, nonceSize);
}

String WebSocketHandshake::getExpectedWebSocketAccept(const String& secWebSocketKey)
{
    static const char* const webSocketKeyGUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    SHA1 sha1;
    CString keyData = secWebSocketKey.ascii();
    sha1.addBytes(reinterpret_cast<const uint8_t*>(keyData.data()), keyData.length());
    sha1.addBytes(reinterpret_cast<const uint8_t*>(webSocketKeyGUID), strlen(webSocketKeyGUID));
    SHA1::Digest hash;
    sha1.computeHash(hash);
    return base64Encode(hash.data(), SHA1::hashSize);
}

WebSocketHandshake::WebSocketHandshake(const URL& url, const String& protocol, const String& userAgent, const String& clientOrigin, bool allowCookies)
    : m_url(url)
    , m_clientProtocol(protocol)
    , m_secure(m_url.protocolIs("wss"))
    , m_mode(Incomplete)
    , m_userAgent(userAgent)
    , m_clientOrigin(clientOrigin)
    , m_allowCookies(allowCookies)
{
    m_secWebSocketKey = generateSecWebSocketKey();
    m_expectedAccept = getExpectedWebSocketAccept(m_secWebSocketKey);
}

WebSocketHandshake::~WebSocketHandshake() = default;

const URL& WebSocketHandshake::url() const
{
    return m_url;
}

void WebSocketHandshake::setURL(const URL& url)
{
    m_url = url.isolatedCopy();
}

// FIXME: Return type should just be String, not const String.
const String WebSocketHandshake::host() const
{
    return m_url.host().convertToASCIILowercase();
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

String WebSocketHandshake::clientLocation() const
{
    return makeString(m_secure ? "wss" : "ws", "://", hostName(m_url, m_secure), resourceName(m_url));
}

CString WebSocketHandshake::clientHandshakeMessage() const
{
    // Keep the following consistent with clientHandshakeRequest().
    StringBuilder builder;

    builder.appendLiteral("GET ");
    builder.append(resourceName(m_url));
    builder.appendLiteral(" HTTP/1.1\r\n");

    Vector<String> fields;
    fields.append("Upgrade: websocket");
    fields.append("Connection: Upgrade");
    fields.append("Host: " + hostName(m_url, m_secure));
    fields.append("Origin: " + m_clientOrigin);
    if (!m_clientProtocol.isEmpty())
        fields.append("Sec-WebSocket-Protocol: " + m_clientProtocol);

    // Note: Cookies are not retrieved in the WebContent process. Instead, a proxy object is
    // added in the handshake, and is exchanged for actual cookies in the Network process.

    // Add no-cache headers to avoid compatibility issue.
    // There are some proxies that rewrite "Connection: upgrade"
    // to "Connection: close" in the response if a request doesn't contain
    // these headers.
    fields.append("Pragma: no-cache");
    fields.append("Cache-Control: no-cache");

    fields.append("Sec-WebSocket-Key: " + m_secWebSocketKey);
    fields.append("Sec-WebSocket-Version: 13");
    const String extensionValue = m_extensionDispatcher.createHeaderValue();
    if (extensionValue.length())
        fields.append("Sec-WebSocket-Extensions: " + extensionValue);

    // Add a User-Agent header.
    fields.append(makeString("User-Agent: ", m_userAgent));

    // Fields in the handshake are sent by the client in a random order; the
    // order is not meaningful.  Thus, it's ok to send the order we constructed
    // the fields.

    for (auto& field : fields) {
        builder.append(field);
        builder.appendLiteral("\r\n");
    }

    builder.appendLiteral("\r\n");

    return builder.toString().utf8();
}

ResourceRequest WebSocketHandshake::clientHandshakeRequest(Function<String(const URL&)>&& cookieRequestHeaderFieldValue) const
{
    // Keep the following consistent with clientHandshakeMessage().
    ResourceRequest request(m_url);
    request.setHTTPMethod("GET");

    request.setHTTPHeaderField(HTTPHeaderName::Connection, "Upgrade");
    request.setHTTPHeaderField(HTTPHeaderName::Host, hostName(m_url, m_secure));
    request.setHTTPHeaderField(HTTPHeaderName::Origin, m_clientOrigin);
    if (!m_clientProtocol.isEmpty())
        request.setHTTPHeaderField(HTTPHeaderName::SecWebSocketProtocol, m_clientProtocol);

    URL url = httpURLForAuthenticationAndCookies();
    if (m_allowCookies) {
        String cookie = cookieRequestHeaderFieldValue(url);
        if (!cookie.isEmpty())
            request.setHTTPHeaderField(HTTPHeaderName::Cookie, cookie);
    }

    request.setHTTPHeaderField(HTTPHeaderName::Pragma, "no-cache");
    request.setHTTPHeaderField(HTTPHeaderName::CacheControl, "no-cache");

    request.setHTTPHeaderField(HTTPHeaderName::SecWebSocketKey, m_secWebSocketKey);
    request.setHTTPHeaderField(HTTPHeaderName::SecWebSocketVersion, "13");
    const String extensionValue = m_extensionDispatcher.createHeaderValue();
    if (extensionValue.length())
        request.setHTTPHeaderField(HTTPHeaderName::SecWebSocketExtensions, extensionValue);

    // Add a User-Agent header.
    request.setHTTPUserAgent(m_userAgent);

    return request;
}

void WebSocketHandshake::reset()
{
    m_mode = Incomplete;
    m_extensionDispatcher.reset();
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
    LOG(Network, "WebSocketHandshake %p readServerHandshake() Status code is %d", this, statusCode);

    m_serverHandshakeResponse = ResourceResponse();
    m_serverHandshakeResponse.setHTTPStatusCode(statusCode);
    m_serverHandshakeResponse.setHTTPStatusText(statusText);

    if (statusCode != 101) {
        m_mode = Failed;
        m_failureReason = makeString("Unexpected response code: ", statusCode);
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
        LOG(Network, "WebSocketHandshake %p readServerHandshake() readHTTPHeaders() failed", this);
        m_mode = Failed; // m_failureReason is set inside readHTTPHeaders().
        return len;
    }
    if (!checkResponseHeaders()) {
        LOG(Network, "WebSocketHandshake %p readServerHandshake() checkResponseHeaders() failed", this);
        m_mode = Failed;
        return p - header;
    }

    m_mode = Connected;
    return p - header;
}

WebSocketHandshake::Mode WebSocketHandshake::mode() const
{
    return m_mode;
}

String WebSocketHandshake::failureReason() const
{
    return m_failureReason;
}

String WebSocketHandshake::serverWebSocketProtocol() const
{
    return m_serverHandshakeResponse.httpHeaderFields().get(HTTPHeaderName::SecWebSocketProtocol);
}

String WebSocketHandshake::serverSetCookie() const
{
    return m_serverHandshakeResponse.httpHeaderFields().get(HTTPHeaderName::SetCookie);
}

String WebSocketHandshake::serverUpgrade() const
{
    return m_serverHandshakeResponse.httpHeaderFields().get(HTTPHeaderName::Upgrade);
}

String WebSocketHandshake::serverConnection() const
{
    return m_serverHandshakeResponse.httpHeaderFields().get(HTTPHeaderName::Connection);
}

String WebSocketHandshake::serverWebSocketAccept() const
{
    return m_serverHandshakeResponse.httpHeaderFields().get(HTTPHeaderName::SecWebSocketAccept);
}

String WebSocketHandshake::acceptedExtensions() const
{
    return m_extensionDispatcher.acceptedExtensions();
}

const ResourceResponse& WebSocketHandshake::serverHandshakeResponse() const
{
    return m_serverHandshakeResponse;
}

void WebSocketHandshake::addExtensionProcessor(std::unique_ptr<WebSocketExtensionProcessor> processor)
{
    m_extensionDispatcher.addProcessor(WTFMove(processor));
}

URL WebSocketHandshake::httpURLForAuthenticationAndCookies() const
{
    URL url = m_url.isolatedCopy();
    bool couldSetProtocol = url.setProtocol(m_secure ? "https" : "http");
    ASSERT_UNUSED(couldSetProtocol, couldSetProtocol);
    return url;
}

// https://tools.ietf.org/html/rfc6455#section-4.1
// "The HTTP version MUST be at least 1.1."
static inline bool headerHasValidHTTPVersion(StringView httpStatusLine)
{
    const char* httpVersionStaticPreambleLiteral = "HTTP/";
    StringView httpVersionStaticPreamble(reinterpret_cast<const LChar*>(httpVersionStaticPreambleLiteral), strlen(httpVersionStaticPreambleLiteral));
    if (!httpStatusLine.startsWith(httpVersionStaticPreamble))
        return false;

    // Check that there is a version number which should be at least three characters after "HTTP/"
    unsigned preambleLength = httpVersionStaticPreamble.length();
    if (httpStatusLine.length() < preambleLength + 3)
        return false;

    auto dotPosition = httpStatusLine.find('.', preambleLength);
    if (dotPosition == notFound)
        return false;

    StringView majorVersionView = httpStatusLine.substring(preambleLength, dotPosition - preambleLength);
    bool isValid;
    int majorVersion = majorVersionView.toIntStrict(isValid);
    if (!isValid)
        return false;

    unsigned minorVersionLength;
    unsigned charactersLeftAfterDotPosition = httpStatusLine.length() - dotPosition;
    for (minorVersionLength = 1; minorVersionLength < charactersLeftAfterDotPosition; minorVersionLength++) {
        if (!isASCIIDigit(httpStatusLine[dotPosition + minorVersionLength]))
            break;
    }
    int minorVersion = (httpStatusLine.substring(dotPosition + 1, minorVersionLength)).toIntStrict(isValid);
    if (!isValid)
        return false;

    return (majorVersion >= 1 && minorVersion >= 1) || majorVersion >= 2;
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

    const char* space1 = nullptr;
    const char* space2 = nullptr;
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
            m_failureReason = "Status line contains embedded null"_s;
            return p + 1 - header;
        } else if (!isASCII(*p)) {
            m_failureReason = "Status line contains non-ASCII character"_s;
            return p + 1 - header;
        } else if (*p == '\n')
            break;
    }
    if (consumedLength == headerLength)
        return -1; // We have not received '\n' yet.

    const char* end = p + 1;
    int lineLength = end - header;
    if (lineLength > maximumLength) {
        m_failureReason = "Status line is too long"_s;
        return maximumLength;
    }

    // The line must end with "\r\n".
    if (lineLength < 2 || *(end - 2) != '\r') {
        m_failureReason = "Status line does not end with CRLF"_s;
        return lineLength;
    }

    if (!space1 || !space2) {
        m_failureReason = makeString("No response code found: ", trimInputSample(header, lineLength - 2));
        return lineLength;
    }

    StringView httpStatusLine(reinterpret_cast<const LChar*>(header), space1 - header);
    if (!headerHasValidHTTPVersion(httpStatusLine)) {
        m_failureReason = makeString("Invalid HTTP version string: ", httpStatusLine);
        return lineLength;
    }

    StringView statusCodeString(reinterpret_cast<const LChar*>(space1 + 1), space2 - space1 - 1);
    if (statusCodeString.length() != 3) // Status code must consist of three digits.
        return lineLength;
    for (int i = 0; i < 3; ++i)
        if (!isASCIIDigit(statusCodeString[i])) {
            m_failureReason = makeString("Invalid status code: ", statusCodeString);
            return lineLength;
        }

    bool ok = false;
    statusCode = statusCodeString.toIntStrict(ok);
    ASSERT(ok);

    statusText = String(space2 + 1, end - space2 - 3); // Exclude "\r\n".
    return lineLength;
}

const char* WebSocketHandshake::readHTTPHeaders(const char* start, const char* end)
{
    StringView name;
    String value;
    bool sawSecWebSocketExtensionsHeaderField = false;
    bool sawSecWebSocketAcceptHeaderField = false;
    bool sawSecWebSocketProtocolHeaderField = false;
    const char* p = start;
    for (; p < end; p++) {
        size_t consumedLength = parseHTTPHeader(p, end - p, m_failureReason, name, value);
        if (!consumedLength)
            return nullptr;
        p += consumedLength;

        // Stop once we consumed an empty line.
        if (name.isEmpty())
            break;

        HTTPHeaderName headerName;
        if (!findHTTPHeaderName(name, headerName)) {
            // Evidence in the wild shows that services make use of custom headers in the handshake
            m_serverHandshakeResponse.addHTTPHeaderField(name.toString(), value);
            continue;
        }

        // https://tools.ietf.org/html/rfc7230#section-3.2.4
        // "Newly defined header fields SHOULD limit their field values to US-ASCII octets."
        if ((headerName == HTTPHeaderName::SecWebSocketExtensions
            || headerName == HTTPHeaderName::SecWebSocketAccept
            || headerName == HTTPHeaderName::SecWebSocketProtocol)
            && !value.isAllASCII()) {
            m_failureReason = makeString(name, " header value should only contain ASCII characters");
            return nullptr;
        }
        
        if (headerName == HTTPHeaderName::SecWebSocketExtensions) {
            if (sawSecWebSocketExtensionsHeaderField) {
                m_failureReason = "The Sec-WebSocket-Extensions header must not appear more than once in an HTTP response"_s;
                return nullptr;
            }
            if (!m_extensionDispatcher.processHeaderValue(value)) {
                m_failureReason = m_extensionDispatcher.failureReason();
                return nullptr;
            }
            sawSecWebSocketExtensionsHeaderField = true;
        } else {
            if (headerName == HTTPHeaderName::SecWebSocketAccept) {
                if (sawSecWebSocketAcceptHeaderField) {
                    m_failureReason = "The Sec-WebSocket-Accept header must not appear more than once in an HTTP response"_s;
                    return nullptr;
                }
                sawSecWebSocketAcceptHeaderField = true;
            } else if (headerName == HTTPHeaderName::SecWebSocketProtocol) {
                if (sawSecWebSocketProtocolHeaderField) {
                    m_failureReason = "The Sec-WebSocket-Protocol header must not appear more than once in an HTTP response"_s;
                    return nullptr;
                }
                sawSecWebSocketProtocolHeaderField = true;
            }

            m_serverHandshakeResponse.addHTTPHeaderField(headerName, value);
        }
    }
    return p;
}

bool WebSocketHandshake::checkResponseHeaders()
{
    const String& serverWebSocketProtocol = this->serverWebSocketProtocol();
    const String& serverUpgrade = this->serverUpgrade();
    const String& serverConnection = this->serverConnection();
    const String& serverWebSocketAccept = this->serverWebSocketAccept();

    if (serverUpgrade.isNull()) {
        m_failureReason = "Error during WebSocket handshake: 'Upgrade' header is missing"_s;
        return false;
    }
    if (serverConnection.isNull()) {
        m_failureReason = "Error during WebSocket handshake: 'Connection' header is missing"_s;
        return false;
    }
    if (serverWebSocketAccept.isNull()) {
        m_failureReason = "Error during WebSocket handshake: 'Sec-WebSocket-Accept' header is missing"_s;
        return false;
    }

    if (!equalLettersIgnoringASCIICase(serverUpgrade, "websocket")) {
        m_failureReason = "Error during WebSocket handshake: 'Upgrade' header value is not 'WebSocket'"_s;
        return false;
    }
    if (!equalLettersIgnoringASCIICase(serverConnection, "upgrade")) {
        m_failureReason = "Error during WebSocket handshake: 'Connection' header value is not 'Upgrade'"_s;
        return false;
    }

    if (serverWebSocketAccept != m_expectedAccept) {
        m_failureReason = "Error during WebSocket handshake: Sec-WebSocket-Accept mismatch"_s;
        return false;
    }
    if (!serverWebSocketProtocol.isNull()) {
        if (m_clientProtocol.isEmpty()) {
            m_failureReason = "Error during WebSocket handshake: Sec-WebSocket-Protocol mismatch"_s;
            return false;
        }
        Vector<String> result = m_clientProtocol.split(WebSocket::subprotocolSeparator());
        if (!result.contains(serverWebSocketProtocol)) {
            m_failureReason = "Error during WebSocket handshake: Sec-WebSocket-Protocol mismatch"_s;
            return false;
        }
    }
    return true;
}

} // namespace WebCore
