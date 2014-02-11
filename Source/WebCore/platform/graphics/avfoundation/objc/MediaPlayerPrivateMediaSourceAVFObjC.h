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
#include <wtf/WeakPtr.h>

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

    virtual MediaPlayer::NetworkState networkState() const override;
    virtual MediaPlayer::ReadyState readyState() const override;
    void setReadyState(MediaPlayer::ReadyState);
    void setNetworkState(MediaPlayer::NetworkState);

    void seekInternal(double, double, double);
    void setLoadingProgresssed(bool flag) { m_loadingProgressed = flag; }
    void setHasAvailableVideoFrame(bool flag) { m_hasAvailableVideoFrame = flag; }
    void durationChanged();

    void effectiveRateChanged();
    void sizeChanged();

#if ENABLE(ENCRYPTED_MEDIA_V2)
    void keyNeeded(Uint8Array*);
#endif

private:
    // MediaPlayerPrivateInterface
    virtual void load(const String& url) override;
    virtual void load(const String& url, MediaSourcePrivateClient*) override;
    virtual void cancelLoad() override;

    virtual void prepareToPlay() override;
    virtual PlatformMedia platformMedia() const override;
    virtual PlatformLayer* platformLayer() const override;

    virtual void play() override;
    void playInternal();

    virtual void pause() override;
    void pauseInternal();

    virtual bool paused() const override;

    virtual void setVolume(float volume) override;
    virtual bool supportsMuting() const override { return true; }
    virtual void setMuted(bool) override;

    virtual bool supportsScanning() const override;

    virtual IntSize naturalSize() const override;

    virtual bool hasVideo() const override;
    virtual bool hasAudio() const override;

    virtual void setVisible(bool) override;

    virtual double durationDouble() const override;
    virtual double currentTimeDouble() const override;
    virtual double startTimeDouble() const override;
    virtual double initialTime() const override;

    virtual void seekWithTolerance(double time, double negativeThreshold, double positiveThreshold) override;
    virtual bool seeking() const override;
    virtual void setRateDouble(double) override;

    virtual PassRefPtr<TimeRanges> seekable() const override;
    virtual double maxTimeSeekableDouble() const override;
    virtual double minTimeSeekable() const override;
    virtual PassRefPtr<TimeRanges> buffered() const override;

    virtual bool didLoadingProgress() const override;

    virtual void setSize(const IntSize&) override;

    virtual void paint(GraphicsContext*, const IntRect&) override;
    virtual void paintCurrentFrameInContext(GraphicsContext*, const IntRect&) override;

    virtual bool hasAvailableVideoFrame() const override;

    virtual bool supportsAcceleratedRendering() const override;
    // called when the rendering system flips the into or out of accelerated rendering mode.
    virtual void acceleratedRenderingStateChanged() override;

    virtual MediaPlayer::MovieLoadType movieLoadType() const override;

    virtual void prepareForRendering() override;

    virtual String engineDescription() const override;

    virtual String languageOfPrimaryAudioTrack() const override;

    virtual size_t extraMemoryCost() const override;

    virtual unsigned long totalVideoFrames() override;
    virtual unsigned long droppedVideoFrames() override;
    virtual unsigned long corruptedVideoFrames() override;
    virtual double totalFrameDelay() override;

    void ensureLayer();
    void destroyLayer();

    WeakPtr<MediaPlayerPrivateMediaSourceAVFObjC> createWeakPtr() { return m_weakPtrFactory.createWeakPtr(); }

    // MediaPlayer Factory Methods
    static PassOwnPtr<MediaPlayerPrivateInterface> create(MediaPlayer*);
    static bool isAvailable();
    static void getSupportedTypes(HashSet<String>& types);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    friend class MediaSourcePrivateAVFObjC;

    MediaPlayer* m_player;
    WeakPtrFactory<MediaPlayerPrivateMediaSourceAVFObjC> m_weakPtrFactory;
    RefPtr<MediaSourcePrivateClient> m_mediaSource;
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

