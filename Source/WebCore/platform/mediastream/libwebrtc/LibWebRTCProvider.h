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
#include <wtf/CompletionHandler.h>
#include <wtf/Expected.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/WTFString.h>

#if USE(LIBWEBRTC)

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/api/peer_connection_interface.h>
#include <webrtc/api/video_codecs/video_encoder_factory.h>
#include <webrtc/api/video_codecs/video_decoder_factory.h>
#include <webrtc/api/scoped_refptr.h>

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

    virtual void setActive(bool);

    virtual void setH264HardwareEncoderAllowed(bool) { }

    using IPAddressOrError = Expected<String, MDNSRegisterError>;
    using MDNSNameOrError = Expected<String, MDNSRegisterError>;

    virtual void unregisterMDNSNames(DocumentIdentifier) { }

    virtual void registerMDNSName(DocumentIdentifier, const String& ipAddress, CompletionHandler<void(MDNSNameOrError&&)>&& callback)
    {
        UNUSED_PARAM(ipAddress);
        callback(makeUnexpected(MDNSRegisterError::NotImplemented));
    }

#if USE(LIBWEBRTC)
    virtual rtc::scoped_refptr<webrtc::PeerConnectionInterface> createPeerConnection(webrtc::PeerConnectionObserver&, rtc::PacketSocketFactory*, webrtc::PeerConnectionInterface::RTCConfiguration&&);

    webrtc::PeerConnectionFactoryInterface* factory();

    // FIXME: Make these methods not static.
    static void callOnWebRTCNetworkThread(Function<void()>&&);
    static void callOnWebRTCSignalingThread(Function<void()>&&);
    static bool hasWebRTCThreads();

    // Used for mock testing
    void setPeerConnectionFactory(rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>&&);

    void disableEnumeratingAllNetworkInterfaces();
    void enableEnumeratingAllNetworkInterfaces();

    void setH265Support(bool value) { m_supportsH265 = value; }
    void setVP9Support(bool value) { m_supportsVP9 = value; }
    bool isSupportingH265() const { return m_supportsH265; }
    bool isSupportingVP9() const { return m_supportsVP9; }
    virtual void disableNonLocalhostConnections() { m_disableNonLocalhostConnections = true; }

    rtc::RTCCertificateGenerator& certificateGenerator();

    Optional<RTCRtpCapabilities> receiverCapabilities(const String& kind);
    Optional<RTCRtpCapabilities> senderCapabilities(const String& kind);

    void clearFactory() { m_factory = nullptr; }

    void setEnableLogging(bool);
    void setEnableWebRTCEncryption(bool);
    void setUseDTLS10(bool);

    class SuspendableSocketFactory : public rtc::PacketSocketFactory {
    public:
        virtual ~SuspendableSocketFactory() = default;
        virtual void suspend() { };
        virtual void resume() { };
    };
    virtual std::unique_ptr<SuspendableSocketFactory> createSocketFactory(String&& /* userAgent */) { return nullptr; }

protected:
    LibWebRTCProvider() = default;

    rtc::scoped_refptr<webrtc::PeerConnectionInterface> createPeerConnection(webrtc::PeerConnectionObserver&, rtc::NetworkManager&, rtc::PacketSocketFactory&, webrtc::PeerConnectionInterface::RTCConfiguration&&, std::unique_ptr<webrtc::AsyncResolverFactory>&&);

    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> createPeerConnectionFactory(rtc::Thread* networkThread, rtc::Thread* signalingThread, LibWebRTCAudioModule*);
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
    bool m_supportsVP9 { false };
    bool m_enableLogging { true };
    bool m_useDTLS10 { false };
#endif
};

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
