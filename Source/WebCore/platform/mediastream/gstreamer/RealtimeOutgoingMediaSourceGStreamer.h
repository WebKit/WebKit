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

#include "GRefPtrGStreamer.h"
#include "MediaStreamTrackPrivate.h"
#include "RTCRtpCapabilities.h"

#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class MediaStreamTrack;

class RealtimeOutgoingMediaSourceGStreamer : public ThreadSafeRefCounted<RealtimeOutgoingMediaSourceGStreamer>, public MediaStreamTrackPrivate::Observer {
public:
    ~RealtimeOutgoingMediaSourceGStreamer();

    void start();
    void stop();
    void setSource(Ref<MediaStreamTrackPrivate>&&);
    MediaStreamTrackPrivate& source() const { return m_source->get(); }

    const String& mediaStreamID() const { return m_mediaStreamId; }
    const GRefPtr<GstCaps>& allowedCaps() const;

    void link();
    const GRefPtr<GstPad>& pad() const { return m_webrtcSinkPad; }
    void setSinkPad(GRefPtr<GstPad>&&);

    GRefPtr<GstWebRTCRTPSender> sender() const { return m_sender; }
    GRefPtr<GstElement> bin() const { return m_bin; }

    virtual bool setPayloadType(const GRefPtr<GstCaps>&) { return false; }

protected:
    explicit RealtimeOutgoingMediaSourceGStreamer(const String& mediaStreamId, MediaStreamTrack&);

    void initializeFromTrack();

    String m_mediaStreamId;
    String m_trackId;

    bool m_enabled { true };
    bool m_muted { false };
    bool m_isStopped { false };
    std::optional<Ref<MediaStreamTrackPrivate>> m_source;
    std::optional<RealtimeMediaSourceSettings> m_initialSettings;
    GRefPtr<GstElement> m_bin;
    GRefPtr<GstElement> m_outgoingSource;
    GRefPtr<GstElement> m_valve;
    GRefPtr<GstElement> m_preEncoderQueue;
    GRefPtr<GstElement> m_encoder;
    GRefPtr<GstElement> m_payloader;
    GRefPtr<GstElement> m_postEncoderQueue;
    GRefPtr<GstElement> m_capsFilter;
    mutable GRefPtr<GstCaps> m_allowedCaps;
    GRefPtr<GstWebRTCRTPTransceiver> m_transceiver;
    GRefPtr<GstWebRTCRTPSender> m_sender;
    GRefPtr<GstPad> m_webrtcSinkPad;

private:
    void sourceMutedChanged();
    void sourceEnabledChanged();

    virtual RTCRtpCapabilities rtpCapabilities() const = 0;
    virtual void codecPreferencesChanged(const GRefPtr<GstCaps>&) { }

    // MediaStreamTrackPrivate::Observer API
    void trackMutedChanged(MediaStreamTrackPrivate&) override { sourceMutedChanged(); }
    void trackEnabledChanged(MediaStreamTrackPrivate&) override { sourceEnabledChanged(); }
    void trackSettingsChanged(MediaStreamTrackPrivate&) override { initializeFromTrack(); }
    void trackEnded(MediaStreamTrackPrivate&) override { }
};

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
