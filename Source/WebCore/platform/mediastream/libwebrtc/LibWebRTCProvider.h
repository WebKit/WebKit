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
#include <wtf/Forward.h>

#if USE(LIBWEBRTC)

#include <webrtc/api/peerconnectioninterface.h>
#include <webrtc/base/scoped_ref_ptr.h>

namespace rtc {
class NetworkManager;
class PacketSocketFactory;
}

namespace webrtc {
class PeerConnectionFactoryInterface;
}
#endif

namespace WebCore {

class WEBCORE_EXPORT LibWebRTCProvider {
public:
    LibWebRTCProvider() = default;
    virtual ~LibWebRTCProvider() = default;

    static bool webRTCAvailable();
#if USE(LIBWEBRTC)
    WEBCORE_EXPORT virtual rtc::scoped_refptr<webrtc::PeerConnectionInterface> createPeerConnection(webrtc::PeerConnectionObserver&, webrtc::PeerConnectionInterface::RTCConfiguration&&);

    WEBCORE_EXPORT webrtc::PeerConnectionFactoryInterface* factory();

    // FIXME: Make these methods not static.
    static WEBCORE_EXPORT void callOnWebRTCNetworkThread(Function<void()>&&);
    static WEBCORE_EXPORT void callOnWebRTCSignalingThread(Function<void()>&&);
    static WEBCORE_EXPORT void setDecoderFactoryGetter(Function<std::unique_ptr<cricket::WebRtcVideoDecoderFactory>()>&&);
    static WEBCORE_EXPORT void setEncoderFactoryGetter(Function<std::unique_ptr<cricket::WebRtcVideoEncoderFactory>()>&&);

    // Used for mock testing
    static void setPeerConnectionFactory(rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>&&);

    void disableEnumeratingAllNetworkInterfaces() { m_enableEnumeratingAllNetworkInterfaces = false; }
    void enableEnumeratingAllNetworkInterfaces() { m_enableEnumeratingAllNetworkInterfaces = true; }

protected:
    WEBCORE_EXPORT rtc::scoped_refptr<webrtc::PeerConnectionInterface> createPeerConnection(webrtc::PeerConnectionObserver&, rtc::NetworkManager&, rtc::PacketSocketFactory&, webrtc::PeerConnectionInterface::RTCConfiguration&&);

    bool m_enableEnumeratingAllNetworkInterfaces { false };
    bool m_useNetworkThreadWithSocketServer { true };
#endif
};

} // namespace WebCore
