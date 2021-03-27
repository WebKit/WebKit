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
#include "WebKitMediaSourceGStreamer.h"

namespace WebCore {

class AppendPipeline;
class PlaybackPipeline;

class MediaPlayerPrivateGStreamerMSE : public MediaPlayerPrivateGStreamer {
    WTF_MAKE_NONCOPYABLE(MediaPlayerPrivateGStreamerMSE); WTF_MAKE_FAST_ALLOCATED;

public:
    explicit MediaPlayerPrivateGStreamerMSE(MediaPlayer*);
    virtual ~MediaPlayerPrivateGStreamerMSE();

    static void registerMediaEngine(MediaEngineRegistrar);

    void load(const String&) override;
    void load(const URL&, const ContentType&, MediaSourcePrivateClient*) override;

    void updateDownloadBufferingFlag() override { };

    MediaTime currentMediaTime() const override;

    void pause() override;
    bool seeking() const override;
    void seek(const MediaTime&) override;
    void configurePlaySink() override;
    bool changePipelineState(GstState) override;

    void durationChanged() override;
    MediaTime durationMediaTime() const override;

    std::unique_ptr<PlatformTimeRanges> buffered() const override;
    MediaTime maxMediaTimeSeekable() const override;

    void sourceSetup(GstElement*) override;

    void setReadyState(MediaPlayer::ReadyState);
    void waitForSeekCompleted();
    void seekCompleted();
    MediaSourcePrivateClient* mediaSourcePrivateClient() { return m_mediaSource.get(); }

    void markEndOfStream(MediaSourcePrivate::EndOfStreamStatus);

    void trackDetected(AppendPipeline&, RefPtr<WebCore::TrackPrivateBase>, bool firstTrackDetected);
    void notifySeekNeedsDataForTime(const MediaTime&);

    void blockDurationChanges();
    void unblockDurationChanges();

    PlaybackPipeline* playbackPipeline() const { return m_playbackPipeline.get(); }

#if !RELEASE_LOG_DISABLED
    WTFLogChannel& logChannel() const final { return WebCore::LogMediaSource; }
#endif

private:
    friend class MediaPlayerFactoryGStreamerMSE;
    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>&);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    // FIXME: Reduce code duplication.
    void updateStates() override;

    bool doSeek(const MediaTime&, float, GstSeekFlags) override;
    void maybeFinishSeek();

    void asyncStateChangeDone() override;

    // FIXME: Implement videoPlaybackQualityMetrics.
    bool isTimeBuffered(const MediaTime&) const;

    bool isMediaSource() const override { return true; }

    bool m_eosMarked = false;
    mutable bool m_eosPending = false;
    bool m_gstSeekCompleted = true;
    RefPtr<MediaSourcePrivateClient> m_mediaSource;
    RefPtr<MediaSourcePrivateGStreamer> m_mediaSourcePrivate;
    MediaTime m_mediaTimeDuration;
    bool m_mseSeekCompleted = true;
    bool m_areDurationChangesBlocked = false;
    bool m_shouldReportDurationWhenUnblocking = false;
    RefPtr<PlaybackPipeline> m_playbackPipeline;
};

} // namespace WebCore

#endif // USE(GSTREAMER)
