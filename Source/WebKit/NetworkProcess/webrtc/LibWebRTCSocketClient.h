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

#include <WebCore/LibWebRTCMacros.h>
#include <webrtc/rtc_base/asyncpacketsocket.h>
#include <webrtc/rtc_base/third_party/sigslot/sigslot.h>

namespace rtc {
class AsyncPacketSocket;
class SocketAddress;
struct PacketOptions;
struct PacketTime;
struct SentPacket;
}

namespace WebCore {
class SharedBuffer;
}

namespace WebKit {

class NetworkRTCProvider;

class LibWebRTCSocketClient final : public sigslot::has_slots<> {
public:
    enum class Type { UDP, ServerTCP, ClientTCP, ServerConnectionTCP };

    LibWebRTCSocketClient(uint64_t identifier, NetworkRTCProvider&, std::unique_ptr<rtc::AsyncPacketSocket>&&, Type);

    uint64_t identifier() const { return m_identifier; }
    Type type() const { return m_type; }
    void close();

private:
    friend class NetworkRTCSocket;

    void setOption(int option, int value);
    void sendTo(const WebCore::SharedBuffer&, const rtc::SocketAddress&, const rtc::PacketOptions&);

    void signalReadPacket(rtc::AsyncPacketSocket*, const char*, size_t, const rtc::SocketAddress&, const rtc::PacketTime&);
    void signalSentPacket(rtc::AsyncPacketSocket*, const rtc::SentPacket&);
    void signalAddressReady(rtc::AsyncPacketSocket*, const rtc::SocketAddress&);
    void signalConnect(rtc::AsyncPacketSocket*);
    void signalClose(rtc::AsyncPacketSocket*, int);
    void signalNewConnection(rtc::AsyncPacketSocket* socket, rtc::AsyncPacketSocket* newSocket);

    void signalAddressReady();

    uint64_t m_identifier;
    Type m_type;
    NetworkRTCProvider& m_rtcProvider;
    std::unique_ptr<rtc::AsyncPacketSocket> m_socket;
};

} // namespace WebKit

#endif // USE(LIBWEBRTC)
