/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009, 2010, 2016 Igalia S.L
 * Copyright (C) 2015 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015, 2016 Metrological Group B.V.
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

#if ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(MEDIA_SOURCE)

#include "GStreamerCommon.h"
#include "MediaPlayerPrivateGStreamer.h"
#include "MediaSample.h"
#include "MediaSourcePrivateGStreamer.h"

struct WebKitMediaSrc;

namespace WebCore {

class AppendPipeline;
class TrackQueue;
class MediaSourceTrackGStreamer;

class MediaPlayerPrivateGStreamerMSE : public MediaPlayerPrivateGStreamer {

public:
    Ref<MediaPlayerPrivateGStreamerMSE> create(MediaPlayer* player) { return adoptRef(*new MediaPlayerPrivateGStreamerMSE(player)); }
    virtual ~MediaPlayerPrivateGStreamerMSE();

    static void registerMediaEngine(MediaEngineRegistrar);

    void load(const String&) override;
    void load(const URL&, const ContentType&, MediaSourcePrivateClient&) override;

    void updateDownloadBufferingFlag() override { };

    void play() override;
    void pause() override;
    void seekToTarget(const SeekTarget&) override;
    bool doSeek(const SeekTarget&, float rate) override;

    void updatePipelineState(GstState);

    void durationChanged() override;
    MediaTime durationMediaTime() const override;

    const PlatformTimeRanges& buffered() const override;
    MediaTime maxMediaTimeSeekable() const override;
    bool currentMediaTimeMayProgress() const override;
    void notifyActiveSourceBuffersChanged() final;

    void sourceSetup(GstElement*) override;

    // return false to avoid false-positive "stalled" event - it should be soon addressed in the spec
    // see: https://github.com/w3c/media-source/issues/88
    // see: https://w3c.github.io/media-source/#h-note-19
    bool supportsProgressMonitoring() const override { return false; }

    void setNetworkState(MediaPlayer::NetworkState);
    void setReadyState(MediaPlayer::ReadyState);

    void setInitialVideoSize(const FloatSize&);

    void didPreroll() override;

    void startSource(const Vector<RefPtr<MediaSourceTrackGStreamer>>& tracks);
    WebKitMediaSrc* webKitMediaSrc() { return reinterpret_cast<WebKitMediaSrc*>(m_source.get()); }

#if !RELEASE_LOG_DISABLED
    WTFLogChannel& logChannel() const final { return WebCore::LogMediaSource; }
#endif

private:
    explicit MediaPlayerPrivateGStreamerMSE(MediaPlayer*);

    friend class MediaPlayerFactoryGStreamerMSE;
    static void getSupportedTypes(HashSet<String>&);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    friend class AppendPipeline;
    friend class SourceBufferPrivateGStreamer;
    friend class MediaSourcePrivateGStreamer;

    void updateStates() override;

    // FIXME: Implement videoPlaybackQualityMetrics.
    bool isTimeBuffered(const MediaTime&) const;

    bool isMediaSource() const override { return true; }

    void propagateReadyStateToPlayer();

    RefPtr<MediaSourcePrivateGStreamer> m_mediaSourcePrivate;
    MediaTime m_mediaTimeDuration { MediaTime::invalidTime() };
    Vector<RefPtr<MediaSourceTrackGStreamer>> m_tracks;

    bool m_isWaitingForPreroll = true;
    MediaPlayer::ReadyState m_mediaSourceReadyState = MediaPlayer::ReadyState::HaveNothing;
    MediaPlayer::NetworkState m_mediaSourceNetworkState = MediaPlayer::NetworkState::Empty;
};

} // namespace WebCore

#endif // USE(GSTREAMER)
