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
#include "WebRTCProvider.h"

#include "ContentType.h"
#include "MediaCapabilitiesDecodingInfo.h"
#include "MediaCapabilitiesEncodingInfo.h"
#include "MediaDecodingConfiguration.h"
#include "MediaEncodingConfiguration.h"

#include <wtf/Function.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

#if !USE(LIBWEBRTC) && !USE(GSTREAMER_WEBRTC)
UniqueRef<WebRTCProvider> WebRTCProvider::create()
{
    return makeUniqueRef<WebRTCProvider>();
}

bool WebRTCProvider::webRTCAvailable()
{
    return false;
}

void WebRTCProvider::setActive(bool)
{
}

void WebRTCProvider::setH264HardwareEncoderAllowed(bool)
{
}

#endif

RefPtr<RTCDataChannelRemoteHandlerConnection> WebRTCProvider::createRTCDataChannelRemoteHandlerConnection()
{
    return nullptr;
}

void WebRTCProvider::setH265Support(bool value)
{
    m_supportsH265 = value;
#if ENABLE(WEB_RTC)
    m_videoDecodingCapabilities = { };
    m_videoEncodingCapabilities = { };
#endif
}

void WebRTCProvider::setVP9Support(bool supportsVP9Profile0, bool supportsVP9Profile2)
{
    m_supportsVP9Profile0 = supportsVP9Profile0;
    m_supportsVP9Profile2 = supportsVP9Profile2;

#if ENABLE(WEB_RTC)
    m_videoDecodingCapabilities = { };
    m_videoEncodingCapabilities = { };
#endif
}

void WebRTCProvider::setAV1Support(bool supportsAV1)
{
    m_supportsAV1 = supportsAV1;

#if ENABLE(WEB_RTC)
    m_videoDecodingCapabilities = { };
    m_videoEncodingCapabilities = { };
#endif
}

bool WebRTCProvider::isSupportingAV1() const
{
    return m_supportsAV1;
}

bool WebRTCProvider::isSupportingH265() const
{
    return m_supportsH265;
}

bool WebRTCProvider::isSupportingVP9Profile0() const
{
    return m_supportsVP9Profile0;
}

bool WebRTCProvider::isSupportingVP9Profile2() const
{
    return m_supportsVP9Profile2;
}

bool WebRTCProvider::isSupportingMDNS() const
{
    return m_supportsMDNS;
}

void WebRTCProvider::setLoggingLevel(WTFLogLevel)
{

}

void WebRTCProvider::clearFactory()
{

}

#if ENABLE(WEB_RTC)

std::optional<RTCRtpCapabilities> WebRTCProvider::receiverCapabilities(const String&)
{
    return { };
}

std::optional<RTCRtpCapabilities> WebRTCProvider::senderCapabilities(const String&)
{
    return { };
}

std::optional<RTCRtpCodecCapability> WebRTCProvider::codecCapability(const ContentType& contentType, const std::optional<RTCRtpCapabilities>& capabilities)
{
    if (!capabilities)
        return { };

    auto containerType = contentType.containerType();
    for (auto& codec : capabilities->codecs) {
        if (equalIgnoringASCIICase(containerType, codec.mimeType))
            return codec;
    }
    return { };
}

std::optional<RTCRtpCapabilities>& WebRTCProvider::audioDecodingCapabilities()
{
    if (!m_audioDecodingCapabilities)
        initializeAudioDecodingCapabilities();
    return m_audioDecodingCapabilities;
}

std::optional<RTCRtpCapabilities>& WebRTCProvider::videoDecodingCapabilities()
{
    if (!m_videoDecodingCapabilities)
        initializeVideoDecodingCapabilities();
    return m_videoDecodingCapabilities;
}

std::optional<RTCRtpCapabilities>& WebRTCProvider::audioEncodingCapabilities()
{
    if (!m_audioEncodingCapabilities)
        initializeAudioEncodingCapabilities();
    return m_audioEncodingCapabilities;
}

