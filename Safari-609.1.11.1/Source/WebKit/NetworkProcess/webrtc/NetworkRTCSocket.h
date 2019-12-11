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

#include "RTCNetwork.h"

#include <WebCore/LibWebRTCMacros.h>
#include <WebCore/LibWebRTCSocketIdentifier.h>
#include <webrtc/rtc_base/async_packet_socket.h>
#include <webrtc/rtc_base/third_party/sigslot/sigslot.h>

namespace IPC {
class Connection;
class DataReference;
}

namespace rtc {
class AsyncPacketSocket;
class SocketAddress;
struct PacketOptions;
struct SentPacket;
typedef int64_t PacketTime;
}

namespace WebCore {
class SharedBuffer;
}

namespace WebKit {

class NetworkConnectionToWebProcess;
class NetworkRTCProvider;
struct RTCPacketOptions;

class NetworkRTCSocket {
public:
    NetworkRTCSocket(WebCore::LibWebRTCSocketIdentifier, NetworkRTCProvider&);
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
private:
    void sendTo(const IPC::DataReference&, RTCNetwork::SocketAddress&&, RTCPacketOptions&&);
    void close();
    void setOption(int option, int value);

    WebCore::LibWebRTCSocketIdentifier m_identifier;
    NetworkRTCProvider& m_rtcProvider;
};

} // namespace WebKit

#endif // USE(LIBWEBRTC)
