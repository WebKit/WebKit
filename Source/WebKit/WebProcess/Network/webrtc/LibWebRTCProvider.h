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

#if USE(LIBWEBRTC) && PLATFORM(COCOA)
#include <WebCore/LibWebRTCProviderCocoa.h>
#elif USE(LIBWEBRTC) && USE(GSTREAMER)
#include <WebCore/LibWebRTCProviderGStreamer.h>
#else
#include <WebCore/LibWebRTCProvider.h>
#endif

namespace WebKit {

class WebPage;

#if USE(LIBWEBRTC)

#if PLATFORM(COCOA)
using LibWebRTCProviderBase = WebCore::LibWebRTCProviderCocoa;
#elif USE(GSTREAMER)
using LibWebRTCProviderBase = WebCore::LibWebRTCProviderGStreamer;
#else
using LibWebRTCProviderBase = WebCore::LibWebRTCProvider;
#endif

class LibWebRTCProvider final : public LibWebRTCProviderBase {
public:
    explicit LibWebRTCProvider(WebPage&);

private:
    std::unique_ptr<SuspendableSocketFactory> createSocketFactory(String&& /* userAgent */, bool /* isFirstParty */, WebCore::RegistrableDomain&&) final;

    rtc::scoped_refptr<webrtc::PeerConnectionInterface> createPeerConnection(webrtc::PeerConnectionObserver&, rtc::PacketSocketFactory*, webrtc::PeerConnectionInterface::RTCConfiguration&&) final;

    void unregisterMDNSNames(WebCore::DocumentIdentifier) final;
    void registerMDNSName(WebCore::DocumentIdentifier, const String& ipAddress, CompletionHandler<void(MDNSNameOrError&&)>&&) final;
    void disableNonLocalhostConnections() final;
    void startedNetworkThread() final;
    RefPtr<WebCore::RTCDataChannelRemoteHandlerConnection> createRTCDataChannelRemoteHandlerConnection() final;
    void setLoggingLevel(WTFLogLevel) final;

    WebPage& m_webPage;
#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA) && !PLATFORM(MACCATALYST)
    bool m_didInitializeCallback { false };
#endif
};

inline LibWebRTCProvider::LibWebRTCProvider(WebPage& webPage)
    : m_webPage(webPage)
{
    m_useNetworkThreadWithSocketServer = false;
}

inline UniqueRef<LibWebRTCProvider> createLibWebRTCProvider(WebPage& page)
{
    return makeUniqueRef<LibWebRTCProvider>(page);
}
#else
using LibWebRTCProvider = WebCore::LibWebRTCProvider;

inline UniqueRef<LibWebRTCProvider> createLibWebRTCProvider(WebPage&)
{
    return makeUniqueRef<LibWebRTCProvider>();
}
#endif // USE(LIBWEBRTC)

} // namespace WebKit
