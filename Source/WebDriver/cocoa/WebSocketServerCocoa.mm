/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebSocketServer.h"

#if PLATFORM(COCOA) && ENABLE(WEBDRIVER_BIDI)

#include "Logging.h"
#include <CommonCrypto/CommonDigest.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wtf/MainThread.h>
#include <wtf/Vector.h>
#include <wtf/text/Base64.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebDriver {

// RFC 6455 Section 4.2.2: The WebSocket magic GUID for computing Sec-WebSocket-Accept.
static constexpr auto webSocketMagicGUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static String computeAcceptKey(const String& clientKey)
{
    auto combined = makeString(clientKey, String::fromUTF8(webSocketMagicGUID));
    auto utf8 = combined.utf8();
    unsigned char hash[CC_SHA1_DIGEST_LENGTH];
    CC_SHA1(utf8.data(), static_cast<CC_LONG>(utf8.length()), hash);
    return base64EncodeToString(std::span<const uint8_t>(hash, CC_SHA1_DIGEST_LENGTH));
}

// Build a WebSocket text frame (server->client, unmasked).
static Vector<uint8_t> buildTextFrame(const CString& payload)
{
    Vector<uint8_t> frame;
    size_t len = payload.length();

    frame.append(0x81); // FIN + Text opcode

    if (len < 126)
        frame.append(static_cast<uint8_t>(len));
    else if (len < 65536) {
        frame.append(126);
        frame.append(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.append(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.append(127);
        for (int i = 7; i >= 0; --i)
            frame.append(static_cast<uint8_t>((len >> (8 * i)) & 0xFF));
    }

    frame.append(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(payload.data()), len));
    return frame;
}

// Build a WebSocket close frame (server->client, unmasked).
static Vector<uint8_t> buildCloseFrame(uint16_t statusCode = 1000)
{
    Vector<uint8_t> frame;
    frame.append(0x88); // FIN + Close opcode
    frame.append(2); // Payload length = 2 (status code)
    frame.append(static_cast<uint8_t>((statusCode >> 8) & 0xFF));
    frame.append(static_cast<uint8_t>(statusCode & 0xFF));
    return frame;
}

// Build a WebSocket pong frame echoing the ping payload.
static Vector<uint8_t> buildPongFrame(const Vector<uint8_t>& pingPayload)
{
    Vector<uint8_t> frame;
    size_t len = pingPayload.size();
    frame.append(0x8A); // FIN + Pong opcode

    if (len < 126)
        frame.append(static_cast<uint8_t>(len));
    else if (len < 65536) {
        frame.append(126);
        frame.append(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.append(static_cast<uint8_t>(len & 0xFF));
    }
    // Pong payloads > 65535 bytes are unrealistic.

    frame.append(pingPayload.span());
    return frame;
}

struct WebSocketFrame {
    bool fin;
    uint8_t opcode; // 0x1=text, 0x8=close, 0x9=ping, 0xA=pong
    Vector<uint8_t> payload;
    size_t bytesConsumed;
};

// Try to parse one WebSocket frame from the buffer. Returns nullopt if incomplete.
static std::optional<WebSocketFrame> parseFrame(const Vector<uint8_t>& buffer)
{
    if (buffer.size() < 2)
        return std::nullopt;

    bool fin = buffer[0] & 0x80;
    uint8_t opcode = buffer[0] & 0x0F;
    bool masked = buffer[1] & 0x80;
    uint64_t payloadLen = buffer[1] & 0x7F;
    size_t headerLen = 2;

    if (payloadLen == 126) {
        if (buffer.size() < 4)
            return std::nullopt;
        payloadLen = (static_cast<uint64_t>(buffer[2]) << 8) | buffer[3];
        headerLen = 4;
    } else if (payloadLen == 127) {
        if (buffer.size() < 10)
            return std::nullopt;
        payloadLen = 0;
        for (int i = 0; i < 8; ++i)
            payloadLen = (payloadLen << 8) | buffer[2 + i];
        headerLen = 10;
    }

    uint8_t mask[4] = { };
    if (masked) {
        if (buffer.size() < headerLen + 4)
            return std::nullopt;
        memcpySpan(std::span(mask), buffer.span().subspan(headerLen, 4));
        headerLen += 4;
    }

    if (buffer.size() < headerLen + payloadLen)
        return std::nullopt;

    Vector<uint8_t> payload(payloadLen);
    for (uint64_t i = 0; i < payloadLen; ++i) {
        if (masked)
            payload[i] = buffer[headerLen + i] ^ mask[i % 4];
        else
            payload[i] = buffer[headerLen + i];
    }

    return WebSocketFrame { fin, opcode, WTF::move(payload), headerLen + payloadLen };
}

// Parse the HTTP Upgrade request for the WebSocket handshake.
// Extracts the path and Sec-WebSocket-Key. Returns true if valid.
static bool parseUpgradeRequest(const Vector<uint8_t>& buffer, String& path, String& websocketKey, size_t& headerEndOffset)
{
    const char* data = reinterpret_cast<const char*>(buffer.span().data());
    size_t size = buffer.size();

    const char* headerEnd = strnstr(data, "\r\n\r\n", size);
    if (!headerEnd)
        return false;

    headerEndOffset = (headerEnd - data) + 4;

    // Parse request line.
    const char* lineEnd = strnstr(data, "\r\n", size);
    if (!lineEnd)
        return false;

    String requestLine = String::fromUTF8(std::span<const char>(data, lineEnd - data));
    auto parts = requestLine.split(' ');
    if (parts.size() < 2 || parts[0] != "GET"_s)
        return false;

    path = parts[1];

    // Check for Upgrade: websocket header.
    if (!strcasestr(data, "Upgrade: websocket"))
        return false;

    // Extract Sec-WebSocket-Key.
    const char* keyHeader = strcasestr(data, "Sec-WebSocket-Key:");
    if (!keyHeader || keyHeader >= headerEnd)
        return false;

    keyHeader += strlen("Sec-WebSocket-Key:");
    while (*keyHeader == ' ')
        keyHeader++;
    auto keyString = String::fromUTF8(std::span<const char>(keyHeader, headerEnd - keyHeader));
    auto crlfPos = keyString.find("\r\n"_s);
    if (crlfPos == notFound)
        return false;

    websocketKey = keyString.left(crlfPos);
    return true;
}

static CString formatUpgradeResponse(const String& acceptKey)
{
    StringBuilder builder;
    builder.append("HTTP/1.1 101 Switching Protocols\r\n"_s);
    builder.append("Upgrade: websocket\r\n"_s);
    builder.append("Connection: Upgrade\r\n"_s);
    builder.append("Sec-WebSocket-Accept: "_s, acceptKey, "\r\n"_s);
    builder.append("\r\n"_s);
    return builder.toString().utf8();
}

static void writeAll(int fd, const uint8_t* data, size_t length)
{
    size_t written = 0;
    while (written < length) {
        ssize_t result = ::write(fd, data + written, length - written);
        if (result <= 0)
            break;
        written += result;
    }
}

static void writeAll(int fd, const CString& str)
{
    writeAll(fd, reinterpret_cast<const uint8_t*>(str.data()), str.length());
}

// Per-connection state machine: first reads HTTP upgrade, then switches to frame mode.
enum class ConnectionPhase { Handshake, WebSocketFrames };

std::optional<String> WebSocketServer::listen(const String& host, unsigned port)
{
    m_listenSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenSocket < 0) {
        RELEASE_LOG(WebDriverBiDi, "Failed to create WebSocket server socket: %s", strerror(errno));
        return std::nullopt;
    }

    int reuseAddr = 1;
    setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));

    struct sockaddr_in addr = { };
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (host == "local"_s || host.isEmpty())
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    else if (host == "all"_s)
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    else
        inet_pton(AF_INET, host.utf8().data(), &addr.sin_addr);

    if (::bind(m_listenSocket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        RELEASE_LOG(WebDriverBiDi, "Failed to bind WebSocket server to port %u: %s", port, strerror(errno));
        ::close(m_listenSocket);
        m_listenSocket = -1;
        return std::nullopt;
    }

    if (::listen(m_listenSocket, 16) < 0) {
        RELEASE_LOG(WebDriverBiDi, "Failed to listen on WebSocket server: %s", strerror(errno));
        ::close(m_listenSocket);
        m_listenSocket = -1;
        return std::nullopt;
    }

    String listenerHost = (host == "all"_s || host.isEmpty()) ? "localhost"_s : host;
    m_listener = WebSocketListener::create(listenerHost, port, false);

    m_listenSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, m_listenSocket, 0, WTF::mainDispatchQueueSingleton()));

    dispatch_source_set_event_handler(m_listenSource.get(), ^{
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = ::accept(m_listenSocket, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);
        if (clientFd < 0)
            return;

        LOG(WebDriverBiDi, "WebSocketServerCocoa: New connection fd=%d", clientFd);

        // Per-connection state.
        auto phase = new ConnectionPhase(ConnectionPhase::Handshake);
        m_clientBuffers.add(clientFd, Vector<uint8_t>());

        auto clientSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, clientFd, 0, WTF::mainDispatchQueueSingleton()));
        m_clientSources.add(clientFd, clientSource);

        dispatch_source_set_event_handler(clientSource.get(), ^{
            uint8_t chunk[4096];
            ssize_t bytesRead = ::read(clientFd, chunk, sizeof(chunk));
            if (bytesRead <= 0) {
                // Connection closed by client.
                m_messageHandler.clientDisconnected(clientFd);
                removeConnection(clientFd);
                dispatch_source_cancel(clientSource.get());
                return;
            }

            auto it = m_clientBuffers.find(clientFd);
            if (it == m_clientBuffers.end())
                return;
            it->value.append(std::span<const uint8_t>(chunk, bytesRead));

            if (*phase == ConnectionPhase::Handshake) {
                String path, websocketKey;
                size_t headerEndOffset;

                if (!parseUpgradeRequest(it->value, path, websocketKey, headerEndOffset))
                    return; // Need more data.

                // Validate the path via the message handler.
                HTTPRequestHandler::Request request;
                request.method = "GET"_s;
                request.path = path;
                if (!m_messageHandler.acceptHandshake(WTF::move(request))) {
                    LOG(WebDriverBiDi, "WebSocketServerCocoa: Rejected handshake for path %s", path.utf8().data());
                    ::close(clientFd);
                    dispatch_source_cancel(clientSource.get());
                    return;
                }

                // Send 101 Switching Protocols.
                auto acceptKey = computeAcceptKey(websocketKey);
                auto response = formatUpgradeResponse(acceptKey);
                writeAll(clientFd, response);

                LOG(WebDriverBiDi, "WebSocketServerCocoa: Handshake complete for %s", path.utf8().data());

                // Switch to frame mode.
                *phase = ConnectionPhase::WebSocketFrames;
                it->value.removeAt(0, headerEndOffset);

                // Associate connection with session.
                auto sessionId = getSessionID(path);
                if (!sessionId.isNull()) {
                    int fd = clientFd;
                    addConnection(WTF::move(fd), sessionId);
                } else {
                    int fd = clientFd;
                    addStaticConnection(WTF::move(fd));
                }
            }

            // Process any complete WebSocket frames in the buffer.
            while (*phase == ConnectionPhase::WebSocketFrames) {
                auto frame = parseFrame(it->value);
                if (!frame)
                    break;

                it->value.removeAt(0, frame->bytesConsumed);

                switch (frame->opcode) {
                case 0x1: { // Text frame.
                    CString payload(std::span<const char>(reinterpret_cast<const char*>(frame->payload.span().data()), frame->payload.size()));
                    LOG(WebDriverBiDi, "WebSocketServerCocoa: Received text frame (%zu bytes) on fd=%d", payload.length(), clientFd);

                    WebSocketMessageHandler::Message message { clientFd, payload };
                    m_messageHandler.handleMessage(WTF::move(message), [this, clientFd](WebSocketMessageHandler::Message&& reply) {
                        if (!reply.connection)
                            reply.connection = clientFd;
                        sendMessage(reply.connection, String::fromUTF8(reply.payload.span()));
                    });
                    break;
                }
                case 0x8: { // Close frame.
                    LOG(WebDriverBiDi, "WebSocketServerCocoa: Received close frame on fd=%d", clientFd);
                    auto closeFrame = buildCloseFrame();
                    writeAll(clientFd, closeFrame.span().data(), closeFrame.size());
                    m_messageHandler.clientDisconnected(clientFd);
                    removeConnection(clientFd);
                    dispatch_source_cancel(clientSource.get());
                    return;
                }
                case 0x9: { // Ping frame.
                    auto pong = buildPongFrame(frame->payload);
                    writeAll(clientFd, pong.span().data(), pong.size());
                    break;
                }
                case 0xA: // Pong frame -- ignore.
                    break;
                default:
                    LOG(WebDriverBiDi, "WebSocketServerCocoa: Unknown opcode 0x%x on fd=%d", frame->opcode, clientFd);
                    break;
                }
            }
        });

        dispatch_source_set_cancel_handler(clientSource.get(), ^{
            ::close(clientFd);
            m_clientBuffers.remove(clientFd);
            m_clientSources.remove(clientFd);
            delete phase;
        });

        dispatch_resume(clientSource.get());
    });

    dispatch_resume(m_listenSource.get());

    LOG(WebDriverBiDi, "WebSocketServerCocoa: Listening on %s:%u", listenerHost.utf8().data(), port);
    return getWebSocketURL(m_listener, nullString());
}

