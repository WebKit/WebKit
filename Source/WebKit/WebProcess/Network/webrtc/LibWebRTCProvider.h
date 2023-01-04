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

// FIXME: We should likely rename this header file to WebRTCProvider.h because depending on the
// build configuration we create either a LibWebRTCProvider, or a GStreamerWebRTCProvider or
// fallback to WebRTCProvider. This rename would open another can of worms though, leading to the
// renaming of more LibWebRTC-prefixed files in WebKit.
// https://bugs.webkit.org/show_bug.cgi?id=243774

#if USE(LIBWEBRTC)

#if PLATFORM(COCOA)
#include <WebCore/LibWebRTCProviderCocoa.h>
#elif USE(GSTREAMER)
#include <WebCore/LibWebRTCProviderGStreamer.h>
#endif
#elif USE(GSTREAMER_WEBRTC)
#include <WebCore/GStreamerWebRTCProvider.h>
#else // !USE(LIBWEBRTC) && !USE(GSTREAMER_WEBRTC)
#include <WebCore/WebRTCProvider.h>
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

    rtc::scoped_refptr<webrtc::PeerConnectionInterface> createPeerConnection(WebCore::ScriptExecutionContextIdentifier, webrtc::PeerConnectionObserver&, rtc::PacketSocketFactory*, webrtc::PeerConnectionInterface::RTCConfiguration&&) final;

    void disableNonLocalhostConnections() final;
    void startedNetworkThread() final;
    RefPtr<WebCore::RTCDataChannelRemoteHandlerConnection> createRTCDataChannelRemoteHandlerConnection() final;
    void setLoggingLevel(WTFLogLevel) final;

    WebPage& m_webPage;
};

inline LibWebRTCProvider::LibWebRTCProvider(WebPage& webPage)
    : m_webPage(webPage)
{
    m_useNetworkThreadWithSocketServer = false;
    m_supportsMDNS = true;
}

inline UniqueRef<LibWebRTCProvider> createLibWebRTCProvider(WebPage& page)
{
    return makeUniqueRef<LibWebRTCProvider>(page);
}

#elif USE(GSTREAMER_WEBRTC)
using LibWebRTCProvider = WebCore::GStreamerWebRTCProvider;

inline UniqueRef<LibWebRTCProvider> createLibWebRTCProvider(WebPage&)
{
    return makeUniqueRef<LibWebRTCProvider>();
}

#else
using LibWebRTCProvider = WebCore::WebRTCProvider;

inline UniqueRef<LibWebRTCProvider> createLibWebRTCProvider(WebPage&)
{
    return makeUniqueRef<LibWebRTCProvider>();
}
#endif // USE(LIBWEBRTC)

} // namespace WebKit
