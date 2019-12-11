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
#include "NetworkRTCProvider.h"
#include "RTCPacketOptions.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/Function.h>

namespace WebKit {

NetworkRTCSocket::NetworkRTCSocket(WebCore::LibWebRTCSocketIdentifier identifier, NetworkRTCProvider& rtcProvider)
    : m_identifier(identifier)
    , m_rtcProvider(rtcProvider)
{
}

void NetworkRTCSocket::sendTo(const IPC::DataReference& data, RTCNetwork::SocketAddress&& socketAddress, RTCPacketOptions&& options)
{
    auto buffer = WebCore::SharedBuffer::create(data.data(), data.size());
    m_rtcProvider.callSocket(m_identifier, [buffer = WTFMove(buffer), socketAddress = WTFMove(socketAddress), options = WTFMove(options.options)](auto& socket) {
        socket.sendTo(buffer.get(), socketAddress.value, options);
    });
}

void NetworkRTCSocket::close()
{
    m_rtcProvider.callSocket(m_identifier, [](auto& socket) {
        socket.close();
    });
}
    
void NetworkRTCSocket::setOption(int option, int value)
{
    m_rtcProvider.callSocket(m_identifier, [option, value](auto& socket) {
        socket.setOption(option, value);
    });
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
