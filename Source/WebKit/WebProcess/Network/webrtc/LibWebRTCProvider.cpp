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

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
#include "LibWebRTCCodecs.h"
#endif

#include "LibWebRTCNetwork.h"
#include "WebProcess.h"
#include <webrtc/api/async_resolver_factory.h>
#include <webrtc/pc/peer_connection_factory.h>

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

void LibWebRTCProvider::registerMDNSName(uint64_t documentIdentifier, const String& ipAddress, CompletionHandler<void(MDNSNameOrError&&)>&& callback)
{
    WebProcess::singleton().libWebRTCNetwork().mdnsRegister().registerMDNSName(documentIdentifier, ipAddress, WTFMove(callback));
}

class RTCSocketFactory final : public LibWebRTCProvider::SuspendableSocketFactory {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RTCSocketFactory(String&& userAgent);

private:
    // SuspendableSocketFactory
    rtc::AsyncPacketSocket* CreateUdpSocket(const rtc::SocketAddress&, uint16_t minPort, uint16_t maxPort) final;
    rtc::AsyncPacketSocket* CreateServerTcpSocket(const rtc::SocketAddress&, uint16_t minPort, uint16_t maxPort, int options) final;
    rtc::AsyncPacketSocket* CreateClientTcpSocket(const rtc::SocketAddress& localAddress, const rtc::SocketAddress& remoteAddress, const rtc::ProxyInfo&, const std::string&, const rtc::PacketSocketTcpOptions&) final;
    rtc::AsyncResolverInterface* CreateAsyncResolver() final;
    void suspend() final;
    void resume() final;

private:
    String m_userAgent;
};

RTCSocketFactory::RTCSocketFactory(String&& userAgent)
    : m_userAgent(WTFMove(userAgent))
{
}

rtc::AsyncPacketSocket* RTCSocketFactory::CreateUdpSocket(const rtc::SocketAddress& address, uint16_t minPort, uint16_t maxPort)
{
    return WebProcess::singleton().libWebRTCNetwork().socketFactory().createUdpSocket(this, address, minPort, maxPort);
}

rtc::AsyncPacketSocket* RTCSocketFactory::CreateServerTcpSocket(const rtc::SocketAddress& address, uint16_t minPort, uint16_t maxPort, int options)
{
    return WebProcess::singleton().libWebRTCNetwork().socketFactory().createServerTcpSocket(this, address, minPort, maxPort, options);
}

rtc::AsyncPacketSocket* RTCSocketFactory::CreateClientTcpSocket(const rtc::SocketAddress& localAddress, const rtc::SocketAddress& remoteAddress, const rtc::ProxyInfo&, const std::string&, const rtc::PacketSocketTcpOptions& options)
{
    return WebProcess::singleton().libWebRTCNetwork().socketFactory().createClientTcpSocket(this, localAddress, remoteAddress, String { m_userAgent }, options);
}

rtc::AsyncResolverInterface* RTCSocketFactory::CreateAsyncResolver()
{
    return WebProcess::singleton().libWebRTCNetwork().socketFactory().createAsyncResolver();
}

void RTCSocketFactory::suspend()
{
    WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([socketGroup = this] {
        WebProcess::singleton().libWebRTCNetwork().socketFactory().forSocketInGroup(socketGroup, [](auto& socket) {
            socket.suspend();
        });
    });
}

void RTCSocketFactory::resume()
{
    WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([socketGroup = this] {
        WebProcess::singleton().libWebRTCNetwork().socketFactory().forSocketInGroup(socketGroup, [](auto& socket) {
            socket.resume();
        });
    });
}

std::unique_ptr<LibWebRTCProvider::SuspendableSocketFactory> LibWebRTCProvider::createSocketFactory(String&& userAgent)
{
    return makeUnique<RTCSocketFactory>(WTFMove(userAgent));
}

#if PLATFORM(COCOA)
std::unique_ptr<webrtc::VideoDecoderFactory> LibWebRTCProvider::createDecoderFactory()
{
#if ENABLE(GPU_PROCESS)
    // We only support efficient sending of video frames with IOSURFACE
#if HAVE(IOSURFACE) && !PLATFORM(MACCATALYST)
    LibWebRTCCodecs::setCallbacks(m_useGPUProcess);
#endif
#endif
    return LibWebRTCProviderCocoa::createDecoderFactory();
}
#endif

} // namespace WebKit

#endif // USE(LIBWEBRTC)