std::optional<RTCRtpCapabilities>& WebRTCProvider::videoEncodingCapabilities()
{
    if (!m_videoEncodingCapabilities)
        initializeVideoEncodingCapabilities();
    return m_videoEncodingCapabilities;
}

#endif // ENABLE(WEB_RTC)

std::optional<MediaCapabilitiesInfo> WebRTCProvider::computeVPParameters(const VideoConfiguration&)
{
    return { };
}

bool WebRTCProvider::isVPSoftwareDecoderSmooth(const VideoConfiguration&)
{
    return true;
}

bool WebRTCProvider::isVPXEncoderSmooth(const VideoConfiguration&)
{
    return false;
}

bool WebRTCProvider::isH264EncoderSmooth(const VideoConfiguration&)
{
    return true;
}

void WebRTCProvider::createDecodingConfiguration(MediaDecodingConfiguration&& configuration, DecodingConfigurationCallback&& callback)
{
    ASSERT(configuration.type == MediaDecodingType::WebRTC);

    // FIXME: Validate additional parameters, in particular mime type parameters.
    MediaCapabilitiesDecodingInfo info { WTFMove(configuration) };

#if ENABLE(WEB_RTC)
    if (info.supportedConfiguration.video) {
        ContentType contentType { info.supportedConfiguration.video->contentType };
        auto codec = codecCapability(contentType, videoDecodingCapabilities());
        if (!codec) {
            callback({ });
            return;
        }
        if (auto infoOverride = videoDecodingCapabilitiesOverride(*info.supportedConfiguration.video)) {
            if (!infoOverride->supported) {
                callback({ });
                return;
            }
            info.smooth = infoOverride->smooth;
            info.powerEfficient = infoOverride->powerEfficient;
        }
    }
    if (info.supportedConfiguration.audio) {
        ContentType contentType { info.supportedConfiguration.audio->contentType };
        auto codec = codecCapability(contentType, audioDecodingCapabilities());
        if (!codec) {
            callback({ });
            return;
        }
    }
#endif
    info.supported = true;
    callback(WTFMove(info));
}

void WebRTCProvider::createEncodingConfiguration(MediaEncodingConfiguration&& configuration, EncodingConfigurationCallback&& callback)
{
    ASSERT(configuration.type == MediaEncodingType::WebRTC);

    // FIXME: Validate additional parameters, in particular mime type parameters.
    MediaCapabilitiesEncodingInfo info { WTFMove(configuration) };

#if ENABLE(WEB_RTC)
    if (info.supportedConfiguration.video) {
        ContentType contentType { info.supportedConfiguration.video->contentType };
        auto codec = codecCapability(contentType, videoEncodingCapabilities());
        if (!codec) {
            callback({ });
            return;
        }
        if (auto infoOverride = videoEncodingCapabilitiesOverride(*info.supportedConfiguration.video)) {
            if (!infoOverride->supported) {
                callback({ });
                return;
            }
            info.smooth = infoOverride->smooth;
            info.powerEfficient = infoOverride->powerEfficient;
        }
    }
    if (info.supportedConfiguration.audio) {
        ContentType contentType { info.supportedConfiguration.audio->contentType };
        auto codec = codecCapability(contentType, audioEncodingCapabilities());
        if (!codec) {
            callback({ });
            return;
        }
    }
#endif
    info.supported = true;
    callback(WTFMove(info));
}

void WebRTCProvider::initializeAudioDecodingCapabilities()
{

}

void WebRTCProvider::initializeVideoDecodingCapabilities()
{

}

void WebRTCProvider::initializeAudioEncodingCapabilities()
{

}

void WebRTCProvider::initializeVideoEncodingCapabilities()
{

}

std::optional<MediaCapabilitiesDecodingInfo> WebRTCProvider::videoDecodingCapabilitiesOverride(const VideoConfiguration&)
{
    return { };
}

std::optional<MediaCapabilitiesEncodingInfo> WebRTCProvider::videoEncodingCapabilitiesOverride(const VideoConfiguration&)
{
    return { };
}

} // namespace WebCore
