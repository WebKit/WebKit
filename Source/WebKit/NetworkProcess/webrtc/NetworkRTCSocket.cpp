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

#include "config.h"
#include "NetworkRTCSocket.h"

#if USE(LIBWEBRTC)

#include "DataReference.h"
#include "LibWebRTCSocketClient.h"
#include "NetworkRTCProvider.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/Function.h>

namespace WebKit {

NetworkRTCSocket::NetworkRTCSocket(uint64_t identifier, NetworkRTCProvider& rtcProvider)
    : m_identifier(identifier)
    , m_rtcProvider(rtcProvider)
{
}

void NetworkRTCSocket::sendTo(const IPC::DataReference& data, const RTCNetwork::SocketAddress& socketAddress, int packetID, int rtpSendtimeExtensionID, String srtpAuth, int64_t srtpPacketIndex, int dscp)
{
    auto buffer = WebCore::SharedBuffer::create(data.data(), data.size());

    rtc::PacketOptions options;
    options.packet_id = packetID;
    options.packet_time_params.rtp_sendtime_extension_id = rtpSendtimeExtensionID;
    options.packet_time_params.srtp_packet_index = srtpPacketIndex;
    options.dscp = static_cast<rtc::DiffServCodePoint>(dscp);
    auto srtpAuthUTF8 = srtpAuth.utf8();
    if (srtpAuthUTF8.length()) {
        options.packet_time_params.srtp_auth_key = std::vector<char>(srtpAuthUTF8.data(), srtpAuthUTF8.data() + srtpAuthUTF8.length());
        options.packet_time_params.srtp_auth_tag_len = srtpAuthUTF8.length();
    } else
        options.packet_time_params.srtp_auth_tag_len = -1;
    
    m_rtcProvider.callSocket(m_identifier, [buffer = WTFMove(buffer), socketAddress, options](LibWebRTCSocketClient& client) {
        client.sendTo(buffer.get(), socketAddress.value, options);
    });
}

void NetworkRTCSocket::close()
{
    m_rtcProvider.callSocket(m_identifier, [](LibWebRTCSocketClient& socket) {
        socket.close();
    });
}
    
void NetworkRTCSocket::setOption(int option, int value)
{
    m_rtcProvider.callSocket(m_identifier, [option, value](LibWebRTCSocketClient& socket) {
        socket.setOption(option, value);
    });
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
