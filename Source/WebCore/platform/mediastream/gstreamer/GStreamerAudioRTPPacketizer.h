/*
 *  Copyright (C) 2024 Igalia S.L. All rights reserved.
 *  Copyright (C) 2024 Metrological Group B.V.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#if USE(GSTREAMER_WEBRTC)

#include "GStreamerRTPPacketizer.h"
#include "GStreamerWebRTCUtils.h"

namespace WebCore {

class GStreamerAudioRTPPacketizer final : public GStreamerRTPPacketizer {
public:
    static RefPtr<GStreamerAudioRTPPacketizer> create(RefPtr<UniqueSSRCGenerator>, const GstStructure* codecParameters, GUniquePtr<GstStructure>&& encodingParameters);

private:
    explicit GStreamerAudioRTPPacketizer(GRefPtr<GstCaps>&& inputCaps, GRefPtr<GstElement>&& encoder, GRefPtr<GstElement>&& payloader, GUniquePtr<GstStructure>&& encodingParameters, GRefPtr<GstCaps>&& rtpCaps);

    GRefPtr<GstElement> m_audioconvert;
    GRefPtr<GstElement> m_audioresample;
    GRefPtr<GstElement> m_inputCapsFilter;
    GRefPtr<GstCaps> m_inputCaps;
};

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
