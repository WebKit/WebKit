/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if USE(LIBWEBRTC)

#include <WebCore/LibWebRTCProvider.h>
#include <WebCore/LibWebRTCSocketIdentifier.h>
#include <webrtc/rtc_base/async_packet_socket.h>
#include <wtf/Deque.h>
#include <wtf/Forward.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebKit {

class LibWebRTCSocketFactory;

class LibWebRTCSocket final : public rtc::AsyncPacketSocket {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Type { UDP, ServerTCP, ClientTCP, ServerConnectionTCP };

    LibWebRTCSocket(LibWebRTCSocketFactory&, const void* socketGroup, Type, const rtc::SocketAddress& localAddress, const rtc::SocketAddress& remoteAddress);
    ~LibWebRTCSocket();

    const void* socketGroup() const { return m_socketGroup; }
    WebCore::LibWebRTCSocketIdentifier identifier() const { return m_identifier; }
    const rtc::SocketAddress& localAddress() const { return m_localAddress; }
    const rtc::SocketAddress& remoteAddress() const { return m_remoteAddress; }

    void setError(int error) { m_error = error; }
    void setState(State state) { m_state = state; }

    void suspend();
    void resume();

private:
    bool willSend(size_t);

    friend class LibWebRTCNetwork;
    void signalReadPacket(const uint8_t*, size_t, rtc::SocketAddress&&, int64_t);
    void signalSentPacket(int, int64_t);
    void signalAddressReady(const rtc::SocketAddress&);
    void signalConnect();
    void signalClose(int);
    void signalNewConnection(rtc::AsyncPacketSocket*);

    // AsyncPacketSocket API
    int GetError() const final { return m_error; }
    void SetError(int error) final { setError(error); }
    rtc::SocketAddress GetLocalAddress() const final;
    rtc::SocketAddress GetRemoteAddress() const final;
    int Send(const void *pv, size_t cb, const rtc::PacketOptions& options) final { return SendTo(pv, cb, m_remoteAddress, options); }
    int SendTo(const void *, size_t, const rtc::SocketAddress&, const rtc::PacketOptions&) final;
    int Close() final;
    State GetState() const final { return m_state; }
    int GetOption(rtc::Socket::Option, int*) final;
    int SetOption(rtc::Socket::Option, int) final;

    LibWebRTCSocketFactory& m_factory;
    WebCore::LibWebRTCSocketIdentifier m_identifier;
    Type m_type;
    rtc::SocketAddress m_localAddress;
    rtc::SocketAddress m_remoteAddress;

    int m_error { 0 };
    State m_state { STATE_BINDING };

    static const unsigned MAX_SOCKET_OPTION { rtc::Socket::OPT_RTP_SENDTIME_EXTN_ID + 1 };
    std::optional<int> m_options[MAX_SOCKET_OPTION];

    Deque<size_t> m_beingSentPacketSizes;
    size_t m_availableSendingBytes { 65536 };
    bool m_shouldSignalReadyToSend { false };
    bool m_isSuspended { false };
    const void* m_socketGroup { nullptr };
};

} // namespace WebKit

#endif // USE(LIBWEBRTC)