void WebSocketServer::disconnect()
{
    if (m_listenSource) {
        dispatch_source_cancel(m_listenSource.get());
        m_listenSource = nullptr;
    }

    for (auto& pair : m_clientSources) {
        dispatch_source_cancel(pair.value.get());
        // Cancel handler closes fd and cleans up.
    }
    m_clientSources.clear();
    m_clientBuffers.clear();

    if (m_listenSocket >= 0) {
        ::close(m_listenSocket);
        m_listenSocket = -1;
    }
}

void WebSocketServer::disconnectSession(const String& sessionId)
{
    auto conn = connection(sessionId);
    if (!conn)
        return;

    int fd = *conn;

    // Send close frame before disconnecting.
    auto closeFrame = buildCloseFrame();
    writeAll(fd, closeFrame.span().data(), closeFrame.size());

    auto it = m_clientSources.find(fd);
    if (it != m_clientSources.end())
        dispatch_source_cancel(it->value.get());

    removeConnection(fd);
}

void WebSocketServer::sendMessage(WebSocketMessageHandler::Connection connection, const String& message)
{
    auto utf8 = message.utf8();
    auto frame = buildTextFrame(utf8);
    writeAll(connection, frame.span().data(), frame.size());
}

} // namespace WebDriver

#endif // PLATFORM(COCOA) && ENABLE(WEBDRIVER_BIDI)
