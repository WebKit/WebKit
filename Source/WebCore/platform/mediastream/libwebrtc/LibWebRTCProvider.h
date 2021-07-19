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

#include "DocumentIdentifier.h"
#include "LibWebRTCMacros.h"
#include "RTCDataChannelRemoteHandlerConnection.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Expected.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/WTFString.h>

#if USE(LIBWEBRTC)

ALLOW_UNUSED_PARAMETERS_BEGIN
ALLOW_DEPRECATED_DECLARATIONS_BEGIN

#include <webrtc/api/peer_connection_interface.h>
#include <webrtc/api/video_codecs/video_encoder_factory.h>
#include <webrtc/api/video_codecs/video_decoder_factory.h>
#include <webrtc/api/scoped_refptr.h>

ALLOW_DEPRECATED_DECLARATIONS_END
ALLOW_UNUSED_PARAMETERS_END

namespace rtc {
class NetworkManager;
class PacketSocketFactory;
class Thread;
class RTCCertificateGenerator;
}

namespace webrtc {
class AsyncResolverFactory;
class PeerConnectionFactoryInterface;
}
#endif

namespace WebCore {

class LibWebRTCAudioModule;
class RegistrableDomain;
struct PeerConnectionFactoryAndThreads;
struct RTCRtpCapabilities;

enum class MDNSRegisterError { NotImplemented, BadParameter, DNSSD, Internal, Timeout };

class WEBCORE_EXPORT LibWebRTCProvider {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static UniqueRef<LibWebRTCProvider> create();

    virtual ~LibWebRTCProvider() = default;

    static bool webRTCAvailable();
    static void registerWebKitVP9Decoder();
    static void registerWebKitVP8Decoder();
    static void setH264HardwareEncoderAllowed(bool);
    static void setRTCLogging(WTFLogLevel);

    virtual void setActive(bool);

    using IPAddressOrError = Expected<String, MDNSRegisterError>;
    using MDNSNameOrError = Expected<String, MDNSRegisterError>;

    virtual void unregisterMDNSNames(DocumentIdentifier) { }

    virtual void registerMDNSName(DocumentIdentifier, const String& ipAddress, CompletionHandler<void(MDNSNameOrError&&)>&& callback)
    {
        UNUSED_PARAM(ipAddress);
        callback(makeUnexpected(MDNSRegisterError::NotImplemented));
    }

    virtual RefPtr<RTCDataChannelRemoteHandlerConnection> createRTCDataChannelRemoteHandlerConnection() { return nullptr; }

#if USE(LIBWEBRTC)
    virtual rtc::scoped_refptr<webrtc::PeerConnectionInterface> createPeerConnection(webrtc::PeerConnectionObserver&, rtc::PacketSocketFactory*, webrtc::PeerConnectionInterface::RTCConfiguration&&);

    webrtc::PeerConnectionFactoryInterface* factory();

    // FIXME: Make these methods not static.
    static void callOnWebRTCNetworkThread(Function<void()>&&);
    static void callOnWebRTCSignalingThread(Function<void()>&&);
    static bool hasWebRTCThreads();
    static rtc::Thread& signalingThread();

    // Used for mock testing
    void setPeerConnectionFactory(rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>&&);

    void disableEnumeratingAllNetworkInterfaces();
    void enableEnumeratingAllNetworkInterfaces();
    bool isEnumeratingAllNetworkInterfacesEnabled() const { return m_enableEnumeratingAllNetworkInterfaces; }

    void setH265Support(bool value) { m_supportsH265 = value; }
    void setVP9Support(bool supportsVP9Profile0, bool supportsVP9Profile2);
    void setVP9VTBSupport(bool value) { m_supportsVP9VTB = value; }
    bool isSupportingH265() const { return m_supportsH265; }
    bool isSupportingVP9Profile0() const { return m_supportsVP9Profile0; }
    bool isSupportingVP9Profile2() const { return m_supportsVP9Profile2; }
    bool isSupportingVP9VTB() const { return m_supportsVP9VTB; }
    virtual void disableNonLocalhostConnections() { m_disableNonLocalhostConnections = true; }

    // Callback is executed on a background thread.
    void prepareCertificateGenerator(Function<void(rtc::RTCCertificateGenerator&)>&&);

    std::optional<RTCRtpCapabilities> receiverCapabilities(const String& kind);
    std::optional<RTCRtpCapabilities> senderCapabilities(const String& kind);

    void clearFactory() { m_factory = nullptr; }

    virtual void setLoggingLevel(WTFLogLevel);
    void setEnableWebRTCEncryption(bool);
    void setUseDTLS10(bool);

    class SuspendableSocketFactory : public rtc::PacketSocketFactory {
    public:
        virtual ~SuspendableSocketFactory() = default;
        virtual void suspend() { };
        virtual void resume() { };
        virtual void disableRelay() { };
    };
    virtual std::unique_ptr<SuspendableSocketFactory> createSocketFactory(String&& /* userAgent */, bool /* isFirstParty */, RegistrableDomain&&) { return nullptr; }

protected:
    LibWebRTCProvider() = default;

    rtc::scoped_refptr<webrtc::PeerConnectionInterface> createPeerConnection(webrtc::PeerConnectionObserver&, rtc::NetworkManager&, rtc::PacketSocketFactory&, webrtc::PeerConnectionInterface::RTCConfiguration&&, std::unique_ptr<webrtc::AsyncResolverFactory>&&);

    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> createPeerConnectionFactory(rtc::Thread* networkThread, rtc::Thread* signalingThread);
    virtual std::unique_ptr<webrtc::VideoDecoderFactory> createDecoderFactory();
    virtual std::unique_ptr<webrtc::VideoEncoderFactory> createEncoderFactory();

    virtual void startedNetworkThread() { };

    PeerConnectionFactoryAndThreads& getStaticFactoryAndThreads(bool useNetworkThreadWithSocketServer);

    bool m_enableEnumeratingAllNetworkInterfaces { false };
    // FIXME: Remove m_useNetworkThreadWithSocketServer member variable and make it a global.
    bool m_useNetworkThreadWithSocketServer { true };

    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> m_factory;
    bool m_disableNonLocalhostConnections { false };
    bool m_supportsH265 { false };
    bool m_supportsVP9Profile0 { false };
    bool m_supportsVP9Profile2 { false };
    bool m_supportsVP9VTB { false };
    bool m_useDTLS10 { false };
#endif
};

#if USE(LIBWEBRTC)
inline void LibWebRTCProvider::setVP9Support(bool supportsVP9Profile0, bool supportsVP9Profile2)
{
    m_supportsVP9Profile0 = supportsVP9Profile0;
    m_supportsVP9Profile2 = supportsVP9Profile2;
}
#endif

} // namespace WebCore

namespace WTF {
template<> struct EnumTraits<WebCore::MDNSRegisterError> {
    using values = EnumValues<
        WebCore::MDNSRegisterError,
        WebCore::MDNSRegisterError::NotImplemented,
        WebCore::MDNSRegisterError::BadParameter,
        WebCore::MDNSRegisterError::DNSSD,
        WebCore::MDNSRegisterError::Internal,
        WebCore::MDNSRegisterError::Timeout
    >;
};
}
