/*
 *  Copyright (C) 2024 Igalia S.L.
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

#include "GStreamerMediaEndpoint.h"
#include "GStreamerWebRTCCommon.h"
#include "GUniquePtrGStreamer.h"
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class GStreamerIncomingTrackProcessor : public RefCounted<GStreamerIncomingTrackProcessor> {
    WTF_MAKE_TZONE_ALLOCATED(GStreamerIncomingTrackProcessor);

public:
    static Ref<GStreamerIncomingTrackProcessor> create()
    {
        return adoptRef(*new GStreamerIncomingTrackProcessor());
    }
    ~GStreamerIncomingTrackProcessor() = default;

    void configure(ThreadSafeWeakPtr<GStreamerMediaEndpoint>&&, GRefPtr<GstPad>&&);
    const GRefPtr<GstPad>& pad() const { return m_pad; }

    GstElement* bin() const { return m_bin.get(); }

    const GstStructure* stats();

    bool isDecoding() const { return m_isDecoding; }
    bool isReady() const { return m_isReady; }
    const String& trackId() const { return m_data.trackId; }

private:
    GStreamerIncomingTrackProcessor();

    void retrieveMediaStreamAndTrackIdFromSDP();
    String mediaStreamIdFromPad();

    GRefPtr<GstElement> incomingTrackProcessor();
    GRefPtr<GstElement> createParser();

    void installRtpBufferPadProbe(const GRefPtr<GstPad>&);

    void trackReady();

    ThreadSafeWeakPtr<GStreamerMediaEndpoint> m_endPoint;
    GRefPtr<GstPad> m_pad;
    GRefPtr<GstElement> m_bin;
    WebRTCTrackData m_data;

    std::pair<String, String> m_sdpMsIdAndTrackId;

    bool m_isDecoding { false };
    IntSize m_videoSize;
    double m_frameRate;
    uint64_t m_decodedVideoFrames { 0 };
    GRefPtr<GstElement> m_sink;
    GUniquePtr<GstStructure> m_stats;
    bool m_isReady { false };

    // https://www.w3.org/TR/webrtc-stats/#dom-rtcinboundrtpstreamstats-totaldecodetime
    MediaTime m_totalVideoDecodeTime { MediaTime::zeroTime() };
};

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
