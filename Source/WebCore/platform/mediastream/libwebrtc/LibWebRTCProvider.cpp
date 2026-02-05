/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#include "ContentType.h"
#include "LibWebRTCAudioModule.h"
#include "LibWebRTCLogSink.h"
#include "LibWebRTCUtils.h"
#include "Logging.h"
#include "PlatformMediaCapabilitiesDecodingInfo.h"
#include "PlatformMediaCapabilitiesEncodingInfo.h"
#include "PlatformMediaDecodingConfiguration.h"
#include "PlatformMediaEncodingConfiguration.h"
#include "ProcessQualified.h"
#include <dlfcn.h>
#include <wtf/TZoneMallocInlines.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN

#include <webrtc/api/audio_codecs/builtin_audio_decoder_factory.h>
#include <webrtc/api/audio_codecs/builtin_audio_encoder_factory.h>
#include <webrtc/api/create_modular_peer_connection_factory.h>
#include <webrtc/api/enable_media.h>
#include <webrtc/api/environment/environment_factory.h>
IGNORE_CLANG_WARNINGS_BEGIN("nullability-completeness")
#include <webrtc/api/rtc_event_log/rtc_event_log_factory.h>
#include <webrtc/modules/audio_processing/include/audio_processing.h>
#include <webrtc/p2p/base/basic_packet_socket_factory.h>
#include <webrtc/p2p/client/basic_port_allocator.h>
IGNORE_CLANG_WARNINGS_END
// See Bug 274508: Disable thread-safety-reference-return warnings in libwebrtc
IGNORE_CLANG_WARNINGS_BEGIN("thread-safety-reference-return")
#include <webrtc/pc/peer_connection_factory.h>
IGNORE_CLANG_WARNINGS_END
#include <webrtc/pc/peer_connection_factory_proxy.h>
#include <webrtc/rtc_base/physical_socket_server.h>
#include <webrtc/rtc_base/task_queue_gcd.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#include <wtf/Function.h>
#include <wtf/NeverDestroyed.h>

#if PLATFORM(COCOA)
#include "VP9UtilitiesCocoa.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LibWebRTCProvider);

LibWebRTCProvider::LibWebRTCProvider()
{
}

LibWebRTCProvider::~LibWebRTCProvider()
{
}

#if !PLATFORM(COCOA)
void LibWebRTCProvider::registerWebKitVP9Decoder()
{
}
#endif

static inline webrtc::SocketAddress prepareSocketAddress(const webrtc::SocketAddress& address, bool disableNonLocalhostConnections)
{
    auto result = address;
    if (disableNonLocalhostConnections)
        result.SetIP("127.0.0.1");
    return result;
}

class BasicPacketSocketFactory : public webrtc::PacketSocketFactory {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(BasicPacketSocketFactory);
public:
    explicit BasicPacketSocketFactory(webrtc::Thread& networkThread)
        : m_socketFactory(makeUniqueRefWithoutFastMallocCheck<webrtc::BasicPacketSocketFactory>(networkThread.socketserver()))
    {
    }

    void setDisableNonLocalhostConnections(bool disableNonLocalhostConnections) { m_disableNonLocalhostConnections = disableNonLocalhostConnections; }

    std::unique_ptr<webrtc::AsyncPacketSocket> CreateUdpSocket(const webrtc::Environment& env, const webrtc::SocketAddress& address, uint16_t minPort, uint16_t maxPort) final
    {
        return m_socketFactory->CreateUdpSocket(env, prepareSocketAddress(address, m_disableNonLocalhostConnections), minPort, maxPort);
    }

    std::unique_ptr<webrtc::AsyncListenSocket> CreateServerTcpSocket(const webrtc::Environment&, const webrtc::SocketAddress&, uint16_t, uint16_t, int) final
    {
        return nullptr;
    }

