/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaPlayerPrivateMediaSourceAVFObjC_h
#define MediaPlayerPrivateMediaSourceAVFObjC_h

#if ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#include "MediaPlayerPrivate.h"
#include "SourceBufferPrivateClient.h"
#include <wtf/MediaTime.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS AVAsset;
OBJC_CLASS AVSampleBufferAudioRenderer;
OBJC_CLASS AVSampleBufferDisplayLayer;
OBJC_CLASS AVSampleBufferRenderSynchronizer;
OBJC_CLASS AVStreamSession;

typedef struct OpaqueCMTimebase* CMTimebaseRef;

namespace WebCore {

class CDMSessionMediaSourceAVFObjC;
class PlatformClockCM;
class MediaSourcePrivateAVFObjC;

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
class VideoFullscreenLayerManager;
#endif

class MediaPlayerPrivateMediaSourceAVFObjC : public MediaPlayerPrivateInterface {
public:
    explicit MediaPlayerPrivateMediaSourceAVFObjC(MediaPlayer*);
    virtual ~MediaPlayerPrivateMediaSourceAVFObjC();

    static void registerMediaEngine(MediaEngineRegistrar);

    // MediaPlayer Factory Methods
    static bool isAvailable();
    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    void addDisplayLayer(AVSampleBufferDisplayLayer*);
    void removeDisplayLayer(AVSampleBufferDisplayLayer*);

    void addAudioRenderer(AVSampleBufferAudioRenderer*);
    void removeAudioRenderer(AVSampleBufferAudioRenderer*);

    MediaPlayer::NetworkState networkState() const override;
    MediaPlayer::ReadyState readyState() const override;
    void setReadyState(MediaPlayer::ReadyState);
    void setNetworkState(MediaPlayer::NetworkState);

    void seekInternal();
    void waitForSeekCompleted();
    void seekCompleted();
    void setLoadingProgresssed(bool flag) { m_loadingProgressed = flag; }
    void setHasAvailableVideoFrame(bool flag) { m_hasAvailableVideoFrame = flag; }
    void durationChanged();

    void effectiveRateChanged();
    void sizeChanged();
    void characteristicsChanged();

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    void setVideoFullscreenLayer(PlatformLayer*) override;
    void setVideoFullscreenFrame(FloatRect) override;
#endif

#if ENABLE(ENCRYPTED_MEDIA_V2)
    bool hasStreamSession() { return m_streamSession; }
    AVStreamSession *streamSession();
    void setCDMSession(CDMSession*) override;
    CDMSessionMediaSourceAVFObjC* cdmSession() const { return m_session; }
    void keyNeeded(Uint8Array*);
#endif

    WeakPtr<MediaPlayerPrivateMediaSourceAVFObjC> createWeakPtr() { return m_weakPtrFactory.createWeakPtr(); }

private:
    // MediaPlayerPrivateInterface
    void load(const String& url) override;
    void load(const String& url, MediaSourcePrivateClient*) override;
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) override;
#endif
    void cancelLoad() override;

    void prepareToPlay() override;
    PlatformMedia platformMedia() const override;
    PlatformLayer* platformLayer() const override;

    bool supportsFullscreen() const override { return true; }

    void play() override;
    void playInternal();

    void pause() override;
    void pauseInternal();

    bool paused() const override;

    void setVolume(float volume) override;
    bool supportsMuting() const override { return true; }
    void setMuted(bool) override;

    bool supportsScanning() const override;

    FloatSize naturalSize() const override;

    bool hasVideo() const override;
    bool hasAudio() const override;

    void setVisible(bool) override;

    MediaTime durationMediaTime() const override;
    MediaTime currentMediaTime() const override;
    MediaTime startTime() const override;
    MediaTime initialTime() const override;

    void seekWithTolerance(const MediaTime&, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold) override;
    bool seeking() const override;
    void setRateDouble(double) override;

    void setPreservesPitch(bool) override;

    std::unique_ptr<PlatformTimeRanges> seekable() const override;
    MediaTime maxMediaTimeSeekable() const override;
    MediaTime minMediaTimeSeekable() const override;
    std::unique_ptr<PlatformTimeRanges> buffered() const override;

    bool didLoadingProgress() const override;

    void setSize(const IntSize&) override;

    void paint(GraphicsContext&, const FloatRect&) override;
    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&) override;

    bool hasAvailableVideoFrame() const override;

    bool supportsAcceleratedRendering() const override;
    // called when the rendering system flips the into or out of accelerated rendering mode.
    void acceleratedRenderingStateChanged() override;

    MediaPlayer::MovieLoadType movieLoadType() const override;

    void prepareForRendering() override;

    String engineDescription() const override;

    String languageOfPrimaryAudioTrack() const override;

    size_t extraMemoryCost() const override;

    unsigned long totalVideoFrames() override;
    unsigned long droppedVideoFrames() override;
    unsigned long corruptedVideoFrames() override;
    MediaTime totalFrameDelay() override;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    bool isCurrentPlaybackTargetWireless() const override;
    void setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&&) override;
    void setShouldPlayToPlaybackTarget(bool) override;
    bool wirelessVideoPlaybackDisabled() const override { return false; }
#endif

    void ensureLayer();
    void destroyLayer();
    bool shouldBePlaying() const;

    friend class MediaSourcePrivateAVFObjC;

    struct PendingSeek {
        PendingSeek(const MediaTime& targetTime, const MediaTime& negativeThreshold, const MediaTime& positiveThreshold)
            : targetTime(targetTime)
            , negativeThreshold(negativeThreshold)
            , positiveThreshold(positiveThreshold)
        {
        }
        MediaTime targetTime;
        MediaTime negativeThreshold;
        MediaTime positiveThreshold;
    };
    std::unique_ptr<PendingSeek> m_pendingSeek;

    MediaPlayer* m_player;
    WeakPtrFactory<MediaPlayerPrivateMediaSourceAVFObjC> m_weakPtrFactory;
    RefPtr<MediaSourcePrivateAVFObjC> m_mediaSourcePrivate;
    RetainPtr<AVAsset> m_asset;
    RetainPtr<AVSampleBufferDisplayLayer> m_sampleBufferDisplayLayer;
    Vector<RetainPtr<AVSampleBufferAudioRenderer>> m_sampleBufferAudioRenderers;
    RetainPtr<AVSampleBufferRenderSynchronizer> m_synchronizer;
    RetainPtr<id> m_timeJumpedObserver;
    RetainPtr<id> m_durationObserver;
    RetainPtr<AVStreamSession> m_streamSession;
    Timer m_seekTimer;
    CDMSessionMediaSourceAVFObjC* m_session;
    MediaPlayer::NetworkState m_networkState;
    MediaPlayer::ReadyState m_readyState;
    MediaTime m_lastSeekTime;
    double m_rate;
    bool m_playing;
    bool m_seeking;
    bool m_seekCompleted;
    mutable bool m_loadingProgressed;
    bool m_hasAvailableVideoFrame;
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    RefPtr<MediaPlaybackTarget> m_playbackTarget;
    bool m_shouldPlayToTarget { false };
#endif
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    std::unique_ptr<VideoFullscreenLayerManager> m_videoFullscreenLayerManager;
#endif
};

}

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#endif // MediaPlayerPrivateMediaSourceAVFObjC_h

