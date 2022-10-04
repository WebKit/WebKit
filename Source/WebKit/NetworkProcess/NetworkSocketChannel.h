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

#include "DataReference.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/Timer.h>
#include <WebCore/WebSocketIdentifier.h>
#include <pal/SessionID.h>
#include <wtf/CompletionHandler.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
struct ClientOrigin;
class ResourceRequest;
class ResourceResponse;
}

namespace IPC {
class Connection;
class Decoder;
}

namespace WebKit {

class WebSocketTask;
class NetworkConnectionToWebProcess;
class NetworkProcess;
class NetworkSession;

class NetworkSocketChannel : public IPC::MessageSender, public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<NetworkSocketChannel> create(NetworkConnectionToWebProcess&, PAL::SessionID, const WebCore::ResourceRequest&, const String& protocol, WebCore::WebSocketIdentifier, WebPageProxyIdentifier, const WebCore::ClientOrigin&, bool hadMainFrameMainResourcePrivateRelayed, bool allowPrivacyProxy, bool networkConnectionIntegrityEnabled);

    NetworkSocketChannel(NetworkConnectionToWebProcess&, NetworkSession*, const WebCore::ResourceRequest&, const String& protocol, WebCore::WebSocketIdentifier, WebPageProxyIdentifier, const WebCore::ClientOrigin&, bool hadMainFrameMainResourcePrivateRelayed, bool allowPrivacyProxy, bool networkConnectionIntegrityEnabled);
    ~NetworkSocketChannel();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    friend class WebSocketTask;

private:
    void didConnect(const String& subprotocol, const String& extensions);
    void didReceiveText(const String&);
    void didReceiveBinaryData(const uint8_t* data, size_t length);
    void didClose(unsigned short code, const String& reason);
    void didReceiveMessageError(String&&);
    void didSendHandshakeRequest(WebCore::ResourceRequest&&);
    void didReceiveHandshakeResponse(WebCore::ResourceResponse&&);

    void sendString(const IPC::DataReference&, CompletionHandler<void()>&&);
    void sendData(const IPC::DataReference&, CompletionHandler<void()>&&);
    void close(int32_t code, const String& reason);
    void sendDelayedError();

    NetworkSession* session();

    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final { return m_identifier.toUInt64(); }

    void finishClosingIfPossible();

    NetworkConnectionToWebProcess& m_connectionToWebProcess;
    WebCore::WebSocketIdentifier m_identifier;
    WeakPtr<NetworkSession> m_session;
    std::unique_ptr<WebSocketTask> m_socket;

    enum class State { Open, Closing, Closed };
    State m_state { State::Open };
    WebCore::Timer m_errorTimer;
    String m_errorMessage;
    std::optional<std::pair<unsigned short, String>> m_closeInfo;
    WebPageProxyIdentifier m_webPageProxyID;
};

} // namespace WebKit