    std::unique_ptr<webrtc::AsyncPacketSocket> CreateClientTcpSocket(const webrtc::Environment& env, const webrtc::SocketAddress& localAddress, const webrtc::SocketAddress& remoteAddress, const webrtc::PacketSocketTcpOptions& options) final
    {
        return m_socketFactory->CreateClientTcpSocket(env, prepareSocketAddress(localAddress, m_disableNonLocalhostConnections), remoteAddress, options);
    }

    std::unique_ptr<webrtc::AsyncDnsResolverInterface> CreateAsyncDnsResolver() final { return m_socketFactory->CreateAsyncDnsResolver(); }

private:
    bool m_disableNonLocalhostConnections { false };
    const UniqueRef<webrtc::BasicPacketSocketFactory> m_socketFactory;
};

struct PeerConnectionFactoryAndThreads {
    std::unique_ptr<webrtc::Thread> networkThread;
    std::unique_ptr<webrtc::Thread> signalingThread;
    bool networkThreadWithSocketServer { false };
    std::unique_ptr<webrtc::NetworkManager> networkManager;
    std::unique_ptr<BasicPacketSocketFactory> packetSocketFactory;
    std::unique_ptr<webrtc::RTCCertificateGenerator> certificateGenerator;
};

static void doReleaseLogging(webrtc::LoggingSeverity severity, const char* message)
{
#if RELEASE_LOG_DISABLED
    UNUSED_PARAM(severity);
    UNUSED_PARAM(message);
#else
    if (severity == webrtc::LS_ERROR)
        RELEASE_LOG_ERROR_FORWARDABLE_UNSAFE_ARGS(WebRTC, LIBWEBRTC_LOG_ERROR, message);
    else
        RELEASE_LOG_FORWARDABLE_UNSAFE_ARGS(WebRTC, LIBWEBRTC_LOG_MESSAGE, message);
#endif
}

static webrtc::LoggingSeverity computeLogLevel(WTFLogLevel level)
{
#if !RELEASE_LOG_DISABLED
    switch (level) {
    case WTFLogLevel::Always:
    case WTFLogLevel::Error:
        return webrtc::LS_ERROR;
    case WTFLogLevel::Warning:
        return webrtc::LS_WARNING;
    case WTFLogLevel::Info:
        return webrtc::LS_INFO;
    case WTFLogLevel::Debug:
        return webrtc::LS_VERBOSE;
    }
    RELEASE_ASSERT_NOT_REACHED();
#else
    UNUSED_PARAM(level);
#endif
    return webrtc::LS_NONE;
}

static LibWebRTCLogSink& getRTCLogSink()
{
    static LazyNeverDestroyed<LibWebRTCLogSink> logSink;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        LibWebRTCLogSink::LogCallback callback = [] (auto&& severity, auto&& message) {
            doReleaseLogging(severity, message.c_str());
        };
        logSink.construct(WTF::move(callback));
    });
    return logSink.get();
}

void LibWebRTCProvider::setRTCLogging(WTFLogLevel level)
{
    getRTCLogSink().start(computeLogLevel(level));
}

