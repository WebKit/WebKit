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

#pragma once

#include "WebRTCProvider.h"

#if USE(GSTREAMER_WEBRTC)

namespace WebCore {

class WEBCORE_EXPORT GStreamerWebRTCProvider : public WebRTCProvider {
    WTF_MAKE_FAST_ALLOCATED;
public:
    std::optional<RTCRtpCapabilities> receiverCapabilities(const String& kind) final;
    std::optional<RTCRtpCapabilities> senderCapabilities(const String& kind) final;

private:
    void initializeAudioDecodingCapabilities() final;
    void initializeVideoDecodingCapabilities() final;
    void initializeAudioEncodingCapabilities() final;
    void initializeVideoEncodingCapabilities() final;
    std::optional<MediaCapabilitiesDecodingInfo> videoDecodingCapabilitiesOverride(const VideoConfiguration&) final;
};

} // namespace Webcore

#endif
