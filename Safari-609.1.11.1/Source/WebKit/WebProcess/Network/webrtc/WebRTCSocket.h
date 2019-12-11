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
#include <WebCore/LibWebRTCSocketIdentifier.h>
#include <wtf/Function.h>

namespace IPC {
class Connection;
class DataReference;
class Decoder;
}

namespace WebKit {

class LibWebRTCSocket;
class LibWebRTCSocketFactory;

class WebRTCSocket {
public:
    WebRTCSocket(LibWebRTCSocketFactory&, WebCore::LibWebRTCSocketIdentifier);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    static void signalOnNetworkThread(LibWebRTCSocketFactory&, WebCore::LibWebRTCSocketIdentifier, Function<void(LibWebRTCSocket&)>&&);

private:
    void signalReadPacket(const IPC::DataReference&, const RTCNetwork::IPAddress&, uint16_t port, int64_t);
    void signalSentPacket(int, int64_t);
    void signalAddressReady(const RTCNetwork::SocketAddress&);
    void signalConnect();
    void signalClose(int);
    void signalNewConnection(WebCore::LibWebRTCSocketIdentifier newSocketIdentifier, const WebKit::RTCNetwork::SocketAddress&);

    LibWebRTCSocketFactory& m_factory;
    WebCore::LibWebRTCSocketIdentifier m_identifier;
};

} // namespace WebKit

#endif // USE(LIBWEBRTC)