static void initializePeerConnectionFactoryAndThreads(PeerConnectionFactoryAndThreads& factoryAndThreads)
{
    ASSERT(!factoryAndThreads.networkThread);

    factoryAndThreads.networkThread = factoryAndThreads.networkThreadWithSocketServer ? webrtc::Thread::CreateWithSocketServer() : webrtc::Thread::Create();
    factoryAndThreads.networkThread->SetName("WebKitWebRTCNetwork", nullptr);
    bool result = factoryAndThreads.networkThread->Start();
    ASSERT_UNUSED(result, result);

    factoryAndThreads.signalingThread = webrtc::Thread::Create();
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

bool LibWebRTCProvider::hasWebRTCThreads()
{
    return !!staticFactoryAndThreads().networkThread;
}

void LibWebRTCProvider::callOnWebRTCNetworkThread(Function<void()>&& callback)
{
    PeerConnectionFactoryAndThreads& threads = staticFactoryAndThreads();
    threads.networkThread->PostTask(WTF::move(callback));
}

void LibWebRTCProvider::callOnWebRTCSignalingThread(Function<void()>&& callback)
{
    PeerConnectionFactoryAndThreads& threads = staticFactoryAndThreads();
    threads.signalingThread->PostTask(WTF::move(callback));
}

webrtc::Thread& LibWebRTCProvider::signalingThread()
{
    PeerConnectionFactoryAndThreads& threads = staticFactoryAndThreads();
    return *threads.signalingThread;
}

void LibWebRTCProvider::setLoggingLevel(WTFLogLevel level)
{
    setRTCLogging(level);
}

bool LibWebRTCProvider::isEnumeratingAllNetworkInterfacesEnabled() const
{
    return m_enableEnumeratingAllNetworkInterfaces;
}

void LibWebRTCProvider::disableEnumeratingAllNetworkInterfaces()
{
    m_enableEnumeratingAllNetworkInterfaces = false;
}

void LibWebRTCProvider::enableEnumeratingAllNetworkInterfaces()
{
    m_enableEnumeratingAllNetworkInterfaces = true;
}

void LibWebRTCProvider::enableEnumeratingVisibleNetworkInterfaces()
{
    m_enableEnumeratingVisibleNetworkInterfaces = true;
}

void LibWebRTCProvider::disableNonLocalhostConnections()
{
    m_disableNonLocalhostConnections = true;
}

std::unique_ptr<LibWebRTCProvider::SuspendableSocketFactory> LibWebRTCProvider::createSocketFactory(String&& /* userAgent */, ScriptExecutionContextIdentifier, bool /* isFirstParty */, RegistrableDomain&&, bool)
{
    return nullptr;
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

    m_factory = createPeerConnectionFactory(factoryAndThreads.networkThread.get(), factoryAndThreads.signalingThread.get());

    return m_factory.get();
}

void LibWebRTCProvider::clearFactory()
{
    m_audioModule = nullptr;
    m_factory = nullptr;

    m_videoDecodingCapabilities = { };
    m_videoEncodingCapabilities = { };
}

void LibWebRTCProvider::setUseL4S(bool useL4S)
{
    if (m_useL4S == useL4S)
        return;

    m_useL4S = useL4S;
    clearFactory();
}

class LibWebRTCProviderFieldTrials final : public webrtc::FieldTrialsView {
    WTF_MAKE_TZONE_ALLOCATED(LibWebRTCProviderFieldTrials);
public:
    explicit LibWebRTCProviderFieldTrials(bool useL4S)
        : m_useL4S(useL4S)
    {
    }

private:
    std::string Lookup(std::string_view trial) const final
    {
        if (!m_useL4S || trial != "WebRTC-RFC8888CongestionControlFeedback")
            return "";
        return "Enabled,force_send:true";
    }

    bool m_useL4S { false };
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(LibWebRTCProviderFieldTrials);

Ref<webrtc::PeerConnectionFactoryInterface> LibWebRTCProvider::createPeerConnectionFactory(webrtc::Thread* networkThread, webrtc::Thread* signalingThread)
{
    willCreatePeerConnectionFactory();

    ASSERT(!m_audioModule);
    m_audioModule = LibWebRTCAudioModule::create();

    webrtc::PeerConnectionFactoryDependencies dependencies;
    dependencies.env = webrtc::CreateEnvironment(makeUnique<LibWebRTCProviderFieldTrials>(m_useL4S)
#if PLATFORM(COCOA)
        , webrtc::CreateTaskQueueGcdFactory()
#endif
    );
    dependencies.network_thread = networkThread;
    dependencies.worker_thread = signalingThread;
    dependencies.signaling_thread = signalingThread;
    dependencies.event_log_factory = std::make_unique<webrtc::RtcEventLogFactory>();

    if (networkThread)
        dependencies.socket_factory = networkThread->socketserver();

    dependencies.adm = webrtc::scoped_refptr<webrtc::AudioDeviceModule>(m_audioModule.get());
    dependencies.audio_encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
    dependencies.audio_decoder_factory = webrtc::CreateBuiltinAudioDecoderFactory();
    dependencies.video_encoder_factory = createEncoderFactory();
    dependencies.video_decoder_factory = createDecoderFactory();

    webrtc::EnableMedia(dependencies);

    return toRef(webrtc::CreateModularPeerConnectionFactory(WTF::move(dependencies)));
}

std::unique_ptr<webrtc::VideoDecoderFactory> LibWebRTCProvider::createDecoderFactory()
{
    return nullptr;
}

std::unique_ptr<webrtc::VideoEncoderFactory> LibWebRTCProvider::createEncoderFactory()
{
    return nullptr;
}

void LibWebRTCProvider::startedNetworkThread()
{

}

void LibWebRTCProvider::setPeerConnectionFactory(webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>&& factory)
{
    auto* thread = getStaticFactoryAndThreads(m_useNetworkThreadWithSocketServer).signalingThread.get();
    m_factory = toRef<webrtc::PeerConnectionFactoryInterface>(webrtc::PeerConnectionFactoryProxy::Create(thread, thread, WTF::move(factory)));
}

webrtc::scoped_refptr<webrtc::PeerConnectionInterface> LibWebRTCProvider::createPeerConnection(ScriptExecutionContextIdentifier, webrtc::PeerConnectionObserver& observer, webrtc::PacketSocketFactory*, webrtc::PeerConnectionInterface::RTCConfiguration&& configuration)
{
    // Default WK1 implementation.
    ASSERT(m_useNetworkThreadWithSocketServer);
    auto& factoryAndThreads = getStaticFactoryAndThreads(m_useNetworkThreadWithSocketServer);

    if (!factoryAndThreads.networkManager)
        factoryAndThreads.networkManager = makeUniqueWithoutFastMallocCheck<webrtc::BasicNetworkManager>(webrtc::CreateEnvironment(), factoryAndThreads.networkThread->socketserver());

    if (!factoryAndThreads.packetSocketFactory)
        factoryAndThreads.packetSocketFactory = makeUnique<BasicPacketSocketFactory>(*factoryAndThreads.networkThread);
    factoryAndThreads.packetSocketFactory->setDisableNonLocalhostConnections(m_disableNonLocalhostConnections);

    return createPeerConnection(observer, *factoryAndThreads.networkManager, *factoryAndThreads.packetSocketFactory, WTF::move(configuration), nullptr);
}

void LibWebRTCProvider::setEnableWebRTCEncryption(bool enableWebRTCEncryption)
{
    auto* factory = this->factory();
    if (!factory)
        return;

    webrtc::PeerConnectionFactoryInterface::Options options;
    options.disable_encryption = !enableWebRTCEncryption;
    m_factory->SetOptions(options);
}

webrtc::scoped_refptr<webrtc::PeerConnectionInterface> LibWebRTCProvider::createPeerConnection(webrtc::PeerConnectionObserver& observer, webrtc::NetworkManager& networkManager, webrtc::PacketSocketFactory& packetSocketFactory, webrtc::PeerConnectionInterface::RTCConfiguration&& configuration, std::unique_ptr<webrtc::AsyncDnsResolverFactoryInterface>&& asyncDnsResolverFactory)
{
    auto* factory = this->factory();
    if (!factory)
        return nullptr;

    auto& factoryAndThreads = getStaticFactoryAndThreads(m_useNetworkThreadWithSocketServer);

    std::unique_ptr<webrtc::BasicPortAllocator> portAllocator;
    factoryAndThreads.signalingThread->BlockingCall([&]() {
        auto basicPortAllocator = makeUniqueWithoutFastMallocCheck<webrtc::BasicPortAllocator>(webrtc::CreateEnvironment(), &networkManager, &packetSocketFactory);

        basicPortAllocator->set_allow_tcp_listen(false);
        portAllocator = WTF::move(basicPortAllocator);
    });

    if (auto portRange = portAllocatorRange())
        portAllocator->SetPortRange(portRange->first, portRange->second);

    webrtc::PeerConnectionDependencies dependencies { &observer };
    dependencies.allocator = WTF::move(portAllocator);
    dependencies.async_dns_resolver_factory = WTF::move(asyncDnsResolverFactory);

    auto peerConnectionOrError = m_factory->CreatePeerConnectionOrError(configuration, WTF::move(dependencies));
    if (!peerConnectionOrError.ok())
        return nullptr;

    return peerConnectionOrError.MoveValue();
}

void LibWebRTCProvider::prepareCertificateGenerator(Function<void(webrtc::RTCCertificateGenerator&)>&& callback)
{
    auto& factoryAndThreads = getStaticFactoryAndThreads(m_useNetworkThreadWithSocketServer);
    if (!factoryAndThreads.certificateGenerator)
        factoryAndThreads.certificateGenerator = makeUniqueWithoutFastMallocCheck<webrtc::RTCCertificateGenerator>(factoryAndThreads.signalingThread.get(), factoryAndThreads.networkThread.get());

    auto& generator = *factoryAndThreads.certificateGenerator;
    callOnWebRTCSignalingThread([&generator, callback = WTF::move(callback)]() mutable {
        callback(generator);
    });
}

static inline std::optional<webrtc::MediaType> typeFromKind(const String& kind)
{
    if (kind == "audio"_s)
        return webrtc::MediaType::AUDIO;
    if (kind == "video"_s)
        return webrtc::MediaType::VIDEO;
    return { };
}

static inline RTCRtpCapabilities toRTCRtpCapabilities(const webrtc::RtpCapabilities& rtpCapabilities)
{
    RTCRtpCapabilities capabilities;

    capabilities.codecs = WTF::map(rtpCapabilities.codecs, [](auto& codec) {
        StringBuilder sdpFmtpLineBuilder;
        bool hasParameter = false;
        for (auto& parameter : codec.parameters) {
            sdpFmtpLineBuilder.append(hasParameter ? ";"_s : ""_s, std::span(parameter.first), '=', std::span(parameter.second));
            hasParameter = true;
        }
        String sdpFmtpLine;
        if (sdpFmtpLineBuilder.length())
            sdpFmtpLine = sdpFmtpLineBuilder.toString();
        return RTCRtpCodecCapability { fromStdString(codec.mime_type()), static_cast<uint32_t>(codec.clock_rate ? *codec.clock_rate : 0), codec.num_channels, WTF::move(sdpFmtpLine) };

    });

    capabilities.headerExtensions = WTF::map(rtpCapabilities.header_extensions, [](auto& header) {
        return RTCRtpCapabilities::HeaderExtensionCapability { fromStdString(header.uri) };
    });

    return capabilities;
}

std::optional<RTCRtpCapabilities> LibWebRTCProvider::receiverCapabilities(const String& kind)
{
    auto mediaType = typeFromKind(kind);
    if (!mediaType)
        return { };

    switch (*mediaType) {
    case webrtc::MediaType::AUDIO:
        return audioDecodingCapabilities();
    case webrtc::MediaType::VIDEO:
        return videoDecodingCapabilities();
    case webrtc::MediaType::ANY:
        ASSERT_NOT_REACHED();
        return { };
    case webrtc::MediaType::DATA:
        ASSERT_NOT_REACHED();
        return { };
    case webrtc::MediaType::UNSUPPORTED:
        ASSERT_NOT_REACHED();
        return { };
    }

    ASSERT_NOT_REACHED();
    return { };
}

void LibWebRTCProvider::initializeAudioDecodingCapabilities()
{
    if (auto* factory = this->factory())
        m_audioDecodingCapabilities = toRTCRtpCapabilities(factory->GetRtpReceiverCapabilities(webrtc::MediaType::AUDIO));
}

void LibWebRTCProvider::initializeVideoDecodingCapabilities()
{
    if (auto* factory = this->factory())
        m_videoDecodingCapabilities = toRTCRtpCapabilities(factory->GetRtpReceiverCapabilities(webrtc::MediaType::VIDEO));
}

std::optional<RTCRtpCapabilities> LibWebRTCProvider::senderCapabilities(const String& kind)
{
    auto mediaType = typeFromKind(kind);
    if (!mediaType)
        return { };

    switch (*mediaType) {
    case webrtc::MediaType::AUDIO:
        return audioEncodingCapabilities();
    case webrtc::MediaType::VIDEO:
        return videoEncodingCapabilities();
    case webrtc::MediaType::ANY:
        ASSERT_NOT_REACHED();
        return { };
    case webrtc::MediaType::DATA:
        ASSERT_NOT_REACHED();
        return { };
    case webrtc::MediaType::UNSUPPORTED:
        ASSERT_NOT_REACHED();
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

void LibWebRTCProvider::initializeAudioEncodingCapabilities()
{
    if (auto* factory = this->factory())
        m_audioEncodingCapabilities = toRTCRtpCapabilities(factory->GetRtpSenderCapabilities(webrtc::MediaType::AUDIO));
}

void LibWebRTCProvider::initializeVideoEncodingCapabilities()
{
    if (auto* factory = this->factory())
        m_videoEncodingCapabilities = toRTCRtpCapabilities(factory->GetRtpSenderCapabilities(webrtc::MediaType::VIDEO));
}

std::optional<PlatformMediaCapabilitiesDecodingInfo> LibWebRTCProvider::videoDecodingCapabilitiesOverride(const PlatformMediaCapabilitiesVideoConfiguration& configuration)
{
    PlatformMediaCapabilitiesDecodingInfo info;
    ContentType contentType { configuration.contentType };
    auto containerType = contentType.containerType();
    if (equalLettersIgnoringASCIICase(containerType, "video/vp8"_s)) {
        info.powerEfficient = false;
        info.smooth = isVPSoftwareDecoderSmooth(configuration);
    } else if (equalLettersIgnoringASCIICase(containerType, "video/vp9"_s)) {
        auto decodingInfo = computeVPParameters(configuration);
        if (decodingInfo && !decodingInfo->supported && isSupportingVP9HardwareDecoder()) {
            info.supported = false;
            return { info };
        }
        info.powerEfficient = decodingInfo ? decodingInfo->powerEfficient : isSupportingVP9HardwareDecoder();
        info.smooth = decodingInfo ? decodingInfo->smooth : isVPSoftwareDecoderSmooth(configuration);
    } else if (equalLettersIgnoringASCIICase(containerType, "video/h264"_s))
        info.powerEfficient = info.smooth = true;
    else if (equalLettersIgnoringASCIICase(containerType, "video/h265"_s))
        info.powerEfficient = info.smooth = true;
    else if (equalLettersIgnoringASCIICase(containerType, "video/av1"_s)) {
        // FIXME: Set value to true if AV1 is only enabled when HW decoder support is enabled.
        info.powerEfficient = false;
    }

    info.supported = true;
    return { info };
}

std::optional<PlatformMediaCapabilitiesEncodingInfo> LibWebRTCProvider::videoEncodingCapabilitiesOverride(const PlatformMediaCapabilitiesVideoConfiguration& configuration)
{
    PlatformMediaCapabilitiesEncodingInfo info;
    ContentType contentType { configuration.contentType };
    auto containerType = contentType.containerType();
    if (equalLettersIgnoringASCIICase(containerType, "video/vp8"_s) || equalLettersIgnoringASCIICase(containerType, "video/vp9"_s))
        info.powerEfficient = info.smooth = isVPXEncoderSmooth(configuration);
    else if (equalLettersIgnoringASCIICase(containerType, "video/h264"_s))
        info.powerEfficient = info.smooth = isH264EncoderSmooth(configuration);
    else if (equalLettersIgnoringASCIICase(containerType, "video/h265"_s))
        info.powerEfficient = info.smooth = true;
    else if (equalLettersIgnoringASCIICase(containerType, "video/av1"_s))
        info.powerEfficient = info.smooth = false;

    info.supported = true;
    info.configuration.type = PlatformMediaEncodingType::WebRTC;
    return { info };
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
