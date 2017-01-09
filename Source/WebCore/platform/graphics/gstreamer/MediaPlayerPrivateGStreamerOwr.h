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

#if ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER) && USE(OPENWEBRTC)

#include "AudioTrackPrivateMediaStream.h"
#include "MediaPlayerPrivateGStreamerBase.h"
#include "MediaStreamTrackPrivate.h"
#include "VideoTrackPrivateMediaStream.h"

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

    void setSize(const IntSize&) final;

    FloatSize naturalSize() const final;

private:
    GstElement* createVideoSink() final;
    GstElement* audioSink() const final { return m_audioSink.get(); }
    bool isLiveStream() const final { return true; }

    String engineDescription() const final { return "OpenWebRTC"; }

    void load(const String&) final;
#if ENABLE(MEDIA_SOURCE)
    void load(const String&, MediaSourcePrivateClient*) final;
#endif
    void load(MediaStreamPrivate&) final;
    void cancelLoad() final { }

    void prepareToPlay() final { }
    void play() final;
    void pause() final;

    bool hasVideo() const final;
    bool hasAudio() const final;

    float duration() const final { return 0; }

    float currentTime() const final;
    void seek(float) final { }
    bool seeking() const final { return false; }

    void setRate(float) final { }
    void setPreservesPitch(bool) final { }
    bool paused() const final { return m_paused; }

    void setVolume(float) final;
    void setMuted(bool) final;

    bool hasClosedCaptions() const final { return false; }
    void setClosedCaptionsVisible(bool) final { };

    float maxTimeSeekable() const final { return 0; }
    std::unique_ptr<PlatformTimeRanges> buffered() const final { return std::make_unique<PlatformTimeRanges>(); }
    bool didLoadingProgress() const final;

    unsigned long long totalBytes() const final { return 0; }

    bool canLoadPoster() const final { return false; }
    void setPoster(const String&) final { }
    bool ended() const final { return m_ended; }

    // MediaStreamTrackPrivate::Observer implementation.
    void trackEnded(MediaStreamTrackPrivate&) final;
    void trackMutedChanged(MediaStreamTrackPrivate&) final;
    void trackSettingsChanged(MediaStreamTrackPrivate&) final;
    void trackEnabledChanged(MediaStreamTrackPrivate&) final;

    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>&);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);
    static bool initializeGStreamerAndGStreamerDebugging();
    void createGSTAudioSinkBin();
    void loadingFailed(MediaPlayer::NetworkState error);
    void stop();
    void maybeHandleChangeMutedState(MediaStreamTrackPrivate&);
    void disableMediaTracks();

    bool m_paused { true };
    bool m_ended { false };
    RefPtr<MediaStreamTrackPrivate> m_videoTrack;
    RefPtr<MediaStreamTrackPrivate> m_audioTrack;
    GRefPtr<GstElement> m_audioSink;
    RefPtr<MediaStreamPrivate> m_streamPrivate;
    GRefPtr<OwrGstVideoRenderer> m_videoRenderer;
    GRefPtr<OwrGstAudioRenderer> m_audioRenderer;

    HashMap<String, RefPtr<AudioTrackPrivateMediaStream>> m_audioTrackMap;
    HashMap<String, RefPtr<VideoTrackPrivateMediaStream>> m_videoTrackMap;
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER) && USE(OPENWEBRTC)

#endif // MediaPlayerPrivateGStreamerOwr_h
