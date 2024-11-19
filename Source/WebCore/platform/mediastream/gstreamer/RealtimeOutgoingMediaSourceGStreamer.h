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
#include "GStreamerRTPPacketizer.h"
#include "GStreamerWebRTCUtils.h"
#include "MediaStreamTrackPrivate.h"
#include "RTCRtpCapabilities.h"

#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class MediaStreamTrack;

class RealtimeOutgoingMediaSourceGStreamer : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<RealtimeOutgoingMediaSourceGStreamer, WTF::DestructionThread::Main>, public MediaStreamTrackPrivateObserver {
public:
    ~RealtimeOutgoingMediaSourceGStreamer();
    void start();
    void stop();

    const RefPtr<MediaStreamTrackPrivate>& track() const { return m_track; }

    const String& mediaStreamID() const { return m_mediaStreamId; }
    const GRefPtr<GstCaps>& allowedCaps() const;
    WARN_UNUSED_RETURN GRefPtr<GstCaps> rtpCaps() const;

    void link();
    const GRefPtr<GstPad>& pad() const { return m_webrtcSinkPad; }
    void setSinkPad(GRefPtr<GstPad>&&);

    GRefPtr<GstWebRTCRTPSender> sender() const { return m_sender; }
    GRefPtr<GstElement> bin() const { return m_bin; }

    bool configurePacketizers(GRefPtr<GstCaps>&&);

    GUniquePtr<GstStructure> parameters();
    void setInitialParameters(GUniquePtr<GstStructure>&&);
    void setParameters(GUniquePtr<GstStructure>&&);

    void configure(GRefPtr<GstCaps>&&);

    WARN_UNUSED_RETURN GUniquePtr<GstStructure> stats();

    virtual WARN_UNUSED_RETURN GRefPtr<GstPad> outgoingSourcePad() const = 0;
    virtual RefPtr<GStreamerRTPPacketizer> createPacketizer(RefPtr<UniqueSSRCGenerator>, const GstStructure*, GUniquePtr<GstStructure>&&) = 0;

    void replaceTrack(RefPtr<MediaStreamTrackPrivate>&&);

    virtual void teardown();

protected:
    enum Type {
        Audio,
        Video
    };
    explicit RealtimeOutgoingMediaSourceGStreamer(Type, const RefPtr<UniqueSSRCGenerator>&, const String& mediaStreamId, MediaStreamTrack&);
    explicit RealtimeOutgoingMediaSourceGStreamer(Type, const RefPtr<UniqueSSRCGenerator>&);

    void initializeSourceFromTrackPrivate();
    virtual void sourceEnabledChanged();

    bool isStopped() const { return m_isStopped; }

    bool linkPacketizer(RefPtr<GStreamerRTPPacketizer>&&);

    Type m_type;
    String m_mediaStreamId;
    String m_trackId;
    String m_mid;

    bool m_enabled { true };
    bool m_muted { false };
    bool m_isStopped { true };
    RefPtr<MediaStreamTrackPrivate> m_track;
    std::optional<RealtimeMediaSourceSettings> m_initialSettings;
    GRefPtr<GstElement> m_bin;
    GRefPtr<GstElement> m_outgoingSource;
    GRefPtr<GstElement> m_liveSync;
    GRefPtr<GstElement> m_tee;
    GRefPtr<GstElement> m_rtpFunnel;
    GRefPtr<GstElement> m_rtpCapsfilter;
    mutable GRefPtr<GstCaps> m_allowedCaps;
    GRefPtr<GstWebRTCRTPTransceiver> m_transceiver;
    GRefPtr<GstWebRTCRTPSender> m_sender;
    GRefPtr<GstPad> m_webrtcSinkPad;
    RefPtr<UniqueSSRCGenerator> m_ssrcGenerator;
    GUniquePtr<GstStructure> m_parameters;

    Vector<RefPtr<GStreamerRTPPacketizer>> m_packetizers;

private:
    void initialize();

    void sourceMutedChanged();

    void stopOutgoingSource();

    virtual bool linkTee() = 0;
    virtual RTCRtpCapabilities rtpCapabilities() const = 0;
    void codecPreferencesChanged();

    // MediaStreamTrackPrivateObserver API
    void trackMutedChanged(MediaStreamTrackPrivate&) override { sourceMutedChanged(); }
    void trackEnabledChanged(MediaStreamTrackPrivate&) override { sourceEnabledChanged(); }
    void trackSettingsChanged(MediaStreamTrackPrivate&) override { initializeSourceFromTrackPrivate(); }
    void trackEnded(MediaStreamTrackPrivate&) override { }

    void checkMid();

    struct ExtensionLookupResults {
        bool hasRtpStreamIdExtension { false };
        bool hasRtpRepairedStreamIdExtension { false };
        bool hasMidExtension { false };
        int lastIdentifier { 0 };
    };
    ExtensionLookupResults lookupRtpExtensions(const GstStructure*);

    void startUpdatingStats();
    void stopUpdatingStats();

    RefPtr<GStreamerRTPPacketizer> getPacketizerForRid(StringView);
};

} // namespace WebCore

#endif // USE(GSTREAMER_WEBRTC)
