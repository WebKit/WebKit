/*
 * Copyright (C) 2022 Metrological Group B.V.
 * Author: Philippe Normand <philn@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if USE(GSTREAMER_WEBRTC)
#include "GStreamerWebRTCProvider.h"

#include "ContentType.h"
#include "GStreamerRegistryScanner.h"
#include "MediaCapabilitiesDecodingInfo.h"
#include "MediaCapabilitiesEncodingInfo.h"
#include "MediaDecodingConfiguration.h"
#include "MediaEncodingConfiguration.h"
#include "NotImplemented.h"

namespace WebCore {

void WebRTCProvider::setH264HardwareEncoderAllowed(bool)
{
    // TODO: Hook this into GStreamerRegistryScanner.
    notImplemented();
}

UniqueRef<WebRTCProvider> WebRTCProvider::create()
{
    return makeUniqueRef<GStreamerWebRTCProvider>();
}

bool WebRTCProvider::webRTCAvailable()
{
    return true;
}

void WebRTCProvider::setActive(bool)
{
    notImplemented();
}

std::optional<RTCRtpCapabilities> GStreamerWebRTCProvider::receiverCapabilities(const String& kind)
{
    if (kind == "audio"_s)
        return audioDecodingCapabilities();
    if (kind == "video"_s)
        return videoDecodingCapabilities();

    return { };
}

std::optional<RTCRtpCapabilities> GStreamerWebRTCProvider::senderCapabilities(const String& kind)
{
    if (kind == "audio"_s)
        return audioEncodingCapabilities();
    if (kind == "video"_s)
        return videoEncodingCapabilities();
    return { };
}

void GStreamerWebRTCProvider::initializeAudioEncodingCapabilities()
{
    m_audioEncodingCapabilities = GStreamerRegistryScanner::singleton().audioRtpCapabilities(GStreamerRegistryScanner::Configuration::Encoding);
}

void GStreamerWebRTCProvider::initializeVideoEncodingCapabilities()
{
    ensureGStreamerInitialized();
    registerWebKitGStreamerVideoEncoder();
    m_videoEncodingCapabilities = GStreamerRegistryScanner::singleton().videoRtpCapabilities(GStreamerRegistryScanner::Configuration::Encoding);
    m_videoEncodingCapabilities->codecs.removeAllMatching([isSupportingVP9Profile0 = isSupportingVP9Profile0(), isSupportingVP9Profile2 = isSupportingVP9Profile2(), isSupportingH265 = isSupportingH265()](const auto& codec) {
        if (!isSupportingVP9Profile0 && codec.sdpFmtpLine == "profile-id=0"_s)
            return true;
        if (!isSupportingVP9Profile2 && codec.sdpFmtpLine == "profile-id=2"_s)
            return true;
        if (!isSupportingH265 && codec.mimeType == "video/H265"_s)
            return true;

        return false;
    });
}

void GStreamerWebRTCProvider::initializeAudioDecodingCapabilities()
{
    m_audioDecodingCapabilities = GStreamerRegistryScanner::singleton().audioRtpCapabilities(GStreamerRegistryScanner::Configuration::Decoding);
}

void GStreamerWebRTCProvider::initializeVideoDecodingCapabilities()
{
    m_videoDecodingCapabilities = GStreamerRegistryScanner::singleton().videoRtpCapabilities(GStreamerRegistryScanner::Configuration::Decoding);
    m_videoDecodingCapabilities->codecs.removeAllMatching([isSupportingVP9Profile0 = isSupportingVP9Profile0(), isSupportingVP9Profile2 = isSupportingVP9Profile2(), isSupportingH265 = isSupportingH265()](const auto& codec) {
        if (!isSupportingVP9Profile0 && codec.sdpFmtpLine == "profile-id=0"_s)
            return true;
        if (!isSupportingVP9Profile2 && codec.sdpFmtpLine == "profile-id=2"_s)
            return true;
        if (!isSupportingH265 && codec.mimeType == "video/H265"_s)
            return true;

        return false;
    });
}

std::optional<MediaCapabilitiesDecodingInfo> GStreamerWebRTCProvider::videoDecodingCapabilitiesOverride(const VideoConfiguration& configuration)
{
    MediaCapabilitiesDecodingInfo info;
    ContentType contentType { configuration.contentType };
    auto containerType = contentType.containerType();
    if (equalLettersIgnoringASCIICase(containerType, "video/vp8"_s)) {
        info.powerEfficient = false;
        info.smooth = isVPSoftwareDecoderSmooth(configuration);
    } else if (equalLettersIgnoringASCIICase(containerType, "video/vp9"_s)) {
        auto decodingInfo = computeVPParameters(configuration);
        info.powerEfficient = decodingInfo ? decodingInfo->powerEfficient : true;
        info.smooth = decodingInfo ? decodingInfo->smooth : isVPSoftwareDecoderSmooth(configuration);
    } else {
        // FIXME: Provide more granular H.264 decoder information.
        info.powerEfficient = true;
        info.smooth = true;
    }
    info.supported = true;
    return { info };
}

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
