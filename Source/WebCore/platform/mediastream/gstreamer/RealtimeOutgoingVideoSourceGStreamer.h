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

#include "GUniquePtrGStreamer.h"
#include "RealtimeOutgoingMediaSourceGStreamer.h"

namespace WebCore {

class RealtimeOutgoingVideoSourceGStreamer final : public RealtimeOutgoingMediaSourceGStreamer {
public:
    static Ref<RealtimeOutgoingVideoSourceGStreamer> create(const String& mediaStreamId, MediaStreamTrack& track) { return adoptRef(*new RealtimeOutgoingVideoSourceGStreamer(mediaStreamId, track)); }

    void setApplyRotation(bool shouldApplyRotation) { m_shouldApplyRotation = shouldApplyRotation; }

    bool setPayloadType(const GRefPtr<GstCaps>&) final;

    const GstStructure* stats() const { return m_stats.get(); }

protected:
    explicit RealtimeOutgoingVideoSourceGStreamer(const String& mediaStreamId, MediaStreamTrack&);

    bool m_shouldApplyRotation { false };

private:
    void codecPreferencesChanged(const GRefPtr<GstCaps>&) final;
    RTCRtpCapabilities rtpCapabilities() const final;

    void updateStats(GstBuffer*);

    GRefPtr<GstElement> m_videoConvert;
    GRefPtr<GstElement> m_videoFlip;
    GUniquePtr<GstStructure> m_stats;
};

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
