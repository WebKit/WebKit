/*
 *  Copyright (C) 2015 Igalia S.L. All rights reserved.
 *  Copyright (C) 2015 Metrological. All rights reserved.
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

#ifndef MediaPlayerPrivateGStreamerOwr_h
#define MediaPlayerPrivateGStreamerOwr_h

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER) && USE(OPENWEBRTC)

#include "MediaPlayerPrivateGStreamerBase.h"
#include "MediaStreamTrackPrivate.h"

typedef struct _OwrGstVideoRenderer OwrGstVideoRenderer;
typedef struct _OwrGstAudioRenderer OwrGstAudioRenderer;

namespace WebCore {

class MediaStreamPrivate;
class RealtimeMediaSourceOwr;

class MediaPlayerPrivateGStreamerOwr : public MediaPlayerPrivateGStreamerBase, private MediaStreamTrackPrivate::Observer {
public:
    explicit MediaPlayerPrivateGStreamerOwr(MediaPlayer*);
    ~MediaPlayerPrivateGStreamerOwr();

    static void registerMediaEngine(MediaEngineRegistrar);

private:
    GstElement* createVideoSink() override;
    GstElement* audioSink() const override { return m_audioSink.get(); }
    bool isLiveStream() const override { return true; }

    String engineDescription() const override { return "OpenWebRTC"; }

    void load(const String&) override;
#if ENABLE(MEDIA_SOURCE)
    void load(const String&, MediaSourcePrivateClient*) override { }
#endif
    void load(MediaStreamPrivate&) override;
    void cancelLoad() override { }

    void prepareToPlay() override { }
    void play() override;
    void pause() override;

    bool hasVideo() const override;
    bool hasAudio() const override;

    float duration() const override { return 0; }

    float currentTime() const override;
    void seek(float) override { }
    bool seeking() const override { return false; }

    void setRate(float) override { }
    void setPreservesPitch(bool) override { }
    bool paused() const override { return m_paused; }

    bool hasClosedCaptions() const override { return false; }
    void setClosedCaptionsVisible(bool) override { };

    float maxTimeSeekable() const override { return 0; }
    std::unique_ptr<PlatformTimeRanges> buffered() const override { return std::make_unique<PlatformTimeRanges>(); }
    bool didLoadingProgress() const override;

    unsigned long long totalBytes() const override { return 0; }

    bool canLoadPoster() const override { return false; }
    void setPoster(const String&) override { }

    // MediaStreamTrackPrivate::Observer implementation.
    void trackEnded(MediaStreamTrackPrivate&) override final;
    void trackMutedChanged(MediaStreamTrackPrivate&) override final;
    void trackSettingsChanged(MediaStreamTrackPrivate&) override final;
    void trackEnabledChanged(MediaStreamTrackPrivate&) override final;

    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>&);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);
    static bool initializeGStreamerAndGStreamerDebugging();
    void createGSTAudioSinkBin();
    void loadingFailed(MediaPlayer::NetworkState error);
    bool internalLoad();
    void stop();

    bool m_paused { true };
    bool m_stopped { true };
    RefPtr<MediaStreamTrackPrivate> m_videoTrack;
    RefPtr<MediaStreamTrackPrivate> m_audioTrack;
    GRefPtr<GstElement> m_audioSink;
    RefPtr<MediaStreamPrivate> m_streamPrivate;
    GRefPtr<OwrGstVideoRenderer> m_videoRenderer;
    GRefPtr<OwrGstAudioRenderer> m_audioRenderer;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER) && USE(OPENWEBRTC)

#endif // MediaPlayerPrivateGStreamerOwr_h
