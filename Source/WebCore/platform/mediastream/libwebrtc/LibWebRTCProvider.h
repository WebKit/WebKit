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

#include "LibWebRTCMacros.h"
#include <pal/SessionID.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Expected.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/WTFString.h>

#if USE(LIBWEBRTC)

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/api/peerconnectioninterface.h>
#include <webrtc/api/video_codecs/video_encoder_factory.h>
#include <webrtc/api/video_codecs/video_decoder_factory.h>
#include <webrtc/rtc_base/scoped_ref_ptr.h>

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
struct RTCRtpCapabilities;

enum class MDNSRegisterError { NotImplemented, BadParameter, DNSSD, Internal, Timeout };

class WEBCORE_EXPORT LibWebRTCProvider {
public:
    static UniqueRef<LibWebRTCProvider> create();

    virtual ~LibWebRTCProvider() = default;

    static bool webRTCAvailable();

    virtual void setActive(bool);

    virtual void setH264HardwareEncoderAllowed(bool) { }

    using IPAddressOrError = Expected<String, MDNSRegisterError>;
    using MDNSNameOrError = Expected<String, MDNSRegisterError>;

    virtual void unregisterMDNSNames(uint64_t documentIdentifier)
    {
        UNUSED_PARAM(documentIdentifier);
    }

    virtual void registerMDNSName(PAL::SessionID, uint64_t documentIdentifier, const String& ipAddress, CompletionHandler<void(MDNSNameOrError&&)>&& callback)
    {
        UNUSED_PARAM(documentIdentifier);
        UNUSED_PARAM(ipAddress);
        callback(makeUnexpected(MDNSRegisterError::NotImplemented));
    }

#if USE(LIBWEBRTC)
    virtual rtc::scoped_refptr<webrtc::PeerConnectionInterface> createPeerConnection(webrtc::PeerConnectionObserver&, webrtc::PeerConnectionInterface::RTCConfiguration&&);

    webrtc::PeerConnectionFactoryInterface* factory();

    // FIXME: Make these methods not static.
    static void callOnWebRTCNetworkThread(Function<void()>&&);
    static void callOnWebRTCSignalingThread(Function<void()>&&);

    // Used for mock testing
    void setPeerConnectionFactory(rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>&&);

    void disableEnumeratingAllNetworkInterfaces();
    void enableEnumeratingAllNetworkInterfaces();

    void supportsVP8(bool value) { m_supportsVP8 = value; }
    virtual void disableNonLocalhostConnections() { m_disableNonLocalhostConnections = true; }

    rtc::RTCCertificateGenerator& certificateGenerator();

    Optional<RTCRtpCapabilities> receiverCapabilities(const String& kind);
    Optional<RTCRtpCapabilities> senderCapabilities(const String& kind);

protected:
    LibWebRTCProvider() = default;

    rtc::scoped_refptr<webrtc::PeerConnectionInterface> createPeerConnection(webrtc::PeerConnectionObserver&, rtc::NetworkManager&, rtc::PacketSocketFactory&, webrtc::PeerConnectionInterface::RTCConfiguration&&, std::unique_ptr<webrtc::AsyncResolverFactory>&&);

    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> createPeerConnectionFactory(rtc::Thread* networkThread, rtc::Thread* signalingThread, LibWebRTCAudioModule*);
    virtual std::unique_ptr<webrtc::VideoDecoderFactory> createDecoderFactory();
    virtual std::unique_ptr<webrtc::VideoEncoderFactory> createEncoderFactory();

    bool m_enableEnumeratingAllNetworkInterfaces { false };
    // FIXME: Remove m_useNetworkThreadWithSocketServer member variable and make it a global.
    bool m_useNetworkThreadWithSocketServer { true };

    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> m_factory;
    bool m_disableNonLocalhostConnections { false };
    bool m_supportsVP8 { false };
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
