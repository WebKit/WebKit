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
#include "RTCRtpCapabilities.h"
#include <dlfcn.h>

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/api/async_resolver_factory.h>
#include <webrtc/api/audio_codecs/builtin_audio_decoder_factory.h>
#include <webrtc/api/audio_codecs/builtin_audio_encoder_factory.h>
#include <webrtc/api/create_peerconnection_factory.h>
#include <webrtc/api/peer_connection_factory_proxy.h>
#include <webrtc/modules/audio_processing/include/audio_processing.h>
#include <webrtc/p2p/base/basic_packet_socket_factory.h>
#include <webrtc/p2p/client/basic_port_allocator.h>
#include <webrtc/pc/peer_connection_factory.h>
#include <webrtc/rtc_base/physical_socket_server.h>

ALLOW_UNUSED_PARAMETERS_END

#include <wtf/Function.h>
#include <wtf/NeverDestroyed.h>
#endif

namespace WebCore {

#if !USE(LIBWEBRTC)
UniqueRef<LibWebRTCProvider> LibWebRTCProvider::create()
{
    return makeUniqueRef<LibWebRTCProvider>();
}

bool LibWebRTCProvider::webRTCAvailable()
{
    return false;
}
#endif

#if !USE(LIBWEBRTC) || !PLATFORM(COCOA)
void LibWebRTCProvider::registerWebKitVP9Decoder()
{
}

void LibWebRTCProvider::setH264HardwareEncoderAllowed(bool)
{
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
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit BasicPacketSocketFactory(rtc::Thread& networkThread)
        : m_socketFactory(makeUniqueRefWithoutFastMallocCheck<rtc::BasicPacketSocketFactory>(&networkThread))
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

    rtc::AsyncPacketSocket* CreateClientTcpSocket(const rtc::SocketAddress& localAddress, const rtc::SocketAddress& remoteAddress, const rtc::ProxyInfo& info, const std::string& name, const rtc::PacketSocketTcpOptions& options)
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
    std::unique_ptr<rtc::NetworkManager> networkManager;
    std::unique_ptr<BasicPacketSocketFactory> packetSocketFactory;
    std::unique_ptr<rtc::RTCCertificateGenerator> certificateGenerator;

private:
    void OnMessage(rtc::Message*);
};

static void doReleaseLogging(rtc::LoggingSeverity severity, const char* message)
{
#if RELEASE_LOG_DISABLED
    UNUSED_PARAM(severity);
    UNUSED_PARAM(message);
#else
    if (severity == rtc::LS_ERROR)
        RELEASE_LOG_ERROR(WebRTC, "LibWebRTC error: %" PUBLIC_LOG_STRING, message);
    else
        RELEASE_LOG(WebRTC, "LibWebRTC message: %" PUBLIC_LOG_STRING, message);
#endif
}

static void setLogging(rtc::LoggingSeverity level)
{
    rtc::LogMessage::SetLogOutput(level, (level == rtc::LS_NONE) ? nullptr : doReleaseLogging);
}

static rtc::LoggingSeverity computeLogLevel()
{
#if defined(NDEBUG)
#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    if (LogWebRTC.state != WTFLogChannelState::On)
        return rtc::LS_ERROR;

    switch (LogWebRTC.level) {
    case WTFLogLevel::Always:
    case WTFLogLevel::Error:
        return rtc::LS_ERROR;
    case WTFLogLevel::Warning:
        return rtc::LS_WARNING;
    case WTFLogLevel::Info:
        return rtc::LS_INFO;
    case WTFLogLevel::Debug:
        return rtc::LS_VERBOSE;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return rtc::LS_NONE;
#else
    return rtc::LS_NONE;
#endif
#else
    return (LogWebRTC.state != WTFLogChannelState::On) ? rtc::LS_WARNING : rtc::LS_INFO;
#endif
}

static void initializePeerConnectionFactoryAndThreads(PeerConnectionFactoryAndThreads& factoryAndThreads)
{
    ASSERT(!factoryAndThreads.networkThread);

    factoryAndThreads.networkThread = factoryAndThreads.networkThreadWithSocketServer ? rtc::Thread::CreateWithSocketServer() : rtc::Thread::Create();
    factoryAndThreads.networkThread->SetName("WebKitWebRTCNetwork", nullptr);
    bool result = factoryAndThreads.networkThread->Start();
    ASSERT_UNUSED(result, result);

    factoryAndThreads.signalingThread = rtc::Thread::Create();
    factoryAndThreads.signalingThread->SetName("WebKitWebRTCSignaling", nullptr);

    result = factoryAndThreads.signalingThread->Start();
    ASSERT(result);
}

static inline PeerConnectionFactoryAndThreads& staticFactoryAndThreads()
{
    static NeverDestroyed<PeerConnectionFactoryAndThreads> factoryAndThreads;
    return factoryAndThreads.get();
}

PeerConnectionFactoryAndThreads& LibWebRTCProvider::getStaticFactoryAndThreads(bool useNetworkThreadWithSocketServer)
{
    auto& factoryAndThreads = staticFactoryAndThreads();

    ASSERT(!factoryAndThreads.networkThread || factoryAndThreads.networkThreadWithSocketServer == useNetworkThreadWithSocketServer);

    if (!factoryAndThreads.networkThread) {
        factoryAndThreads.networkThreadWithSocketServer = useNetworkThreadWithSocketServer;
        initializePeerConnectionFactoryAndThreads(factoryAndThreads);
        startedNetworkThread();
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

bool LibWebRTCProvider::hasWebRTCThreads()
{
    return !!staticFactoryAndThreads().networkThread;
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

void LibWebRTCProvider::setEnableLogging(bool enableLogging)
{
    if (!m_enableLogging)
        return;
    m_enableLogging = enableLogging;
    setLogging(enableLogging ? computeLogLevel() : rtc::LS_NONE);
}

webrtc::PeerConnectionFactoryInterface* LibWebRTCProvider::factory()
{
    if (m_factory)
        return m_factory.get();

    if (!webRTCAvailable()) {
        RELEASE_LOG_ERROR(WebRTC, "LibWebRTC is not available to create a factory");
        return nullptr;
    }

    auto& factoryAndThreads = getStaticFactoryAndThreads(m_useNetworkThreadWithSocketServer);

    m_factory = createPeerConnectionFactory(factoryAndThreads.networkThread.get(), factoryAndThreads.networkThread.get());

    return m_factory;
}

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> LibWebRTCProvider::createPeerConnectionFactory(rtc::Thread* networkThread, rtc::Thread* signalingThread)
{
    auto audioModule = rtc::scoped_refptr<webrtc::AudioDeviceModule>(new rtc::RefCountedObject<LibWebRTCAudioModule>());

    return webrtc::CreatePeerConnectionFactory(networkThread, signalingThread, signalingThread, WTFMove(audioModule), webrtc::CreateBuiltinAudioEncoderFactory(), webrtc::CreateBuiltinAudioDecoderFactory(), createEncoderFactory(), createDecoderFactory(), nullptr, nullptr);
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

rtc::scoped_refptr<webrtc::PeerConnectionInterface> LibWebRTCProvider::createPeerConnection(webrtc::PeerConnectionObserver& observer, rtc::PacketSocketFactory*, webrtc::PeerConnectionInterface::RTCConfiguration&& configuration)
{
    // Default WK1 implementation.
    ASSERT(m_useNetworkThreadWithSocketServer);
    auto& factoryAndThreads = getStaticFactoryAndThreads(m_useNetworkThreadWithSocketServer);

    if (!factoryAndThreads.networkManager)
        factoryAndThreads.networkManager = makeUniqueWithoutFastMallocCheck<rtc::BasicNetworkManager>();

    if (!factoryAndThreads.packetSocketFactory)
        factoryAndThreads.packetSocketFactory = makeUnique<BasicPacketSocketFactory>(*factoryAndThreads.networkThread);
    factoryAndThreads.packetSocketFactory->setDisableNonLocalhostConnections(m_disableNonLocalhostConnections);

    return createPeerConnection(observer, *factoryAndThreads.networkManager, *factoryAndThreads.packetSocketFactory, WTFMove(configuration), nullptr);
}

void LibWebRTCProvider::setEnableWebRTCEncryption(bool enableWebRTCEncryption)
{
    auto* factory = this->factory();
    if (!factory)
        return;

    webrtc::PeerConnectionFactoryInterface::Options options;
    options.disable_encryption = !enableWebRTCEncryption;
    options.ssl_max_version = m_useDTLS10 ? rtc::SSL_PROTOCOL_DTLS_10 : rtc::SSL_PROTOCOL_DTLS_12;
    m_factory->SetOptions(options);
}

void LibWebRTCProvider::setUseDTLS10(bool useDTLS10)
{
    m_useDTLS10 = useDTLS10;

    auto* factory = this->factory();
    if (!factory)
        return;

    webrtc::PeerConnectionFactoryInterface::Options options;
    options.ssl_max_version = useDTLS10 ? rtc::SSL_PROTOCOL_DTLS_10 : rtc::SSL_PROTOCOL_DTLS_12;
    m_factory->SetOptions(options);
}

rtc::scoped_refptr<webrtc::PeerConnectionInterface> LibWebRTCProvider::createPeerConnection(webrtc::PeerConnectionObserver& observer, rtc::NetworkManager& networkManager, rtc::PacketSocketFactory& packetSocketFactory, webrtc::PeerConnectionInterface::RTCConfiguration&& configuration, std::unique_ptr<webrtc::AsyncResolverFactory>&& asyncResolveFactory)
{
    auto& factoryAndThreads = getStaticFactoryAndThreads(m_useNetworkThreadWithSocketServer);

    std::unique_ptr<cricket::BasicPortAllocator> portAllocator;
    factoryAndThreads.signalingThread->Invoke<void>(RTC_FROM_HERE, [&]() {
        auto basicPortAllocator = makeUniqueWithoutFastMallocCheck<cricket::BasicPortAllocator>(&networkManager, &packetSocketFactory);
        if (!m_enableEnumeratingAllNetworkInterfaces)
            basicPortAllocator->set_flags(basicPortAllocator->flags() | cricket::PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION);
        portAllocator = WTFMove(basicPortAllocator);
    });

    auto* factory = this->factory();
    if (!factory)
        return nullptr;

    webrtc::PeerConnectionDependencies dependencies { &observer };
    dependencies.allocator = WTFMove(portAllocator);
    dependencies.async_resolver_factory = WTFMove(asyncResolveFactory);

    return m_factory->CreatePeerConnection(configuration, WTFMove(dependencies));
}

void LibWebRTCProvider::prepareCertificateGenerator(Function<void(rtc::RTCCertificateGenerator&)>&& callback)
{
    auto& factoryAndThreads = getStaticFactoryAndThreads(m_useNetworkThreadWithSocketServer);
    if (!factoryAndThreads.certificateGenerator)
        factoryAndThreads.certificateGenerator = makeUniqueWithoutFastMallocCheck<rtc::RTCCertificateGenerator>(factoryAndThreads.signalingThread.get(), factoryAndThreads.networkThread.get());

    auto& generator = *factoryAndThreads.certificateGenerator;
    callOnWebRTCSignalingThread([&generator, callback = WTFMove(callback)]() mutable {
        callback(generator);
    });
}

static inline Optional<cricket::MediaType> typeFromKind(const String& kind)
{
    if (kind == "audio"_s)
        return cricket::MediaType::MEDIA_TYPE_AUDIO;
    if (kind == "video"_s)
        return cricket::MediaType::MEDIA_TYPE_VIDEO;
    return { };
}

static inline String fromStdString(const std::string& value)
{
    return String::fromUTF8(value.data(), value.length());
}

static inline Optional<uint16_t> toChannels(absl::optional<int> numChannels)
{
    if (!numChannels)
        return { };
    return static_cast<uint32_t>(*numChannels);
}

static inline RTCRtpCapabilities toRTCRtpCapabilities(const webrtc::RtpCapabilities& rtpCapabilities)
{
    RTCRtpCapabilities capabilities;

    capabilities.codecs.reserveInitialCapacity(rtpCapabilities.codecs.size());
    for (auto& codec : rtpCapabilities.codecs) {
        StringBuilder sdpFmtpLine;
        bool hasParameter = false;
        for (auto& parameter : codec.parameters) {
            sdpFmtpLine.append(hasParameter ? ";" : "", StringView(parameter.first.data(), parameter.first.length()), '=', StringView(parameter.second.data(), parameter.second.length()));
            hasParameter = true;
        }
        capabilities.codecs.uncheckedAppend(RTCRtpCodecCapability { fromStdString(codec.mime_type()), static_cast<uint32_t>(codec.clock_rate ? *codec.clock_rate : 0), toChannels(codec.num_channels), sdpFmtpLine.toString() });
    }

    capabilities.headerExtensions.reserveInitialCapacity(rtpCapabilities.header_extensions.size());
    for (auto& header : rtpCapabilities.header_extensions)
        capabilities.headerExtensions.uncheckedAppend(RTCRtpCapabilities::HeaderExtensionCapability { fromStdString(header.uri) });

    return capabilities;
}

Optional<RTCRtpCapabilities> LibWebRTCProvider::receiverCapabilities(const String& kind)
{
    auto mediaType = typeFromKind(kind);
    if (!mediaType)
        return { };

    auto* factory = this->factory();
    if (!factory)
        return { };

    return toRTCRtpCapabilities(factory->GetRtpReceiverCapabilities(*mediaType));
}

Optional<RTCRtpCapabilities> LibWebRTCProvider::senderCapabilities(const String& kind)
{
    auto mediaType = typeFromKind(kind);
    if (!mediaType)
        return { };

    auto* factory = this->factory();
    if (!factory)
        return { };

    return toRTCRtpCapabilities(factory->GetRtpSenderCapabilities(*mediaType));
}

#endif // USE(LIBWEBRTC)

} // namespace WebCore
