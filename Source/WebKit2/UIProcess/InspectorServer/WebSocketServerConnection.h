/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(INSPECTOR_SERVER)

#include <WebCore/SocketStreamHandleClient.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class HTTPHeaderMap;
class SocketStreamError;
class SocketStreamHandle;
}

namespace WebKit {

class HTTPRequest;
class WebSocketServer;
class WebSocketServerClient;

class WebSocketServerConnection final : public WebCore::SocketStreamHandleClient {
public:
    enum WebSocketServerMode { HTTP, WebSocket };
    WebSocketServerConnection(WebSocketServerClient*, WebSocketServer*);
    virtual ~WebSocketServerConnection();

    unsigned identifier() const { return m_identifier; }
    void setIdentifier(unsigned id) { m_identifier = id; }
    void setSocketHandle(Ref<WebCore::SocketStreamHandle>&&);

    // Sending data over the connection.
    void sendWebSocketMessage(const String& message);
    void sendHTTPResponseHeader(int statusCode, const String& statusText, const WebCore::HTTPHeaderMap& headerFields);
    void sendRawData(const char* data, size_t length);

    // Terminating the connection.
    void shutdownNow();
    void shutdownAfterSendOrNow();

private:
    // SocketStreamHandleClient implementation.
    void didOpenSocketStream(WebCore::SocketStreamHandle&) final { }
    void didCloseSocketStream(WebCore::SocketStreamHandle&) final;
    void didReceiveSocketStreamData(WebCore::SocketStreamHandle&, const char* data, std::optional<size_t> length) final;
    void didUpdateBufferedAmount(WebCore::SocketStreamHandle&, size_t bufferedAmount) final;
    void didFailSocketStream(WebCore::SocketStreamHandle&, const WebCore::SocketStreamError&) final { }

    // HTTP Mode.
    void readHTTPMessage();

    // WebSocket Mode.
    void upgradeToWebSocketServerConnection(PassRefPtr<HTTPRequest>);
    void readWebSocketFrames();
    bool readWebSocketFrame();

    unsigned m_identifier { 0 };
    Vector<char> m_bufferedData;
    WebSocketServerMode m_mode { HTTP };
    RefPtr<WebCore::SocketStreamHandle> m_socket;
    WebSocketServer* m_server { nullptr };
    WebSocketServerClient* m_client { nullptr };
    bool m_shutdownAfterSend { false };
};

}

#endif // ENABLE(INSPECTOR_SERVER)
