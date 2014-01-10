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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

OBJC_CLASS AVAsset;
OBJC_CLASS AVSampleBufferAudioRenderer;
OBJC_CLASS AVSampleBufferDisplayLayer;
OBJC_CLASS AVSampleBufferRenderSynchronizer;

typedef struct OpaqueCMTimebase* CMTimebaseRef;

namespace WebCore {

class PlatformClockCM;
class MediaSourcePrivateAVFObjC;

class MediaPlayerPrivateMediaSourceAVFObjC : public MediaPlayerPrivateInterface {
public:
    MediaPlayerPrivateMediaSourceAVFObjC(MediaPlayer*);
    virtual ~MediaPlayerPrivateMediaSourceAVFObjC();

    static void registerMediaEngine(MediaEngineRegistrar);

    void addDisplayLayer(AVSampleBufferDisplayLayer*);
    void removeDisplayLayer(AVSampleBufferDisplayLayer*);

    void addAudioRenderer(AVSampleBufferAudioRenderer*);
    void removeAudioRenderer(AVSampleBufferAudioRenderer*);

    virtual MediaPlayer::NetworkState networkState() const OVERRIDE;
    virtual MediaPlayer::ReadyState readyState() const OVERRIDE;
    void setReadyState(MediaPlayer::ReadyState);
    void setNetworkState(MediaPlayer::NetworkState);

    void seekInternal(double, double, double);
    void setLoadingProgresssed(bool flag) { m_loadingProgressed = flag; }
    void setHasAvailableVideoFrame(bool flag) { m_hasAvailableVideoFrame = flag; }
    void durationChanged();

    void effectiveRateChanged();
    void sizeChanged();

private:
    // MediaPlayerPrivateInterface
    virtual void load(const String& url) OVERRIDE;
    virtual void load(const String& url, PassRefPtr<HTMLMediaSource>) OVERRIDE;
    virtual void cancelLoad() OVERRIDE;

    virtual void prepareToPlay() OVERRIDE;
    virtual PlatformMedia platformMedia() const OVERRIDE;
#if USE(ACCELERATED_COMPOSITING)
    virtual PlatformLayer* platformLayer() const OVERRIDE;
#endif

    virtual void play() OVERRIDE;
    void playInternal();

    virtual void pause() OVERRIDE;
    void pauseInternal();

    virtual bool paused() const OVERRIDE;

    virtual void setVolume(float volume) OVERRIDE;
    virtual bool supportsMuting() const OVERRIDE { return true; }
    virtual void setMuted(bool) OVERRIDE;

    virtual bool supportsScanning() const OVERRIDE;

    virtual IntSize naturalSize() const OVERRIDE;

    virtual bool hasVideo() const OVERRIDE;
    virtual bool hasAudio() const OVERRIDE;

    virtual void setVisible(bool) OVERRIDE;

    virtual double durationDouble() const OVERRIDE;
    virtual double currentTimeDouble() const OVERRIDE;
    virtual double startTimeDouble() const OVERRIDE;
    virtual double initialTime() const OVERRIDE;

    virtual void seekWithTolerance(double time, double negativeThreshold, double positiveThreshold) OVERRIDE;
    virtual bool seeking() const OVERRIDE;
    virtual void setRateDouble(double) OVERRIDE;

    virtual PassRefPtr<TimeRanges> seekable() const OVERRIDE;
    virtual double maxTimeSeekableDouble() const OVERRIDE;
    virtual double minTimeSeekable() const OVERRIDE;
    virtual PassRefPtr<TimeRanges> buffered() const OVERRIDE;

    virtual bool didLoadingProgress() const OVERRIDE;

    virtual void setSize(const IntSize&) OVERRIDE;

    virtual void paint(GraphicsContext*, const IntRect&) OVERRIDE;
    virtual void paintCurrentFrameInContext(GraphicsContext*, const IntRect&) OVERRIDE;

    virtual bool hasAvailableVideoFrame() const OVERRIDE;

#if USE(ACCELERATED_COMPOSITING)
    virtual bool supportsAcceleratedRendering() const OVERRIDE;
    // called when the rendering system flips the into or out of accelerated rendering mode.
    virtual void acceleratedRenderingStateChanged() OVERRIDE;
#endif

    virtual MediaPlayer::MovieLoadType movieLoadType() const OVERRIDE;

    virtual void prepareForRendering() OVERRIDE;

    virtual String engineDescription() const OVERRIDE;

    virtual String languageOfPrimaryAudioTrack() const OVERRIDE;

    virtual size_t extraMemoryCost() const OVERRIDE;

    virtual unsigned long totalVideoFrames() OVERRIDE;
    virtual unsigned long droppedVideoFrames() OVERRIDE;
    virtual unsigned long corruptedVideoFrames() OVERRIDE;
    virtual double totalFrameDelay() OVERRIDE;

    void ensureLayer();
    void destroyLayer();

    // MediaPlayer Factory Methods
    static PassOwnPtr<MediaPlayerPrivateInterface> create(MediaPlayer*);
    static bool isAvailable();
    static void getSupportedTypes(HashSet<String>& types);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    friend class MediaSourcePrivateAVFObjC;

    MediaPlayer* m_player;
    RefPtr<HTMLMediaSource> m_mediaSource;
    RefPtr<MediaSourcePrivateAVFObjC> m_mediaSourcePrivate;
    RetainPtr<AVAsset> m_asset;
    RetainPtr<AVSampleBufferDisplayLayer> m_sampleBufferDisplayLayer;
    Vector<RetainPtr<AVSampleBufferAudioRenderer>> m_sampleBufferAudioRenderers;
    RetainPtr<AVSampleBufferRenderSynchronizer> m_synchronizer;
    RetainPtr<id> m_timeJumpedObserver;
    MediaPlayer::NetworkState m_networkState;
    MediaPlayer::ReadyState m_readyState;
    double m_rate;
    bool m_playing;
    bool m_seeking;
    mutable bool m_loadingProgressed;
    bool m_hasAvailableVideoFrame;
};

}

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#endif // MediaPlayerPrivateMediaSourceAVFObjC_h

