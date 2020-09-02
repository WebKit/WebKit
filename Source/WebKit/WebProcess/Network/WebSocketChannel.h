/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "MessageReceiver.h"
#include "MessageSender.h"
#include <WebCore/NetworkSendQueue.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/ThreadableWebSocketChannel.h>
#include <WebCore/WebSocketChannelInspector.h>
#include <WebCore/WebSocketFrame.h>
#include <wtf/WeakPtr.h>

namespace IPC {
class Connection;
class Decoder;
class DataReference;
}

namespace WebKit {

class WebSocketChannel : public IPC::MessageSender, public IPC::MessageReceiver, public WebCore::ThreadableWebSocketChannel, public RefCounted<WebSocketChannel>, public CanMakeWeakPtr<WebSocketChannel> {
public:
    static Ref<WebSocketChannel> create(WebCore::Document&, WebCore::WebSocketChannelClient&);
    ~WebSocketChannel();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    void networkProcessCrashed();

    using RefCounted<WebSocketChannel>::ref;
    using RefCounted<WebSocketChannel>::deref;

private:
    WebSocketChannel(WebCore::Document&, WebCore::WebSocketChannelClient&);

    static WebCore::NetworkSendQueue createMessageQueue(WebCore::Document&, WebSocketChannel&);

    // ThreadableWebSocketChannel
    ConnectStatus connect(const URL&, const String& protocol) final;
    String subprotocol() final;
    String extensions() final;
    SendResult send(const String& message) final;
    SendResult send(const JSC::ArrayBuffer&, unsigned byteOffset, unsigned byteLength) final;
    SendResult send(WebCore::Blob&) final;
    unsigned bufferedAmount() const final;
    void close(int code, const String& reason) final;
    void fail(const String& reason) final;
    void disconnect() final;
    void suspend() final;
    void resume() final;
    void refThreadableWebSocketChannel() final { ref(); }
    void derefThreadableWebSocketChannel() final { deref(); }

    void notifySendFrame(WebCore::WebSocketFrame::OpCode, const char* data, size_t length);
    void logErrorMessage(const String&);

    // Message receivers
    void didConnect(String&& subprotocol, String&& extensions);
    void didReceiveText(String&&);
    void didReceiveBinaryData(IPC::DataReference&&);
    void didClose(unsigned short code, String&&);
    void didReceiveMessageError(String&&);
    void didSendHandshakeRequest(WebCore::ResourceRequest&&);
    void didReceiveHandshakeResponse(WebCore::ResourceResponse&&);

    // MessageSender
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    bool increaseBufferedAmount(size_t);
    void decreaseBufferedAmount(size_t);
    template<typename T> void sendMessage(T&&, size_t byteLength);
    void enqueueTask(Function<void()>&&);

    bool hasCreatedHandshake() const final { return !m_url.isNull(); }
    bool isConnected() const final { return !m_handshakeResponse.isNull(); }
    WebCore::ResourceRequest clientHandshakeRequest(const CookieGetter&) const final { return m_handshakeRequest; }
    const WebCore::ResourceResponse& serverHandshakeResponse() const final { return m_handshakeResponse; }

    WeakPtr<WebCore::Document> m_document;
    WeakPtr<WebCore::WebSocketChannelClient> m_client;
    URL m_url;
    String m_subprotocol;
    String m_extensions;
    size_t m_bufferedAmount { 0 };
    bool m_isClosing { false };
    bool m_isSuspended { false };
    Deque<Function<void()>> m_pendingTasks;
    WebCore::NetworkSendQueue m_messageQueue;
    WebCore::WebSocketChannelInspector m_inspector;
    WebCore::ResourceRequest m_handshakeRequest;
    WebCore::ResourceResponse m_handshakeResponse;
};

} // namespace WebKit
