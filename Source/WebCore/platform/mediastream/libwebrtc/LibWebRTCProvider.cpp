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
#include "LibWebRTCAudioModule.h"
#include "Logging.h"
#include <dlfcn.h>

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/api/audio_codecs/builtin_audio_decoder_factory.h>
#include <webrtc/api/audio_codecs/builtin_audio_encoder_factory.h>
#include <webrtc/api/peerconnectionfactoryproxy.h>
#include <webrtc/modules/audio_processing/include/audio_processing.h>
#include <webrtc/p2p/base/basicpacketsocketfactory.h>
#include <webrtc/p2p/client/basicportallocator.h>
#include <webrtc/pc/peerconnectionfactory.h>
#include <webrtc/rtc_base/physicalsocketserver.h>

ALLOW_UNUSED_PARAMETERS_END

#include <wtf/Function.h>
#include <wtf/NeverDestroyed.h>
#endif

namespace WebCore {

#if !PLATFORM(COCOA) && !PLATFORM(GTK) && !PLATFORM(WPE)
UniqueRef<LibWebRTCProvider> LibWebRTCProvider::create()
{
    return makeUniqueRef<LibWebRTCProvider>();
}
#endif

void LibWebRTCProvider::setActive(bool)
{
}

#if USE(LIBWEBRTC)
static inline rtc::SocketAddress prepareSocketAddress(const rtc::SocketAddress& address, bool disableNonLocalhostConnections)
{
    auto result = address;
    if (disableNonLocalhostConnections)
        result.SetIP("127.0.0.1");
    return result;
}

class BasicPacketSocketFactory : public rtc::BasicPacketSocketFactory {
public:
    explicit BasicPacketSocketFactory(rtc::Thread& networkThread)
        : m_socketFactory(makeUniqueRef<rtc::BasicPacketSocketFactory>(&networkThread))
    {
    }

    void setDisableNonLocalhostConnections(bool disableNonLocalhostConnections) { m_disableNonLocalhostConnections = disableNonLocalhostConnections; }

    rtc::AsyncPacketSocket* CreateUdpSocket(const rtc::SocketAddress& address, uint16_t minPort, uint16_t maxPort) final
    {
        return m_socketFactory->CreateUdpSocket(prepareSocketAddress(address, m_disableNonLocalhostConnections), minPort, maxPort);
    }

    rtc::AsyncPacketSocket* CreateServerTcpSocket(const rtc::SocketAddress& address, uint16_t minPort, uint16_t maxPort, int options) final
    {
        return m_socketFactory->CreateServerTcpSocket(prepareSocketAddress(address, m_disableNonLocalhostConnections), minPort, maxPort, options);
    }

