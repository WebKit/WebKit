/*
 * Copyright (C) 2019 Igalia S.L.
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

#pragma once

#include <libsoup/soup.h>
#include <wtf/glib/GRefPtr.h>

namespace IPC {
class DataReference;
}

namespace WebKit {
class NetworkSocketChannel;

class WebSocketTask {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebSocketTask(NetworkSocketChannel&, SoupSession*, SoupMessage*, const String& protocol);
    ~WebSocketTask();

    void sendString(const IPC::DataReference&, CompletionHandler<void()>&&);
    void sendData(const IPC::DataReference&, CompletionHandler<void()>&&);
    void close(int32_t code, const String& reason);

    void cancel();
    void resume();

private:
    void didConnect(GRefPtr<SoupWebsocketConnection>&&);
    void didFail(const String&);
    void didClose(unsigned short code, const String& reason);

    String acceptedExtensions() const;

    static void didReceiveMessageCallback(WebSocketTask*, SoupWebsocketDataType, GBytes*);
    static void didReceiveErrorCallback(WebSocketTask*, GError*);
    static void didCloseCallback(WebSocketTask*);

    NetworkSocketChannel& m_channel;
    GRefPtr<SoupMessage> m_handshakeMessage;
    GRefPtr<SoupWebsocketConnection> m_connection;
    GRefPtr<GCancellable> m_cancellable;
    bool m_receivedDidFail { false };
    bool m_receivedDidClose { false };
};

} // namespace WebKit
