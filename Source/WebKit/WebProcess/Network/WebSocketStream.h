/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#include "DataReference.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include <WebCore/SocketStreamHandle.h>
#include <WebCore/WebSocketIdentifier.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebCore {
class SocketStreamError;
}

namespace WebKit {

class WebSocketStream : public IPC::MessageSender, public IPC::MessageReceiver, public WebCore::SocketStreamHandle {
public:
    static Ref<WebSocketStream> create(const URL&, WebCore::SocketStreamHandleClient&, WebCore::WebSocketIdentifier, const String& credentialPartition);
    static void networkProcessCrashed();
    static WebSocketStream* streamWithIdentifier(WebCore::WebSocketIdentifier);
    
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    
    // SocketStreamHandle
    void platformSend(const uint8_t*, size_t, Function<void(bool)>&&) final;
    void platformSendHandshake(const uint8_t*, size_t, const std::optional<WebCore::CookieRequestHeaderFieldProxy>&, Function<void(bool, bool)>&&);
    void platformClose() final;
    size_t bufferedAmount() final;

    // Message receivers
    void didOpenSocketStream();
    void didCloseSocketStream();
    void didReceiveSocketStreamData(const IPC::DataReference&);
    void didFailToReceiveSocketStreamData();
    void didUpdateBufferedAmount(uint64_t);
    void didFailSocketStream(WebCore::SocketStreamError&&);

    void didSendData(uint64_t, bool success);
    void didSendHandshake(uint64_t, bool success, bool didAccessSecureCookies);
    
private:
    // MessageSender
    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

    WebSocketStream(const URL&, WebCore::SocketStreamHandleClient&, WebCore::WebSocketIdentifier, const String& credentialPartition);
    ~WebSocketStream();

    WebCore::WebSocketIdentifier m_identifier;
    size_t m_bufferedAmount { 0 };
    WebCore::SocketStreamHandleClient& m_client;
    HashMap<uint64_t, Function<void(bool)>> m_sendDataCallbacks;
    HashMap<uint64_t, Function<void(bool, bool)>> m_sendHandshakeCallbacks;
};

} // namespace WebKit
