/*
 *  Copyright (C) 2017-2022 Igalia S.L. All rights reserved.
 *  Copyright (C) 2022 Metrological Group B.V.
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

#include "RealtimeOutgoingMediaSourceGStreamer.h"

namespace WebCore {

class RealtimeOutgoingAudioSourceGStreamer final : public RealtimeOutgoingMediaSourceGStreamer {
public:
    static Ref<RealtimeOutgoingAudioSourceGStreamer> create(const RefPtr<UniqueSSRCGenerator>& ssrcGenerator, const String& mediaStreamId, MediaStreamTrack& track) { return adoptRef(*new RealtimeOutgoingAudioSourceGStreamer(ssrcGenerator, mediaStreamId, track)); }

    bool setPayloadType(const GRefPtr<GstCaps>&) final;
    void setParameters(GUniquePtr<GstStructure>&&) final;
    void teardown() final;

protected:
    explicit RealtimeOutgoingAudioSourceGStreamer(const RefPtr<UniqueSSRCGenerator>&, const String& mediaStreamId, MediaStreamTrack&);

private:
    RTCRtpCapabilities rtpCapabilities() const final;

    void connectFallbackSource() final;
    void unlinkOutgoingSource() final;
    void linkOutgoingSource() final;

    GRefPtr<GstElement> m_audioconvert;
    GRefPtr<GstElement> m_audioresample;
    GRefPtr<GstElement> m_inputCapsFilter;
    GRefPtr<GstCaps> m_inputCaps;
};

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
