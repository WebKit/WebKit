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
    UNUSED_PARAM(kind);
    return { };
}

std::optional<RTCRtpCapabilities> GStreamerWebRTCProvider::senderCapabilities(const String& kind)
{
    UNUSED_PARAM(kind);
    return { };
}

void GStreamerWebRTCProvider::initializeAudioEncodingCapabilities()
{
}

void GStreamerWebRTCProvider::initializeVideoEncodingCapabilities()
{
}

void GStreamerWebRTCProvider::initializeAudioDecodingCapabilities()
{
}

void GStreamerWebRTCProvider::initializeVideoDecodingCapabilities()
{
}

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
