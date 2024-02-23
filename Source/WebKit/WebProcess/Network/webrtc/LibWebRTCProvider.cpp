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

#include "LibWebRTCDnsResolverFactory.h"
#include "LibWebRTCNetwork.h"
#include "LibWebRTCNetworkManager.h"
#include "RTCDataChannelRemoteManager.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/Page.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/Settings.h>

ALLOW_COMMA_BEGIN

#include <webrtc/pc/peer_connection_factory.h>

ALLOW_COMMA_END

namespace WebKit {
using namespace WebCore;

LibWebRTCProvider::LibWebRTCProvider(WebPage& webPage)
    : m_webPage(webPage)
{
    m_useNetworkThreadWithSocketServer = false;
#if PLATFORM(GTK) || PLATFORM(WPE)
    m_supportsMDNS = false;
#else
    m_supportsMDNS = true;
#endif
}

LibWebRTCProvider::~LibWebRTCProvider() = default;

rtc::scoped_refptr<webrtc::PeerConnectionInterface> LibWebRTCProvider::createPeerConnection(ScriptExecutionContextIdentifier identifier, webrtc::PeerConnectionObserver& observer, rtc::PacketSocketFactory* socketFactory, webrtc::PeerConnectionInterface::RTCConfiguration&& configuration)
{
#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA) && !PLATFORM(MACCATALYST)
    LibWebRTCCodecs::initializeIfNeeded();
#endif

    auto* networkManager = LibWebRTCNetworkManager::getOrCreate(identifier);
    if (!networkManager)
        return nullptr;

    networkManager->setEnumeratingAllNetworkInterfacesEnabled(isEnumeratingAllNetworkInterfacesEnabled());
    networkManager->setEnumeratingVisibleNetworkInterfacesEnabled(isEnumeratingVisibleNetworkInterfacesEnabled());

    return WebCore::LibWebRTCProvider::createPeerConnection(observer, *networkManager, *socketFactory, WTFMove(configuration), makeUnique<LibWebRTCDnsResolverFactory>());
}

void LibWebRTCProvider::disableNonLocalhostConnections()
{
    WebProcess::singleton().libWebRTCNetwork().disableNonLocalhostConnections();
}

class RTCSocketFactory final : public LibWebRTCProvider::SuspendableSocketFactory {
    WTF_MAKE_FAST_ALLOCATED;
public:
    RTCSocketFactory(WebPageProxyIdentifier, String&& userAgent, ScriptExecutionContextIdentifier, bool isFirstParty, RegistrableDomain&&);

    void disableRelay() final { m_isRelayDisabled = true; }

private:
    // SuspendableSocketFactory
    rtc::AsyncPacketSocket* CreateUdpSocket(const rtc::SocketAddress&, uint16_t minPort, uint16_t maxPort) final;
    rtc::AsyncListenSocket* CreateServerTcpSocket(const rtc::SocketAddress&, uint16_t minPort, uint16_t maxPort, int options) final { return nullptr; }
    rtc::AsyncPacketSocket* CreateClientTcpSocket(const rtc::SocketAddress& localAddress, const rtc::SocketAddress& remoteAddress, const rtc::ProxyInfo&, const std::string&, const rtc::PacketSocketTcpOptions&) final;
    std::unique_ptr<webrtc::AsyncDnsResolverInterface> CreateAsyncDnsResolver() final;
    void suspend() final;
    void resume() final;

private:
    WebPageProxyIdentifier m_pageIdentifier;
    String m_userAgent;
    ScriptExecutionContextIdentifier m_contextIdentifier;
    bool m_isFirstParty { false };
    bool m_isRelayDisabled { false };
    RegistrableDomain m_domain;
};

RTCSocketFactory::RTCSocketFactory(WebPageProxyIdentifier pageIdentifier, String&& userAgent, ScriptExecutionContextIdentifier identifier, bool isFirstParty, RegistrableDomain&& domain)
    : m_pageIdentifier(pageIdentifier)
    , m_userAgent(WTFMove(userAgent))
    , m_contextIdentifier(identifier)
    , m_isFirstParty(isFirstParty)
    , m_domain(WTFMove(domain))
{
}

rtc::AsyncPacketSocket* RTCSocketFactory::CreateUdpSocket(const rtc::SocketAddress& address, uint16_t minPort, uint16_t maxPort)
{
    return WebProcess::singleton().libWebRTCNetwork().socketFactory().createUdpSocket(m_contextIdentifier, address, minPort, maxPort, m_pageIdentifier, m_isFirstParty, m_isRelayDisabled, m_domain);
}

rtc::AsyncPacketSocket* RTCSocketFactory::CreateClientTcpSocket(const rtc::SocketAddress& localAddress, const rtc::SocketAddress& remoteAddress, const rtc::ProxyInfo&, const std::string&, const rtc::PacketSocketTcpOptions& options)
{
    return WebProcess::singleton().libWebRTCNetwork().socketFactory().createClientTcpSocket(m_contextIdentifier, localAddress, remoteAddress, String { m_userAgent }, options, m_pageIdentifier, m_isFirstParty, m_isRelayDisabled, m_domain);
}

std::unique_ptr<webrtc::AsyncDnsResolverInterface> RTCSocketFactory::CreateAsyncDnsResolver()
{
    return WebProcess::singleton().libWebRTCNetwork().socketFactory().createAsyncDnsResolver();
}

void RTCSocketFactory::suspend()
{
    WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([identifier = m_contextIdentifier] {
        WebProcess::singleton().libWebRTCNetwork().socketFactory().forSocketInGroup(identifier, [](auto& socket) {
            socket.suspend();
        });
    });
}

void RTCSocketFactory::resume()
{
    WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([identifier = m_contextIdentifier] {
        WebProcess::singleton().libWebRTCNetwork().socketFactory().forSocketInGroup(identifier, [](auto& socket) {
            socket.resume();
        });
    });
}

void LibWebRTCProvider::startedNetworkThread()
{
    WebProcess::singleton().libWebRTCNetwork().setAsActive();
}

std::unique_ptr<LibWebRTCProvider::SuspendableSocketFactory> LibWebRTCProvider::createSocketFactory(String&& userAgent, ScriptExecutionContextIdentifier identifier, bool isFirstParty, RegistrableDomain&& domain)
{
    Ref webPage { m_webPage.get() };
    auto factory = makeUnique<RTCSocketFactory>(webPage->webPageProxyIdentifier(), WTFMove(userAgent), identifier, isFirstParty, WTFMove(domain));

    auto* page = webPage->corePage();
    if (!page || !page->settings().webRTCSocketsProxyingEnabled())
        factory->disableRelay();

    return factory;
}

RefPtr<RTCDataChannelRemoteHandlerConnection> LibWebRTCProvider::createRTCDataChannelRemoteHandlerConnection()
{
    return &RTCDataChannelRemoteManager::sharedManager().remoteHandlerConnection();
}

void LibWebRTCProvider::setLoggingLevel(WTFLogLevel level)
{
    WebCore::LibWebRTCProvider::setLoggingLevel(level);
#if PLATFORM(COCOA)
    WebProcess::singleton().libWebRTCCodecs().setLoggingLevel(level);
#endif
}

void LibWebRTCProvider::willCreatePeerConnectionFactory()
{
#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA) && !PLATFORM(MACCATALYST)
    LibWebRTCCodecs::initializeIfNeeded();
    if (isSupportingVP9VTB())
        WebProcess::singleton().libWebRTCCodecs().setVP9VTBSupport(true);
#endif
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
