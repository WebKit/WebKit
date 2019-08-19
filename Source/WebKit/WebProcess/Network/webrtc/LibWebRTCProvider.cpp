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
#include "LibWebRTCProvider.h"

#if USE(LIBWEBRTC)

#include "LibWebRTCNetwork.h"
#include "WebProcess.h"
#include <webrtc/api/asyncresolverfactory.h>
#include <webrtc/pc/peerconnectionfactory.h>

namespace WebKit {
using namespace WebCore;

class AsyncResolverFactory : public webrtc::AsyncResolverFactory {
    WTF_MAKE_FAST_ALLOCATED;
private:
    rtc::AsyncResolverInterface* Create() final
    {
        return WebProcess::singleton().libWebRTCNetwork().socketFactory().createAsyncResolver();
    }
};

rtc::scoped_refptr<webrtc::PeerConnectionInterface> LibWebRTCProvider::createPeerConnection(webrtc::PeerConnectionObserver& observer, rtc::PacketSocketFactory* socketFactory, webrtc::PeerConnectionInterface::RTCConfiguration&& configuration)
{
    return WebCore::LibWebRTCProvider::createPeerConnection(observer, WebProcess::singleton().libWebRTCNetwork().monitor(), *socketFactory, WTFMove(configuration), makeUnique<AsyncResolverFactory>());
}

void LibWebRTCProvider::disableNonLocalhostConnections()
{
    WebProcess::singleton().libWebRTCNetwork().disableNonLocalhostConnections();
}

void LibWebRTCProvider::unregisterMDNSNames(uint64_t documentIdentifier)
{
    WebProcess::singleton().libWebRTCNetwork().mdnsRegister().unregisterMDNSNames(documentIdentifier);
}

void LibWebRTCProvider::registerMDNSName(PAL::SessionID sessionID, uint64_t documentIdentifier, const String& ipAddress, CompletionHandler<void(MDNSNameOrError&&)>&& callback)
{
    WebProcess::singleton().libWebRTCNetwork().mdnsRegister().registerMDNSName(sessionID, documentIdentifier, ipAddress, WTFMove(callback));
}

class RTCSocketFactory final : public rtc::PacketSocketFactory {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RTCSocketFactory(PAL::SessionID sessionID, String&& userAgent)
        : m_sessionID(sessionID)
        , m_userAgent(WTFMove(userAgent))
    {
    }

private:
    rtc::AsyncPacketSocket* CreateUdpSocket(const rtc::SocketAddress& address, uint16_t minPort, uint16_t maxPort) final
    {
        return factory().createUdpSocket(address, minPort, maxPort);
    }

    rtc::AsyncPacketSocket* CreateServerTcpSocket(const rtc::SocketAddress& address, uint16_t minPort, uint16_t maxPort, int options) final
    {
        return factory().createServerTcpSocket(address, minPort, maxPort, options);
    }

    rtc::AsyncPacketSocket* CreateClientTcpSocket(const rtc::SocketAddress& localAddress, const rtc::SocketAddress& remoteAddress, const rtc::ProxyInfo&, const std::string&, int options) final
    {
        return factory().createClientTcpSocket(localAddress, remoteAddress, m_sessionID, String { m_userAgent }, options);
    }

    rtc::AsyncResolverInterface* CreateAsyncResolver() final
    {
        return factory().createAsyncResolver();
    }

    LibWebRTCSocketFactory& factory() { return WebProcess::singleton().libWebRTCNetwork().socketFactory(); }

private:
    PAL::SessionID m_sessionID;
    String m_userAgent;
};

std::unique_ptr<rtc::PacketSocketFactory> LibWebRTCProvider::createSocketFactory(PAL::SessionID sessionID, String&& userAgent)
{
    return makeUnique<RTCSocketFactory>(sessionID, WTFMove(userAgent));
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
