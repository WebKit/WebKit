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
#include "HTTPServer.h"

#if PLATFORM(COCOA)

#include "Logging.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wtf/MainThread.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebDriver {

static const char* httpReasonPhrase(unsigned statusCode)
{
    switch (statusCode) {
    case 200: return "OK";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 500: return "Internal Server Error";
    default: return "Unknown";
    }
}

static CString formatHTTPResponse(const HTTPRequestHandler::Response& response)
{
    StringBuilder builder;
    builder.append("HTTP/1.0 "_s, String::number(response.statusCode), ' ', String::fromUTF8(httpReasonPhrase(response.statusCode)), "\r\n"_s);
    if (!response.contentType.isEmpty())
        builder.append("Content-Type: "_s, response.contentType, "\r\n"_s);
    builder.append("Content-Length: "_s, String::number(response.data.length()), "\r\n"_s);
    builder.append("Cache-Control: no-cache\r\n"_s);
    builder.append("\r\n"_s);
    auto header = builder.toString().utf8();

    // Combine header + body into one buffer.
    Vector<char> result;
    result.reserveInitialCapacity(header.length() + response.data.length());
    result.append(std::span<const char>(header.data(), header.length()));
    result.append(std::span<const char>(response.data.data(), response.data.length()));
    return CString(result.span());
}

// Parse an HTTP request from a raw buffer. Returns true if a complete request was found.
// On success, populates method, path, bodyData, bodyLength.
// headerEndOffset is set to the offset past the \r\n\r\n header terminator.
static bool parseHTTPRequest(const Vector<uint8_t>& buffer, String& method, String& path, size_t& contentLength, size_t& headerEndOffset)
{
    // Find end of headers.
    const char* data = reinterpret_cast<const char*>(buffer.span().data());
    size_t size = buffer.size();

    const char* headerEnd = strnstr(data, "\r\n\r\n", size);
    if (!headerEnd)
        return false;

    headerEndOffset = (headerEnd - data) + 4;

    // Parse request line: "METHOD /path HTTP/1.x\r\n"
    const char* lineEnd = strnstr(data, "\r\n", size);
    if (!lineEnd)
        return false;

    String requestLine = String::fromUTF8(std::span<const char>(data, lineEnd - data));
    auto parts = requestLine.split(' ');
    if (parts.size() < 2)
        return false;

    method = parts[0];
    path = parts[1];

    // Parse Content-Length header.
    contentLength = 0;
    const char* clHeader = strcasestr(data, "Content-Length:");
    if (clHeader && clHeader < headerEnd) {
        clHeader += strlen("Content-Length:");
        while (*clHeader == ' ')
            clHeader++;
        contentLength = strtoul(clHeader, nullptr, 10);
    }

    return true;
}

static void handleClientConnection(int clientFd, HTTPRequestHandler& requestHandler)
{
    // Read source for this client fd.
    auto readSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, clientFd, 0, WTF::mainDispatchQueueSingleton()));

    // Shared state for this connection. Use pointers captured by the block.
    auto buffer = new Vector<uint8_t>();
    auto readSourceRef = readSource; // Extra ref for cancel handler.

    dispatch_source_set_event_handler(readSource.get(), ^{
        uint8_t chunk[4096];
        ssize_t bytesRead = ::read(clientFd, chunk, sizeof(chunk));
        if (bytesRead <= 0) {
            dispatch_source_cancel(readSourceRef.get());
            return;
        }

        buffer->append(std::span<const uint8_t>(chunk, bytesRead));

        // Try to parse a complete HTTP request.
        String method, path;
        size_t contentLength = 0;
        size_t headerEndOffset = 0;

        if (!parseHTTPRequest(*buffer, method, path, contentLength, headerEndOffset))
            return; // Need more data.

        // Check if we have the full body.
        if (buffer->size() < headerEndOffset + contentLength)
            return; // Need more data.

        const char* bodyData = contentLength > 0 ? reinterpret_cast<const char*>(buffer->span().data() + headerEndOffset) : nullptr;

        HTTPRequestHandler::Request request;
        request.method = WTF::move(method);
        request.path = WTF::move(path);
        request.data = bodyData;
        request.dataLength = contentLength;

        LOG(SessionHost, "HTTPServerCocoa: %s %s (%zu bytes body)", request.method.utf8().data(), request.path.utf8().data(), contentLength);

        requestHandler.handleRequest(WTF::move(request), [clientFd, readSourceRef](HTTPRequestHandler::Response&& response) {
            auto formatted = formatHTTPResponse(response);
            // Write the full response. For simplicity, use a blocking write.
            // WebDriver responses are small (< 1MB typically), so this is fine.
            size_t totalWritten = 0;
            while (totalWritten < formatted.length()) {
                ssize_t written = ::write(clientFd, formatted.data() + totalWritten, formatted.length() - totalWritten);
                if (written <= 0)
                    break;
                totalWritten += written;
            }

            // Close connection (HTTP/1.0 style).
            dispatch_source_cancel(readSourceRef.get());
        });
    });

    dispatch_source_set_cancel_handler(readSource.get(), ^{
        ::close(clientFd);
        delete buffer;
    });

    dispatch_resume(readSource.get());
}

bool HTTPServer::listen(const std::optional<String>& host, unsigned port)
{
    m_listenSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenSocket < 0) {
        LOG(SessionHost, "HTTPServerCocoa: Failed to create socket: %s", strerror(errno));
        return false;
    }

    int reuseAddr = 1;
    setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));

    struct sockaddr_in addr = { };
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (!host || *host == "local"_s)
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    else if (*host == "all"_s)
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    else
        inet_pton(AF_INET, host->utf8().data(), &addr.sin_addr);

    if (::bind(m_listenSocket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        LOG(SessionHost, "HTTPServerCocoa: Failed to bind to port %u: %s", port, strerror(errno));
        ::close(m_listenSocket);
        m_listenSocket = -1;
        return false;
    }

    if (::listen(m_listenSocket, 16) < 0) {
        LOG(SessionHost, "HTTPServerCocoa: Failed to listen: %s", strerror(errno));
        ::close(m_listenSocket);
        m_listenSocket = -1;
        return false;
    }

    if (!host || *host == "local"_s || *host == "all"_s)
        m_visibleHost = "localhost"_s;
    else
        m_visibleHost = *host;

    m_listenSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, m_listenSocket, 0, WTF::mainDispatchQueueSingleton()));

    auto& requestHandler = m_requestHandler;
    dispatch_source_set_event_handler(m_listenSource.get(), ^{
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = ::accept(m_listenSocket, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);
        if (clientFd >= 0)
            handleClientConnection(clientFd, requestHandler);
    });

    dispatch_resume(m_listenSource.get());

    LOG(SessionHost, "HTTPServerCocoa: Listening on %s:%u", m_visibleHost.utf8().data(), port);
    return true;
}

void HTTPServer::disconnect()
{
    if (m_listenSource) {
        dispatch_source_cancel(m_listenSource.get());
        m_listenSource = nullptr;
    }

    if (m_listenSocket >= 0) {
        ::close(m_listenSocket);
        m_listenSocket = -1;
    }
}

} // namespace WebDriver

#endif // PLATFORM(COCOA)
