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
#include "MediaSourceGStreamer.h"
#include "WebKitMediaSourceGStreamer.h"

namespace WebCore {

class MediaSourceClientGStreamerMSE;
class AppendPipeline;
class PlaybackPipeline;

class MediaPlayerPrivateGStreamerMSE : public MediaPlayerPrivateGStreamer {
    WTF_MAKE_NONCOPYABLE(MediaPlayerPrivateGStreamerMSE); WTF_MAKE_FAST_ALLOCATED;

    friend class MediaSourceClientGStreamerMSE;

public:
    explicit MediaPlayerPrivateGStreamerMSE(MediaPlayer*);
    virtual ~MediaPlayerPrivateGStreamerMSE();

    static void registerMediaEngine(MediaEngineRegistrar);

    void load(const String&) override;
    void load(const String&, MediaSourcePrivateClient*) override;

    void updateDownloadBufferingFlag() override { };

    bool isLiveStream() const override { return false; }

    void play() override;
    void pause() override;
    void seek(const MediaTime&) override;
    void reportSeekCompleted();
    void updatePipelineState(GstState);

    void durationChanged() override;
    MediaTime durationMediaTime() const override;

    void setRate(float) override;
    std::unique_ptr<PlatformTimeRanges> buffered() const override;
    MediaTime maxMediaTimeSeekable() const override;

    void sourceSetup(GstElement*) override;

    void setReadyState(MediaPlayer::ReadyState);
    MediaSourcePrivateClient* mediaSourcePrivateClient() { return m_mediaSource.get(); }

    void trackDetected(RefPtr<AppendPipeline>, RefPtr<WebCore::TrackPrivateBase>, bool firstTrackDetected);

    void blockDurationChanges();
    void unblockDurationChanges();

    void asyncStateChangeDone() override { }

protected:
    void didEnd() override;

private:
    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>&);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    void updateStates() override;

    // FIXME: Implement videoPlaybackQualityMetrics.
    bool isTimeBuffered(const MediaTime&) const;

    bool isMediaSource() const override { return true; }

    void setMediaSourceClient(Ref<MediaSourceClientGStreamerMSE>);

    HashMap<RefPtr<SourceBufferPrivateGStreamer>, RefPtr<AppendPipeline>> m_appendPipelinesMap;
    RefPtr<MediaSourcePrivateClient> m_mediaSource;
    RefPtr<MediaSourceClientGStreamerMSE> m_mediaSourceClient;
    MediaTime m_mediaTimeDuration;
    bool m_areDurationChangesBlocked = false;
    bool m_shouldReportDurationWhenUnblocking = false;
    bool m_isPipelinePlaying = true;
};

} // namespace WebCore

#endif // USE(GSTREAMER)
