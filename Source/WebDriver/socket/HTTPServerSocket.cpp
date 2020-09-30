/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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
#include <wtf/text/StringBuilder.h>

namespace WebDriver {

bool HTTPServer::listen(const Optional<String>& host, unsigned port)
{
    auto& endpoint = RemoteInspectorSocketEndpoint::singleton();

    if (auto id = endpoint.listenInet(host ? host.value().utf8().data() : "", port, *this)) {
        m_server = id;
        return true;
    }
    return false;
}

void HTTPServer::disconnect()
{
    auto& endpoint = RemoteInspectorSocketEndpoint::singleton();
    endpoint.disconnect(m_server.value());
}

Optional<ConnectionID> HTTPServer::doAccept(RemoteInspectorSocketEndpoint& endpoint, PlatformSocketType socket)
{
    if (auto id = endpoint.createClient(socket, m_requestHandler)) {
        m_requestHandler.connect(id.value());
        return id;
    }

    return WTF::nullopt;
}

void HTTPServer::didClose(RemoteInspectorSocketEndpoint&, ConnectionID)
{
    m_server = WTF::nullopt;
}

void HTTPRequestHandler::connect(ConnectionID id)
{
    m_client = id;
    reset();
}

void HTTPRequestHandler::reset()
{
    m_parser = { };
}

void HTTPRequestHandler::didReceive(RemoteInspectorSocketEndpoint&, ConnectionID, Vector<uint8_t>&& data)
{
    switch (m_parser.parse(WTFMove(data))) {
    case HTTPParser::Phase::Complete: {
        auto message = m_parser.pullMessage();
        HTTPRequestHandler::Request request {
            message.method,
            message.path,
            reinterpret_cast<const char*>(message.requestBody.data()),
            static_cast<size_t>(message.requestBody.size())
        };

        handleRequest(WTFMove(request), [this](HTTPRequestHandler::Response&& response) {
            sendResponse(WTFMove(response));
        });
        break;
    }
    case HTTPParser::Phase::Error: {
        HTTPRequestHandler::Response response {
            400,
            "text/html; charset=utf-8",
            "<h1>Bad client</h1> Invalid HTML format",
        };
        sendResponse(WTFMove(response));
        return;
    }
    default:
        return;
    }
}

void HTTPRequestHandler::sendResponse(HTTPRequestHandler::Response&& response)
{
    auto& endpoint = RemoteInspectorSocketEndpoint::singleton();
    auto packet = packHTTPMessage(WTFMove(response));
    endpoint.send(m_client.value(), packet.utf8().data(), packet.length());
    reset();
}

String HTTPRequestHandler::packHTTPMessage(HTTPRequestHandler::Response&& response) const
{
    StringBuilder builder;
    const char* EOL = "\r\n";

    builder.append("HTTP/1.0 ");
    builder.appendNumber(response.statusCode);
    builder.append(" ");
    builder.append(response.statusCode == 200 ? "OK" : "ERROR");
    builder.append(EOL);

    if (!response.data.isNull()) {
        builder.append("Content-Type: ");
        builder.append(response.contentType);
        builder.append(EOL);

        builder.append("Content-Length: ");
        builder.appendNumber(response.data.length());
        builder.append(EOL);

        builder.append("Cache-Control: no-cache");
        builder.append(EOL);
    }

    builder.append(EOL);

    if (!response.data.isNull())
        builder.append(response.data.data());

    return builder.toString();
}

void HTTPRequestHandler::didClose(RemoteInspectorSocketEndpoint&, ConnectionID)
{
    m_client = WTF::nullopt;
    reset();
}

} // namespace WebDriver