    rtc::AsyncPacketSocket* CreateClientTcpSocket(const rtc::SocketAddress& localAddress, const rtc::SocketAddress& remoteAddress, const rtc::ProxyInfo& info, const std::string& name, int options)
    {
        return m_socketFactory->CreateClientTcpSocket(prepareSocketAddress(localAddress, m_disableNonLocalhostConnections), remoteAddress, info, name, options);
    }

private:
    bool m_disableNonLocalhostConnections { false };
    UniqueRef<rtc::BasicPacketSocketFactory> m_socketFactory;
};

struct PeerConnectionFactoryAndThreads : public rtc::MessageHandler {
    std::unique_ptr<rtc::Thread> networkThread;
    std::unique_ptr<rtc::Thread> signalingThread;
    bool networkThreadWithSocketServer { false };
    std::unique_ptr<LibWebRTCAudioModule> audioDeviceModule;
    std::unique_ptr<rtc::NetworkManager> networkManager;
    std::unique_ptr<BasicPacketSocketFactory> packetSocketFactory;
    std::unique_ptr<rtc::RTCCertificateGenerator> certificateGenerator;

private:
    void OnMessage(rtc::Message*);
};

static void initializePeerConnectionFactoryAndThreads(PeerConnectionFactoryAndThreads& factoryAndThreads)
{
    ASSERT(!factoryAndThreads.networkThread);

#if defined(NDEBUG)
#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    rtc::LogMessage::LogToDebug(LogWebRTC.state != WTFLogChannelOn ? rtc::LS_NONE : rtc::LS_INFO);
#else
    rtc::LogMessage::LogToDebug(rtc::LS_NONE);
#endif
#else
    rtc::LogMessage::LogToDebug(LogWebRTC.state != WTFLogChannelOn ? rtc::LS_WARNING : rtc::LS_INFO);
#endif

    factoryAndThreads.networkThread = factoryAndThreads.networkThreadWithSocketServer ? rtc::Thread::CreateWithSocketServer() : rtc::Thread::Create();
    factoryAndThreads.networkThread->SetName("WebKitWebRTCNetwork", nullptr);
    bool result = factoryAndThreads.networkThread->Start();
    ASSERT_UNUSED(result, result);

    factoryAndThreads.signalingThread = rtc::Thread::Create();
    factoryAndThreads.signalingThread->SetName("WebKitWebRTCSignaling", nullptr);

    result = factoryAndThreads.signalingThread->Start();
    ASSERT(result);

    factoryAndThreads.audioDeviceModule = std::make_unique<LibWebRTCAudioModule>();
}

static inline PeerConnectionFactoryAndThreads& staticFactoryAndThreads()
{
    static NeverDestroyed<PeerConnectionFactoryAndThreads> factoryAndThreads;
    return factoryAndThreads.get();
}

static inline PeerConnectionFactoryAndThreads& getStaticFactoryAndThreads(bool useNetworkThreadWithSocketServer)
{
    auto& factoryAndThreads = staticFactoryAndThreads();

    ASSERT(!factoryAndThreads.networkThread || factoryAndThreads.networkThreadWithSocketServer == useNetworkThreadWithSocketServer);

    if (!factoryAndThreads.networkThread) {
        factoryAndThreads.networkThreadWithSocketServer = useNetworkThreadWithSocketServer;
        initializePeerConnectionFactoryAndThreads(factoryAndThreads);
    }
    return factoryAndThreads;
}

struct ThreadMessageData : public rtc::MessageData {
    ThreadMessageData(Function<void()>&& callback)
        : callback(WTFMove(callback))
    { }
    Function<void()> callback;
};

void PeerConnectionFactoryAndThreads::OnMessage(rtc::Message* message)
{
    ASSERT(message->message_id == 1);
    auto* data = static_cast<ThreadMessageData*>(message->pdata);
    data->callback();
    delete data;
}

void LibWebRTCProvider::callOnWebRTCNetworkThread(Function<void()>&& callback)
{
    PeerConnectionFactoryAndThreads& threads = staticFactoryAndThreads();
    threads.networkThread->Post(RTC_FROM_HERE, &threads, 1, new ThreadMessageData(WTFMove(callback)));
}

void LibWebRTCProvider::callOnWebRTCSignalingThread(Function<void()>&& callback)
{
    PeerConnectionFactoryAndThreads& threads = staticFactoryAndThreads();
    threads.signalingThread->Post(RTC_FROM_HERE, &threads, 1, new ThreadMessageData(WTFMove(callback)));
}

webrtc::PeerConnectionFactoryInterface* LibWebRTCProvider::factory()
{
    if (m_factory)
        return m_factory.get();

    if (!webRTCAvailable())
        return nullptr;

    auto& factoryAndThreads = getStaticFactoryAndThreads(m_useNetworkThreadWithSocketServer);

    m_factory = createPeerConnectionFactory(factoryAndThreads.networkThread.get(), factoryAndThreads.networkThread.get(), factoryAndThreads.audioDeviceModule.get());

    return m_factory;
}

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> LibWebRTCProvider::createPeerConnectionFactory(rtc::Thread* networkThread, rtc::Thread* signalingThread, LibWebRTCAudioModule* audioModule)
{
    return webrtc::CreatePeerConnectionFactory(networkThread, networkThread, signalingThread, audioModule, webrtc::CreateBuiltinAudioEncoderFactory(), webrtc::CreateBuiltinAudioDecoderFactory(), createEncoderFactory(), createDecoderFactory(), nullptr, nullptr);
}

std::unique_ptr<webrtc::VideoDecoderFactory> LibWebRTCProvider::createDecoderFactory()
{
    return nullptr;
}

std::unique_ptr<webrtc::VideoEncoderFactory> LibWebRTCProvider::createEncoderFactory()
{
    return nullptr;
}

void LibWebRTCProvider::setPeerConnectionFactory(rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>&& factory)
{
    m_factory = webrtc::PeerConnectionFactoryProxy::Create(getStaticFactoryAndThreads(m_useNetworkThreadWithSocketServer).signalingThread.get(), WTFMove(factory));
}

void LibWebRTCProvider::disableEnumeratingAllNetworkInterfaces()
{
    m_enableEnumeratingAllNetworkInterfaces = false;
}

void LibWebRTCProvider::enableEnumeratingAllNetworkInterfaces()
{
    m_enableEnumeratingAllNetworkInterfaces = true;
}

rtc::scoped_refptr<webrtc::PeerConnectionInterface> LibWebRTCProvider::createPeerConnection(webrtc::PeerConnectionObserver& observer, webrtc::PeerConnectionInterface::RTCConfiguration&& configuration)
{
    // Default WK1 implementation.
    ASSERT(m_useNetworkThreadWithSocketServer);
    auto& factoryAndThreads = getStaticFactoryAndThreads(m_useNetworkThreadWithSocketServer);

    if (!factoryAndThreads.networkManager)
        factoryAndThreads.networkManager = std::make_unique<rtc::BasicNetworkManager>();

    if (!factoryAndThreads.packetSocketFactory)
        factoryAndThreads.packetSocketFactory = std::make_unique<BasicPacketSocketFactory>(*factoryAndThreads.networkThread);
    factoryAndThreads.packetSocketFactory->setDisableNonLocalhostConnections(m_disableNonLocalhostConnections);

    return createPeerConnection(observer, *factoryAndThreads.networkManager, *factoryAndThreads.packetSocketFactory, WTFMove(configuration));
}

rtc::scoped_refptr<webrtc::PeerConnectionInterface> LibWebRTCProvider::createPeerConnection(webrtc::PeerConnectionObserver& observer, rtc::NetworkManager& networkManager, rtc::PacketSocketFactory& packetSocketFactory, webrtc::PeerConnectionInterface::RTCConfiguration&& configuration)
{
    auto& factoryAndThreads = getStaticFactoryAndThreads(m_useNetworkThreadWithSocketServer);

    std::unique_ptr<cricket::BasicPortAllocator> portAllocator;
    factoryAndThreads.signalingThread->Invoke<void>(RTC_FROM_HERE, [&]() {
        auto basicPortAllocator = std::make_unique<cricket::BasicPortAllocator>(&networkManager, &packetSocketFactory);
        if (!m_enableEnumeratingAllNetworkInterfaces)
            basicPortAllocator->set_flags(basicPortAllocator->flags() | cricket::PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION);
        portAllocator = WTFMove(basicPortAllocator);
    });

    auto* factory = this->factory();
    if (!factory)
        return nullptr;

    return m_factory->CreatePeerConnection(configuration, WTFMove(portAllocator), nullptr, &observer);
}

rtc::RTCCertificateGenerator& LibWebRTCProvider::certificateGenerator()
{
    auto& factoryAndThreads = getStaticFactoryAndThreads(m_useNetworkThreadWithSocketServer);
    if (!factoryAndThreads.certificateGenerator)
        factoryAndThreads.certificateGenerator = std::make_unique<rtc::RTCCertificateGenerator>(factoryAndThreads.signalingThread.get(), factoryAndThreads.networkThread.get());

    return *factoryAndThreads.certificateGenerator;
}

#endif // USE(LIBWEBRTC)

} // namespace WebCore
