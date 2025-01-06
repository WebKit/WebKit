/*
 * Copyright (C) 2022 Sony Interactive Entertainment Inc.
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

#include "NetworkSessionCurl.h"
#include "WebSocketTask.h"
#include <WebCore/CurlStream.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ThreadableWebSocketChannel.h>
#include <WebCore/WebSocketDeflateFramer.h>
#include <WebCore/WebSocketFrame.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {
class WebSocketTask;
}

namespace WebCore {
class CertificateInfo;
class CurlStreamScheduler;
class SharedBuffer;
class WebSocketHandshake;
struct ClientOrigin;
}

namespace WebKit {

class NetworkSocketChannel;
struct SessionSet;

class WebSocketTask : public CanMakeWeakPtr<WebSocketTask>, public CanMakeCheckedPtr<WebSocketTask>, public WebCore::CurlStream::Client {
    WTF_MAKE_TZONE_ALLOCATED(WebSocketTask);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WebSocketTask);
public:
    WebSocketTask(NetworkSocketChannel&, WebPageProxyIdentifier, const WebCore::ResourceRequest&, const String& protocol, const WebCore::ClientOrigin&);
    virtual ~WebSocketTask();

    void sendString(std::span<const uint8_t>, CompletionHandler<void()>&&);
    void sendData(std::span<const uint8_t>, CompletionHandler<void()>&&);
    void close(int32_t code, const String& reason);

    void cancel();
    void resume();

    NetworkSessionCurl* networkSession();
    SessionSet* sessionSet() { return nullptr; }

    WebPageProxyIdentifier webProxyPageID() const { return m_webProxyPageID; }

    const WebCore::SecurityOriginData& topOrigin() const { return m_topOrigin; }

private:
    enum class State : uint8_t {
        Connecting,
        Handshaking,
        Opened,
        Closing,
        Closed
    };

    void didOpen(WebCore::CurlStreamID) final;
    void didSendData(WebCore::CurlStreamID, size_t) final { };
    void didReceiveData(WebCore::CurlStreamID, const WebCore::SharedBuffer&) final;
    void didFail(WebCore::CurlStreamID, CURLcode, WebCore::CertificateInfo&&) final;

    Ref<NetworkSocketChannel> protectedChannel() const;

    void tryServerTrustEvaluation(WebCore::AuthenticationChallenge&&, String&&);

    bool appendReceivedBuffer(const WebCore::SharedBuffer&);
    void skipReceivedBuffer(size_t len);

    Expected<bool, String> validateOpeningHandshake();
    std::optional<String> receiveFrames(Function<void(WebCore::WebSocketFrame::OpCode, std::span<const uint8_t>)>&&);
    std::optional<String> validateFrame(const WebCore::WebSocketFrame&);

    bool sendFrame(WebCore::WebSocketFrame::OpCode, std::span<const uint8_t> data);
    void sendClosingHandshakeIfNeeded(int32_t, const String& reason);

    void didFail(String&& reason);
    void didClose(int32_t code, const String& reason);

    bool isStreamInvalidated() { return m_streamID == WebCore::invalidCurlStreamID; }
    void destructStream();

    WeakRef<NetworkSocketChannel> m_channel;
    WebPageProxyIdentifier m_webProxyPageID;
    WebCore::ResourceRequest m_request;
    String m_protocol;
    WebCore::SecurityOriginData m_topOrigin;

    WebCore::CurlStreamScheduler& m_scheduler;
    WebCore::CurlStreamID m_streamID { WebCore::invalidCurlStreamID };

    State m_state { State::Connecting };

    std::unique_ptr<WebCore::WebSocketHandshake> m_handshake;
    WebCore::WebSocketDeflateFramer m_deflateFramer;

    bool m_didCompleteOpeningHandshake { false };

    bool m_shouldDiscardReceivedData { false };
    Vector<uint8_t> m_receiveBuffer;

    bool m_hasContinuousFrame { false };
    WebCore::WebSocketFrame::OpCode m_continuousFrameOpCode { WebCore::WebSocketFrame::OpCode::OpCodeInvalid };
    Vector<uint8_t> m_continuousFrameData;

    bool m_receivedClosingHandshake { false };
    int32_t m_closeEventCode { WebCore::ThreadableWebSocketChannel::CloseEventCode::CloseEventCodeNotSpecified };
    String m_closeEventReason;

    bool m_didSendClosingHandshake { false };
    bool m_receivedDidFail { false };
};

} // namespace WebKit
